// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_format.h"
#include "base/notreached.h"

namespace gl {

GLSurfaceFormat::GLSurfaceFormat() {
}

GLSurfaceFormat::GLSurfaceFormat(const GLSurfaceFormat& other) = default;

GLSurfaceFormat::~GLSurfaceFormat() {
}

void GLSurfaceFormat::SetRGB565() {
  is565_ = true;
}

bool GLSurfaceFormat::IsRGB565() const {
  return is565_;
}

}  // namespace gl
