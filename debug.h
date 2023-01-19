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

#ifdef DEBUG
 #define  LOG_FILE  "/var/log/sakura.log"
 FILE     *fptr;
 time_t   t;
 struct timespec ts;
 #define _D(s,...) do {clock_gettime(CLOCK_TAI, &ts); t = (time_t)ts.tv_sec - 37; \
                       fprintf(fptr, "[%d][%.19s.%09ld]  "s"\n", getpid(), ctime(&t), ts.tv_nsec,  ##__VA_ARGS__); fflush(fptr);} while(0)
 #define ASSERT(rc,s) do {if(rc == -1) {perror(s); fprintf(fptr, "%s: %s\n", s,  strerror(errno)); fflush(fptr); exit(EXIT_FAILURE);}} while(0)
#else
 #define _D(s,...) do{ } while(0)
 #define ASSERT(rc,s) do {if(rc == -1) {perror(s); exit(EXIT_FAILURE);}} while(0)
#endif

