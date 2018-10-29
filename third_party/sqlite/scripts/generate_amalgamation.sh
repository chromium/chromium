#!/bin/bash
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd src

mkdir bld
cd bld
../configure

OPTS="-DSQLITE_OMIT_UPSERT -DSQLITE_OMIT_WINDOWFUNC"
make "OPTS=$OPTS" shell.c sqlite3.h sqlite3.c
cp -f sqlite3.h sqlite3.c ../../amalgamation

# shell.c must be placed in a different directory from sqlite3.h, because it
# contains an '#include "sqlite3.h"' that we want to resolve to our custom
# //third_party/sqlite/sqlite3.h, not to the sqlite3.h produced here.
mkdir -p ../../amalgamation/shell/
cp -f shell.c ../../amalgamation/shell/

cd ..
rm -rf bld

../scripts/extract_sqlite_api.py ../amalgamation/sqlite3.h \
                                 ../amalgamation/rename_exports.h
