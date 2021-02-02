/*
  zip_mkstempm.c -- mkstemp replacement that accepts a mode argument
  Copyright (C) 2019 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <libzip@nih.at>

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
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "zipint.h"

/*
 * create temporary file with same permissions as previous one;
 * or default permissions if there is no previous file
 */
int
_zip_mkstempm(char *path, int mode) {
    int fd;
    char *start, *end, *xs;

    int xcnt = 0;

    end = path + strlen(path);
    start = end - 1;
    while (start >= path && *start == 'X') {
	xcnt++;
	start--;
    }

    if (xcnt == 0) {
	errno = EINVAL;
	return -1;
    }

    start++;

    for (;;) {
	zip_uint32_t value = zip_random_uint32();

	xs = start;

	while (xs < end) {
	    char digit = value % 36;
	    if (digit < 10) {
		*(xs++) = digit + '0';
	    }
	    else {
		*(xs++) = digit - 10 + 'a';
	    }
	    value /= 36;
	}

	if ((fd = open(path, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, mode == -1 ? 0666 : (mode_t)mode)) >= 0) {
	    if (mode != -1) {
		/* open() honors umask(), which we don't want in this case */
		(void)chmod(path, (mode_t)mode);
	    }
	    return fd;
	}
	if (errno != EEXIST) {
	    return -1;
	}
    }
}
