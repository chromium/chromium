// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/raw_span.h"

class SomeClass;

struct MyStruct {
  // Rewrite expected - this file is force included in the rewrite using ! in
  // tests/paths-to-ignore.txt file.
  raw_ptr<SomeClass> ptr_field_;
  const raw_ref<SomeClass> ref_field_;
  base::raw_span<SomeClass> span_field_;
};
