// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This binary opens the provided library and calls get_sum method from it.
//
// It is used solely for testing purposes to validate that the library is
// still working after applying the compression_script on it.

#include <dlfcn.h>
#include <iostream>

using TestFunction = int (*)();

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Library name not provided" << std::endl;
    return 1;
  }
  char* name = argv[1];
  void* handle = dlopen(name, RTLD_NOW);
  if (handle == nullptr) {
    std::cerr << dlerror() << std::endl;
    return 1;
  }

  TestFunction get_sum =
      reinterpret_cast<TestFunction>(dlsym(handle, "GetSum"));
  if (get_sum == nullptr) {
    std::cerr << "GetSum method not found" << std::endl;
    return 1;
  }

  std::cout << get_sum() << std::endl;
  return 0;
}
