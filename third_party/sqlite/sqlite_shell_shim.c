// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a shim that injects Chrome's ICU initialization sequence into
// SQLite's shell. BUILD.gn uses this instead of building the sqlite_shell.c
// file in the amalgamation directly.

#include "third_party/sqlite/sqlite_shell_icu_helper.h"

// On Windows, SQLite's shell.c defines wmain() instead of main() by default.
// This preprocessor macro causes it to use main().
#define SQLITE_SHELL_IS_UTF8 1

// While processing shell.c, rename main() to sqlite_shell_main().
#define main sqlite_shell_main
#include "third_party/sqlite/amalgamation/shell/shell.c"
#undef main

int main(int argc, char** argv) {
  InitializeICUForSqliteShell();
  return sqlite_shell_main(argc, argv);
}