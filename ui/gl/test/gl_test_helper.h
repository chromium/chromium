// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TEST_GL_TEST_HELPER_H_
#define UI_GL_TEST_GL_TEST_HELPER_H_

#include <stdint.h>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

class GLTestHelper {
 public:
  // Creates a texture object.
  // Does not check for errors, always returns texture.
  static GLuint CreateTexture(GLenum target);

  // Creates a framebuffer, attaches a color buffer, and checks for errors.
  // Returns framebuffer, 0 on failure.
  static GLuint SetupFramebuffer(int width, int height);

  // Checks an area of pixels for a color.
  static bool CheckPixels(GLint x,
                          GLint y,
                          GLsizei width,
                          GLsizei height,
                          const uint8_t expected_color[4]);

  // Checks an area of pixels for a color, given an admissible per component
  // error.
  static bool CheckPixelsWithError(GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height,
                                   int error,
                                   const uint8_t expected_color[4]);

#if BUILDFLAG(IS_WIN)
  // Read back the content of |window| inside a rectangle at the origin with
  // size |size|.
  static std::vector<SkColor> ReadBackWindow(HWND window,
                                             const gfx::Size& size);

  // Read back the content of |window| of the pixel at point |point|.
  static SkColor ReadBackWindowPixel(HWND window, const gfx::Point& point);
#endif
};

}  // namespace gl

#endif  // UI_GL_TEST_GL_TEST_HELPER_H_
