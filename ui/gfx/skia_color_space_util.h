// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
#define UI_GFX_SKIA_COLOR_SPACE_UTIL_H_

#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space_export.h"

namespace gfx {

// Return the parameterized function in |fn|, evaluated at |x|. This will not
// clamp output values.
// TODO(crbug.com/40944641): Replace this function with
// skcms_TransferFunction_eval.
float COLOR_SPACE_EXPORT
SkTransferFnEvalUnclamped(const skcms_TransferFunction& fn, float x);

// TODO(crbug.com/40944641): Replace this function with
// skcms_TransferFunction_invert.
skcms_TransferFunction COLOR_SPACE_EXPORT
SkTransferFnInverse(const skcms_TransferFunction& fn);

// Return true if `a` and `b` approximately cancel out.
// TODO(crbug.com/40944641): This function determines the result by testing `b`
// after `a` on several points on the unit interval, which is not efficient or
// accurate.
bool COLOR_SPACE_EXPORT
SkTransferFnsApproximatelyCancel(const skcms_TransferFunction& a,
                                 const skcms_TransferFunction& b);

// Returns true if `fn` is approximately the identity.
// TODO(crbug.com/40944641): This function determines the result by testing `fn`
// on several points in the unit interval, which is not efficient or accurate.
bool COLOR_SPACE_EXPORT
SkTransferFnIsApproximatelyIdentity(const skcms_TransferFunction& fn);

// Returns true if `m` is within `epsilon` of the identity in the L-infinity
// sense.
bool COLOR_SPACE_EXPORT SkM44IsApproximatelyIdentity(const SkM44& m,
                                                     float epsilon = 1.f /
                                                                     256.f);

// Convert between skcms and Skia matrices. These assume that the 4th row and
// column of the SkM44 are [0,0,0,1].
skcms_Matrix3x3 COLOR_SPACE_EXPORT SkcmsMatrix3x3FromSkM44(const SkM44& in);
SkM44 COLOR_SPACE_EXPORT SkM44FromSkcmsMatrix3x3(const skcms_Matrix3x3& in);
SkM44 COLOR_SPACE_EXPORT SkM44FromRowMajor3x3(const float* scale);

}  // namespace gfx

#endif  // UI_GFX_SKIA_COLOR_SPACE_UTIL_H_
