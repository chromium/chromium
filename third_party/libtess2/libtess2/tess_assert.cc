// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libtess2/libtess2/tess_assert.h"

#include "base/check.h"

void tess_assert(int conditional) {
  CHECK(conditional);
}
