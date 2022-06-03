// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
#define UI_GFX_SKIA_COLOR_SPACE_UTIL_H_

#include "skia/ext/skia_matrix_44.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/color_space_export.h"

namespace gfx {

// Return the parameterized function in |fn|, evaluated at |x|. Note that this
// will clamp output values to the range [0, 1].
float COLOR_SPACE_EXPORT SkTransferFnEval(const skcms_TransferFunction& fn,
                                          float x);

// Return the parameterized function in |fn|, evaluated at |x|. This will not
// clamp output values.
float COLOR_SPACE_EXPORT
SkTransferFnEvalUnclamped(const skcms_TransferFunction& fn, float x);

skcms_TransferFunction COLOR_SPACE_EXPORT
SkTransferFnInverse(const skcms_TransferFunction& fn);

skcms_TransferFunction COLOR_SPACE_EXPORT
SkTransferFnScaled(const skcms_TransferFunction& fn, float scale);

bool COLOR_SPACE_EXPORT
SkTransferFnsApproximatelyCancel(const skcms_TransferFunction& a,
                                 const skcms_TransferFunction& b);

bool COLOR_SPACE_EXPORT
SkTransferFnIsApproximatelyIdentity(const skcms_TransferFunction& fn);

bool COLOR_SPACE_EXPORT
SkMatrixIsApproximatelyIdentity(const skia::Matrix44& m);

}  // namespace gfx

#endif  // UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
