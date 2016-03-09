/*
Copyright 1994 The PL-J Project. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE PL-J PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE PL-J PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
   OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors and should not be
interpreted as representing official policies, either expressed or implied, of the PL-J Project.
*/

/**
 * file:			comm_logging.h.
 * author:			PostgreSQL developement group.
 * author:			Laszlo Hornyak
 */

#ifndef __COMM_LOGGING_H__
#define __COMM_LOGGING_H__

/*
  COMM_STANDALONE should be defined for standalone interpreters
  running inside containers, since they don't have access to postgres
  symbols. If it was defined, lprintf will print the logs to stdout or
  in case of an error to stderr. pmalloc, pfree & pstrdup will use the
  std library.
*/
#ifdef COMM_STANDALONE

#include <utils/elog.h>

#define lprintf(lvl, fmt, ...)            \
    do {                                  \
        FILE *out = stdout;               \
        if (lvl >= ERROR) {               \
            out = stderr;                 \
        }                                 \
        fprintf(out, #lvl ": ");          \
        fprintf(out, fmt, ##__VA_ARGS__); \
        fprintf(out, "\n");               \
        if (lvl >= ERROR) {               \
            abort();                      \
        }                                 \
    } while (0)
#define pmalloc malloc
#define pfree free
#define pstrdup strdup

#else /* COMM_STANDALONE */

#include "postgres.h"

#define lprintf elog
#define pmalloc palloc
/* pfree & pstrdup are already defined by postgres */

#endif /* COMM_STANDALONE */

#endif /* __COMM_LOGGING_H__ */