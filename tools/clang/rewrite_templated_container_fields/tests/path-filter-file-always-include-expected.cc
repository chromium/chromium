// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "base/memory/raw_ptr.h"

class SomeClass;

struct MyStruct {
  // Rewrite expected - this file is force included in the rewrite using ! in
  // tests/paths-to-ignore.txt file.
  std::vector<raw_ptr<SomeClass>> ptr_field_;
};
