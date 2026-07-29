// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_surface_control.h"

#include <android/hardware_buffer.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace gl {

TEST(GLSurfaceEGLSurfaceControlTest, YuvEvenCoordinateAlignment_FractionalDPI) {
  // Simulates YouTube 1.5x DPI fractional drift (0.666px left shift on 2048x960
  // NV12 buffer)
  gfx::RectF scaled_rect(0.666435f, 0.0f, 1918.67f, 960.0f);
  gfx::Size buffer_size(2048, 960);

  gfx::Rect result = GLSurfaceEGLSurfaceControl::CalculateSourceCrop(
      scaled_rect, buffer_size, AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
      /*is_yuv_alignment_enabled=*/true);

  // Must align cleanly to even 2x2 YUV grid [0, 0, 1920, 960]
  EXPECT_EQ(gfx::Rect(0, 0, 1920, 960), result);
}

TEST(GLSurfaceEGLSurfaceControlTest, YuvV12Alignment) {
  // https://developer.android.com/reference/android/graphics/ImageFormat#YV12
  constexpr unsigned int AHARDWAREBUFFER_FORMAT_YV12 = 0x32315659;
  gfx::RectF scaled_rect(0.666435f, 0.0f, 1918.67f, 960.0f);
  gfx::Size buffer_size(2048, 960);

  gfx::Rect result = GLSurfaceEGLSurfaceControl::CalculateSourceCrop(
      scaled_rect, buffer_size, AHARDWAREBUFFER_FORMAT_YV12,
      /*is_yuv_alignment_enabled=*/true);

  EXPECT_EQ(gfx::Rect(0, 0, 1920, 960), result);
}

TEST(GLSurfaceEGLSurfaceControlTest, RgbUnconstrainedAlignment) {
  // Standard RGB web UI elements should preserve intentional odd coordinates
  gfx::RectF scaled_rect(1.0f, 1.0f, 101.0f, 55.0f);
  gfx::Size buffer_size(1024, 1024);

  gfx::Rect result = GLSurfaceEGLSurfaceControl::CalculateSourceCrop(
      scaled_rect, buffer_size, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
      /*is_yuv_alignment_enabled=*/true);

  EXPECT_EQ(gfx::Rect(1, 1, 101, 55), result);
}

TEST(GLSurfaceEGLSurfaceControlTest, YuvP010AndP210HdrAlignment) {
  gfx::RectF scaled_rect(1.0f, 0.0f, 3838.0f, 2160.0f);
  gfx::Size buffer_size(3840, 2160);

  gfx::Rect p010_result = GLSurfaceEGLSurfaceControl::CalculateSourceCrop(
      scaled_rect, buffer_size, AHARDWAREBUFFER_FORMAT_YCbCr_P010,
      /*is_yuv_alignment_enabled=*/true);
  EXPECT_EQ(gfx::Rect(0, 0, 3840, 2160), p010_result);

  gfx::Rect p210_result = GLSurfaceEGLSurfaceControl::CalculateSourceCrop(
      scaled_rect, buffer_size, AHARDWAREBUFFER_FORMAT_YCbCr_P210,
      /*is_yuv_alignment_enabled=*/true);
  EXPECT_EQ(gfx::Rect(0, 0, 3840, 2160), p210_result);
}

TEST(GLSurfaceEGLSurfaceControlTest, YuvEvenAlignmentFeatureDisabled) {
  // When feature flag is explicitly disabled, fractional coordinates remain
  // unaligned
  gfx::RectF scaled_rect(0.666435f, 0.0f, 1918.67f, 960.0f);
  gfx::Size buffer_size(2048, 960);

  gfx::Rect result = GLSurfaceEGLSurfaceControl::CalculateSourceCrop(
      scaled_rect, buffer_size, AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
      /*is_yuv_alignment_enabled=*/false);

  EXPECT_EQ(gfx::Rect(1, 0, 1918, 960), result);
}

}  // namespace gl
