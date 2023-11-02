// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a shim that injects Chrome-specific definitions into sqlite3.c
// BUILD.gn uses this instead of building the sqlite3 amalgamation directly.

// We prefix chrome_ to SQLite's exported symbols, so that we don't clash with
// other SQLite libraries loaded by the system libraries. This only matters when
// using the component build, where our SQLite's symbols are visible to the
// dynamic library loader.
#include "third_party/sqlite/src/amalgamation/rename_exports.h"

#include "third_party/sqlite/sqlite3_shim_fixups.h"

#include "third_party/sqlite/src/amalgamation/sqlite3.c"
