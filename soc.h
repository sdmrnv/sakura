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
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/sysinfo.h>


//constants
#define  LISTEN_BACKLOG     500  /* max limit is defined in /proc/sys/net/core/somaxconn */
#define  MAX_EVENTS         64
#define  EPOLL_TIMEOUT      -1
#define  MAX_THREADS        1000 // ~ simultaneously connected clients in peak

#define  TEXT               "SAKURAipsum dolor sit amet, consectetur adipiscing elit. Curabitur sit amet feugiat quam. Mauris tempor placerat urna, vel posuere tellus blandit quis. Phasellus ac odio erat. Mauris id convallis velit. Suspendisse sit amet lobortis elit. Sed id dolor consectetur, dapibus arcu at, lobortis dolor. Sed ut eros vel mauris congue lobortis. Maecenas vel commodo augue. Nulla bibendum nibh arcu, sit amet elementum ipsum euismod sed. Duis vitae dolor mauris. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Mauris vitae egestas lacus.Aenean commodo vehicula interdum. Nunc semper enim vel lacus fringilla imperdiet. Etiam at diam eu diam mollis mattis. Donec et vehicula diam. Nunc vulputate imperdiet turpis, nec interdum quam cursus vel. Sed malesuada turpis nec mauris pellentesque, quis pharetra urna viverra. Maecenas tempor est lacus, at convallis sem condimentum sed. Nulla non magna a orci maximus malesuada. Sed et dolor ut ex consectetur cursus. Curabitur tortor dui, tempus eget arcu sed, finibus sodales velit. Maecenas nec aliquet leo, a ultrices nisl. Maecenas quis orci nisi.Ut mattis maximus facilisis. Interdum et malesuada fames ac ante ipsum primis in faucibus. Etiam quis massa vitae dui fringilla tempus sit amet eget orci. Mauris sit amet imperdiet quam, quis imperdiet tortor. Fusce nec rutrum ante, ac placerat tellus. Integer rhoncus, quam vitae facilisis elementum, odio magna lobortis ipsum, sed aliquam erat urna eu ex. Duis placerat aliquet dictum. Fusce vitae gravida quam. Aliquam arcu sem, laoreet quis metus sit amet, ornare mattis nibh. Vestibulum id elit quam. Morbi fringilla eu metus eget auctor. Vivamus quis nibh id nunc iaculis tincidunt.Curabitur tempus tellus tincidunt eleifend tempus. Vivamus elit eros, vestibulum sit amet mattis ac, feugiat eu tortor. Nullam imperdiet interdum massa vitae lacinia. Cras ipsum erat, feugiat eget velit ut, hendrerit placerat dolor. Donec eu augue posuere, scelerisque lorem at, congue risus. Aliquam semper laoreet auctor. Aenean ut congue tortor. Maecenas euismod suscipit ligula at tempor. Maecenas eget turpis in sapien mollis ultrices a aliquet mi. Vestibulum pulvinar molestie dictum. Phasellus iaculis justo sit amet orci ultricies interdum. Curabitur porta orci ac mauris accumsan, et venenatis lorem mattis.Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; Quisque fringilla turpis nisi, ut lacinia purus congue ut. In sit amet accumsan ex. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Nulla non felis in nulla bibendum fringilla at ut enim. Maecenas hendrerit odio et ex tempor euismod. Donec non lacus non orci egestas mollis. Nam accumsan turpis vitae erat faucibus dapibus. Aenean a odio sed lorem scelerisque cursus.Cras nec sem ornare, ultrices nisl a, bibendum lorem. Integer elementum ultrices nisl a tincidunt. Suspendisse feugiat sapien lectus, non sollicitudin purus cursus sed. Etiam luctus pretium tincidunt. Etiam rhoncus augue augue. Nam vel ullamcorper est. In vel ipsum consectetur, aliquam ipsum eu, varius tortor. Integer vel metus mi. In ipsum turpis, venenatis non blandit ut, feugiat nec risus. Etiam quis condimentum ligula. Integer fringilla accumsan consequat.Vestibulum consectetur, elit sit amet lobortis interdum, lacus nisl consectetur odio, nec pretium ipsum erat ac justo. Suspendisse eget ante accumsan, vestibulum magna a, iaculis arcu. Aliquam mattis congue nibh, condimentum sagittis nibh porta et. Ut vitae massa mi. Donec id massa congue, iaculis tortor id, consectetur purus. Pellentesque gravida lorem in sapien lobortis finibus. Aliquam porttitor a justo eget aliquam. Dixi!"
#define  TEXT_LEN           3730

#ifdef MULTI
#define  CPU_MULT           2
#endif

#ifdef UNIX
#define  SOC_FILE     	    "/var/run/soc"
#endif


//macros
#include "debug.h"


//global vars
int	            listen_soc;
int	            epollfd;


//funcs declarations
#ifdef MULTI
void worker_process();
#endif
void sig_handler(int signo);
void set_process_title(char *d, const char *s);

int _get_byte4(unsigned char *a);
int _get_packet_len(int soc);
void write_answer(int soc, char typ, char *a);
void wrong_command(int soc);
void sakura_get(int soc);

