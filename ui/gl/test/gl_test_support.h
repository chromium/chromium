// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TEST_GL_TEST_SUPPORT_H_
#define UI_GL_TEST_GL_TEST_SUPPORT_H_

#include <stdint.h>

#include <optional>

#include "base/containers/span.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_implementation.h"

namespace gl {
class GLDisplay;

class GLTestSupport {
 public:
  // Initialize GL for image testing. |prefered_impl| is the GL implementation
  // to select if it is an allowed GL implementation. Otherwise it selects the
  // first allowed GL implementation.
  static GLDisplay* InitializeGL(
      std::optional<GLImplementationParts> prefered_impl);

  // Cleanup GL after being initialized for image testing.
  static void CleanupGL(GLDisplay* display);

  // Initialize buffer of a specific |format| to |color|.
  static void SetBufferDataToColor(int width,
                                   int height,
                                   int stride,
                                   int plane,
                                   gfx::BufferFormat format,
                                   base::span<const uint8_t, 4> color,
                                   uint8_t* data);
};

}  // namespace gl

#endif  // UI_GL_TEST_GL_TEST_SUPPORT_H_
