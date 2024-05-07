// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/containers/span.h"

class SomeClass;

struct MyStruct {
  // No rewrite expected - this whole source file is mentioned in the
  // tests/paths-to-ignore.txt file.
  SomeClass* ptr_field_;
  SomeClass& ref_field_;
  base::span<SomeClass> span_field_;
};
