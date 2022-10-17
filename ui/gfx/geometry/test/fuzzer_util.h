// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TEST_FUZZER_UTIL_H_
#define UI_GFX_GEOMETRY_TEST_FUZZER_UTIL_H_

#include "ui/gfx/geometry/transform.h"

class FuzzedDataProvider;

namespace gfx {

class Transform;

Transform ConsumeTransform(FuzzedDataProvider&);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_TEST_FUZZER_UTIL_H_
