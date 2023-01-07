// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_GBM_DEFINES_H_
#define UI_GFX_LINUX_GBM_DEFINES_H_

#include <gbm.h>

// Minigbm has some defines that are used by ozone/gbm. However, when we build
// Ozone for Linux and use system libgbm, these defines are not present and
// compilation fails. Thus, to fix the issue, mask out these defines and let the
// compilation go through. The values for these are copied from the minigbm's
// gbm.h file.
#if !defined(MINIGBM)
#define GBM_MAX_PLANES 4

#define GBM_BO_USE_TEXTURING 0
#define GBM_BO_USE_CAMERA_WRITE 0
#define GBM_BO_USE_HW_VIDEO_DECODER 0
#define GBM_BO_USE_HW_VIDEO_ENCODER 0
#define GBM_BO_USE_PROTECTED 0
#define GBM_BO_USE_SW_READ_OFTEN 0
#define GBM_BO_USE_FRONT_RENDERING 0
#endif

#endif  // UI_GFX_LINUX_GBM_DEFINES_H_
