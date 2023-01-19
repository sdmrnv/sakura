/*
 * Copyright (C) 2021-2022 sdmrnv
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS 'AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "soc.h"
#include "ora.h"


#ifdef MULTI
int main(int argc, char *argv[])
{
  set_process_title(argv[0],"sakura master process");

  int     CORE_CNT = CPU_MULT*get_nprocs();
  pid_t   pid;
  int     i;

  for(i=0;i<CORE_CNT;i++){
    pid = fork();
    if (pid==0){
      set_process_title(argv[0],"sakura worker process");
      worker_process();
    }
  }

  for(i=0;i<CORE_CNT;i++){
    int chstatus = 0;
    wait(&chstatus);
  }

  exit(EXIT_SUCCESS);
}

void worker_process()
{
#else
int main(int argc, char *argv[])
{
  set_process_title(argv[0],"sakura application server");
#endif

// --- main/worker -----------------------------
#ifdef DEBUG
  fptr = fopen(LOG_FILE, "a");
  time(&t);
  fprintf(fptr,"\n\n======== START SAKURA SERVER ========= |%s", ctime(&t));
  fflush(fptr);
#endif

  //declare local vars
#ifdef UNIX
  struct sockaddr_un      addr;
  struct sockaddr_un      peer_addr;
#else
  struct sockaddr_in      addr;
  struct sockaddr_in      peer_addr;
  int                     opt;
#endif
  socklen_t               peer_addr_size = sizeof(peer_addr);
  int                     connected_soc;

  int                     nfds, n;
  struct                  epoll_event ev, events[MAX_EVENTS];

  int                     rc;
  char                    header[3];

#ifndef NF
  pid_t                   pid_op;
#endif

  //initialize ORACLE
  ora_init();
  _D("ORACLE initialized successfully");

  //avoid zombie process
  signal(SIGCHLD, SIG_IGN);

  //define signals handlers
  signal(SIGTERM, sig_handler);
 
  //define signal for reload
  signal(SIGUSR1, sig_handler);

#ifdef UNIX
  unlink(SOC_FILE);

  //define server address
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, SOC_FILE);

  //create listen socket
  listen_soc = socket(AF_UNIX, SOCK_STREAM, 0);
//  listen_soc = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if(listen_soc == -1) {
    perror("get listen socket failed");
    exit(EXIT_FAILURE);
  }
#else
  //define server address
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(2733);

  //create listen socket
  listen_soc = socket(AF_INET, SOCK_STREAM, 0);
//  listen_soc = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if(listen_soc == -1) {
    perror("get listen socket failed");
    exit(EXIT_FAILURE);
  }

  //set REUSE_PORT for listen socket
  opt = 1;
  ASSERT(setsockopt(listen_soc, SOL_SOCKET, SO_REUSEPORT, (int *)&opt, sizeof(opt)), "setsockopt failed");
#endif

  //bind listen socket
  ASSERT(bind(listen_soc,(struct sockaddr *) &addr, sizeof(addr)), "bing listen socket failed");

  //run listen
  ASSERT(listen(listen_soc, LISTEN_BACKLOG), "listen for listen_socket failed");

  _D("listen_socket %d created and listen", listen_soc);

  //create epoll_fd_set
  epollfd = epoll_create1(0);
  if (epollfd == -1) {
    perror("sakura epoll_create1 problem");
    exit(EXIT_FAILURE);
  }

  //add listen socket (with event type) into epoll_fd_set
  ev.events = EPOLLIN;
//  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = listen_soc;
  ASSERT(epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_soc, &ev), "epoll_add for listen socket failed");

  _D("epoll fd set %d created and initializeted with listen socket %d", epollfd, listen_soc);


  _D("run infinite loop");
  while(1)
  {
    nfds = epoll_wait(epollfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
    _D("  epoll_wait unblocked with %d event(s)", nfds);
    if(nfds == -1){
      if(errno == EINTR){
        _D("  EINTR raised");
        continue;
      }
      perror("sakura epoll_wait problem");
      exit(EXIT_FAILURE);
    }

    _D("  let process theese event(s)");
    for(n=0;n<nfds;n++){
       _D("    start process event %d (0-based index)", n);
       if(events[n].events & EPOLLIN) //catch all events contained EPOLLIN, e.g. EPOLLIN|EPOLLHUP, EPOLLIN|EPOLLERR|EPOLLHUP, etc.
       {
         _D("      event %d (0-based index) containts EPOLLIN and = %d", n, events[n].events);
         _D("      event socket for event %d (0-based index) is %d", n, events[n].data.fd);
         if(events[n].data.fd == listen_soc) //listen_soc
         {
           _D("      this %d socket is LiStEn socket", events[n].data.fd);
           _D("      let accept event(i.e. connect client) on this listen socket");

           connected_soc = accept(listen_soc, (struct sockaddr *)&peer_addr, (socklen_t *)&peer_addr_size);
           if(connected_soc == -1){
             perror("accept failure");
             exit(EXIT_FAILURE);
           }
           //ev.events = EPOLLIN;
           ev.events = EPOLLIN | EPOLLET;
           ev.data.fd = connected_soc;
           ASSERT(epoll_ctl(epollfd, EPOLL_CTL_ADD, connected_soc, &ev),"epoll_add for connected socket failure");
           _D("      connected socket %d accepted via listen socket %d and added into fd set", connected_soc, events[n].data.fd);

         } else
         {
           _D("      this %d socket is CoNnEcTeD socket", events[n].data.fd);
           _D("      let process(i.e. read data from) this %d connected socket", events[n].data.fd);
           rc = read(events[n].data.fd, header, 3);
           if (rc == -1){
             perror("sakura read problem");
             exit(EXIT_FAILURE);
           }
           _D("        %d bytes readed from connected socket %d", rc, events[n].data.fd);
           if(rc>0)
           {
             _D("        we read <%c%c%c> from connected socket %d", header[0], header[1], header[2], events[n].data.fd);
	     switch (header[2]){
               case 'C': ora_connect(events[n].data.fd); break;
               case 'S':
                 #ifdef NF
                   _D("        NOT forked ora_select and write answer");
                     ora_select(events[n].data.fd);
                 #else
                   _D("        let fork for ora_select processing data and write answer");
                   pid_op = fork();
                   if (pid_op==0){
                     ora_select(events[n].data.fd);
                   }
                   _D("        child process for ora_select is forked");
                 #endif
                 break;
               case 'Q':
                 #ifdef NF
                   _D("        NOT forked for ora_query and write answer");
                   ora_query(events[n].data.fd);
                 #else
                   _D("        let fork for ora_query processing data and write answer");
                   pid_op = fork();
                   if (pid_op==0){
                     ora_query(events[n].data.fd);
                   }
                   _D("        child process for ora_query is forked");
                 #endif
                 break;
               case 'P': ora_prepare(events[n].data.fd); break;
               case 'E': ora_execute(events[n].data.fd); break;
               case 'Z': ora_releaze(events[n].data.fd); break;
               case 'D': ora_disconnect(events[n].data.fd); break;
               case 'T': ora_commit(events[n].data.fd); break;
               case 'R': ora_rollback(events[n].data.fd); break;
               case 'G':
                 #ifdef NF
                   _D("        NOT forked for sakura_get and write answer");
                   sakura_get(events[n].data.fd);
                 #else
                   _D("        let fork for sakura_get processing data and write answer");
                   pid_op = fork();
                   if (pid_op==0){
                     sakura_get(events[n].data.fd);
                   }
                   _D("        child process for sakura_get is forked");
                 #endif
                 break;
                default:
                  _D("        readed data are NOT valid");
                  wrong_command(events[n].data.fd);
             }
           } else //i.e zero
           {
             _D("        we read <sign to close> from connected socket %d", events[n].data.fd);
             _D("        let close connected socket %d", events[n].data.fd);
             ASSERT(epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, NULL), "epoll_del for connected socket failure");
             close(events[n].data.fd);
             _D("        connected socket %d closed and deleted from fd_set", events[n].data.fd);
           }
           _D("      connected socket %d was processed", events[n].data.fd);
         }
       } else
       {
         _D("      event %d (0-based index) does NOT containt EPOLLIN and = %d. Nothing to process(?)!", n, events[n].events);
       }
      _D("    end process event %d (0-based index)", n);
    }//end for

    _D("  another process cycle for unblocked event(s) finished");
  }//end while


  exit(EXIT_SUCCESS);
}


//define sig_nandler
void sig_handler(int signo)
{
  if (signo == SIGTERM){
    _D("terminate infinite loop");
    close(epollfd);
    _D("epoll fd set %d closed", epollfd);
    close(listen_soc);
    _D("listen socket %d closed", listen_soc);
    //free ORACLE
    ora_free();
    _D("ORACLE resources are free");
  #ifdef DEBUG
    time(&t);
    fprintf(fptr,"======== STOP SAKURA SERVER ========== |%s", ctime(&t));
    fclose(fptr);
  #endif
  #ifdef UNIX
    unlink(SOC_FILE);
  #endif
    exit(EXIT_SUCCESS);
  }
  if (signo == SIGUSR1){
    _D("  reload config");
  }
}


// ------------------------------------------
//
//        aux functions
//
// ------------------------------------------


/* set process title -------------------------------------------------------------*/
void set_process_title(char *d, const char *s){
  int i=0;
  while(s[i]){
    d[i] = s[i];
    i++;
  }
  d[i]='\0';

}

/* convert 4bytes to integer ----------------------------------------------------*/
int _get_byte4(unsigned char *a)
{
  return  a[0] | (a[1] << 8) | (a[2] << 16) | (a[3] << 24);
}


/* get packet length -------------------------------------------------------------*/
int _get_packet_len(int soc)
{
  unsigned char pac_len[4];
  int len;
  read(soc, pac_len, 4);
  len = _get_byte4(pac_len);
  return len;
}


/* write answer to socket -------------------------------------------------------------*/
void write_answer(int soc, char typ, char *a){
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


/* send wrong_command message into socket ----------------------------------------*/
void wrong_command(int soc)
{
  int len = _get_packet_len(soc);
  char dummy_string[len+1];
  read(soc, dummy_string, len);
  dummy_string[len] = '\0';

  write_answer(soc,'E', "wrong command");
}


/*  test  function ---------------------------------------------------------------*/
void sakura_get(int soc)
{
  //start of subprocess
  _D("            [%d]start sakura_get subprocess for socket", soc);

  //write result into socket
  _D("            [%d]just before write TEXT into socket", soc);
  write_answer(soc, 'O', TEXT);
  _D("            [%d]just after write TEXT into socket", soc);

   //end of func
  _D("            [%d]stop sakura_get subprocess for socket", soc);

  exit(EXIT_SUCCESS);
}



