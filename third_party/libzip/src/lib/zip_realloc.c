/*
  zip_realloc.c -- reallocate with additional elements
  Copyright (C) 2009-2025 Dieter Baron and Thomas Klausner

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

#include <stdlib.h>

#include "zipint.h"

bool zip_realloc(void **memory, zip_uint64_t *alloced_elements, zip_uint64_t element_size, zip_uint64_t additional_elements, zip_error_t *error) {
    zip_uint64_t new_alloced_elements;
    void *new_memory;

    if (additional_elements == 0) {
        return true;
    }

    new_alloced_elements = *alloced_elements + additional_elements;

    if (new_alloced_elements < additional_elements || new_alloced_elements > SIZE_MAX / element_size) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return false;
    }

    if ((new_memory = realloc(*memory, (size_t)(new_alloced_elements * element_size))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return false;
    }

    *memory = new_memory;
    *alloced_elements = new_alloced_elements;

    return true;
}
