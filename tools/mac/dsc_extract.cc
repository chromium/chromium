// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tool dsc_extract is used to dump the contents of a macOS dyld shared cache.
// It is recommended to only use this on the version of macOS with the matching
// shared cache macOS version because the format of the cache can change
// between macOS versions.

#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

using ExtractDylibsProgressT = int (*)(const char* shared_cache_file_path,
                                       const char* extraction_root_path,
                                       void (^progress)(unsigned current,
                                                        unsigned total));

int main(int argc, const char* argv[]) {
  if (argc != 3) {
    fprintf(stderr,
            "usage: dsc_extract <path-to-cache-file> <path-to-device-dir>\n");
    return EXIT_FAILURE;
  }

  void* handle = dlopen("/usr/lib/dsc_extractor.bundle", RTLD_LAZY);
  if (handle == nullptr) {
    fprintf(stderr, "dsc_extractor.bundle could not be loaded\n");
    return EXIT_FAILURE;
  }

  auto* extract = reinterpret_cast<ExtractDylibsProgressT>(
      dlsym(handle, "dyld_shared_cache_extract_dylibs_progress"));
  if (extract == nullptr) {
    fprintf(stderr,
            "dsc_extractor.bundle did not have "
            "dyld_shared_cache_extract_dylibs_progress symbol\n");
    return EXIT_FAILURE;
  }

  int result = (*extract)(argv[1], argv[2], ^(unsigned c, unsigned total) {
    fprintf(stdout, "%d/%d\n", c, total);
  });
  fprintf(stderr, "dyld_shared_cache_extract_dylibs_progress() => %d\n",
          result);
  return EXIT_SUCCESS;
}
