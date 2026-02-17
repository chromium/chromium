// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

int UnsafeIndex();  // This function might return an out-of-bound index.

// void type does not have size information and thus cannot be rewritten to
// span<void>.
// TODO(crbug.com/402595516): void should be rewritten to another type like
// uint8_t.

void fct() {
  int buffer[]{1, 2};
  // No rewrite expected.
  void* void_ptr = buffer;
  int* ptr = static_cast<int*>(void_ptr);
  std::ignore = ptr[UnsafeIndex()];
}

// No rewrite expected.
void my_func(void* void_ptr) {
  int* ptr = static_cast<int*>(void_ptr);
  std::ignore = ptr[UnsafeIndex()];
}

void fct2() {
  int buffer[]{1, 2};
  my_func(buffer);
}

struct MyStruct {
  // No rewrite expected.
  void* void_ptr;
};

void fct4() {
  int buffer[]{1, 2};
  MyStruct my_struct;
  my_struct.void_ptr = buffer;
  int* ptr = static_cast<int*>(my_struct.void_ptr);
  std::ignore = ptr[UnsafeIndex()];
}
