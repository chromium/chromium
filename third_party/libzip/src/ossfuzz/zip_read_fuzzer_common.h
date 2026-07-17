/*
  zip_read_fuzzer_common.h -- common function for fuzzers to read all files in an archive
  Copyright (C) 2023 Dieter Baron and Thomas Klausner

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

#include "zip.h"

void fuzzer_read(zip_t *za, zip_error_t *error, const char *password) {
    zip_int64_t i, n, ret;
    char buf[32768];

    if (za == NULL) {
        fprintf(stderr, "Error opening archive: %s\n", zip_error_strerror(error));
        zip_error_fini(error);
        return;
    }

    zip_set_default_password(za, password);

    zip_error_fini(error);

    n = zip_get_num_entries(za, 0);
    for (i = 0; i < n; i++) {
        zip_file_t *f = zip_fopen_index(za, i, 0);
        if (f == NULL) {
            fprintf(stderr, "Error opening file %d: %s\n", (int)i, zip_strerror(za));
            continue;
        }

        while ((ret = zip_fread(f, buf, sizeof(buf))) > 0) {
            ;
        }
        if (ret < 0) {
            fprintf(stderr, "Error reading file %d: %s\n", (int)i, zip_strerror(za));
        }
        if (zip_fclose(f) < 0) {
            fprintf(stderr, "Error closing file %d: %s\n", (int)i, zip_strerror(za));
            continue;
        }
    }
    if (zip_close(za) < 0) {
        fprintf(stderr, "Error closing archive: %s\n", zip_strerror(za));
        zip_discard(za);
    }
}
