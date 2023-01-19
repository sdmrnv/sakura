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

//libraries
#include <oci.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

//constants
#define  DATABASE        "localhost:1521/orclcdb"

#define  SES_MIN         100
#define  SES_INCR        50
#define  SES_MAX         1000
#define  MAX_SESSIONS    SES_MAX

#define  MAXBINDS        100
#define  BIND_LEN        4001

#define  SPOOL_TIMEOUT   30

#define  MAX_STATEMENTS  100



//global vars
OCIEnv                   *envhp;
OCIError                 *errhp;
OCISPool                 *poolhp;
OraText                  *poolName;
ub4                      poolNameLen;


typedef struct{
 OCIStmt    *bind_stmthp;

} stmnt_t;

struct session_t{
  int         soc;
  OCISvcCtx   *ora_svchp;
  stmnt_t     stmnts[MAX_STATEMENTS];
  int         stmnts_hwm;
} sessions[MAX_SESSIONS];

int hwm = 0; //(h)igh (w)ater (m)ark for sessions array

typedef struct buf{
 char *str;
 int len;
} buf_t;

int         	         _bind_len = BIND_LEN;

//funcs declaratons
void checkenverr(OCIEnv *errhp, sword status);
void checkerr(OCIError *errhp, sword status, int line, char **err_msg);
void escape(char *d, char *s, char ch);
OCISvcCtx *get_svchp_by_soc(int soc, int *ses_i);

//extern funcs
extern int _get_packet_len(int soc);
extern void write_answer(int soc, char typ, char *a);



/****************************
         INIT
****************************/
void ora_init()
{
  int lstat;
  CONST OraText *database = (OraText *)DATABASE;
  char *ora_err;

  lstat = OCIEnvCreate (&envhp, OCI_DEFAULT, (dvoid *)0,  NULL, NULL, NULL, 0, (dvoid *)0);
  if (lstat != 0) {
    (void) printf("OCIEnvCreate failed with status = %d.\n", lstat);
    checkenverr(envhp, lstat);
    exit(EXIT_FAILURE);
  }

  checkerr(NULL, OCIHandleAlloc( (dvoid *) envhp, (dvoid **) &errhp,
                         OCI_HTYPE_ERROR, (size_t) 0, (dvoid **) 0), __LINE__, &ora_err);
  free(ora_err);
  checkerr(errhp, OCIHandleAlloc((dvoid *) envhp, (dvoid **) &poolhp, OCI_HTYPE_SPOOL,
                          (size_t) 0, (dvoid **) 0), __LINE__, &ora_err);
  free(ora_err);

  int timeout = SPOOL_TIMEOUT;
  checkerr(errhp, OCIAttrSet((dvoid *) poolhp, (ub4) OCI_HTYPE_SPOOL, (dvoid *) &timeout, (ub4)0, OCI_ATTR_SPOOL_TIMEOUT, errhp), __LINE__, &ora_err);
  free(ora_err);
  //OCI_ATTR_SPOOL_MAX_LIFETIME_SESSION

  lstat = OCISessionPoolCreate(envhp,
                     errhp,poolhp, &poolName, &poolNameLen,
                     database,(sb4)strlen((char *)database),
                     (ub4) SES_MIN,(ub4) SES_MAX,(ub4) SES_INCR,
                     (OraText *)NULL,(ub4) 0,
                     (OraText *)NULL,(ub4) 0
                     /////,OCI_SPC_STMTCACHE);
                     ,OCI_DEFAULT);

  if(lstat != OCI_SUCCESS){
    (void) printf("OCISessionPoolCreate failed with status = %d.\n", lstat);
    checkerr(errhp, lstat, __LINE__, &ora_err);
    free(ora_err);
    exit(EXIT_FAILURE);
  }

}

/****************************
         FREE
****************************/
void ora_free()
{
  char *ora_err;
  checkerr(errhp, OCISessionPoolDestroy(poolhp, errhp, OCI_DEFAULT),__LINE__, &ora_err); free(ora_err);
  checkerr(errhp, OCIHandleFree((dvoid *)poolhp, OCI_HTYPE_SPOOL),__LINE__, &ora_err); free(ora_err);
  checkerr(NULL, OCIHandleFree((dvoid *)errhp, OCI_HTYPE_ERROR),__LINE__, &ora_err); free(ora_err);
  checkerr (NULL, OCIHandleFree((dvoid *) envhp, OCI_HTYPE_ENV),__LINE__, &ora_err); free(ora_err);

  OCITerminate(OCI_DEFAULT);
}


/****************************
         CONNECT
****************************/
void ora_connect(int soc)
{
  //start of connect
  _D("            [%d]start connect for socket", soc);

  OCISvcCtx   *svchp = (OCISvcCtx *) 0;
  sword       lstat;
  int         len = _get_packet_len(soc);

  OraText     *username;
  OraText     *password;

  char        *ora_err;

  char *conn_string;
  conn_string = (char *)malloc(len*sizeof(unsigned char)+1);
  read(soc, conn_string, len);
  conn_string[len] = '\0';

  if(strchr(conn_string, '/')==NULL){
    write_answer(soc,'E', "/ not found in conn_string");
    return;
  }

  int upd = strchr(conn_string, '/')-conn_string; //(U)ser (P)assword (D)elimiter
  int pdd = (strchr(conn_string, '@')!=NULL) ? strchr(conn_string, '@') - conn_string : len; //(P)assword (D)atabase (D)elimiter

  username = (OraText *)malloc(sizeof(OraText)*upd+1);
  memcpy((OraText *)username, conn_string, upd);
  username[upd] = '\0';

  password = (OraText *)malloc(sizeof(OraText)*(pdd-upd));
  memcpy((OraText *)password, conn_string+upd+1,pdd-upd-1); 
  password[pdd-upd-1] = '\0';

  free(conn_string);

  //logon in Session Pool mode
  lstat = OCILogon2(envhp, errhp, &svchp,
      (CONST OraText *)username, (ub4)strlen((char *)username),
      (CONST OraText *)password, (ub4)strlen((char *)password),
      (CONST OraText *)poolName, (ub4)poolNameLen,
      OCI_LOGON2_SPOOL);

  free(username);
  free(password);

  if(lstat != 0){
      checkerr(errhp, lstat, __LINE__, &ora_err);
      //write_answer(soc, 'E', "sakura connect failed=>ORA");
      write_answer(soc, 'E', ora_err);
      free(ora_err);
      return;
  }

  {
    int h=0; //(h)ole
    while((h<hwm) && (sessions[h].soc != -1)) h++;
    if (h == MAX_SESSIONS)
    {
      write_answer(soc, 'E', "sakura MAX_SESSIONS reached!");
      return;
    }
    sessions[h].soc = soc;
    sessions[h].ora_svchp = svchp;
    sessions[h].stmnts_hwm = 0;
    if (h==hwm) hwm++;
  }

  write_answer(soc, 'O', "Connected");

  //end of connect
  _D("            [%d]end connect for socket", soc);
}


/****************************
         DISCONNECT
****************************/
void ora_disconnect(int soc)
{
  //start of disconnect
  _D("            [%d]start disconnect for socket", soc);

  OCISvcCtx  *svchp  = (OCISvcCtx *) 0;
  sword      lstat;

  char       *ora_err;

  int h=0;
  while((h<hwm) && (sessions[h].soc != soc)) h++;
  if(h==hwm) {
    write_answer(soc,'E',"Error(disconnect): svchp not found in sessions array");
    exit(EXIT_FAILURE);
  }
  svchp = sessions[h].ora_svchp;

  lstat = OCILogoff((dvoid *) svchp, errhp);
  if(lstat != 0){
    checkerr(errhp, lstat, __LINE__, &ora_err);
    free(ora_err);
    write_answer(soc, 'E', "sakura disconnect failed");
    return;
  }

  sessions[h].soc = -1;
  sessions[h].ora_svchp = (OCISvcCtx *) 0;
  sessions[h].stmnts_hwm = 0;

  write_answer(soc, 'O', "Disonnected");

  //end of disconnect
  _D("            [%d]end disconnect for socket", soc);
}


/****************************
         SELECT
****************************/
void ora_select(int soc)
{

  //start of select
  _D("            [%d]start select SUBPROCESS for socket", soc);

  OCISvcCtx  *svchp  = (OCISvcCtx *) 0;
  OCIStmt    *stmthp = (OCIStmt *)0;
  OCIDefine  *defhp[MAXBINDS];
  OCIParam   *paramdp = (OCIParam *) 0;

  sword      status = 0;
  sword      lstat  = 0;

  text       *select;

  ub4        counter;
  sb4        param_status;
  int        record_comma, item_comma;
  char       col[MAXBINDS][_bind_len];

  char       *col_escaped=(char *)NULL;
  buf_t      out_buf;

  char       *ora_err;
  int        ses_no;

  //get select statement
  int len = _get_packet_len(soc);
  select = (text *)malloc(len*sizeof(text)+1);
  read(soc, select, len);
  select[len] = '\0';


  //Initiate out_buf and open JSON
  out_buf.str = (char *)malloc(2*sizeof(char));
  out_buf.len = 2;
  strcpy(out_buf.str, "["); //copy [ with '\0' into out_buf

  //get svchp
  svchp = get_svchp_by_soc(soc, &ses_no);

  // prepare the query
  lstat = OCIStmtPrepare2 (svchp, (OCIStmt **)&stmthp, errhp, (text *)select,
            (ub4)strlen((char *)select), (const OraText  *)NULL, (ub4) 0, OCI_NTV_SYNTAX,
            OCI_DEFAULT);

  free(select);

  if(lstat){
    checkerr(errhp, lstat, __LINE__, &ora_err);
    free(ora_err);
    write_answer(soc,'E', "OCIStmtPrepare2 failed");
    exit(EXIT_FAILURE);
  }

  // execute the query
  lstat = OCIStmtExecute(svchp, stmthp, errhp, (ub4)0, (ub4)0, (OCISnapshot *)0, (OCISnapshot *)0, OCI_DEFAULT );
  if(lstat){
    checkerr(errhp, lstat, __LINE__, &ora_err);
//    write_answer(soc,'E', "OCIStmtExecute failed");
    write_answer(soc,'E', ora_err);
    free(ora_err);
    exit(EXIT_FAILURE);
  }

  //describe & define
  counter = 1;
  param_status = OCIParamGet((void *)stmthp, OCI_HTYPE_STMT, errhp, (void **)&paramdp, (ub4) counter);
  while (param_status == OCI_SUCCESS){
    //define
    lstat = OCIDefineByPos(stmthp, &defhp[counter-1], errhp, (ub4)counter, (dvoid *)&col[counter-1],
                                     (sb4) sizeof(col[counter-1]), (ub2)SQLT_STR, (dvoid *) 0, (ub2 *) 0,(ub2 *) 0, OCI_DEFAULT);
    if(lstat){
      checkerr(errhp, lstat, __LINE__, &ora_err);
      free(ora_err);
      write_answer(soc,'E', "OCIDefineByPos failed");
      exit(EXIT_FAILURE);
    }

    counter++;
    param_status = OCIParamGet((void *)stmthp, OCI_HTYPE_STMT, errhp, (void **)&paramdp, (ub4) counter);
  }//end while


  //fetching the values
  record_comma=0;
  status = OCIStmtFetch2(stmthp, errhp, 1, OCI_FETCH_NEXT, (sb4) 0, OCI_DEFAULT);
  while (status != OCI_NO_DATA)
  {
    //add comma begining from second record
    if(record_comma){
      out_buf.len = (out_buf.len + 2) * sizeof(char);
      out_buf.str = (char *)realloc(out_buf.str, out_buf.len);
      strcat(out_buf.str, ",");
    }
    else{
      record_comma=1;
    }

    //open JSON-record
    out_buf.len = (out_buf.len + 2) * sizeof(char);
    out_buf.str = (char *)realloc(out_buf.str, out_buf.len);
    strcat(out_buf.str, "[");

    item_comma = 0;
    int j;
    for (j=1;j<counter;j++){
      //add comma begining from second item
      if(item_comma){
        out_buf.len = (out_buf.len + 2) * sizeof(char);
        out_buf.str = (char *)realloc(out_buf.str, out_buf.len);
        strcat(out_buf.str, ",");
      }
      else{
        item_comma=1;
      }
      //add item into out_buf
      col_escaped = (char *)malloc((strlen(col[j-1])*2+1) * sizeof(char));
      escape(col_escaped, col[j-1], '"');
      out_buf.len = (out_buf.len + strlen(col_escaped) + 1 + 2) * sizeof(char);
      out_buf.str = (char *)realloc(out_buf.str, out_buf.len);
      strcat(out_buf.str, "\"");
      strcat(out_buf.str, col_escaped);
      strcat(out_buf.str, "\"");
      free(col_escaped);
    }//end for

    //close JSON-record
    out_buf.len = (out_buf.len + 2) * sizeof(char);
    out_buf.str = (char *)realloc(out_buf.str, out_buf.len);
    strcat(out_buf.str, "]");

    status = OCIStmtFetch2(stmthp, errhp, 1, OCI_FETCH_NEXT, (sb4) 0, OCI_DEFAULT);
  }//end while

  lstat = OCIStmtRelease(stmthp, errhp, (const OraText  *) NULL, (ub4) 0,  OCI_DEFAULT);
  if(lstat){
    checkerr(errhp, lstat, __LINE__, &ora_err);
    write_answer(soc,'E', "OCIStmtRelease failed");
////    write_answer(soc,'E', ora_err);
    free(ora_err);
    free(out_buf.str);
    exit(EXIT_FAILURE);
  }

  //close JSON
  out_buf.str = (char *)realloc(out_buf.str, (out_buf.len+2)*sizeof(char));
  out_buf.len+=2;
  strcat(out_buf.str, "]"); //copy ] and '\0' into out_buf; so out_buf.str will be c-string (ended by '\0') with right strlen in the future use

  write_answer(soc, 'O', out_buf.str);
  free(out_buf.str);

  //end of select
  _D("            [%d]end select SUBPROCESS for socket", soc);

  #ifndef NF
   exit(EXIT_SUCCESS);
  #endif

}


/****************************
         QUERY
****************************/
void ora_query(int soc)
{
  //start of query
  _D("            [%d]start query SUBPROCESS for socket", soc);
  OCISvcCtx *svchp  = (OCISvcCtx *) 0;
  OCIStmt   *stmthp = (OCIStmt *)0;

  sword     lstat;
  text      *query;

  char      *ora_err;
  int       ses_no;

  //get query statement
  int len = _get_packet_len(soc);
  query = (text *)malloc(len*sizeof(text)+1);
  read(soc, query, len);
  query[len] = '\0';

  //get svchp
  svchp = get_svchp_by_soc(soc, &ses_no);

  lstat = OCIStmtPrepare2 (svchp, (OCIStmt **)&stmthp, errhp, (text *)query,
          (ub4)strlen((char *)query), (const OraText  *)NULL, (ub4) 0, OCI_NTV_SYNTAX,
          OCI_DEFAULT);

  free(query);

  if(lstat){
    checkerr(errhp, lstat, __LINE__, &ora_err);
    write_answer(soc,'E', "OCIStmtPrepare2 failed");
    free(ora_err);
    exit(EXIT_FAILURE);
  }

  // execute the query
  lstat = OCIStmtExecute(svchp, stmthp, errhp, (ub4)1, (ub4)0, (OCISnapshot *)0, (OCISnapshot *)0, OCI_DEFAULT);
  if(lstat){
    checkerr(errhp, lstat, __LINE__, &ora_err);
//    write_answer(soc,'E', "OCIStmtExecute failed");
    write_answer(soc,'E', ora_err);
    free(ora_err);
    exit(EXIT_FAILURE);
  }

  lstat = OCIStmtRelease(stmthp, errhp, (const OraText  *) NULL, (ub4) 0,  OCI_DEFAULT);
  if(lstat){
    checkerr(errhp,  lstat, __LINE__, &ora_err);
    write_answer(soc,'E', "OCIStmtRelease failed");
    free(ora_err);
    exit(EXIT_FAILURE);
  }

  write_answer(soc, 'O', "OK");

  //end of query
  _D("            [%d]end query SUBPROCESS for socket", soc);


  #ifndef NF
   exit(EXIT_SUCCESS);
  #endif
}


/****************************
         PREPARE
****************************/
void ora_prepare(int soc)
{
  //start of query
  _D("            [%d]start prepare statement for socket", soc);
  OCISvcCtx *svchp  = (OCISvcCtx *) 0;

  sword     lstat;
  text      *query;

  char      *ora_err;
  int       ses_no;

  //get query statement
  int len = _get_packet_len(soc);
  query = (text *)malloc(len*sizeof(text)+1);
  read(soc, query, len);
  query[len] = '\0';

  //get svchp
  svchp = get_svchp_by_soc(soc, &ses_no);

  lstat = OCIStmtPrepare2 (svchp, (OCIStmt **)&(sessions[ses_no].stmnts[0].bind_stmthp), errhp, (text *)query,
          (ub4)strlen((char *)query), (const OraText  *)NULL, (ub4) 0, OCI_NTV_SYNTAX,
          OCI_DEFAULT);

  free(query);

  if(lstat){
    checkerr(errhp, lstat, __LINE__, &ora_err);
    write_answer(soc,'E', "OCIStmtPrepare2 failed");
    free(ora_err);
    exit(EXIT_FAILURE);
  }

  write_answer(soc, 'O', "0");
  //end of query
  _D("            [%d]end prepare for socket", soc);

}



/****************************
         EXECUTE
****************************/
void ora_execute(int soc)
{
  //start of query
  _D("            [%d]start execute statement for socket", soc);
  OCISvcCtx *svchp  = (OCISvcCtx *) 0;
  sword     lstat;
  char      *ora_err;
  int       ses_no;

  //get query statement
/*
  int len = _get_packet_len(soc);
  query = (text *)malloc(len*sizeof(text)+1);
  read(soc, query, len);
  query[len] = '\0';
*/

  //get svchp and ses_no
  svchp = get_svchp_by_soc(soc, &ses_no);

  // execute the query
  lstat = OCIStmtExecute(svchp, sessions[ses_no].stmnts[0].bind_stmthp, errhp, (ub4)1, (ub4)0, (OCISnapshot *)0, (OCISnapshot *)0, OCI_DEFAULT);
  if(lstat){
    checkerr(errhp, lstat, __LINE__, &ora_err);
//    write_answer(soc,'E', "OCIStmtExecute failed");
    write_answer(soc,'E', ora_err);
    free(ora_err);
    exit(EXIT_FAILURE);
  }

  write_answer(soc, 'O', "OK");
  //end of query
  _D("            [%d]end execute for socket", soc);

}




/****************************
         RELEAZE
****************************/
void ora_releaze(int soc)
{
  //start of query
  _D("            [%d]start release statement for socket", soc);

  sword     lstat;
  char      *ora_err;
  int       ses_no;

  //get query statement
/*
  int len = _get_packet_len(soc);
  query = (text *)malloc(len*sizeof(text)+1);
  read(soc, query, len);
  query[len] = '\0';

  free(query);
*/

  //get ses_no
  (void)get_svchp_by_soc(soc, &ses_no);

  lstat = OCIStmtRelease(sessions[ses_no].stmnts[0].bind_stmthp, errhp, (const OraText  *) NULL, (ub4) 0,  OCI_DEFAULT);
  if(lstat){
    checkerr(errhp,  lstat, __LINE__, &ora_err);
    write_answer(soc,'E', "OCIStmtRelease failed");
    free(ora_err);
    exit(EXIT_FAILURE);
  }

  write_answer(soc, 'O', "OK");
  //end of query
  _D("            [%d]end releaze for socket", soc);

}




/****************************
         COMMIT
****************************/
void ora_commit(int soc)
{
  //start of commit
  _D("            [%d]start commit for socket", soc);
  OCISvcCtx  *svchp  = (OCISvcCtx *) 0;
  sword lstat;

  char       *ora_err;
  int        ses_no;

  //get svchp
  svchp = get_svchp_by_soc(soc, &ses_no);

  //commit transacrion
  lstat = OCITransCommit(svchp, errhp, OCI_DEFAULT);
  if(lstat){
    checkerr(errhp, lstat, __LINE__, &ora_err);
    free(ora_err);
    write_answer(soc,'E', "OCITransCommit failed");
    return;
  }

  write_answer(soc, 'O', "Commited");

  //end of commit
  _D("            [%d]end commit for socket", soc);
}


/****************************
         ROLLBACK
****************************/
void ora_rollback(int soc)
{
  //start of rollback
  _D("            [%d]start rollback for socket", soc);
  OCISvcCtx  *svchp  = (OCISvcCtx *) 0;
  sword lstat;

  char       *ora_err;
  int        ses_no;

  //get svchp
  svchp = get_svchp_by_soc(soc, &ses_no);

  //rollback transacrion
  lstat = OCITransRollback(svchp, errhp, OCI_DEFAULT);
  if(lstat){
    checkerr(errhp, lstat, __LINE__, &ora_err);
    free(ora_err);
    write_answer(soc,'E', "OCITransRollback failed");
    return;
  }

  write_answer(soc, 'O', "Rollbacked");

  //end of rollback
  _D("            [%d]end rollback for socket", soc);
}


//---------------------------------------------
//  OCIEnvCreate error checking routine
void checkenverr(OCIEnv *envhp,sword status)
{
  text errbuf[512];
  ub4 errcode;

  switch (status)
  {
  case OCI_SUCCESS_WITH_INFO:
    printf("Error - OCI_SUCCESS_WITH_INFO\n");
    break;
  case OCI_ERROR:
    OCIErrorGet ((dvoid *) envhp, (ub4) 1, (text *) NULL, (sb4 *)&errcode,
            errbuf, (ub4) sizeof(errbuf), (ub4) OCI_HTYPE_ENV);
    printf("Error - %s\n", errbuf);
    break;
  case OCI_INVALID_HANDLE:
    printf("Error - OCI_INVALID_HANDLE\n");
    break;
  default:
    break;
  }
}


//  OCI error checking routine
void checkerr(OCIError *errhp, sword status, int line, char **err_msg)
{
  sb4 errcode;
  (*err_msg) = (char *)malloc(1024*sizeof(char));

  switch (status)
  {
  case OCI_SUCCESS:
    break;
  case OCI_ERROR:
    OCIErrorGet(errhp, 1, NULL, &errcode, (OraText *) *err_msg, 1024*sizeof(char), OCI_HTYPE_ERROR);
//    (void) OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
//                        errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
//    (void) printf("Error - %.*s in line %d\n", 512, errbuf, line);
//    strcpy(*err_msg, (char *)errbuf);
   break;
  case OCI_INVALID_HANDLE:
    (void) printf("Error - OCI_INVALID_HANDLE in line %d\n", line);
    strcpy(*err_msg, "Error - OCI_INVALID_HANDLE");
    break;
  default:
    break;
  }
}

/* add escape sequence -------------------------------------------------------*/
void escape(char *d, char *s, char ch){
 int i=0, k=0;
 while(s[i]){
   if(s[i] != ch){
     d[k] = s[i];
   }else{
     d[k] = '\\';
     k++;
     d[k] = ch;
   }
   k++; i++;
 }

 d[k] = '\0';

}


OCISvcCtx *get_svchp_by_soc(int soc, int *ses_i)
{
  int h=0;
  char str[25];
  while((h<hwm) && (sessions[h].soc != soc)) h++;
  if(h==hwm) {
//    write_answer(soc,'E',"svchp NOT found in sessions array");
    sprintf(str, "%d", hwm);
    write_answer(soc,'E',str);
    exit(EXIT_FAILURE);
  }
  (*ses_i) = h;
  return sessions[h].ora_svchp;
}
