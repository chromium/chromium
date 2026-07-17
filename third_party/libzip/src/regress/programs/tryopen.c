/*
  tryopen.c -- tool for tests that try opening zip archives
  Copyright (C) 1999-2024 Dieter Baron and Thomas Klausner

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

#include "config.h"

#include <errno.h>
#include <stdio.h>

#ifndef HAVE_GETOPT
#include "getopt.h"
#endif

#include "zip.h"
#define TRYOPEN_USAGE                  \
    "usage: %s [-cent] file\n\n"       \
    "\t-c\tcheck consistency\n"        \
    "\t-e\texclusively open archive\n" \
    "\t-n\tcreate new file\n"          \
    "\t-s\tprint error string\n"       \
    "\t-t\ttruncate file to size 0\n"


int main(int argc, char *argv[]) {
    const char *fname;
    zip_t *z;
    int c, flags, ze;
    zip_int64_t count;
    int error_count;
    zip_error_t error;
    int error_strings = 0;

    flags = 0;

    while ((c = getopt(argc, argv, "censt")) != -1) {
        switch (c) {
        case 'c':
            flags |= ZIP_CHECKCONS;
            break;
        case 'e':
            flags |= ZIP_EXCL;
            break;
        case 'n':
            flags |= ZIP_CREATE;
            break;
        case 's':
            error_strings = 1;
            break;
        case 't':
            flags |= ZIP_TRUNCATE;
            break;

        default:
            fprintf(stderr, TRYOPEN_USAGE, argv[0]);
            return 1;
        }
    }

    error_count = 0;
    for (; optind < argc; optind++) {
        fname = argv[optind];
        errno = 0;

        if ((z = zip_open(fname, flags, &ze)) != NULL) {
            count = zip_get_num_entries(z, 0);
            printf("opening '%s' succeeded, %" PRIu64 " entries\n", fname, count);
            zip_close(z);
            continue;
        }

        zip_error_init_with_code(&error, ze);
        printf("opening '%s' returned error ", fname);
        if (error_strings) {
            printf("%s", zip_error_strerror(&error));
        }
        else {
            printf("%d", ze);
            switch (zip_error_system_type(&error)) {
            case ZIP_ET_SYS:
            case ZIP_ET_LIBZIP:
                printf("/%d", zip_error_code_system(&error));
                break;

            default:
                break;
            }
        }
        printf("\n");
        error_count++;
    }

    if (error_count > 0) {
        fprintf(stderr, "%d errors\n", error_count);
    }

    return error_count ? 1 : 0;
}
