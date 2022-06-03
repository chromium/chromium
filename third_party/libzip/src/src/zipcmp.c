/*
  zipcmp.c -- compare zip files
  Copyright (C) 2003-2019 Dieter Baron and Thomas Klausner

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


#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_FTS_H
#include <fts.h>
#endif
#include <zlib.h>

#ifndef HAVE_GETOPT
#include "getopt.h"
#endif

#include "zip.h"

#include "compat.h"

struct archive {
    const char *name;
    zip_t *za;
    zip_uint64_t nentry;
    struct entry *entry;
    const char *comment;
    size_t comment_length;
};

struct ef {
    const char *name;
    zip_uint16_t flags;
    zip_uint16_t id;
    zip_uint16_t size;
    const zip_uint8_t *data;
};

struct entry {
    char *name;
    zip_uint64_t size;
    zip_uint32_t crc;
    zip_uint32_t comp_method;
    struct ef *extra_fields;
    zip_uint16_t n_extra_fields;
    const char *comment;
    zip_uint32_t comment_length;
};


const char *progname;

#define PROGRAM "zipcmp"

#define USAGE "usage: %s [-hipqtVv] archive1 archive2\n"

char help_head[] = PROGRAM " (" PACKAGE ") by Dieter Baron and Thomas Klausner\n\n";

char help[] = "\n\
  -h       display this help message\n\
  -i       compare names ignoring case distinctions\n\
  -p       compare as many details as possible\n\
  -q       be quiet\n\
  -t       test zip files (compare file contents to checksum)\n\
  -V       display version number\n\
  -v       be verbose (print differences, default)\n\
\n\
Report bugs to <libzip@nih.at>.\n";

char version_string[] = PROGRAM " (" PACKAGE " " VERSION ")\n\
Copyright (C) 2003-2019 Dieter Baron and Thomas Klausner\n\
" PACKAGE " comes with ABSOLUTELY NO WARRANTY, to the extent permitted by law.\n";

#define OPTIONS "hVipqtv"


#define BOTH_ARE_ZIPS(a) (a[0].za && a[1].za)

static int comment_compare(const char *c1, size_t l1, const char *c2, size_t l2);
static int compare_list(char *const name[], const void *l[], const zip_uint64_t n[], int size, int (*cmp)(const void *, const void *), int (*checks)(char *const name[2], const void *, const void *), void (*print)(const void *));
static int compare_zip(char *const zn[]);
static int ef_compare(char *const name[2], const struct entry *e1, const struct entry *e2);
static int ef_order(const void *a, const void *b);
static void ef_print(const void *p);
static int ef_read(zip_t *za, zip_uint64_t idx, struct entry *e);
static int entry_cmp(const void *p1, const void *p2);
static int entry_paranoia_checks(char *const name[2], const void *p1, const void *p2);
static void entry_print(const void *p);
static int is_directory(const char *name);
#ifdef HAVE_FTS_H
static int list_directory(const char *name, struct archive *a);
#endif
static int list_zip(const char *name, struct archive *a);
static int test_file(zip_t *za, zip_uint64_t idx, zip_uint64_t size, zip_uint32_t crc);

int ignore_case, test_files, paranoid, verbose;
int header_done;


int
main(int argc, char *const argv[]) {
    int c;

    progname = argv[0];

    ignore_case = 0;
    test_files = 0;
    paranoid = 0;
    verbose = 1;

    while ((c = getopt(argc, argv, OPTIONS)) != -1) {
	switch (c) {
	case 'i':
	    ignore_case = 1;
	    break;
	case 'p':
	    paranoid = 1;
	    break;
	case 'q':
	    verbose = 0;
	    break;
	case 't':
	    test_files = 1;
	    break;
	case 'v':
	    verbose = 1;
	    break;

	case 'h':
	    fputs(help_head, stdout);
	    printf(USAGE, progname);
	    fputs(help, stdout);
	    exit(0);
	case 'V':
	    fputs(version_string, stdout);
	    exit(0);

	default:
	    fprintf(stderr, USAGE, progname);
	    exit(2);
	}
    }

    if (argc != optind + 2) {
	fprintf(stderr, USAGE, progname);
	exit(2);
    }

    exit((compare_zip(argv + optind) == 0) ? 0 : 1);
}


static int
compare_zip(char *const zn[]) {
    struct archive a[2];
    struct entry *e[2];
    zip_uint64_t n[2];
    int i;
    int res;

    for (i = 0; i < 2; i++) {
	a[i].name = zn[i];
	a[i].entry = NULL;
	a[i].nentry = 0;
	a[i].za = NULL;
	a[i].comment = NULL;
	a[i].comment_length = 0;

	if (is_directory(zn[i])) {
#ifndef HAVE_FTS_H
	    fprintf(stderr, "%s: reading directories not supported\n", progname);
	    exit(2);
#else
	    if (list_directory(zn[i], a + i) < 0)
		exit(2);
	    paranoid = 0; /* paranoid checks make no sense for directories, since they compare zip metadata */
#endif
	}
	else {
	    if (list_zip(zn[i], a + i) < 0)
		exit(2);
	}
	if (a[i].nentry > 0)
	    qsort(a[i].entry, a[i].nentry, sizeof(a[i].entry[0]), entry_cmp);
    }

    header_done = 0;

    e[0] = a[0].entry;
    e[1] = a[1].entry;
    n[0] = a[0].nentry;
    n[1] = a[1].nentry;
    res = compare_list(zn, (const void **)e, n, sizeof(e[i][0]), entry_cmp, paranoid ? entry_paranoia_checks : NULL, entry_print);

    if (paranoid) {
	if (comment_compare(a[0].comment, a[0].comment_length, a[1].comment, a[1].comment_length) != 0) {
	    if (verbose) {
		printf("--- archive comment (%zu)\n", a[0].comment_length);
		printf("+++ archive comment (%zu)\n", a[1].comment_length);
	    }
	    res = 1;
	}
    }

    for (i = 0; i < 2; i++) {
	zip_uint64_t j;

	if (a[i].za) {
	    zip_close(a[i].za);
	}
	for (j = 0; j < a[i].nentry; j++) {
	    free(a[i].entry[j].name);
	}
	free(a[i].entry);
    }

    switch (res) {
    case 0:
	exit(0);

    case 1:
	exit(1);

    default:
	exit(2);
    }
}

#ifdef HAVE_FTS_H
static zip_int64_t
compute_crc(const char *fname) {
    FILE *f;
    uLong crc = crc32(0L, Z_NULL, 0);
    size_t n;
    Bytef buffer[8192];


    if ((f = fopen(fname, "rb")) == NULL) {
	fprintf(stderr, "%s: can't open %s: %s\n", progname, fname, strerror(errno));
	return -1;
    }

    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
	crc = crc32(crc, buffer, (unsigned int)n);
    }

    if (ferror(f)) {
	fprintf(stderr, "%s: read error on %s: %s\n", progname, fname, strerror(errno));
	fclose(f);
	return -1;
    }

    fclose(f);

    return (zip_int64_t)crc;
}
#endif


static int
is_directory(const char *name) {
    struct stat st;

    if (stat(name, &st) < 0)
	return 0;

    return S_ISDIR(st.st_mode);
}


#ifdef HAVE_FTS_H
static int
list_directory(const char *name, struct archive *a) {
    FTS *fts;
    FTSENT *ent;
    zip_uint64_t nalloc;
    size_t prefix_length;

    char *const names[2] = {(char *)name, NULL};


    if ((fts = fts_open(names, FTS_NOCHDIR | FTS_LOGICAL, NULL)) == NULL) {
	fprintf(stderr, "%s: can't open directory '%s': %s\n", progname, name, strerror(errno));
	return -1;
    }
    prefix_length = strlen(name) + 1;

    nalloc = 0;

    while ((ent = fts_read(fts))) {
	zip_int64_t crc;

	switch (ent->fts_info) {
	case FTS_D:
	case FTS_DOT:
	case FTS_DP:
	case FTS_DEFAULT:
	case FTS_SL:
	case FTS_NSOK:
	    break;

	case FTS_DC:
	case FTS_DNR:
	case FTS_ERR:
	case FTS_NS:
	case FTS_SLNONE:
	    /* TODO: error */
	    fts_close(fts);
	    return -1;

	case FTS_F:
	    if (a->nentry >= nalloc) {
		nalloc += 16;
		if (nalloc > SIZE_MAX / sizeof(a->entry[0])) {
		    fprintf(stderr, "%s: malloc failure\n", progname);
		    exit(1);
		}
		a->entry = realloc(a->entry, sizeof(a->entry[0]) * nalloc);
		if (a->entry == NULL) {
		    fprintf(stderr, "%s: malloc failure\n", progname);
		    exit(1);
		}
	    }

	    a->entry[a->nentry].name = strdup(ent->fts_path + prefix_length);
	    a->entry[a->nentry].size = (zip_uint64_t)ent->fts_statp->st_size;
	    if ((crc = compute_crc(ent->fts_accpath)) < 0) {
		fts_close(fts);
		return -1;
	    }

	    a->entry[a->nentry].crc = (zip_uint32_t)crc;
	    a->nentry++;
	    break;
	}
    }

    if (fts_close(fts)) {
	fprintf(stderr, "%s: error closing directory '%s': %s\n", progname, a->name, strerror(errno));
	return -1;
    }

    return 0;
}
#endif


static int
list_zip(const char *name, struct archive *a) {
    zip_t *za;
    int err;
    struct zip_stat st;
    unsigned int i;

    if ((za = zip_open(name, paranoid ? ZIP_CHECKCONS : 0, &err)) == NULL) {
	zip_error_t error;
	zip_error_init_with_code(&error, err);
	fprintf(stderr, "%s: cannot open zip archive '%s': %s\n", progname, name, zip_error_strerror(&error));
	zip_error_fini(&error);
	return -1;
    }

    a->za = za;
    a->nentry = (zip_uint64_t)zip_get_num_entries(za, 0);

    if (a->nentry == 0)
	a->entry = NULL;
    else {
	if ((a->nentry > SIZE_MAX / sizeof(a->entry[0])) || (a->entry = (struct entry *)malloc(sizeof(a->entry[0]) * a->nentry)) == NULL) {
	    fprintf(stderr, "%s: malloc failure\n", progname);
	    exit(1);
	}

	for (i = 0; i < a->nentry; i++) {
	    zip_stat_index(za, i, 0, &st);
	    a->entry[i].name = strdup(st.name);
	    a->entry[i].size = st.size;
	    a->entry[i].crc = st.crc;
	    if (test_files)
		test_file(za, i, st.size, st.crc);
	    if (paranoid) {
		a->entry[i].comp_method = st.comp_method;
		ef_read(za, i, a->entry + i);
		a->entry[i].comment = zip_file_get_comment(za, i, &a->entry[i].comment_length, 0);
	    }
	    else {
		a->entry[i].comp_method = 0;
		a->entry[i].n_extra_fields = 0;
	    }
	}

	if (paranoid) {
	    int length;
	    a->comment = zip_get_archive_comment(za, &length, 0);
	    a->comment_length = (size_t)length;
	}
	else {
	    a->comment = NULL;
	    a->comment_length = 0;
	}
    }

    return 0;
}


static int
comment_compare(const char *c1, size_t l1, const char *c2, size_t l2) {
    if (l1 != l2)
	return 1;

    if (l1 == 0)
	return 0;

    if (c1 == NULL || c2 == NULL)
	return c1 == c2;

    return memcmp(c1, c2, (size_t)l2);
}


static int
compare_list(char *const name[2], const void *l[2], const zip_uint64_t n[2], int size, int (*cmp)(const void *, const void *), int (*check)(char *const name[2], const void *, const void *), void (*print)(const void *)) {
    unsigned int i[2];
    int j, c;
    int diff;

#define INC(k) (i[k]++, l[k] = ((const char *)l[k]) + size)
#define PRINT(k)                                          \
    do {                                                  \
	if (header_done == 0 && verbose) {                \
	    printf("--- %s\n+++ %s\n", name[0], name[1]); \
	    header_done = 1;                              \
	}                                                 \
	if (verbose) {                                    \
	    printf("%c ", (k) ? '+' : '-');               \
	    print(l[k]);                                  \
	}                                                 \
	diff = 1;                                         \
    } while (0)

    i[0] = i[1] = 0;
    diff = 0;
    while (i[0] < n[0] && i[1] < n[1]) {
	c = cmp(l[0], l[1]);

	if (c == 0) {
	    if (check)
		diff |= check(name, l[0], l[1]);
	    INC(0);
	    INC(1);
	}
	else if (c < 0) {
	    PRINT(0);
	    INC(0);
	}
	else {
	    PRINT(1);
	    INC(1);
	}
    }

    for (j = 0; j < 2; j++) {
	while (i[j] < n[j]) {
	    PRINT(j);
	    INC(j);
	}
    }

    return diff;
}


static int
ef_read(zip_t *za, zip_uint64_t idx, struct entry *e) {
    zip_int16_t n_local, n_central;
    zip_uint16_t i;

    if ((n_local = zip_file_extra_fields_count(za, idx, ZIP_FL_LOCAL)) < 0 || (n_central = zip_file_extra_fields_count(za, idx, ZIP_FL_CENTRAL)) < 0) {
	return -1;
    }

    e->n_extra_fields = (zip_uint16_t)(n_local + n_central);

    if ((e->extra_fields = (struct ef *)malloc(sizeof(e->extra_fields[0]) * e->n_extra_fields)) == NULL)
	return -1;

    for (i = 0; i < n_local; i++) {
	e->extra_fields[i].name = e->name;
	e->extra_fields[i].data = zip_file_extra_field_get(za, idx, i, &e->extra_fields[i].id, &e->extra_fields[i].size, ZIP_FL_LOCAL);
	if (e->extra_fields[i].data == NULL)
	    return -1;
	e->extra_fields[i].flags = ZIP_FL_LOCAL;
    }
    for (; i < e->n_extra_fields; i++) {
	e->extra_fields[i].name = e->name;
	e->extra_fields[i].data = zip_file_extra_field_get(za, idx, (zip_uint16_t)(i - n_local), &e->extra_fields[i].id, &e->extra_fields[i].size, ZIP_FL_CENTRAL);
	if (e->extra_fields[i].data == NULL)
	    return -1;
	e->extra_fields[i].flags = ZIP_FL_CENTRAL;
    }

    qsort(e->extra_fields, e->n_extra_fields, sizeof(e->extra_fields[0]), ef_order);

    return 0;
}


static int
ef_compare(char *const name[2], const struct entry *e1, const struct entry *e2) {
    struct ef *ef[2];
    zip_uint64_t n[2];

    ef[0] = e1->extra_fields;
    ef[1] = e2->extra_fields;
    n[0] = e1->n_extra_fields;
    n[1] = e2->n_extra_fields;

    return compare_list(name, (const void **)ef, n, sizeof(struct ef), ef_order, NULL, ef_print);
}


static int
ef_order(const void *ap, const void *bp) {
    const struct ef *a, *b;

    a = (struct ef *)ap;
    b = (struct ef *)bp;

    if (a->flags != b->flags)
	return a->flags - b->flags;
    if (a->id != b->id)
	return a->id - b->id;
    if (a->size != b->size)
	return a->size - b->size;
    return memcmp(a->data, b->data, a->size);
}


static void
ef_print(const void *p) {
    const struct ef *ef = (struct ef *)p;
    int i;

    printf("                    %s  ", ef->name);
    printf("%04x %c <", ef->id, ef->flags == ZIP_FL_LOCAL ? 'l' : 'c');
    for (i = 0; i < ef->size; i++)
	printf("%s%02x", i ? " " : "", ef->data[i]);
    printf(">\n");
}


static int
entry_cmp(const void *p1, const void *p2) {
    const struct entry *e1, *e2;
    int c;

    e1 = (struct entry *)p1;
    e2 = (struct entry *)p2;

    if ((c = (ignore_case ? strcasecmp : strcmp)(e1->name, e2->name)) != 0)
	return c;
    if (e1->size != e2->size) {
	if (e1->size > e2->size)
	    return 1;
	else
	    return -1;
    }
    if (e1->crc != e2->crc)
	return (int)e1->crc - (int)e2->crc;

    return 0;
}


static int
entry_paranoia_checks(char *const name[2], const void *p1, const void *p2) {
    const struct entry *e1, *e2;
    int ret;

    e1 = (struct entry *)p1;
    e2 = (struct entry *)p2;

    ret = 0;

    if (ef_compare(name, e1, e2) != 0)
	ret = 1;

    if (e1->comp_method != e2->comp_method) {
	if (verbose) {
	    if (header_done == 0) {
		printf("--- %s\n+++ %s\n", name[0], name[1]);
		header_done = 1;
	    }
	    printf("---                     %s  ", e1->name);
	    printf("method %u\n", e1->comp_method);
	    printf("+++                     %s  ", e1->name);
	    printf("method %u\n", e2->comp_method);
	}
	ret = 1;
    }
    if (comment_compare(e1->comment, e1->comment_length, e2->comment, e2->comment_length) != 0) {
	if (verbose) {
	    if (header_done == 0) {
		printf("--- %s\n+++ %s\n", name[0], name[1]);
		header_done = 1;
	    }
	    printf("---                     %s  ", e1->name);
	    printf("comment %" PRIu32 "\n", e1->comment_length);
	    printf("+++                     %s  ", e1->name);
	    printf("comment %" PRIu32 "\n", e2->comment_length);
	}
	ret = 1;
    }

    return ret;
}


static void
entry_print(const void *p) {
    const struct entry *e;

    e = (struct entry *)p;

    /* TODO PRId64 */
    printf("%10lu %08x %s\n", (unsigned long)e->size, e->crc, e->name);
}


static int
test_file(zip_t *za, zip_uint64_t idx, zip_uint64_t size, zip_uint32_t crc) {
    zip_file_t *zf;
    char buf[8192];
    zip_uint64_t nsize;
    zip_int64_t n;
    zip_uint32_t ncrc;

    if ((zf = zip_fopen_index(za, idx, 0)) == NULL) {
	fprintf(stderr, "%s: cannot open file %" PRIu64 " in archive: %s\n", progname, idx, zip_strerror(za));
	return -1;
    }

    ncrc = (zip_uint32_t)crc32(0, NULL, 0);
    nsize = 0;

    while ((n = zip_fread(zf, buf, sizeof(buf))) > 0) {
	nsize += (zip_uint64_t)n;
	ncrc = (zip_uint32_t)crc32(ncrc, (const Bytef *)buf, (unsigned int)n);
    }

    if (n < 0) {
	fprintf(stderr, "%s: error reading file %" PRIu64 " in archive: %s\n", progname, idx, zip_file_strerror(zf));
	zip_fclose(zf);
	return -1;
    }

    zip_fclose(zf);

    if (nsize != size) {
	fprintf(stderr, "%s: file %" PRIu64 ": unexpected length %" PRId64 " (should be %" PRId64 ")\n", progname, idx, nsize, size);
	return -2;
    }
    if (ncrc != crc) {
	fprintf(stderr, "%s: file %" PRIu64 ": unexpected length %x (should be %x)\n", progname, idx, ncrc, crc);
	return -2;
    }

    return 0;
}
