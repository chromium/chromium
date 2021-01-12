// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_YUV420_RGB_CONVERTER_H_
#define UI_GL_YUV420_RGB_CONVERTER_H_

#include "ui/gfx/geometry/size.h"

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace gl {

struct GLVersionInfo;

class YUVToRGBConverter {
 public:
  explicit YUVToRGBConverter(const GLVersionInfo& gl_version_info,
                             const gfx::ColorSpace& color_space);
  ~YUVToRGBConverter();

  // The input Y and UV textures should be bound to these texture objects
  // prior to calling CopyYUV420ToRGB.
  unsigned y_texture() const { return y_texture_; }
  unsigned uv_texture() const { return uv_texture_; }

  void CopyYUV420ToRGB(unsigned target,
                       const gfx::Size& size,
                       unsigned rgb_texture,
                       unsigned rgb_texture_type);

 private:
  unsigned framebuffer_ = 0;
  unsigned vertex_shader_ = 0;
  unsigned fragment_shader_ = 0;
  unsigned program_ = 0;
  int size_location_ = -1;
  unsigned vertex_buffer_ = 0;
  unsigned y_texture_ = 0;
  unsigned uv_texture_ = 0;
  unsigned vertex_array_object_ = 0;
  unsigned source_texture_target_ = 0;
  bool has_get_tex_level_parameter_ = false;
  bool has_robust_resource_init_ = false;
  bool has_sampler_objects_ = false;
};

}  // namespace gl

#endif  // UI_GL_YUV420_RGB_CONVERTER_H_
