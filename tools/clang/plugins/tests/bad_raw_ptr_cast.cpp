// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bad_raw_ptr_cast.h"

#include <cstring>

void BadRawPtr::Check() {
  // Explicit cast from raw_ptr<T>* is not allowed.
  void* bar = reinterpret_cast<void*>(&foo_);
  bar = static_cast<void*>(&foo_);
  bar = (void*)&foo_;
  // Implicit cast from raw_ptr<T>* is not allowed.
  bar = &foo_;
  memcpy(&foo_, &foo_, 1);
}
