/*
  zip_source_get_dostime.c -- get modification time in DOS format from source
  Copyright (C) 2024 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "zipint.h"

/* Returns -1 on error, 0 on no dostime available, 1 for dostime returned */
int zip_source_get_dos_time(zip_source_t *src, zip_dostime_t *dos_time) {
    if (src->source_closed) {
        return -1;
    }
    if (dos_time == NULL) {
        zip_error_set(&src->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    if (src->write_state == ZIP_SOURCE_WRITE_REMOVED) {
        zip_error_set(&src->error, ZIP_ER_READ, ENOENT);
    }

    if (zip_source_supports(src) & ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_GET_DOS_TIME)) {
        zip_int64_t n = _zip_source_call(src, dos_time, sizeof(*dos_time), ZIP_SOURCE_GET_DOS_TIME);

        if (n < 0) {
            return -1;
        }
        else if (n == 0) {
            return 0;
        }
        else if (n == sizeof(*dos_time)) {
            return 1;
        }
        else {
            zip_error_set(&src->error, ZIP_ER_INTERNAL, 0);
            return -1;
        }
    }
    else {
        return 0;
    }
}
