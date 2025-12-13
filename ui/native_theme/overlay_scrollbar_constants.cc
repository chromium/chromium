// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/overlay_scrollbar_constants.h"

#include "base/time/time.h"
#include "ui/native_theme/features/native_theme_features.h"

namespace ui {

base::TimeDelta GetOverlayScrollbarFadeDelay() {
  return IsFluentOverlayScrollbarEnabled() ? base::Milliseconds(750)
                                           : base::Milliseconds(500);
}

base::TimeDelta GetOverlayScrollbarFadeDuration() {
  return IsFluentOverlayScrollbarEnabled() ? base::Milliseconds(100)
                                           : base::Milliseconds(200);
}

}  // namespace ui
