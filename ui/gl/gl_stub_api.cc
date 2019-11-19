// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_stub_api.h"

namespace gl {

GLStubApi::GLStubApi() {}

GLStubApi::~GLStubApi() = default;

GLenum GLStubApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  return GL_FRAMEBUFFER_COMPLETE;
}

GLuint GLStubApi::glCreateProgramFn(void) {
  return 1;
}

GLuint GLStubApi::glCreateShaderFn(GLenum type) {
  return 2;
}

GLsync GLStubApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  return reinterpret_cast<GLsync>(1);
}

void GLStubApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  GenHelper(n, buffers);
}

void GLStubApi::glGenerateMipmapEXTFn(GLenum target) {}

void GLStubApi::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  GenHelper(n, fences);
}

void GLStubApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  GenHelper(n, fences);
}

void GLStubApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  GenHelper(n, framebuffers);
}

GLuint GLStubApi::glGenPathsNVFn(GLsizei range) {
  return 1;
}

void GLStubApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  GenHelper(n, ids);
}

void GLStubApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  GenHelper(n, renderbuffers);
}

void GLStubApi::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  GenHelper(n, samplers);
}

void GLStubApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  GenHelper(n, textures);
}

void GLStubApi::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  GenHelper(n, ids);
}

void GLStubApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  GenHelper(n, arrays);
}

void GLStubApi::glGetIntegervFn(GLenum pname, GLint* params) {
  // We need some values to be large enough to initialize ContextGroup.
  switch (pname) {
    default:
      *params = 1;
      break;
    case GL_MAX_RENDERBUFFER_SIZE:
      *params = 512;
      break;
    case GL_MAX_SAMPLES:
      *params = 4;
      break;
    case GL_MAX_COLOR_ATTACHMENTS_EXT:
      *params = 4;
      break;
    case GL_MAX_DRAW_BUFFERS_ARB:
      *params = 4;
      break;
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
      *params = 4;
      break;
    case GL_MAX_UNIFORM_BUFFER_BINDINGS:
      *params = 24;
      break;
    case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
      *params = 256;
      break;
    case GL_MAX_VERTEX_ATTRIBS:
      *params = 8;
      break;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      *params = 8;
      break;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      *params = 8;
      break;
    case GL_MAX_TEXTURE_SIZE:
    case GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB:
      *params = 2048;
      break;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      *params = 2048;
      break;
    case GL_MAX_3D_TEXTURE_SIZE:
      *params = 256;
      break;
    case GL_MAX_ARRAY_TEXTURE_LAYERS:
      *params = 256;
      break;
    case GL_MAX_VARYING_VECTORS:
      *params = 8;
      break;
    case GL_MAX_VARYING_FLOATS:
      *params = 32;
      break;
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      *params = 128;
      break;
    case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
      *params = 512;
      break;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      *params = 16;
      break;
    case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
      *params = 64;
      break;
    case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
      *params = 64;
      break;
    case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
      *params = 60;
      break;
    case GL_MAX_PROGRAM_TEXEL_OFFSET:
      *params = 7;
      break;
    case GL_MIN_PROGRAM_TEXEL_OFFSET:
      *params = -8;
      break;
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      *params = 32;
      break;
    case GL_MAX_VIEWPORT_DIMS:
      *params = 1024 << 8;
      break;
    case GL_ALPHA_BITS:
      *params = 8;
      break;
    case GL_DEPTH_BITS:
      *params = 24;
      break;
    case GL_STENCIL_BITS:
      *params = 8;
      break;
    case GL_TEXTURE_BINDING_2D:
      *params = 1;
      break;
    case GL_FRAMEBUFFER_BINDING:
      *params = 1;
      break;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      *params = GL_UNSIGNED_BYTE;
      break;
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      *params = GL_RGBA;
      break;
  }
}

void GLStubApi::glGetProgramivFn(GLuint program, GLenum pname, GLint* params) {
  switch (pname) {
    case GL_LINK_STATUS:
      *params = GL_TRUE;
      break;
    case GL_VALIDATE_STATUS:
      *params = GL_TRUE;
      break;
    default:
      break;
  }
}

void GLStubApi::glGetQueryObjecti64vFn(GLuint id,
                                       GLenum pname,
                                       GLint64* params) {
  switch (pname) {
    case GL_QUERY_RESULT_AVAILABLE:
      *params = GL_TRUE;
      break;
    default:
      break;
  }
}

void GLStubApi::glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) {
  switch (pname) {
    case GL_QUERY_RESULT_AVAILABLE:
      *params = GL_TRUE;
      break;
    default:
      break;
  }
}

void GLStubApi::glGetQueryObjectui64vFn(GLuint id,
                                        GLenum pname,
                                        GLuint64* params) {
  switch (pname) {
    case GL_QUERY_RESULT_AVAILABLE:
      *params = GL_TRUE;
      break;
    default:
      break;
  }
}

void GLStubApi::glGetQueryObjectuivFn(GLuint id, GLenum pname, GLuint* params) {
  switch (pname) {
    case GL_QUERY_RESULT_AVAILABLE:
      *params = GL_TRUE;
      break;
    default:
      break;
  }
}

void GLStubApi::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  switch (pname) {
    case GL_COMPILE_STATUS:
      *params = GL_TRUE;
      break;
    default:
      break;
  }
}

const GLubyte* GLStubApi::glGetStringFn(GLenum name) {
  switch (name) {
    case GL_RENDERER:
    default:
      return reinterpret_cast<const GLubyte*>("");
    case GL_VERSION:
      return reinterpret_cast<const GLubyte*>(version_.c_str());
    case GL_EXTENSIONS:
      return reinterpret_cast<const GLubyte*>(extensions_.c_str());
  }
}

const GLubyte* GLStubApi::glGetStringiFn(GLenum name, GLuint index) {
  return reinterpret_cast<const GLubyte*>("");
}

GLboolean GLStubApi::glIsBufferFn(GLuint buffer) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsEnabledFn(GLenum cap) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsFenceAPPLEFn(GLuint fence) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsFenceNVFn(GLuint fence) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsPathNVFn(GLuint path) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsProgramFn(GLuint program) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsQueryFn(GLuint query) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsSamplerFn(GLuint sampler) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsShaderFn(GLuint shader) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsSyncFn(GLsync sync) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsTextureFn(GLuint texture) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsTransformFeedbackFn(GLuint id) {
  return GL_TRUE;
}

GLboolean GLStubApi::glIsVertexArrayOESFn(GLuint array) {
  return GL_TRUE;
}

GLboolean GLStubApi::glTestFenceAPPLEFn(GLuint fence) {
  return GL_TRUE;
}

GLboolean GLStubApi::glTestFenceNVFn(GLuint fence) {
  return GL_TRUE;
}

GLboolean GLStubApi::glUnmapBufferFn(GLenum target) {
  return GL_TRUE;
}

}  // namespace gl
