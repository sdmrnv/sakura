#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"
#include "unistd.h"
#include "sys/socket.h"
#include "sys/un.h"
#include "netinet/in.h"
#include "arpa/inet.h"

#ifdef UNIX
#define SOC_FILE            "/var/run/soc"
#endif

#define MAX_LEN           4000
#define TEXT_LEN          3730


int _get_byte4(unsigned char *a)
{
  return  a[0] | (a[1] << 8) | (a[2] << 16) | (a[3] << 24);
}

void write_answer(int soc, char typ, char *a)
{
  if (typ == 'G')
  {
    char dummy[3];
    dummy[0] = 'S';
    dummy[1] = 'O';
    dummy[2] = 'G';
    write(soc, dummy, 3);
    return;
  }

  if (typ == 'D')
  {
    char dummy[3];
    dummy[0] = 'S';
    dummy[1] = 'O';
    dummy[2] = 'D';
    write(soc, dummy, 3);
    return;
  }


  int n = strlen(a);
  unsigned char d[n+7]; //+8 if we uncomment d[i+7] = '\0';
  int rc;

  d[0] = 'S';  //(S)akura protocol
  d[1] = 'O';  //(O)racle database
  d[2] = typ;

  d[3] = n & 0xFF;
  d[4] = (n >> 8) & 0xFF;
  d[5] = (n >> 16) & 0xFF;
  d[6] = (n >> 24) & 0xFF;

  int i=0;
  while(a[i]){
    d[i+7] = a[i];
    i++;
  }
  //d[i+7] = '\0';

  rc = write(soc, d, i+7);
  if (rc<0)
  {
    perror("write answer failure");
    exit(EXIT_FAILURE);
  }
}



int main(void)
{
  int soc;
#ifdef UNIX
  struct sockaddr_un addr;
#else
  struct sockaddr_in addr;
#endif

  unsigned char packet_len[4];

  int rc;

  char header[3];
  char answer[MAX_LEN];

#ifdef UNIX
  soc = socket(AF_UNIX, SOCK_STREAM, 0);
  if(soc == -1) {
    perror("get socket failed");
    exit(EXIT_FAILURE);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, SOC_FILE);
#else
  soc = socket(AF_INET, SOCK_STREAM, 0);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(2733);
#endif

  rc = connect(soc, (struct sockaddr *)&addr, sizeof(addr));
  if (rc == -1) {
    perror("client connect failed");
    exit(EXIT_FAILURE);
  }


  char s[100] = "pax/pax";
  int len;

//connect

  write_answer(soc, 'C', s);

  rc = read(soc, header, 3);
  if (rc == -1) {
    perror("client read header for connect failed");
    exit(EXIT_FAILURE);
  }

  rc = read(soc, packet_len, 4);
  if (rc == -1) {
    perror("client read packet_len for connect failed");
    exit(EXIT_FAILURE);
  }

  len = _get_byte4(packet_len);
  rc = read(soc, answer, len);
  if (rc == -1) {
    perror("client read packet_len for connect failed");
    exit(EXIT_FAILURE);
  }
  answer[len] = '\0';

//select
  //char sel[100] = "select f2 from t1 where f1=7";

  char sel[300] = "select n, praenomen, brevis, dsc, fd  from praenomen_masc where sysdate between fd and td and rownum<=2"; 
  write_answer(soc, 'S', sel);

  rc = read(soc, header, 3);
  if (rc == -1) {
    perror("client read header for select failed");
    exit(EXIT_FAILURE);
  }

  rc = read(soc, packet_len, 4);
  if (rc == -1) {
    perror("client read packet_len for select failed");
    exit(EXIT_FAILURE);
  }

  len = _get_byte4(packet_len);
  rc = read(soc, answer, len);
  if (rc == -1) {
    perror("client read packet_len for select failed");
    exit(EXIT_FAILURE);
  }
  answer[len] = '\0';
  printf("%s\n", answer);


//disconnect
  write_answer(soc, 'D', s);

  rc = read(soc, header, 3);
  if (rc == -1) {
    perror("client read header for disconnect failed");
    exit(EXIT_FAILURE);
  }

  rc = read(soc, packet_len, 4);
  if (rc == -1) {
    perror("client read packet_len for disconnect failed");
    exit(EXIT_FAILURE);
  }

  len = _get_byte4(packet_len);
  rc = read(soc, answer, len);
  if (rc == -1) {
    perror("client read packet_len for disconnect failed");
    exit(EXIT_FAILURE);
  }
//  printf("%s\n", answer);



/*
  write_answer(soc, 'G', s);

  rc = read(soc, header, 3);
  if (rc == -1) {
    perror("client read header for disconnect failed");
    exit(EXIT_FAILURE);
  }

  rc = read(soc, packet_len, 4);
  if (rc == -1) {
    perror("client read packet_len for disconnect failed");
    exit(EXIT_FAILURE);
  }

  len = _get_byte4(packet_len);

  rc = read(soc, answer, len);
  if (rc == -1) {
    perror("client read packet_len for connect failed");
    exit(EXIT_FAILURE);
  }
  answer[TEXT_LEN] = '\0';
  printf("%s\n", answer);
*/

  close(soc);
  exit(EXIT_SUCCESS);
}