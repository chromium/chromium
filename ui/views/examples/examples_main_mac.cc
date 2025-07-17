// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "ui/views/examples/examples_main.h"

#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#define VIEWS_EXAMPLES_NAME "Views Examples"

int main(int argc, char** argv) {
  uint32_t exec_path_size = 0;
  int rv = _NSGetExecutablePath(nullptr, &exec_path_size);
  if (rv != -1) {
    fprintf(stderr, "_NSGetExecutablePath: get length failed\n");
    abort();
  }

  std::unique_ptr<char[]> exec_path(new char[exec_path_size]);
  rv = _NSGetExecutablePath(exec_path.get(), &exec_path_size);
  if (rv != 0) {
    fprintf(stderr, "_NSGetExecutablePath: get path failed\n");
    abort();
  }

  // Slice off the last part of the main executable path, and append the
  // version framework information.
  const char* parent_dir = dirname(exec_path.get());
  if (!parent_dir) {
    fprintf(stderr, "dirname %s: %s\n", exec_path.get(), strerror(errno));
    abort();
  }

  const char rel_path[] =
      "../Frameworks/" VIEWS_EXAMPLES_NAME
      " Framework.framework/" VIEWS_EXAMPLES_NAME " Framework";
  const size_t parent_dir_len = strlen(parent_dir);
  const size_t rel_path_len = strlen(rel_path);
  // 2 accounts for a trailing NUL byte and the '/' in the middle of the paths.
  const size_t framework_path_size = parent_dir_len + rel_path_len + 2;
  std::unique_ptr<char[]> framework_path(new char[framework_path_size]);
  snprintf(framework_path.get(), framework_path_size, "%s/%s", parent_dir,
           rel_path);

  void* library =
      dlopen(framework_path.get(), RTLD_LAZY | RTLD_LOCAL | RTLD_FIRST);
  if (!library) {
    fprintf(stderr, "dlopen %s: %s\n", framework_path.get(), dlerror());
    abort();
  }

  auto* views_examples_main = reinterpret_cast<decltype(ViewsExamplesMain)*>(
      dlsym(library, "ViewsExamplesMain"));
  if (!views_examples_main) {
    fprintf(stderr, "dlsym main: %s\n", dlerror());
    abort();
  }
  rv = views_examples_main(argc, argv);

  // Don't return from main to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(rv);
}
