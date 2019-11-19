// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a source code of lib.so file located in this directory. This file
// serves as a testdata for testing elf_headers.py. We need to store .so
// precompiled and not build it on demand since we can't rely on resulting ELF
// structure not changing between different compilations or due to change in
// clang version.
//
// The library was build with the following command:
//    clang lib.c -shared -fPIC -O2 -o lib.so
//
// This library is intentionally very small to both ensure the speed of the test
// and to not add huge binary file into source control system.

int array[3] = {1, 2, 3};

int GetSum() {
  return array[0] + array[1] + array[2];
}
