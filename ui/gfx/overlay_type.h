// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_TYPE_H_
#define UI_GFX_OVERLAY_TYPE_H_

#include <stdint.h>

namespace gfx {

// Identifies the type of the overlay based on the strategy used to promote
// this.
enum OverlayType : uint8_t {
  // Simple overlay. No strategy was used to promote this.
  kSimple = 0,
  // Promoted overlays with specific strategies applied.
  kUnderlay,
  kSingleOnTop,
  kFullScreen,
};

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_TYPE_H_
