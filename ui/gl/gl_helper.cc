// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_helper.h"

#include <string>

#include "base/check_op.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"

namespace gl {

// static
GLuint GLHelper::CompileShader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  // Load the shader source.
  glShaderSource(shader, 1, &src, nullptr);
  // Compile the shader.
  glCompileShader(shader);
  return shader;
}

// static
GLuint GLHelper::LoadShader(GLenum type, const char* src) {
  GLuint shader = CompileShader(type, src);

  // Check the compile status.
  GLint value = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (!value) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    DCHECK_EQ(1, value) << "Error compiling shader: " << log;
    glDeleteShader(shader);
    shader = 0;
  }
  return shader;
}

// static
GLuint GLHelper::LinkProgram(GLuint vertex_shader, GLuint fragment_shader) {
  // Create the program object.
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Link the program.
  glLinkProgram(program);
  return program;
}

// static
GLuint GLHelper::SetupProgram(GLuint vertex_shader, GLuint fragment_shader) {
  GLuint program = LinkProgram(vertex_shader, fragment_shader);
  // Check the link status.
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (!linked) {
    char buffer[1024];
    GLsizei length = 0;
    glGetProgramInfoLog(program, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    DCHECK_EQ(1, linked) << "Error linking program: " << log;
    glDeleteProgram(program);
    program = 0;
  }
  return program;
}

// static
GLuint GLHelper::SetupQuadVertexBuffer() {
  GLuint vertex_buffer = 0;
  glGenBuffersARB(1, &vertex_buffer);
  ScopedBufferBinder buffer_binder(GL_ARRAY_BUFFER, vertex_buffer);
  GLfloat data[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f};
  glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
  return vertex_buffer;
}

// static
void GLHelper::DrawQuad(GLuint vertex_buffer) {
  ScopedBufferBinder buffer_binder(GL_ARRAY_BUFFER, vertex_buffer);
  ScopedVertexAttribArray vertex_attrib_array(0, 2, GL_FLOAT, GL_FALSE,
                                              sizeof(GLfloat) * 2, 0);
  ScopedCapability disable_blending(GL_BLEND, GL_FALSE);
  ScopedCapability disable_culling(GL_CULL_FACE, GL_FALSE);
  ScopedCapability disable_dithering(GL_DITHER, GL_FALSE);
  ScopedCapability disable_depth_test(GL_DEPTH_TEST, GL_FALSE);
  ScopedCapability disable_scissor_test(GL_SCISSOR_TEST, GL_FALSE);
  ScopedColorMask color_mask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

}  // namespace gl
