// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/yuv_to_rgb_converter.h"

#include "base/notreached.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/color_transform.h"
#include "ui/gl/gl_helper.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"

namespace gl {
namespace {

const char kVertexHeaderES2[] =
    "precision mediump float;\n"
    "#define ATTRIBUTE attribute\n"
    "#define VARYING varying\n";

const char kVertexHeaderES3[] =
    "#version 300 es\n"
    "precision mediump float;\n"
    "#define ATTRIBUTE in\n"
    "#define VARYING out\n";

const char kVertexHeaderCompatiblityProfile[] =
    "#version 110\n"
    "#define ATTRIBUTE attribute\n"
    "#define VARYING varying\n";

const char kVertexHeaderCoreProfile[] =
    "#version 150\n"
    "#define ATTRIBUTE in\n"
    "#define VARYING out\n";

const char kFragmentHeaderES22D[] =
    "precision mediump float;\n"
    "#define VARYING varying\n"
    "#define FRAGCOLOR gl_FragColor\n"
    "#define TEX texture2D\n";

const char kFragmentHeaderES2Rect[] =
    "#extension GL_ARB_texture_rectangle : require\n"
    "precision mediump float;\n"
    "#define VARYING varying\n"
    "#define FRAGCOLOR gl_FragColor\n"
    "#define TEX texture2DRect\n";

const char kFragmentHeaderES3[] =
    "#version 300 es\n"
    "precision mediump float;\n"
    "#define VARYING in\n"
    "#define TEX texture\n"
    "#define FRAGCOLOR frag_color\n"
    "out vec4 FRAGCOLOR;\n";

const char kFragmentHeaderCompatiblityProfile2D[] =
    "#version 110\n"
    "#define VARYING varying\n"
    "#define FRAGCOLOR gl_FragColor\n"
    "#define TEX texture2D\n";

const char kFragmentHeaderCompatiblityProfileRect[] =
    "#version 110\n"
    "#extension GL_ARB_texture_rectangle : require\n"
    "#define VARYING varying\n"
    "#define FRAGCOLOR gl_FragColor\n"
    "#define TEX texture2DRect\n";

const char kFragmentHeaderCoreProfile[] =
    "#version 150\n"
    "#define VARYING in\n"
    "#define TEX texture\n"
    "#define FRAGCOLOR frag_color\n"
    "out vec4 FRAGCOLOR;\n";

// clang-format off
const char kVertexShader[] =
STRINGIZE(
  ATTRIBUTE vec2 a_position;
  uniform vec2 a_texScale;
  VARYING vec2 v_texCoord;
  void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = (a_position + vec2(1.0, 1.0)) * 0.5 * a_texScale;
  }
);

const char kFragmentShader2D[] =
STRINGIZE(
  uniform sampler2D a_y_texture;
  uniform sampler2D a_uv_texture;
  VARYING vec2 v_texCoord;
  void main() {
    vec3 yuv = vec3(
        TEX(a_y_texture, v_texCoord).r,
        TEX(a_uv_texture, v_texCoord).rg);
    FRAGCOLOR = vec4(DoColorConversion(yuv), 1.0);
  }
);

const char kFragmentShaderRect[] =
STRINGIZE(
  uniform sampler2DRect a_y_texture;
  uniform sampler2DRect a_uv_texture;
  VARYING vec2 v_texCoord;
  void main() {
    vec3 yuv = vec3(
        TEX(a_y_texture, v_texCoord).r,
        TEX(a_uv_texture, v_texCoord * 0.5).rg);
    FRAGCOLOR = vec4(DoColorConversion(yuv), 1.0);
  }
);

// clang-format on

}  // namespace

YUVToRGBConverter::YUVToRGBConverter(const GLVersionInfo& gl_version_info,
                                     const gfx::ColorSpace& color_space) {
  std::unique_ptr<gfx::ColorTransform> color_transform =
      gfx::ColorTransform::NewColorTransform(
          color_space, color_space.GetAsFullRangeRGB(),
          gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);
  std::string do_color_conversion = color_transform->GetShaderSource();

  // On MacOS, the default texture target for native GpuMemoryBuffers is
  // GL_TEXTURE_RECTANGLE_ARB. This is due to CGL's requirements for creating
  // a GL surface. However, when ANGLE is used on top of SwiftShader or Metal,
  // it's necessary to use GL_TEXTURE_2D instead.
  // TODO(crbug.com/1056312): The proper behavior is to check the config
  // parameter set by the EGL_ANGLE_iosurface_client_buffer extension
  bool is_rect =
      !gl_version_info.is_angle_swiftshader && !gl_version_info.is_angle_metal;
  source_texture_target_ = (is_rect ? GL_TEXTURE_RECTANGLE_ARB : GL_TEXTURE_2D);

  const char* fragment_header = nullptr;
  const char* vertex_header = nullptr;
  if (gl_version_info.is_es2) {
    vertex_header = kVertexHeaderES2;
    fragment_header = (is_rect ? kFragmentHeaderES2Rect : kFragmentHeaderES22D);
  } else if (gl_version_info.is_es3) {
    vertex_header = kVertexHeaderES3;
    fragment_header = kFragmentHeaderES3;
  } else if (gl_version_info.is_desktop_core_profile) {
    vertex_header = kVertexHeaderCoreProfile;
    fragment_header = kFragmentHeaderCoreProfile;
  } else {
    DCHECK(!gl_version_info.is_es);
    vertex_header = kVertexHeaderCompatiblityProfile;
    fragment_header = (is_rect ? kFragmentHeaderCompatiblityProfileRect
                               : kFragmentHeaderCompatiblityProfile2D);
  }
  DCHECK(vertex_header && fragment_header);

  glGenFramebuffersEXT(1, &framebuffer_);

  vertex_buffer_ = GLHelper::SetupQuadVertexBuffer();
  vertex_shader_ = GLHelper::LoadShader(
      GL_VERTEX_SHADER,
      base::StringPrintf("%s\n%s", vertex_header, kVertexShader).c_str());
  fragment_shader_ = GLHelper::LoadShader(
      GL_FRAGMENT_SHADER,
      base::StringPrintf("%s\n%s\n%s", fragment_header,
                         do_color_conversion.c_str(),
                         (is_rect ? kFragmentShaderRect : kFragmentShader2D))
          .c_str());
  program_ = GLHelper::SetupProgram(vertex_shader_, fragment_shader_);

  ScopedUseProgram use_program(program_);
  size_location_ = glGetUniformLocation(program_, "a_texScale");
  DCHECK_NE(-1, size_location_);
  int y_sampler_location = glGetUniformLocation(program_, "a_y_texture");
  DCHECK_NE(-1, y_sampler_location);
  int uv_sampler_location = glGetUniformLocation(program_, "a_uv_texture");
  DCHECK_NE(-1, uv_sampler_location);

  glGenTextures(1, &y_texture_);
  glGenTextures(1, &uv_texture_);

  glUniform1i(y_sampler_location, 0);
  glUniform1i(uv_sampler_location, 1);

  bool has_vertex_array_objects =
      gl_version_info.is_es3 || gl_version_info.is_desktop_core_profile;
  if (has_vertex_array_objects) {
    glGenVertexArraysOES(1, &vertex_array_object_);
  }

  has_get_tex_level_parameter_ =
      !gl_version_info.is_es || gl_version_info.IsAtLeastGLES(3, 1) ||
      g_current_gl_driver->ext.b_GL_ANGLE_get_tex_level_parameter;
  has_robust_resource_init_ =
      g_current_gl_driver->ext.b_GL_ANGLE_robust_resource_initialization;

  has_sampler_objects_ = gl_version_info.IsAtLeastGLES(3, 0) ||
                         gl_version_info.IsAtLeastGL(3, 3) ||
                         g_current_gl_driver->ext.b_GL_ARB_sampler_objects;
}

YUVToRGBConverter::~YUVToRGBConverter() {
  glDeleteTextures(1, &y_texture_);
  glDeleteTextures(1, &uv_texture_);
  glDeleteProgram(program_);
  glDeleteShader(vertex_shader_);
  glDeleteShader(fragment_shader_);
  glDeleteBuffersARB(1, &vertex_buffer_);
  glDeleteFramebuffersEXT(1, &framebuffer_);
  if (vertex_array_object_) {
    glDeleteVertexArraysOES(1, &vertex_array_object_);
  }
}

void YUVToRGBConverter::CopyYUV420ToRGB(unsigned target,
                                        const gfx::Size& size,
                                        unsigned rgb_texture,
                                        unsigned rgb_texture_type) {
  GLenum source_target_getter = 0;
  switch (source_texture_target_) {
    case GL_TEXTURE_2D:
      source_target_getter = GL_TEXTURE_BINDING_2D;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      source_target_getter = GL_TEXTURE_BINDING_RECTANGLE_ARB;
      break;
    default:
      NOTIMPLEMENTED() << " Target not supported.";
      return;
  }
  // Note that state restoration is done explicitly instead of scoped binders to
  // avoid https://crbug.com/601729.
  GLint old_active_texture = -1;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active_texture);
  GLint old_texture0_binding = -1;
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(source_target_getter, &old_texture0_binding);
  GLint old_sampler0_binding = -1;
  if (has_sampler_objects_) {
    glGetIntegerv(GL_SAMPLER_BINDING, &old_sampler0_binding);
    glBindSampler(0, 0);
  }
  GLint old_texture1_binding = -1;
  glActiveTexture(GL_TEXTURE1);
  glGetIntegerv(source_target_getter, &old_texture1_binding);
  GLint old_sampler1_binding = -1;
  if (has_sampler_objects_) {
    glGetIntegerv(GL_SAMPLER_BINDING, &old_sampler1_binding);
    glBindSampler(1, 0);
  }

  // Allocate the rgb texture.
  glActiveTexture(old_active_texture);
  glBindTexture(target, rgb_texture);

  bool needs_texture_init = true;
  if (has_get_tex_level_parameter_) {
    GLint current_internal_format = 0;
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT,
                             &current_internal_format);

    GLint current_type = 0;
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_RED_TYPE, &current_type);

    GLint current_width = 0;
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &current_width);

    GLint current_height = 0;
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &current_height);

    if (current_internal_format == GL_RGB &&
        static_cast<unsigned>(current_type) == rgb_texture_type &&
        current_width == size.width() && current_height == size.height()) {
      needs_texture_init = false;
    }
  }
  if (needs_texture_init) {
    glTexImage2D(target, 0, GL_RGB, size.width(), size.height(), 0, GL_RGB,
                 rgb_texture_type, nullptr);
    if (has_robust_resource_init_) {
      // We're about to overwrite the whole texture with a draw, notify the
      // driver that it doesn't need to perform robust resource init.
      glTexParameteri(target, GL_RESOURCE_INITIALIZED_ANGLE, GL_TRUE);
    }
  }

  // Set up and issue the draw call.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(source_texture_target_, y_texture_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(source_texture_target_, uv_texture_);
  ScopedFramebufferBinder framebuffer_binder(framebuffer_);
  ScopedViewport viewport(0, 0, size.width(), size.height());
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            target, rgb_texture, 0);
  DCHECK_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatusEXT(GL_FRAMEBUFFER));
  ScopedUseProgram use_program(program_);
  if (source_texture_target_ == GL_TEXTURE_RECTANGLE_ARB) {
    glUniform2f(size_location_, size.width(), size.height());
  } else {
    glUniform2f(size_location_, 1, 1);
  }
  // User code may have set up the other vertex attributes in the
  // context in unexpected ways, including setting vertex attribute
  // divisors which may otherwise cause GL_INVALID_OPERATION during
  // glDrawArrays. Avoid interference by binding our own VAO during
  // the draw call. crbug.com/930479
  GLint old_vertex_array_object_ = 0;
  if (vertex_array_object_) {
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vertex_array_object_);
    glBindVertexArrayOES(vertex_array_object_);
  }
  GLHelper::DrawQuad(vertex_buffer_);
  if (vertex_array_object_) {
    glBindVertexArrayOES(old_vertex_array_object_);
  }

  // Restore previous state.
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            target, 0, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(source_texture_target_, old_texture0_binding);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(source_texture_target_, old_texture1_binding);
  glActiveTexture(old_active_texture);
  if (old_sampler0_binding > 0)
    glBindSampler(0, old_sampler0_binding);
  if (old_sampler1_binding > 0)
    glBindSampler(1, old_sampler1_binding);
}

}  // namespace gl
