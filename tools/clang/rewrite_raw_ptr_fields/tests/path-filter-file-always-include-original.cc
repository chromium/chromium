// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

class SomeClass;

struct MyStruct {
  // Rewrite expected - this file is force included in the rewrite using ! in
  // tests/paths-to-ignore.txt file.
  SomeClass* ptr_field_;
  SomeClass& ref_field_;
  base::span<SomeClass> span_field_;
};
