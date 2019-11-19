// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ui/gl/gl_surface_format.h"

namespace gl {

GLSurfaceFormat::GLSurfaceFormat() {
}

GLSurfaceFormat::GLSurfaceFormat(const GLSurfaceFormat& other) = default;

GLSurfaceFormat::~GLSurfaceFormat() {
}

void GLSurfaceFormat::SetRGB565() {
  red_bits_ = blue_bits_ = 5;
  green_bits_ = 6;
  alpha_bits_ = 0;
}

static int GetValue(int num, int default_value) {
  return num == -1 ? default_value : num;
}

static int GetBitSize(int num) {
  return GetValue(num, 8);
}

bool GLSurfaceFormat::IsCompatible(GLSurfaceFormat other) const {
  if (GetBitSize(red_bits_) == GetBitSize(other.red_bits_) &&
      GetBitSize(green_bits_) == GetBitSize(other.green_bits_) &&
      GetBitSize(blue_bits_) == GetBitSize(other.blue_bits_) &&
      GetBitSize(alpha_bits_) == GetBitSize(other.alpha_bits_) &&
      GetValue(stencil_bits_, 8) == GetValue(other.stencil_bits_, 8) &&
      GetValue(depth_bits_, 24) == GetValue(other.depth_bits_, 24) &&
      GetValue(samples_, 0) == GetValue(other.samples_, 0)) {
    return true;
  }
  return false;
}

void GLSurfaceFormat::SetDepthBits(int bits) {
  if (bits != -1) {
    depth_bits_ = bits;
  }
}

void GLSurfaceFormat::SetStencilBits(int bits) {
  if (bits != -1) {
    stencil_bits_ = bits;
  }
}

void GLSurfaceFormat::SetSamples(int num) {
  if (num != -1) {
    samples_ = num;
  }
}

int GLSurfaceFormat::GetBufferSize() const {
  int bits = GetBitSize(red_bits_) + GetBitSize(green_bits_) +
      GetBitSize(blue_bits_) + GetBitSize(alpha_bits_);
  if (bits <= 16) {
    return 16;
  } else if (bits <= 32) {
    return 32;
  }
  NOTREACHED();
  return 64;
}

}  // namespace gl
