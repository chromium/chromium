// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_SURFACE_CONTROL_FRAME_RATE_H_
#define UI_GFX_ANDROID_SURFACE_CONTROL_FRAME_RATE_H_

namespace gfx {

enum class SurfaceControlFrameRateCompatibility {
  kFixedSource,
  kAtLeast,
};

struct SurfaceControlFrameRate {
  float frame_rate = 0.0f;
  SurfaceControlFrameRateCompatibility compatibility =
      SurfaceControlFrameRateCompatibility::kFixedSource;

  bool operator==(const SurfaceControlFrameRate& b) const = default;
};

}  // namespace gfx

#endif  // UI_GFX_ANDROID_SURFACE_CONTROL_FRAME_RATE_H_
