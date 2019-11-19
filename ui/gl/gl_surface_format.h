// Copyright 2017 The Chromium Authors. All rights reserved.
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

  // A given pair of surfaces is considered compatible if glSetSurface
  // can be used to switch between them without generating BAD_MATCH
  // errors, visual errors, or gross inefficiency, and incompatible
  // otherwise. For example, a pixel layout mismatch would be
  // considered incompatible. This comparison only makes sense within
  // the context of a single gl_surface subtype.
  bool IsCompatible(GLSurfaceFormat other_format) const;

  // Default pixel format is RGBA8888. Use this method to select
  // a preference of RGB565. TODO(klausw): use individual setter
  // methods if there's a use case for them.
  void SetRGB565();

  // Other properties for future use.
  void SetDepthBits(int depth_bits);
  int GetDepthBits() const { return depth_bits_; }

  void SetStencilBits(int stencil_bits);
  int GetStencilBits() const { return stencil_bits_; }

  void SetSamples(int samples);
  int GetSamples() const { return samples_; }

  enum SurfaceColorSpace {
    COLOR_SPACE_UNSPECIFIED = -1,
    COLOR_SPACE_SRGB,
    COLOR_SPACE_DISPLAY_P3,
  };
  void SetColorSpace(SurfaceColorSpace color_space) {
    color_space_ = color_space;
  }
  SurfaceColorSpace GetColorSpace() const { return color_space_; }

  // Compute number of bits needed for storing one pixel, not
  // including any padding. At this point mainly used to distinguish
  // RGB565 (16) from RGBA8888 (32).
  int GetBufferSize() const;

 private:
  SurfaceColorSpace color_space_ = COLOR_SPACE_UNSPECIFIED;
  int red_bits_ = -1;
  int green_bits_ = -1;
  int blue_bits_ = -1;
  int alpha_bits_ = -1;
  int depth_bits_ = -1;
  int samples_ = -1;
  int stencil_bits_ = -1;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_FORMAT_H_
