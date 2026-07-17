/*
  zip_read_metadata_fuzzer.c -- fuzz metadata parsing paths in libzip
  Copyright (C) 2026 Google LLC

  SPDX-License-Identifier: BSD-3-Clause

  Coverage gap addressed:
    zip_read_fuzzer.c reads file *content* via zip_fopen_index()/zip_fread().
    It does NOT exercise the metadata accessors:
      - zip_stat_index()           (file stat: size, crc, method, mtime)
      - zip_file_get_external_attributes()
      - zip_get_archive_comment()
      - zip_file_get_comment()
      - zip_file_extra_field_get()
      - zip_file_extra_field_get_by_id()
      - zip_get_num_extra_fields()
      - zip_get_extra_field() (index-based extra field)

  These accessors parse ZIP central-directory and local-file-header
  extra fields (e.g. Zip64, NTFS timestamps, Unix UIDs, WinZip AES
  info) that are separate from the compressed data stream.  Malformed
  extra-field data with crafted lengths/IDs can trigger OOB reads or
  incorrect size arithmetic in zip_extra_field.c and zip_dirent.c.

  Strategy:
    Open the fuzz input as a ZIP archive and, for each entry:
      1. Call zip_stat_index() and inspect all stat fields.
      2. Call zip_file_get_external_attributes().
      3. Call zip_file_get_comment().
      4. Enumerate all extra fields by index and by ID (0x0001 = Zip64,
         0x000d = UNIX, 0x7875 = new Unix, 0x5455 = extended timestamp).
    Also read the archive-level comment via zip_get_archive_comment().
*/

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <zip.h>

/* Extra-field IDs to probe explicitly */
static const zip_uint16_t PROBE_IDS[] = {
    0x0001, /* Zip64 extended information */
    0x000d, /* UNIX                       */
    0x5455, /* Extended timestamp         */
    0x7875, /* New Unix                   */
    0x000a, /* NTFS                       */
    0x9901, /* AES encryption             */
};
#define N_PROBE_IDS (sizeof(PROBE_IDS) / sizeof(PROBE_IDS[0]))

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    zip_source_t *src;
    zip_error_t   error;
    zip_t        *za;

    zip_error_init(&error);

    src = zip_source_buffer_create(data, size, 0, &error);
    if (src == NULL) {
        zip_error_fini(&error);
        return 0;
    }

    za = zip_open_from_source(src, ZIP_RDONLY, &error);
    if (za == NULL) {
        zip_error_fini(&error);
        zip_source_free(src);
        return 0;
    }
    zip_error_fini(&error);

    /* Archive-level comment */
    int comment_len = 0;
    (void)zip_get_archive_comment(za, &comment_len, 0);

    zip_int64_t n = zip_get_num_entries(za, 0);

    for (zip_int64_t i = 0; i < n; i++) {
        /* 1. stat */
        zip_stat_t st;
        zip_stat_init(&st);
        zip_stat_index(za, (zip_uint64_t)i, 0, &st);

        /* 2. external attributes */
        zip_uint8_t  opsys  = 0;
        zip_uint32_t attrib = 0;
        zip_file_get_external_attributes(za, (zip_uint64_t)i, 0, &opsys, &attrib);

        /* 3. per-file comment */
        zip_uint32_t fcomment_len = 0;
        (void)zip_file_get_comment(za, (zip_uint64_t)i, &fcomment_len, 0);

        /* 4a. enumerate extra fields by index */
        zip_int16_t n_extra = zip_file_extra_fields_count(za, (zip_uint64_t)i, ZIP_FL_LOCAL);
        for (zip_int16_t j = 0; j < n_extra && j < 64; j++) {
            zip_uint16_t ef_id  = 0;
            zip_uint16_t ef_len = 0;
            (void)zip_file_extra_field_get(za, (zip_uint64_t)i, (zip_uint16_t)j,
                                           &ef_id, &ef_len, ZIP_FL_LOCAL);
        }
        /* central-directory extra fields too */
        n_extra = zip_file_extra_fields_count(za, (zip_uint64_t)i, ZIP_FL_CENTRAL);
        for (zip_int16_t j = 0; j < n_extra && j < 64; j++) {
            zip_uint16_t ef_id  = 0;
            zip_uint16_t ef_len = 0;
            (void)zip_file_extra_field_get(za, (zip_uint64_t)i, (zip_uint16_t)j,
                                           &ef_id, &ef_len, ZIP_FL_CENTRAL);
        }

        /* 4b. look up specific well-known extra field IDs */
        for (size_t k = 0; k < N_PROBE_IDS; k++) {
            zip_uint16_t ef_len = 0;
            /* occurrence 0 - local header */
            (void)zip_file_extra_field_get_by_id(za, (zip_uint64_t)i,
                                                  PROBE_IDS[k], 0,
                                                  &ef_len, ZIP_FL_LOCAL);
            /* occurrence 0 - central directory */
            (void)zip_file_extra_field_get_by_id(za, (zip_uint64_t)i,
                                                  PROBE_IDS[k], 0,
                                                  &ef_len, ZIP_FL_CENTRAL);
        }
    }

    if (zip_close(za) < 0) {
        zip_discard(za);
    }
    return 0;
}
