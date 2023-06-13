// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GLImage should pass in order
// to be conformant.

#include "ui/ozone/gl/gl_image_test_template.h"
#include "base/strings/stringprintf.h"

namespace gl {

namespace {

GLuint LoadVertexShader() {
  bool is_desktop_core_profile =
      GLContext::GetCurrent()->GetVersionInfo()->is_desktop_core_profile;
  std::string vertex_shader = base::StringPrintf(
      "%s"  // version
      "%s vec2 a_position;\n"
      "%s vec2 v_texCoord;\n"
      "void main() {\n"
      "  gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);\n"
      "  v_texCoord = (a_position + vec2(1.0, 1.0)) * 0.5;\n"
      "}",
      is_desktop_core_profile ? "#version 150\n" : "",
      is_desktop_core_profile ? "in" : "attribute",
      is_desktop_core_profile ? "out" : "varying");
  return GLHelper::LoadShader(GL_VERTEX_SHADER, vertex_shader.c_str());
}

// Compiles a fragment shader for sampling out of a texture of |size| bound to
// |target| and checks for compilation errors.
GLuint LoadFragmentShader(unsigned target, const gfx::Size& size) {
  bool is_desktop_core_profile =
      GLContext::GetCurrent()->GetVersionInfo()->is_desktop_core_profile;
  bool is_gles = GLContext::GetCurrent()->GetVersionInfo()->is_es;

  std::string fragment_shader_main = base::StringPrintf(
      "uniform SamplerType a_texture;\n"
      "%s vec2 v_texCoord;\n"
      "%s"  // output variable declaration
      "void main() {\n"
      "  %s = TextureLookup(a_texture, v_texCoord * TextureScale);\n"
      "}",
      is_desktop_core_profile ? "in" : "varying",
      is_desktop_core_profile ? "out vec4 my_FragData;\n" : "",
      is_desktop_core_profile ? "my_FragData" : "gl_FragData[0]");

  switch (target) {
    case GL_TEXTURE_2D:
      return GLHelper::LoadShader(
          GL_FRAGMENT_SHADER,
          base::StringPrintf("%s"  // version
                             "%s"  // precision
                             "#define SamplerType sampler2D\n"
                             "#define TextureLookup %s\n"
                             "#define TextureScale vec2(1.0, 1.0)\n"
                             "%s",  // main function
                             is_desktop_core_profile ? "#version 150\n" : "",
                             is_gles ? "precision mediump float;\n" : "",
                             is_desktop_core_profile ? "texture" : "texture2D",
                             fragment_shader_main.c_str())
              .c_str());
    case GL_TEXTURE_RECTANGLE_ARB:
      DCHECK(!is_gles);
      return GLHelper::LoadShader(
          GL_FRAGMENT_SHADER,
          base::StringPrintf(
              "%s"  // version
              "%s"  // extension
              "#define SamplerType sampler2DRect\n"
              "#define TextureLookup %s\n"
              "#define TextureScale vec2(%f, %f)\n"
              "%s",  // main function
              is_desktop_core_profile ? "#version 150\n" : "",
              is_desktop_core_profile
                  ? ""
                  : "#extension GL_ARB_texture_rectangle : require\n",
              is_desktop_core_profile ? "texture" : "texture2DRect",
              static_cast<double>(size.width()),
              static_cast<double>(size.height()), fragment_shader_main.c_str())
              .c_str());
    case GL_TEXTURE_EXTERNAL_OES:
      DCHECK(is_gles);
      return GLHelper::LoadShader(
          GL_FRAGMENT_SHADER,
          base::StringPrintf("#extension GL_OES_EGL_image_external : require\n"
                             "%s"  // precision
                             "#define SamplerType samplerExternalOES\n"
                             "#define TextureLookup texture2D\n"
                             "#define TextureScale vec2(1.0, 1.0)\n"
                             "%s",  // main function
                             is_gles ? "precision mediump float;\n" : "",
                             fragment_shader_main.c_str())
              .c_str());
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

namespace internal {

// Draws texture bound to |target| of texture unit 0 to the currently bound
// frame buffer.
void DrawTextureQuad(GLenum target, const gfx::Size& size) {
  GLuint vao = 0;
  if (GLHelper::ShouldTestsUseVAOs()) {
    glGenVertexArraysOES(1, &vao);
    glBindVertexArrayOES(vao);
  }

  GLuint vertex_shader = LoadVertexShader();
  GLuint fragment_shader = LoadFragmentShader(target, size);
  GLuint program = GLHelper::SetupProgram(vertex_shader, fragment_shader);
  EXPECT_NE(program, 0u);
  glUseProgram(program);

  GLint sampler_location = glGetUniformLocation(program, "a_texture");
  ASSERT_NE(sampler_location, -1);
  glUniform1i(sampler_location, 0);

  GLuint vertex_buffer = GLHelper::SetupQuadVertexBuffer();
  GLHelper::DrawQuad(vertex_buffer);

  if (vao != 0) {
    glDeleteVertexArraysOES(1, &vao);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteProgram(program);
  glDeleteBuffersARB(1, &vertex_buffer);
}

}  // namespace internal

absl::optional<GLImplementationParts>
GLImageTestDelegateBase::GetPreferedGLImplementation() const {
  return absl::nullopt;
}

bool GLImageTestDelegateBase::SkipTest(GLDisplay*) const {
  return false;
}

// These suites are instantiated in binaries that use //ui/gl:test_support.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLImageTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GLImageOddSizeTest);

}  // namespace gl
