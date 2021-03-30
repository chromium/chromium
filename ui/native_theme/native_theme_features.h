// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by native theme

#ifndef UI_NATIVE_THEME_NATIVE_THEME_FEATURES_H_
#define UI_NATIVE_THEME_NATIVE_THEME_FEATURES_H_

#include "base/feature_list.h"
#include "ui/native_theme/native_theme_export.h"

namespace features {

NATIVE_THEME_EXPORT extern const base::Feature kOverlayScrollbar;

}  // namespace features

namespace ui {

NATIVE_THEME_EXPORT bool IsOverlayScrollbarEnabled();

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_FEATURES_H_
