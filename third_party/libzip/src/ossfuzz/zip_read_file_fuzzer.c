/*
  zip_random_uwp.c -- fill the user's buffer with random stuff (UWP version)
  Copyright (C) 2017-2024 Dieter Baron and Thomas Klausner

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
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zip.h>

#include "zip_read_fuzzer_common.h"

/**
   This fuzzing target takes input data, creates a ZIP archive from it, checks the archive's consistency,
   and iterates over the entries in the archive, reading data from each entry.
**/

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    zip_t *za;
    const char *name = "test.zip";
    FILE *fp;
    zip_error_t error;
    int err = 0;

    (void)remove(name);
    if ((fp = fopen(name, "wb")) == NULL) {
        fprintf(stderr, "can't create file '%s': %s\n", name, strerror(errno));
        return 0;
    }
    if (fwrite(data, 1, size, fp) != size) {
        fprintf(stderr, "can't write data to file '%s': %s\n", name, strerror(errno));
        fclose(fp);
        (void)remove(name);
        return 0;
    }
    if (fclose(fp) < 0) {
        fprintf(stderr, "can't close file '%s': %s\n", name, strerror(errno));
        (void)remove(name);
        return 0;
    }

    za = zip_open(name, 0, &err);
    zip_error_init_with_code(&error, err);

    fuzzer_read(za, &error, NULL);

    (void)remove(name);
    return 0;
}

#ifdef __cplusplus
}
#endif
