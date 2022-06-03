// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SURFACE_ORIGIN_H_
#define UI_GFX_SURFACE_ORIGIN_H_

namespace gfx {

// Describes where (0,0) coordinate is in a surface. Conventionally this value
// is kBottomLeft for OpenGL.
enum class SurfaceOrigin {
  kTopLeft,
  kBottomLeft,
};

}  // namespace gfx

#endif  // UI_GFX_SURFACE_ORIGIN_H_
