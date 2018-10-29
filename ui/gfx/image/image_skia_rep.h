// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_SKIA_REP_H_
#define UI_GFX_IMAGE_IMAGE_SKIA_REP_H_

#include "build/build_config.h"

#if defined(OS_IOS)
#include "ui/gfx/image/image_skia_rep_ios.h"
#else
#include "ui/gfx/image/image_skia_rep_default.h"
#endif  // defined(OS_IOS)

#endif  // UI_GFX_IMAGE_IMAGE_SKIA_REP_H_
