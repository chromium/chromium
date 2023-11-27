// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_FORMAT_H_
#define UI_GL_GL_SURFACE_FORMAT_H_

#include "ui/gl/gl_export.h"

namespace gl {

// Expresses surface format properties that may vary depending
// on the underlying gl_surface implementation or specific usage
// scenarios. Intended usage is to support copying formats and
// checking compatibility.
class GL_EXPORT GLSurfaceFormat {
 public:
  // Default surface format for the underlying gl_surface subtype.
  // Use the setters below to change attributes if needed.
  GLSurfaceFormat();

  // Copy constructor from pre-existing format.
  GLSurfaceFormat(const GLSurfaceFormat& other);

  ~GLSurfaceFormat();

  // Default pixel format is RGBA8888. Use this method to select
  // a preference of RGB565.
  void SetRGB565();

  bool IsRGB565() const;

 private:
  bool is565_ = false;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_FORMAT_H_
