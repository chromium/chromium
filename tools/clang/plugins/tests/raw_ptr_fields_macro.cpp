// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <raw_ptr_system_test.h>

struct MyStruct {
  int* ptr0;     // Error.
  INT* ptr1;     // Error: typeLoc is macro but identifier is written here.
  INTP_FIELD();  // OK: code owner has no control over fieldDecl.
};
