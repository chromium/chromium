// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_SQLITE_SQLITE3_H_
#define THIRD_PARTY_SQLITE_SQLITE3_H_

// This is a shim header to include the right sqlite3 headers.
// Use this instead of referencing sqlite3 headers directly.

// We prefix chrome_ to SQLite's exported symbols, so that we don't clash with
// other SQLite libraries loaded by the system libraries. This only matters when
// using the component build, where our SQLite's symbols are visible to the
// dynamic library loader.
#include "third_party/sqlite/src/amalgamation/rename_exports.h"

#if defined(SQLITE_OMIT_COMPLETE)
// When SQLITE_OMIT_COMPLETE is defined, sqlite3.h does not emit a declaration
// for sqlite3_complete(). SQLite's shell.c stubs out the function by #defining
// a macro.
//
// In order to avoid a macro redefinition warning, we must undo the #define in
// rename_exports.h.
//
// Historical note: SQLite's shell.c initially did not support building against
// a libary with SQLITE_OMIT_COMPLETE at all. The first attempt at adding
// support was https://www.sqlite.org/src/info/c3e816cca4ddf096 which defined
// sqlite_complete() as a stub function in shell.c. This worked on UNIX systems,
// but caused a compilation error on Windows, where sqlite3.h declares
// sqlite3_complete() as a __declspec(dllimport). The Windows build error was
// fixed in https://www.sqlite.org/src/info/d584a0cb51281594 at our request.
// While the current approach of using a macro requires the workaround here, it
// is preferable to the previous version, which did not build at all on Windows.
#if defined(sqlite3_complete)
#undef sqlite3_complete
#else
#error "This workaround is no longer needed."
#endif  // !defined(sqlite3_complete)
#endif  // defined(SQLITE_OMIT_COMPLETE)

#if defined(SQLITE_OMIT_COMPILEOPTION_DIAGS)
// When SQLITE_OMIT_COMPILEOPTION_DIAGS is defined, sqlite3.h emits macros
// instead of declarations for sqlite3_compileoption_{get,used}().
//
// In order to avoid a macro redefinition warning, we must undo the #define in
// rename_exports.h.
#if defined(sqlite3_compileoption_get)
#undef sqlite3_compileoption_get
#else
#error "This workaround is no longer needed."
#endif  // !defined(sqlite3_compileoption_get)
#if defined(sqlite3_compileoption_used)
#undef sqlite3_compileoption_used
#else
#error "This workaround is no longer needed."
#endif  // !defined(sqlite3_compileoption_used)
#endif  // defined(SQLITE_OMIT_COMPILEOPTION_DIAGS)

#include "third_party/sqlite/src/amalgamation/sqlite3.h"

#endif  // THIRD_PARTY_SQLITE_SQLITE3_H_
