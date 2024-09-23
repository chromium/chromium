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
  Traceable& ref_ptr = traceable;
  (void)ref_ptr;
  std::unique_ptr<Traceable> unique;
  (void)unique;
}

}  // namespace blink
