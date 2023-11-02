// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEST_ICC_PROFILES_H_
#define UI_GFX_TEST_ICC_PROFILES_H_

#include "ui/gfx/icc_profile.h"

namespace gfx {

ICCProfile ICCProfileForTestingAdobeRGB();
ICCProfile ICCProfileForTestingColorSpin();
ICCProfile ICCProfileForTestingGenericRGB();
ICCProfile ICCProfileForTestingSRGB();

// A profile that does not have an analytic transfer function.
ICCProfile ICCProfileForTestingNoAnalyticTrFn();

// A profile that is A2B only.
ICCProfile ICCProfileForTestingA2BOnly();

// A profile that with an approxmation that shoots above 1.
ICCProfile ICCProfileForTestingOvershoot();

}  // namespace gfx

#endif  // UI_GFX_TEST_ICC_PROFILES_H_