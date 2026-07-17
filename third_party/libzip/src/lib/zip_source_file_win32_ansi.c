/*
  zip_source_file_win32_ansi.c -- source for Windows file opened by ANSI name
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

#include "zip_source_file_win32.h"

static char *ansi_allocate_tempname(const char *name, size_t extra_chars, size_t *lengthp);
static HANDLE __stdcall ansi_create_file(const void *name, DWORD access, DWORD share_mode, PSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition, DWORD file_attributes, HANDLE template_file);
static BOOL __stdcall ansi_delete_file(const void *name);
static DWORD __stdcall ansi_get_file_attributes(const void *name);
static BOOL __stdcall ansi_get_file_attributes_ex(const void *name, GET_FILEEX_INFO_LEVELS info_level, void *information);
static void ansi_make_tempname(char *buf, size_t len, const char *name, zip_uint32_t i);
static BOOL __stdcall ansi_move_file(const void *from, const void *to, DWORD flags);
static BOOL __stdcall ansi_set_file_attributes(const void *name, DWORD attributes);
static HANDLE __stdcall ansi_find_first_file(const void *name, void *data);

/* clang-format off */
zip_win32_file_operations_t ops_ansi = {
    ansi_allocate_tempname,
    ansi_create_file,
    ansi_delete_file,
    ansi_get_file_attributes,
    ansi_get_file_attributes_ex,
    ansi_make_tempname,
    ansi_move_file,
    ansi_set_file_attributes,
    strdup,
    ansi_find_first_file,
};
/* clang-format on */

ZIP_EXTERN zip_source_t *zip_source_win32a(zip_t *za, const char *fname, zip_uint64_t start, zip_int64_t len) {
    if (za == NULL) {
        return NULL;
    }

    return zip_source_win32a_create(fname, start, len, &za->error);
}


ZIP_EXTERN zip_source_t *zip_source_win32a_create(const char *fname, zip_uint64_t start, zip_int64_t length, zip_error_t *error) {
    if (fname == NULL || length < ZIP_LENGTH_UNCHECKED) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }

    return zip_source_file_common_new(fname, NULL, start, length, NULL, &_zip_source_file_win32_named_ops, &ops_ansi, error);
}


static char *ansi_allocate_tempname(const char *name, size_t extra_chars, size_t *lengthp) {
    *lengthp = strlen(name) + extra_chars;
    return (char *)malloc(*lengthp);
}

static HANDLE __stdcall ansi_create_file(const void *name, DWORD access, DWORD share_mode, PSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition, DWORD file_attributes, HANDLE template_file) {
    return CreateFileA((const char *)name, access, share_mode, security_attributes, creation_disposition, file_attributes, template_file);
}

static BOOL __stdcall ansi_delete_file(const void *name) {
    return DeleteFileA((const char *)name);
}

static DWORD __stdcall ansi_get_file_attributes(const void *name) {
    return GetFileAttributesA((const char *)name);
}

static BOOL __stdcall ansi_get_file_attributes_ex(const void *name, GET_FILEEX_INFO_LEVELS info_level, void *information) {
    return GetFileAttributesExA((const char *)name, info_level, information);
}

static void ansi_make_tempname(char *buf, size_t len, const char *name, zip_uint32_t i) {
    snprintf_s(buf, len, "%s.%08x", name, i);
}

static BOOL __stdcall ansi_move_file(const void *from, const void *to, DWORD flags) {
    return MoveFileExA((const char *)from, (const char *)to, flags);
}

static BOOL __stdcall ansi_set_file_attributes(const void *name, DWORD attributes) {
    return SetFileAttributesA((const char *)name, attributes);
}

static HANDLE __stdcall ansi_find_first_file(const void *name, void *data) {
    return FindFirstFileA((const char *)name, data);
}
