/*
  zip_open.c -- open zip archive by name
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zipint.h"

typedef enum {
    EXISTS_ERROR = -1,
    EXISTS_NOT = 0,
    EXISTS_OK
} exists_t;

typedef enum {
    CDIR_OK,
    CDIR_INVALID,
    CDIR_NOT_FOUND
} cdir_status_t;

static bool check_eocd(zip_cdir_t *cd, unsigned int flags, zip_error_t *error);
static bool check_magic(zip_uint64_t offset, zip_buffer_t *buffer, zip_uint64_t buffer_offset, zip_source_t *src, const char *magic);
static zip_t *_zip_allocate_new(zip_source_t *src, unsigned int flags, zip_error_t *error);
static zip_int64_t _zip_checkcons(zip_t *za, zip_cdir_t *cdir, zip_error_t *error);
static void zip_check_torrentzip(zip_t *za, const zip_cdir_t *cdir);
static zip_cdir_t *_zip_find_central_dir(zip_t *za, zip_uint64_t len);
static exists_t _zip_file_exists(zip_source_t *src, zip_error_t *error);
static int _zip_headercomp(const zip_dirent_t *, const zip_dirent_t *);
static bool _zip_read_cdir(zip_t *za, zip_buffer_t *buffer, zip_uint64_t buf_offset, zip_cdir_t **cdirp, zip_error_t *error);
static zip_cdir_t *_zip_read_eocd(zip_buffer_t *buffer, zip_uint64_t buf_offset, zip_error_t *error);
static cdir_status_t _zip_read_eocd64(zip_cdir_t *cdir, zip_source_t *src, zip_buffer_t *buffer, zip_uint64_t buf_offset, unsigned int flags, zip_error_t *error);
static const unsigned char *find_eocd(zip_buffer_t *buffer, const unsigned char *last);


ZIP_EXTERN zip_t *zip_open(const char *fn, int _flags, int *zep) {
    zip_t *za;
    zip_source_t *src;
    struct zip_error error;

    zip_error_init(&error);
    if ((src = zip_source_file_create(fn, 0, -1, &error)) == NULL) {
        _zip_set_open_error(zep, &error, 0);
        zip_error_fini(&error);
        return NULL;
    }

    if ((za = zip_open_from_source(src, _flags, &error)) == NULL) {
        zip_source_free(src);
        _zip_set_open_error(zep, &error, 0);
        zip_error_fini(&error);
        return NULL;
    }

    zip_error_fini(&error);
    return za;
}


ZIP_EXTERN zip_t *zip_open_from_source(zip_source_t *src, int _flags, zip_error_t *error) {
    unsigned int flags;
    zip_int64_t supported;
    exists_t exists;

    if (_flags < 0 || src == NULL) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }
    flags = (unsigned int)_flags;

    supported = zip_source_supports(src);
    if ((supported & ZIP_SOURCE_SUPPORTS_SEEKABLE) != ZIP_SOURCE_SUPPORTS_SEEKABLE) {
        zip_error_set(error, ZIP_ER_OPNOTSUPP, 0);
        return NULL;
    }
    if ((supported & ZIP_SOURCE_SUPPORTS_WRITABLE) != ZIP_SOURCE_SUPPORTS_WRITABLE) {
        flags |= ZIP_RDONLY;
    }

    if ((flags & (ZIP_RDONLY | ZIP_TRUNCATE)) == (ZIP_RDONLY | ZIP_TRUNCATE)) {
        zip_error_set(error, ZIP_ER_RDONLY, 0);
        return NULL;
    }

    exists = _zip_file_exists(src, error);
    switch (exists) {
    case EXISTS_ERROR:
        return NULL;

    case EXISTS_NOT:
        if ((flags & ZIP_CREATE) == 0) {
            zip_error_set(error, ZIP_ER_NOENT, 0);
            return NULL;
        }
        return _zip_allocate_new(src, flags, error);

    default: {
        zip_t *za;
        if (flags & ZIP_EXCL) {
            zip_error_set(error, ZIP_ER_EXISTS, 0);
            return NULL;
        }
        if (zip_source_open(src) < 0) {
            zip_error_set_from_source(error, src);
            return NULL;
        }

        if (flags & ZIP_TRUNCATE) {
            za = _zip_allocate_new(src, flags, error);
        }
        else {
            /* ZIP_CREATE gets ignored if file exists and not ZIP_EXCL, just like open() */
            za = _zip_open(src, flags, error);
        }

        if (za == NULL) {
            zip_source_close(src);
            return NULL;
        }
        return za;
    }
    }
}


static bool _is_truncated_zip(zip_source_t *src) {
    unsigned char data[4];
    /* check if the source is a truncated zip archive: true if yes, no
       if not or can't be determined */
    if (zip_source_seek(src, 0, SEEK_SET) < 0) {
        return false;
    }

    if (zip_source_read(src, data, 4) != 4) {
        return false;
    }

    if (memcmp(data, LOCAL_MAGIC, 4) == 0) {
        /* file starts with a ZIP local header signature */
        return true;
    }
    return false;
}


zip_t *_zip_open(zip_source_t *src, unsigned int flags, zip_error_t *error) {
    zip_t *za;
    zip_cdir_t *cdir;
    struct zip_stat st;
    zip_uint64_t len, idx;

    zip_stat_init(&st);
    if (zip_source_stat(src, &st) < 0) {
        zip_error_set_from_source(error, src);
        return NULL;
    }
    if ((st.valid & ZIP_STAT_SIZE) == 0) {
        zip_error_set(error, ZIP_ER_SEEK, EOPNOTSUPP);
        return NULL;
    }
    len = st.size;


    if ((za = _zip_allocate_new(src, flags, error)) == NULL) {
        return NULL;
    }

    /* treat empty files as empty archives */
    if (len == 0 && zip_source_accept_empty(src)) {
        return za;
    }

    if ((cdir = _zip_find_central_dir(za, len)) == NULL) {
        _zip_error_copy(error, &za->error);
        if (zip_error_code_zip(&za->error) == ZIP_ER_NOZIP) {
            /* not a zip - find out if it's truncated */
            if (_is_truncated_zip(src)) {
                zip_error_set(error, ZIP_ER_TRUNCATED_ZIP, 0);
            }
        }
        /* keep src so discard does not get rid of it */
        zip_source_keep(src);
        zip_discard(za);
        return NULL;
    }

    za->entry = cdir->entry;
    za->nentry = cdir->nentry;
    za->nentry_alloc = cdir->nentry_alloc;

    zip_check_torrentzip(za, cdir);

    if (ZIP_IS_TORRENTZIP(za)) {
        /* Torrentzip uses the archive comment to detect changes by tools that are not torrentzip aware. */
        _zip_string_free(cdir->comment);
    }
    else {
        za->comment_orig = cdir->comment;
    }

    free(cdir);

    _zip_hash_reserve_capacity(za->names, za->nentry, &za->error);

    for (idx = 0; idx < za->nentry; idx++) {
        const zip_uint8_t *name = _zip_string_get(za->entry[idx].orig->filename, NULL, 0, error);
        if (name == NULL) {
            /* keep src so discard does not get rid of it */
            zip_source_keep(src);
            zip_discard(za);
            return NULL;
        }

        if (_zip_hash_add(za->names, name, idx, ZIP_FL_UNCHANGED, &za->error) == false) {
            if (za->error.zip_err != ZIP_ER_EXISTS || (flags & ZIP_CHECKCONS)) {
                _zip_error_copy(error, &za->error);
                /* keep src so discard does not get rid of it */
                zip_source_keep(src);
                zip_discard(za);
                return NULL;
            }
        }
    }

    za->ch_flags = za->flags;

    return za;
}


void _zip_set_open_error(int *zep, const zip_error_t *err, int ze) {
    if (err) {
        ze = zip_error_code_zip(err);
        switch (zip_error_system_type(err)) {
        case ZIP_ET_SYS:
        case ZIP_ET_LIBZIP:
            errno = zip_error_code_system(err);
            break;

        default:
            break;
        }
    }

    if (zep) {
        *zep = ze;
    }
}


/* _zip_readcdir:
   tries to find a valid end-of-central-directory at the beginning of
   buf, and then the corresponding central directory entries.
   Returns a struct zip_cdir which contains the central directory
   entries, or NULL if unsuccessful. */

static bool _zip_read_cdir(zip_t *za, zip_buffer_t *buffer, zip_uint64_t buf_offset, zip_cdir_t **cdirp, zip_error_t *error) {
    zip_cdir_t *cd;
    zip_uint16_t comment_len;
    zip_uint64_t i, left;
    zip_uint64_t eocd_offset = _zip_buffer_offset(buffer);
    zip_buffer_t *cd_buffer;
    bool eocd64_found = false;

    *cdirp = NULL;

    if ((cd = _zip_read_eocd(buffer, buf_offset, error)) == NULL) {
        return false;
    }

    if (eocd_offset >= EOCD64LOCLEN && memcmp(_zip_buffer_data(buffer) + eocd_offset - EOCD64LOCLEN, EOCD64LOC_MAGIC, 4) == 0) {
        eocd64_found = true;
        _zip_buffer_set_offset(buffer, eocd_offset - EOCD64LOCLEN);
        switch (_zip_read_eocd64(cd, za->src, buffer, buf_offset, za->open_flags, error)) {
        case CDIR_OK:
            break;

        case CDIR_INVALID:
            _zip_cdir_free(cd);
            return true;

        case CDIR_NOT_FOUND:
            _zip_cdir_free(cd);
            return false;
        }
    }

    if ((cd->eocd_disk != 0 || cd->this_disk != 0) && !eocd64_found && cd->eocd_disk != cd->this_disk) {
        /* If the central directory doesn't start on this disk, we can't check that offset is valid. Check as much as we can instead. */
        if (cd->this_disk < cd->eocd_disk) {
            /* Disks before the start of the central directory don't contain an EOCD. */
            _zip_cdir_free(cd);
            return false;
        }
        if (cd->size <= cd->eocd_offset) {
            /* The complete central directory would fit on this disk. */
            _zip_cdir_free(cd);
            return false;
        }
    }

    if (!eocd64_found) {
        if (cd->this_disk == 0 && cd->eocd_disk == 0 && cd->eocd_offset == 0 && cd->offset == 0 && cd->num_entries == 0) {
            /* An empty archive doesn't contain central directory entries. */
        }
        else if (!check_magic(cd->offset, buffer, buf_offset, za->src, CENTRAL_MAGIC)) {
            _zip_cdir_free(cd);
            return false;
        }
    }

    /* We accept this EOCD as valid and won't search for an earlier one if it is unusable. */

    if (!check_eocd(cd, za->flags, error)) {
        _zip_cdir_free(cd);
        return true;
    }

    _zip_buffer_set_offset(buffer, eocd_offset + 20);
    comment_len = _zip_buffer_get_16(buffer);

    if (cd->offset + cd->size > buf_offset + eocd_offset) {
        /* cdir spans past EOCD record */
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_CDIR_OVERLAPS_EOCD);
        _zip_cdir_free(cd);
        return true;
    }

    if (comment_len || (za->open_flags & ZIP_CHECKCONS)) {
        zip_uint64_t tail_len;

        _zip_buffer_set_offset(buffer, eocd_offset + EOCDLEN);
        tail_len = _zip_buffer_left(buffer);

        if (tail_len != comment_len) {
            if (za->open_flags & ZIP_CHECKCONS) {
                zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_COMMENT_LENGTH_INVALID);
                _zip_cdir_free(cd);
                return true;
            }
            if (tail_len < comment_len) {
                comment_len = tail_len;
            }
        }

        if (comment_len) {
            if ((cd->comment = _zip_string_new(_zip_buffer_get(buffer, comment_len), comment_len, ZIP_FL_ENC_GUESS, error)) == NULL) {
                _zip_cdir_free(cd);
                return true;
            }
        }
    }

    if (cd->offset >= buf_offset) {
        zip_uint8_t *data;
        /* if buffer already read in, use it */
        _zip_buffer_set_offset(buffer, cd->offset - buf_offset);

        if ((data = _zip_buffer_get(buffer, cd->size)) == NULL) {
            zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_CDIR_LENGTH_INVALID);
            _zip_cdir_free(cd);
            return true;
        }
        if ((cd_buffer = _zip_buffer_new(data, cd->size)) == NULL) {
            zip_error_set(error, ZIP_ER_MEMORY, 0);
            _zip_cdir_free(cd);
            return true;
        }
    }
    else {
        cd_buffer = NULL;

        if (zip_source_seek(za->src, (zip_int64_t)cd->offset, SEEK_SET) < 0) {
            zip_error_set_from_source(error, za->src);
            _zip_cdir_free(cd);
            return true;
        }

        /* possible consistency check: cd->offset = len-(cd->size+cd->comment_len+EOCDLEN) ? */
        if (zip_source_tell(za->src) != (zip_int64_t)cd->offset) {
            zip_error_set(error, ZIP_ER_NOZIP, 0);
            _zip_cdir_free(cd);
            return true;
        }
    }

    if (!_zip_cdir_grow(cd, cd->num_entries, error)) {
        _zip_cdir_free(cd);
        _zip_buffer_free(cd_buffer);
        return true;
    }
    left = (zip_uint64_t)cd->size;
    i = 0;
    while (left > 0) {
        bool grown = false;
        zip_int64_t entry_size;

        if (i == cd->nentry) {
            /* InfoZIP has a hack to avoid using Zip64: it stores nentries % 0x10000 */
            /* This hack isn't applicable if we're using Zip64, or if there is no central directory entry following. */

            if (cd->is_zip64 || left < CDENTRYSIZE) {
                break;
            }

            if (!_zip_cdir_grow(cd, 0x10000, error)) {
                _zip_cdir_free(cd);
                _zip_buffer_free(cd_buffer);
                return true;
            }
            grown = true;
        }

        if ((cd->entry[i].orig = _zip_dirent_new()) == NULL || (entry_size = _zip_dirent_read(cd->entry[i].orig, za->src, cd_buffer, false, 0, za->open_flags & ZIP_CHECKCONS, error)) < 0) {
            if (zip_error_code_zip(error) == ZIP_ER_INCONS) {
                zip_error_set(error, ZIP_ER_INCONS, ADD_INDEX_TO_DETAIL(zip_error_code_system(error), i));
            }
            else if (grown && zip_error_code_zip(error) == ZIP_ER_NOZIP) {
                zip_error_set(error, ZIP_ER_INCONS, MAKE_DETAIL_WITH_INDEX(ZIP_ER_DETAIL_CDIR_ENTRY_INVALID, i));
            }
            _zip_cdir_free(cd);
            _zip_buffer_free(cd_buffer);
            return true;
        }
        i++;
        left -= (zip_uint64_t)entry_size;
    }

    /* If we didn't fill all we grew, cd->num_entries was wrong. */
    if (i != cd->nentry || left > 0) {
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_CDIR_WRONG_ENTRIES_COUNT);
        _zip_buffer_free(cd_buffer);
        _zip_cdir_free(cd);
        return true;
    }

    if (za->open_flags & ZIP_CHECKCONS) {
        bool ok;

        if (cd_buffer) {
            ok = _zip_buffer_eof(cd_buffer);
        }
        else {
            zip_int64_t offset = zip_source_tell(za->src);

            if (offset < 0) {
                zip_error_set_from_source(error, za->src);
                _zip_cdir_free(cd);
                return true;
            }
            ok = ((zip_uint64_t)offset == cd->offset + cd->size);
        }

        if (!ok) {
            zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_CDIR_LENGTH_INVALID);
            _zip_buffer_free(cd_buffer);
            _zip_cdir_free(cd);
            return true;
        }
    }

    _zip_buffer_free(cd_buffer);
    *cdirp = cd;
    return true;
}


static bool check_magic(zip_uint64_t offset, zip_buffer_t *buffer, zip_uint64_t buffer_offset, zip_source_t *src, const char *magic) {
    if (buffer_offset <= offset) {
        zip_uint8_t *data;
        if (_zip_buffer_set_offset(buffer, offset - buffer_offset) < 0 || (data = _zip_buffer_get(buffer, MAGIC_LEN)) == NULL) {
            return false;
        }
        return memcmp(data, magic, MAGIC_LEN) == 0;
    }
    else {
        zip_uint8_t data[MAGIC_LEN];

        if (zip_source_seek(src, offset, SEEK_SET) < 0 || zip_source_read(src, data, MAGIC_LEN) != MAGIC_LEN) {
            return false;
        }
        return memcmp(data, magic, MAGIC_LEN) == 0;
    }
}


/* _zip_checkcons:
   Checks the consistency of the central directory by comparing central
   directory entries with local headers and checking for plausible
   file and header offsets. Returns -1 if not plausible, else the
   difference between the lowest and the highest fileposition reached */

static zip_int64_t _zip_checkcons(zip_t *za, zip_cdir_t *cd, zip_error_t *error) {
    zip_uint64_t i;
    zip_uint64_t min, max, j, tail_length;
    struct zip_dirent temp;
    int detail;

    _zip_dirent_init(&temp);
    if (cd->nentry) {
        max = cd->entry[0].orig->offset;
        min = cd->entry[0].orig->offset;
    }
    else {
        min = max = 0;
    }

    for (i = 0; i < cd->nentry; i++) {
        if (cd->entry[i].orig->offset < min) {
            min = cd->entry[i].orig->offset;
        }
        if (min > (zip_uint64_t)cd->offset) {
            zip_error_set(error, ZIP_ER_NOZIP, 0);
            return -1;
        }

        tail_length = _zip_string_length(cd->entry[i].orig->filename) + LENTRYSIZE;
        if (ZIP_CHECK_ADD_OVERFLOW(cd->entry[i].orig->comp_size, tail_length) || ZIP_CHECK_ADD_OVERFLOW(cd->entry[i].orig->offset + tail_length, cd->entry[i].orig->comp_size)) {
            zip_error_set(error, ZIP_ER_NOZIP, 0);
            return -1;
        }
        j = cd->entry[i].orig->offset + cd->entry[i].orig->comp_size + tail_length;

        if (j > max) {
            max = j;
        }
        if (max > (zip_uint64_t)cd->offset) {
            zip_error_set(error, ZIP_ER_NOZIP, 0);
            return -1;
        }

        if (zip_source_seek(za->src, (zip_int64_t)cd->entry[i].orig->offset, SEEK_SET) < 0) {
            zip_error_set_from_source(error, za->src);
            return -1;
        }

        if (_zip_dirent_read(&temp, za->src, NULL, true, cd->entry[i].orig->comp_size, true, error) == -1) {
            if (zip_error_code_zip(error) == ZIP_ER_INCONS) {
                zip_error_set(error, ZIP_ER_INCONS, ADD_INDEX_TO_DETAIL(zip_error_code_system(error), i));
            }
            _zip_dirent_finalize(&temp);
            return -1;
        }

        if (_zip_headercomp(cd->entry[i].orig, &temp) != 0) {
            zip_error_set(error, ZIP_ER_INCONS, MAKE_DETAIL_WITH_INDEX(ZIP_ER_DETAIL_ENTRY_HEADER_MISMATCH, i));
            _zip_dirent_finalize(&temp);
            return -1;
        }

        cd->entry[i].orig->extra_fields = _zip_ef_merge(cd->entry[i].orig->extra_fields, temp.extra_fields);
        cd->entry[i].orig->local_extra_fields_read = 1;
        temp.extra_fields = NULL;

        _zip_dirent_finalize(&temp);

        if ((detail = zip_dirent_check_consistency(cd->entry[i].orig)) != 0) {
            zip_error_set(error, ZIP_ER_INCONS, MAKE_DETAIL_WITH_INDEX(detail, i));
            return -1;
        }
    }

    return (max - min) < ZIP_INT64_MAX ? (zip_int64_t)(max - min) : ZIP_INT64_MAX;
}


/* _zip_headercomp:
   compares a central directory entry and a local file header
   Return 0 if they are consistent, -1 if not. */

static int _zip_headercomp(const zip_dirent_t *central, const zip_dirent_t *local) {
    if ((central->version_needed < local->version_needed)
#if 0
	/* some zip-files have different values in local
	   and global headers for the bitflags */
	|| (central->bitflags != local->bitflags)
#endif
        || (central->comp_method != local->comp_method) || (central->last_mod.time != local->last_mod.time) || (central->last_mod.date != local->last_mod.date) || !_zip_string_equal(central->filename, local->filename))
        return -1;

    if ((central->crc != local->crc) || (central->comp_size != local->comp_size) || (central->uncomp_size != local->uncomp_size)) {
        /* InfoZip stores valid values in local header even when data descriptor is used.
           This is in violation of the appnote.
           macOS Archive sets the compressed size even when data descriptor is used ( but not the others),
           also in violation of the appnote.
        */
        /* if data descriptor is not used, the values must match */
        if ((local->bitflags & ZIP_GPBF_DATA_DESCRIPTOR) == 0) {
            return -1;
        }
        /* when using a data descriptor, the local header value must be zero or match */
        if ((local->crc != 0 && central->crc != local->crc) || (local->comp_size != 0 && central->comp_size != local->comp_size) || (local->uncomp_size != 0 && central->uncomp_size != local->uncomp_size)) {
            return -1;
        }
    }

    return 0;
}


static zip_t *_zip_allocate_new(zip_source_t *src, unsigned int flags, zip_error_t *error) {
    zip_t *za;

    if ((za = _zip_new(error)) == NULL) {
        return NULL;
    }

    za->src = src;
    za->open_flags = flags;
    za->flags = 0;
    za->ch_flags = 0;
    za->write_crc = NULL;

    if (flags & ZIP_RDONLY) {
        za->flags |= ZIP_AFL_RDONLY;
        za->ch_flags |= ZIP_AFL_RDONLY;
    }

    return za;
}


/*
 * tests for file existence
 */
static exists_t _zip_file_exists(zip_source_t *src, zip_error_t *error) {
    struct zip_stat st;

    zip_stat_init(&st);
    if (zip_source_stat(src, &st) != 0) {
        zip_error_t *src_error = zip_source_error(src);
        if (zip_error_code_zip(src_error) == ZIP_ER_READ && zip_error_code_system(src_error) == ENOENT) {
            return EXISTS_NOT;
        }
        _zip_error_copy(error, src_error);
        return EXISTS_ERROR;
    }

    return EXISTS_OK;
}


static zip_cdir_t *_zip_find_central_dir(zip_t *za, zip_uint64_t len) {
    zip_cdir_t *cdir;
    const zip_uint8_t *match;
    zip_int64_t buf_offset;
    zip_uint64_t buflen;
    zip_error_t error;
    zip_buffer_t *buffer;

    if (len < EOCDLEN) {
        zip_error_set(&za->error, ZIP_ER_NOZIP, 0);
        return NULL;
    }

    buflen = (len < CDBUFSIZE ? len : CDBUFSIZE);
    if (zip_source_seek(za->src, -(zip_int64_t)buflen, SEEK_END) < 0) {
        zip_error_t *src_error = zip_source_error(za->src);
        if (zip_error_code_zip(src_error) != ZIP_ER_SEEK || zip_error_code_system(src_error) != EFBIG) {
            /* seek before start of file on my machine */
            _zip_error_copy(&za->error, src_error);
            return NULL;
        }
    }
    if ((buf_offset = zip_source_tell(za->src)) < 0) {
        zip_error_set_from_source(&za->error, za->src);
        return NULL;
    }

    if ((buffer = _zip_buffer_new_from_source(za->src, buflen, NULL, &za->error)) == NULL) {
        return NULL;
    }

    cdir = NULL;
    if (buflen >= CDBUFSIZE) {
        /* EOCD64 locator is before EOCD, so leave place for it */
        _zip_buffer_set_offset(buffer, EOCD64LOCLEN);
    }
    zip_error_set(&error, ZIP_ER_NOZIP, 0);

    match = NULL;
    while ((match = find_eocd(buffer, match)) != NULL) {
        _zip_buffer_set_offset(buffer, (zip_uint64_t)(match - _zip_buffer_data(buffer)));
        if (_zip_read_cdir(za, buffer, (zip_uint64_t)buf_offset, &cdir, &error)) {
            if (cdir != NULL && (za->open_flags & ZIP_CHECKCONS) && _zip_checkcons(za, cdir, &error) < 0) {
                _zip_cdir_free(cdir);
                cdir = NULL;
            }
            break;
        }
    }

    _zip_buffer_free(buffer);

    if (cdir == NULL) {
        _zip_error_copy(&za->error, &error);
    }
    return cdir;
}


static const unsigned char *find_eocd(zip_buffer_t *buffer, const unsigned char *last) {
    const unsigned char *data = _zip_buffer_data(buffer);
    const unsigned char *p;

    if (last == NULL) {
        if (_zip_buffer_size(buffer) < MAGIC_LEN) {
            return NULL;
        }
        last = data + _zip_buffer_size(buffer) - MAGIC_LEN;
    }
    else {
        if (last == _zip_buffer_data(buffer)) {
            return NULL;
        }
        last -= 1;
    }

    for (p = last; p >= data; p -= 1) {
        if (*p == EOCD_MAGIC[0]) {
            if (memcmp(p, EOCD_MAGIC, MAGIC_LEN) == 0) {
                return p;
            }
        }
        if (p == data) {
            /* Avoid undefined behavior by creating pointer outside buffer */
            break;
        }
    }

    return NULL;
}


static zip_cdir_t *_zip_read_eocd(zip_buffer_t *buffer, zip_uint64_t buf_offset, zip_error_t *error) {
    zip_cdir_t *cd;

    if (_zip_buffer_left(buffer) < EOCDLEN) {
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_EOCD_LENGTH_INVALID);
        return NULL;
    }

    if ((cd = _zip_cdir_new(error)) == NULL) {
        return NULL;
    }

    cd->eocd_offset = buf_offset + _zip_buffer_offset(buffer);
    /* This function is only called where EOCD magic was found, so no need to check that here. */
    _zip_buffer_skip(buffer, MAGIC_LEN);
    cd->is_zip64 = false;
    cd->this_disk = _zip_buffer_get_16(buffer);
    cd->eocd_disk = _zip_buffer_get_16(buffer);

    /* number of cdir-entries on this disk */
    cd->disk_entries = _zip_buffer_get_16(buffer);
    /* number of cdir-entries */
    cd->num_entries = _zip_buffer_get_16(buffer);
    cd->size = _zip_buffer_get_32(buffer);
    cd->offset = _zip_buffer_get_32(buffer);

    return cd;
}

static bool check_eocd(zip_cdir_t *cd, unsigned int flags, zip_error_t *error) {
    if (cd->disk_entries != cd->num_entries || cd->this_disk != 0 || cd->eocd_disk != 0) {
        zip_error_set(error, ZIP_ER_MULTIDISK, 0);
        return false;
    }

    if (cd->offset + cd->size < cd->offset) {
        zip_error_set(error, ZIP_ER_SEEK, EFBIG);
        return false;
    }
    if ((flags & ZIP_CHECKCONS) && cd->offset + cd->size != cd->eocd_offset) {
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_CDIR_LENGTH_INVALID);
        return false;
    }

    return true;
}


cdir_status_t _zip_read_eocd64(zip_cdir_t *cdir, zip_source_t *src, zip_buffer_t *buffer, zip_uint64_t buf_offset, unsigned int flags, zip_error_t *error) {
    zip_uint64_t offset;
    zip_uint8_t eocd[EOCD64LEN];
    zip_uint64_t eocd_offset;
    zip_uint64_t size, nentry, i, eocdloc_offset;
    bool free_buffer;
    zip_uint32_t num_disks, eocd_disk, this_disk;

    /* The offset of the end of the buffer is less than ZIP_UINT64_MAX, so this can't overflow. */
    eocdloc_offset = buf_offset + _zip_buffer_offset(buffer);

    _zip_buffer_get(buffer, 4); /* magic already verified */

    eocd_disk = _zip_buffer_get_32(buffer);
    eocd_offset = _zip_buffer_get_64(buffer);
    num_disks = _zip_buffer_get_32(buffer);

    if (!check_magic(eocd_offset, buffer, buf_offset, src, EOCD64_MAGIC)) {
        return CDIR_NOT_FOUND;
    }

    if (num_disks != 1) {
        zip_error_set(error, ZIP_ER_MULTIDISK, 0);
        return CDIR_INVALID;
    }

    /* valid seek value for start of EOCD */
    if (eocd_offset > ZIP_INT64_MAX) {
        zip_error_set(error, ZIP_ER_SEEK, EFBIG);
        return CDIR_INVALID;
    }

    /* does EOCD fit before EOCD locator? */
    if (eocd_offset + EOCD64LEN > eocdloc_offset) {
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_EOCD64_OVERLAPS_EOCD);
        return CDIR_INVALID;
    }

    /* make sure current position of buffer is beginning of EOCD */
    if (eocd_offset >= buf_offset && eocd_offset + EOCD64LEN <= buf_offset + _zip_buffer_size(buffer)) {
        _zip_buffer_set_offset(buffer, eocd_offset - buf_offset);
        free_buffer = false;
    }
    else {
        if (zip_source_seek(src, (zip_int64_t)eocd_offset, SEEK_SET) < 0) {
            zip_error_set_from_source(error, src);
            return CDIR_INVALID;
        }
        if ((buffer = _zip_buffer_new_from_source(src, EOCD64LEN, eocd, error)) == NULL) {
            return CDIR_INVALID;
        }
        free_buffer = true;
    }

    if (memcmp(_zip_buffer_get(buffer, 4), EOCD64_MAGIC, 4) != 0) {
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_EOCD64_WRONG_MAGIC);
        if (free_buffer) {
            _zip_buffer_free(buffer);
        }
        return CDIR_INVALID;
    }

    /* size of EOCD */
    size = _zip_buffer_get_64(buffer);

    /* Is there a hole between EOCD and EOCD locator, or do they overlap? Also check for overflow. */
    if ((flags & ZIP_CHECKCONS) && (ZIP_CHECK_ADD_OVERFLOW(size, eocd_offset + 12) || size + eocd_offset + 12 != eocdloc_offset)) {
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_EOCD64_OVERLAPS_EOCD);
        if (free_buffer) {
            _zip_buffer_free(buffer);
        }
        return CDIR_INVALID;
    }

    _zip_buffer_get(buffer, 4); /* skip version made by/needed */

    this_disk = _zip_buffer_get_32(buffer);
    if (_zip_buffer_get_32(buffer) != eocd_disk) {
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_EOCD64_LOCATOR_MISMATCH);
        if (free_buffer) {
            _zip_buffer_free(buffer);
        }
        return CDIR_INVALID;
    }

    i = _zip_buffer_get_64(buffer);
    nentry = _zip_buffer_get_64(buffer);

    if (nentry != i) {
        zip_error_set(error, ZIP_ER_MULTIDISK, 0);
        if (free_buffer) {
            _zip_buffer_free(buffer);
        }
        return CDIR_INVALID;
    }

    size = _zip_buffer_get_64(buffer);
    offset = _zip_buffer_get_64(buffer);

    /* did we read past the end of the buffer? */
    if (!_zip_buffer_ok(buffer)) {
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
        if (free_buffer) {
            _zip_buffer_free(buffer);
        }
        return CDIR_INVALID;
    }

    if (free_buffer) {
        _zip_buffer_free(buffer);
    }

    if (offset > ZIP_INT64_MAX || offset + size < offset) {
        zip_error_set(error, ZIP_ER_SEEK, EFBIG);
        return CDIR_INVALID;
    }

    if (nentry > size / CDENTRYSIZE) {
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_CDIR_INVALID);
        return CDIR_INVALID;
    }

    if ((cdir->size != 0xffffffff && cdir->size != size) || (cdir->offset != 0xffffffff && cdir->offset != offset) || (cdir->num_entries != 0xffff && cdir->num_entries != nentry) || (cdir->disk_entries != 0xffff && cdir->disk_entries != i) || (cdir->this_disk != 0xffff && cdir->this_disk != this_disk) || (cdir->eocd_disk != 0xffff && cdir->eocd_disk != eocd_disk)) {
        zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_EOCD64_MISMATCH);
        return CDIR_INVALID;
    }

    cdir->is_zip64 = true;
    cdir->size = size;
    cdir->offset = offset;
    cdir->disk_entries = i;
    cdir->num_entries = nentry;
    cdir->this_disk = this_disk;
    cdir->eocd_disk = eocd_disk;

    return CDIR_OK;
}


static int decode_hex(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    else {
        return -1;
    }
}

/* _zip_check_torrentzip:
   check whether ZA has a valid TORRENTZIP comment, i.e. is torrentzipped */

static void zip_check_torrentzip(zip_t *za, const zip_cdir_t *cdir) {
    zip_uint32_t crc_should;
    char buf[8 + 1];
    size_t i;

    if (cdir == NULL) {
        return;
    }

    if (_zip_string_length(cdir->comment) != TORRENTZIP_SIGNATURE_LENGTH + TORRENTZIP_CRC_LENGTH || strncmp((const char *)cdir->comment->raw, TORRENTZIP_SIGNATURE, TORRENTZIP_SIGNATURE_LENGTH) != 0) {
        return;
    }

    memcpy(buf, cdir->comment->raw + TORRENTZIP_SIGNATURE_LENGTH, TORRENTZIP_CRC_LENGTH);
    buf[TORRENTZIP_CRC_LENGTH] = '\0';
    crc_should = 0;
    for (i = 0; i < TORRENTZIP_CRC_LENGTH; i += 2) {
        int low, high;
        high = decode_hex((buf[i]));
        low = decode_hex(buf[i + 1]);
        if (high < 0 || low < 0) {
            return;
        }
        crc_should = (crc_should << 8) + (high << 4) + low;
    }

    {
        zip_stat_t st;
        zip_source_t *src_window;
        zip_source_t *src_crc;
        zip_uint8_t buffer[512];
        zip_int64_t ret;

        zip_stat_init(&st);
        st.valid |= ZIP_STAT_SIZE | ZIP_STAT_CRC;
        st.size = cdir->size;
        st.crc = crc_should;
        if ((src_window = _zip_source_window_new(za->src, cdir->offset, cdir->size, &st, 0, NULL, NULL, NULL, 0, false, NULL)) == NULL) {
            return;
        }
        if ((src_crc = zip_source_crc_create(src_window, 1, NULL)) == NULL) {
            zip_source_free(src_window);
            return;
        }
        if (zip_source_open(src_crc) != 0) {
            zip_source_free(src_crc);
            return;
        }
        while ((ret = zip_source_read(src_crc, buffer, sizeof(buffer))) > 0) {
        }
        zip_source_free(src_crc);
        if (ret < 0) {
            return;
        }
    }

    /* TODO: if check consistency, check cdir entries for valid values */
    za->flags |= ZIP_AFL_IS_TORRENTZIP;
}
