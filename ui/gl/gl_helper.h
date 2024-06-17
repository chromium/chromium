// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_HELPER_H_
#define UI_GL_GL_HELPER_H_

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GL_EXPORT GLHelper {
 public:
  // Compiles a shader.
  // Does not check for errors, always returns shader.
  static GLuint CompileShader(GLenum type, const char* src);

  // Compiles a shader and checks for compilation errors.
  // Returns shader, 0 on failure.
  static GLuint LoadShader(GLenum type, const char* src);

  // Attaches 2 shaders and links them to a program.
  // Does not check for errors, always returns program.
  static GLuint LinkProgram(GLuint vertex_shader, GLuint fragment_shader);

  // Attaches 2 shaders, links them to a program, and checks for errors.
  // Returns program, 0 on failure.
  static GLuint SetupProgram(GLuint vertex_shader, GLuint fragment_shader);

  // Sets up a vertex buffer containing 4 vertices that can be used to draw
  // a quad as a tri-strip.
  static GLuint SetupQuadVertexBuffer();

  // Draws a quad to the currently bound frame buffer.
  static void DrawQuad(GLuint vertex_buffer);
};

}  // namespace gl

#endif  // UI_GL_GL_HELPER_H_
