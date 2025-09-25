// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ptrs_to_traceable_class.h"

namespace blink {

void Foo() {
  Traceable traceable;
  // The following pointers should be ok.
  Traceable* raw_ptr = &traceable;
  (void)raw_ptr;
  const Traceable* const_raw_ptr = &traceable;
  (void)const_raw_ptr;
  Traceable& ref_ptr = traceable;
  (void)ref_ptr;
  std::unique_ptr<Traceable> unique;
  (void)unique;
  std::array<Traceable, 4> array;
  (void)array;
  std::array<Traceable*, 4> array_ptrs;
  (void)array_ptrs;
  std::vector<Traceable> vector;  // Not ok.
  (void)vector;
  std::vector<Traceable*> vector_ptrs;  // Not ok.
  (void)vector_ptrs;
}

}  // namespace blink
