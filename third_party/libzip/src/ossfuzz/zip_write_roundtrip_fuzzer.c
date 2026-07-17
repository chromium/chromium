/*
  zip_write_roundtrip_fuzzer.c -- fuzz the archive write/modify/serialize paths
  Copyright (C) 2026 The libzip Authors

  SPDX-License-Identifier: BSD-3-Clause

  Coverage gap addressed:
    The existing fuzzers (zip_read_fuzzer, zip_read_file_fuzzer,
    zip_read_metadata_fuzzer) only ever OPEN an archive read-only and
    consume it.  They never exercise the *write* side of libzip:

      - zip_file_add() / zip_dir_add() / zip_delete() / zip_file_rename()
      - zip_set_file_compression() (deflate/store encode)
      - zip_file_set_encryption()  (WinZip-AES / PKWARE encode)
      - zip_file_set_comment() / zip_set_archive_comment()
      - zip_file_extra_field_set()
      - zip_close() serialization: _zip_dirent_write(), extra-field
        sizing/merging, Zip64 promotion, central-directory + EOCD writing.

    These paths build ZIP structures from caller-influenced sizes and
    counts and are not reached by any read-only harness.

  Strategy:
    Drive a sequence of mutation operations from the fuzz input against a
    fresh, writable in-memory archive, then serialize it with zip_close().
    The fuzz bytes choose the operation, entry names, payload slices,
    compression methods, encryption methods, comments and extra fields.
    Finally, reopen the freshly written buffer and read every entry back
    (the encode -> decode round trip), so a malformed structure produced
    by the write path is also exercised on the read path.
*/

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>

/* Simple cursor over the fuzz input used to drive the operations. */
typedef struct {
    const uint8_t *p;
    size_t len;
    size_t pos;
} stream_t;

static uint8_t
u8(stream_t *s) {
    return s->pos < s->len ? s->p[s->pos++] : 0;
}

static uint16_t
u16(stream_t *s) {
    uint16_t v = u8(s);
    v = (uint16_t)(v | ((uint16_t)u8(s) << 8));
    return v;
}

/* Consume up to max bytes from the stream, returning a pointer and length. */
static const uint8_t *
take(stream_t *s, size_t max, size_t *out_len) {
    size_t avail = s->len - s->pos;
    size_t n = max;
    if (n > avail) {
        n = avail;
    }
    const uint8_t *r = s->p + s->pos;
    s->pos += n;
    *out_len = n;
    return r;
}

static const char *PASSWORD = "fuzzpw";

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    zip_error_t error;
    zip_source_t *src;
    zip_t *za;
    stream_t st;
    int nops, i;

    zip_error_init(&error);

    /* Empty, growable, writable in-memory archive. */
    src = zip_source_buffer_create(NULL, 0, 0, &error);
    if (src == NULL) {
        zip_error_fini(&error);
        return 0;
    }
    /* Keep an extra reference so the buffer survives the write zip_close()
       and we can reopen it for the read-back round trip. */
    zip_source_keep(src);

    za = zip_open_from_source(src, ZIP_TRUNCATE, &error);
    if (za == NULL) {
        zip_source_free(src); /* release our keep reference */
        zip_error_fini(&error);
        return 0;
    }
    zip_error_fini(&error);

    zip_set_default_password(za, PASSWORD);

    st.p = data;
    st.len = size;
    st.pos = 0;

    nops = (int)(u8(&st) % 32) + 1;
    for (i = 0; i < nops && st.pos < st.len; i++) {
        uint8_t op = u8(&st) % 7;
        char name[64];
        size_t nlen = u8(&st) % (sizeof(name) - 1);
        size_t k;
        for (k = 0; k < nlen; k++) {
            /* avoid embedded NUL so it stays a valid C string */
            name[k] = (char)(u8(&st) | 0x01);
        }
        name[nlen] = '\0';
        if (name[0] == '\0') {
            name[0] = 'f';
            name[1] = '\0';
        }

        switch (op) {
        case 0:
        case 1: {
            /* add a file whose data is a slice of the input */
            size_t dlen;
            const uint8_t *slice = take(&st, u16(&st) % 4096u, &dlen);
            void *buf = malloc(dlen ? dlen : 1);
            zip_source_t *fs;
            zip_int64_t idx;
            zip_int32_t methods[3];
            zip_uint16_t ems[4];

            if (buf == NULL) {
                break;
            }
            if (dlen) {
                memcpy(buf, slice, dlen);
            }
            /* freep = 1: libzip owns buf and frees it with the source */
            fs = zip_source_buffer(za, buf, dlen, 1);
            if (fs == NULL) {
                free(buf);
                break;
            }
            idx = zip_file_add(za, name, fs, ZIP_FL_OVERWRITE | ZIP_FL_ENC_GUESS);
            if (idx < 0) {
                zip_source_free(fs);
                break;
            }
            methods[0] = ZIP_CM_STORE;
            methods[1] = ZIP_CM_DEFLATE;
            methods[2] = ZIP_CM_DEFAULT;
            zip_set_file_compression(za, (zip_uint64_t)idx, methods[u8(&st) % 3], u8(&st) % 10);

            ems[0] = ZIP_EM_NONE;
            ems[1] = ZIP_EM_AES_128;
            ems[2] = ZIP_EM_AES_256;
            ems[3] = ZIP_EM_TRAD_PKWARE;
            zip_file_set_encryption(za, (zip_uint64_t)idx, ems[u8(&st) % 4], PASSWORD);
            break;
        }

        case 2:
            zip_dir_add(za, name, ZIP_FL_ENC_GUESS);
            break;

        case 3: {
            size_t clen;
            const uint8_t *c = take(&st, u8(&st) % 200u, &clen);
            zip_set_archive_comment(za, (const char *)c, (zip_uint16_t)clen);
            break;
        }

        case 4: {
            zip_int64_t n = zip_get_num_entries(za, 0);
            zip_uint64_t idx;
            if (n <= 0) {
                break;
            }
            idx = u16(&st) % (zip_uint64_t)n;
            if (u8(&st) & 1) {
                size_t clen;
                const uint8_t *c = take(&st, u8(&st) % 200u, &clen);
                zip_file_set_comment(za, idx, (const char *)c, (zip_uint16_t)clen, ZIP_FL_ENC_GUESS);
            }
            else {
                zip_uint16_t efid = u16(&st);
                size_t eflen;
                const uint8_t *ef = take(&st, u8(&st) % 200u, &eflen);
                zip_file_extra_field_set(za, idx, efid, ZIP_EXTRA_FIELD_NEW, ef, (zip_uint16_t)eflen, ZIP_FL_LOCAL);
            }
            break;
        }

        case 5: {
            zip_int64_t n = zip_get_num_entries(za, 0);
            if (n <= 0) {
                break;
            }
            zip_file_rename(za, u16(&st) % (zip_uint64_t)n, name, ZIP_FL_ENC_GUESS);
            break;
        }

        case 6: {
            zip_int64_t n = zip_get_num_entries(za, 0);
            if (n <= 0) {
                break;
            }
            zip_delete(za, u16(&st) % (zip_uint64_t)n);
            break;
        }
        }
    }

    /* Serialize: the write/encode path under test. */
    if (zip_close(za) < 0) {
        zip_discard(za);
        zip_source_free(src); /* release keep reference */
        return 0;
    }

    /* Round trip: reopen the freshly written archive and read it back.
       zip_open_from_source() takes ownership of one source reference and
       releases it on zip_close()/zip_discard(); keep one more reference so
       our own reference survives this second open/close cycle. */
    zip_source_keep(src);
    zip_error_init(&error);
    za = zip_open_from_source(src, 0, &error);
    if (za == NULL) {
        zip_source_free(src); /* undo the keep above */
        zip_source_free(src); /* release our original reference */
        zip_error_fini(&error);
        return 0;
    }
    zip_error_fini(&error);

    zip_set_default_password(za, PASSWORD);
    {
        zip_int64_t n = zip_get_num_entries(za, 0);
        zip_int64_t j;
        char rbuf[8192];
        for (j = 0; j < n; j++) {
            zip_file_t *f = zip_fopen_index(za, (zip_uint64_t)j, 0);
            if (f == NULL) {
                continue;
            }
            while (zip_fread(f, rbuf, sizeof(rbuf)) > 0) {
                ;
            }
            zip_fclose(f);
        }
    }

    if (zip_close(za) < 0) {
        zip_discard(za);
    }
    zip_source_free(src); /* release keep reference */
    return 0;
}
