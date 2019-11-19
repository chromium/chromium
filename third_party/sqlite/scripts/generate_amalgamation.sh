#!/bin/sh
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o errexit  # Stop the script on the first error.
set -o nounset  # Catch un-initialized variables.

cd patched

mkdir build
cd build

# The compile flags here should match the defines in BUILD.gn.
BUILD_GN_FLAGS="\
    -DSQLITE_DISABLE_FTS3_UNICODE \
    -DSQLITE_DISABLE_FTS4_DEFERRED \
    -DSQLITE_ENABLE_FTS3 \
    -DSQLITE_ENABLE_ICU \
    -DSQLITE_DEFAULT_FILE_PERMISSIONS=0600 \
    -DSQLITE_DEFAULT_LOOKASIDE=0,0 \
    -DSQLITE_DEFAULT_MEMSTATUS=1 \
    -DSQLITE_DEFAULT_PAGE_SIZE=4096 \
    -DSQLITE_DEFAULT_PCACHE_INITSZ=0 \
    -DSQLITE_LIKE_DOESNT_MATCH_BLOBS \
    -DSQLITE_HAVE_ISNAN \
    -DSQLITE_MAX_WORKER_THREADS=0 \
    -DSQLITE_MAX_MMAP_SIZE=268435456 \
    -DSQLITE_OMIT_ANALYZE \
    -DSQLITE_OMIT_AUTOINIT \
    -DSQLITE_OMIT_AUTORESET \
    -DSQLITE_OMIT_COMPILEOPTION_DIAGS \
    -DSQLITE_OMIT_COMPLETE \
    -DSQLITE_OMIT_DECLTYPE \
    -DSQLITE_OMIT_DEPRECATED \
    -DSQLITE_OMIT_EXPLAIN \
    -DSQLITE_OMIT_GET_TABLE \
    -DSQLITE_OMIT_LOAD_EXTENSION \
    -DSQLITE_OMIT_LOOKASIDE \
    -DSQLITE_OMIT_TCL_VARIABLE \
    -DSQLITE_OMIT_PROGRESS_CALLBACK \
    -DSQLITE_OMIT_REINDEX \
    -DSQLITE_OMIT_SHARED_CACHE \
    -DSQLITE_OMIT_TRACE \
    -DSQLITE_OMIT_UPSERT \
    -DSQLITE_OMIT_WINDOWFUNC \
    -DSQLITE_SECURE_DELETE \
    -DSQLITE_THREADSAFE=1 \
    -DSQLITE_USE_ALLOCA \
"

../configure \
    CFLAGS="-Os $BUILD_GN_FLAGS $(icu-config --cppflags)" \
    LDFLAGS="$(icu-config --ldflags)" \
    --disable-load-extension \
    --enable-amalgamation \
    --enable-threadsafe

make shell.c sqlite3.h sqlite3.c
cp -f sqlite3.h sqlite3.c ../../amalgamation

# shell.c must be placed in a different directory from sqlite3.h, because it
# contains an '#include "sqlite3.h"' that we want to resolve to our custom
# //third_party/sqlite/sqlite3.h, not to the sqlite3.h produced here.
mkdir -p ../../amalgamation/shell/
cp -f shell.c ../../amalgamation/shell/

cd ..
rm -rf build

../scripts/extract_sqlite_api.py ../amalgamation/sqlite3.h \
                                 ../amalgamation/rename_exports.h
