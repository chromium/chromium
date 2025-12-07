// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_STUB_API_H_
#define UI_GL_GL_STUB_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_stub_api_base.h"

namespace gl {

class GL_EXPORT GLStubApi: public GLStubApiBase {
 public:
  GLStubApi();

  GLStubApi(const GLStubApi&) = delete;
  GLStubApi& operator=(const GLStubApi&) = delete;

  ~GLStubApi() override;

  void set_version(std::string version) { version_ = std::move(version); }

  void set_extensions(std::string extensions) {
    extensions_ = std::move(extensions);
  }

  GLenum glCheckFramebufferStatusEXTFn(GLenum target) override;
  GLuint glCreateProgramFn(void) override;
  GLuint glCreateShaderFn(GLenum type) override;
  GLsync glFenceSyncFn(GLenum condition, GLbitfield flags) override;
  void glGenBuffersARBFn(GLsizei n, GLuint* buffers) override;
  void glGenerateMipmapEXTFn(GLenum target) override;
  void glGenFencesNVFn(GLsizei n, GLuint* fences) override;
  void glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) override;
  void glGenQueriesFn(GLsizei n, GLuint* ids) override;
  void glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) override;
  void glGenSamplersFn(GLsizei n, GLuint* samplers) override;
  void glGenTexturesFn(GLsizei n, GLuint* textures) override;
  void glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) override;
  void glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) override;
  void glGetIntegervFn(GLenum pname, GLint* params) override;
  void glGetProgramivFn(GLuint program, GLenum pname, GLint* params) override;
  void glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) override;
  void glGetQueryObjecti64vFn(GLuint id,
                              GLenum pname,
                              GLint64* params) override;
  void glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) override;
  void glGetQueryObjectui64vFn(GLuint id,
                               GLenum pname,
                               GLuint64* params) override;
  void glGetQueryObjectuivFn(GLuint id, GLenum pname, GLuint* params) override;
  const GLubyte* glGetStringFn(GLenum name) override;
  const GLubyte* glGetStringiFn(GLenum name, GLuint index) override;
  GLboolean glIsBufferFn(GLuint buffer) override;
  GLboolean glIsEnabledFn(GLenum cap) override;
  GLboolean glIsFenceNVFn(GLuint fence) override;
  GLboolean glIsFramebufferEXTFn(GLuint framebuffer) override;
  GLboolean glIsProgramFn(GLuint program) override;
  GLboolean glIsQueryFn(GLuint query) override;
  GLboolean glIsRenderbufferEXTFn(GLuint renderbuffer) override;
  GLboolean glIsSamplerFn(GLuint sampler) override;
  GLboolean glIsShaderFn(GLuint shader) override;
  GLboolean glIsSyncFn(GLsync sync) override;
  GLboolean glIsTextureFn(GLuint texture) override;
  GLboolean glIsTransformFeedbackFn(GLuint id) override;
  GLboolean glIsVertexArrayOESFn(GLuint array) override;
  GLboolean glTestFenceNVFn(GLuint fence) override;
  GLboolean glUnmapBufferFn(GLenum target) override;

 private:
  // The only consumers of GLStubApi are GpuChannelTestCommon (gpu_unittests)
  // and GPU fuzzers. We get a new GLStubApi for every case executed by
  // fuzzers, so we don't have to worry about ID exhaustion.
  void GenHelper(GLsizei count, GLuint* objects) {
    for (GLsizei i = 0; i < count; ++i) {
      // SAFETY: required from OpenGL across C API.
      UNSAFE_BUFFERS(objects[i] = next_id_++);
    }
  }

  std::string version_;
  std::string extensions_;
  GLuint next_id_ = 1;
};

}  // namespace gl

#endif  // UI_GL_GL_STUB_API_H_
