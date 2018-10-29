// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SWITCHES_H_
#define UI_GFX_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/gfx/switches_export.h"

namespace switches {

#if defined(OS_WIN)
GFX_SWITCHES_EXPORT extern const char kDisableDirectWriteForUI[];
#endif

GFX_SWITCHES_EXPORT extern const char kDisableFontSubpixelPositioning[];

GFX_SWITCHES_EXPORT extern const char kHeadless[];

}  // namespace switches

namespace features {

GFX_SWITCHES_EXPORT extern const base::Feature kLeftToRightUrls;

GFX_SWITCHES_EXPORT extern const base::Feature kUsePaintRecordForImageSkia;

}  // namespace features

#endif  // UI_GFX_SWITCHES_H_
