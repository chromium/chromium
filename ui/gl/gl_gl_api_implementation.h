// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_GL_API_IMPLEMENTATION_H_
#define UI_GL_GL_GL_API_IMPLEMENTATION_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace gl {

struct GLVersionInfo;

GL_EXPORT GLenum GetInternalFormat(const GLVersionInfo* version,
                                   GLenum internal_format);

GL_EXPORT void InitializeStaticGLBindingsGL();
GL_EXPORT void ClearBindingsGL();

bool SetNullDrawGLBindingsEnabled(bool enabled);
bool GetNullDrawBindingsEnabled();

// This is exported from //ui/gl/gl_bindings.h to retrieve GL bindings.
GL_EXPORT CurrentGL* GetThreadLocalCurrentGL();

// This is only used internally in //ui/gl to set GL bindings from GLContext.
void SetThreadLocalCurrentGL(CurrentGL* current);

class GL_EXPORT GLApiBase : public GLApi {
 public:
  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_gl.h"

 protected:
  GLApiBase();
  ~GLApiBase() override;
  void InitializeBase(DriverGL* driver);

  raw_ptr<DriverGL> driver_;
};

// Implemenents the GL API by calling directly into the driver.
class GL_EXPORT RealGLApi : public GLApiBase {
 public:
  RealGLApi();
  ~RealGLApi() override;
  void Initialize(DriverGL* driver);
  void SetDisabledExtensions(const std::string& disabled_extensions) override;

  void glGetIntegervFn(GLenum pname, GLint* params) override;
  const GLubyte* glGetStringFn(GLenum name) override;
  const GLubyte* glGetStringiFn(GLenum name, GLuint index) override;

  void glTexImage2DFn(GLenum target,
                      GLint level,
                      GLint internalformat,
                      GLsizei width,
                      GLsizei height,
                      GLint border,
                      GLenum format,
                      GLenum type,
                      const void* pixels) override;

  void glTexSubImage2DFn(GLenum target,
                         GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         const void* pixels) override;

  void glTexStorage2DEXTFn(GLenum target,
                           GLsizei levels,
                           GLenum internalformat,
                           GLsizei width,
                           GLsizei height) override;

  void glTexStorageMem2DEXTFn(GLenum target,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLuint memory,
                              GLuint64 offset) override;

  void glTexStorageMemFlags2DANGLEFn(GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLuint memory,
                                     GLuint64 offset,
                                     GLbitfield createFlags,
                                     GLbitfield usageFlags,
                                     const void* imageCreateInfoPNext) override;

  void glRenderbufferStorageEXTFn(GLenum target,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height) override;

  void glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                             GLsizei samples,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height) override;

  void glRenderbufferStorageMultisampleFn(GLenum target,
                                          GLsizei samples,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height) override;

  void glReadPixelsFn(GLint x,
                      GLint y,
                      GLsizei width,
                      GLsizei height,
                      GLenum format,
                      GLenum type,
                      void* pixels) override;

  void glClearFn(GLbitfield mask) override;
  void glDrawArraysFn(GLenum mode, GLint first, GLsizei count) override;
  void glDrawElementsFn(GLenum mode,
                        GLsizei count,
                        GLenum type,
                        const void* indices) override;

  void glClearDepthFn(GLclampd depth) override;
  void glDepthRangeFn(GLclampd z_near, GLclampd z_far) override;

  void glUseProgramFn(GLuint program) override;

  void set_version(std::unique_ptr<GLVersionInfo> version);
  void ClearCachedGLExtensions();

 private:
  // Compute |filtered_exts_| & |filtered_exts_str_| from |disabled_ext_|.
  void InitializeFilteredExtensionsIfNeeded();

  const bool logging_enabled_;
  std::vector<std::string> disabled_exts_;
  // Filtered GL_EXTENSIONS we return to glGetString(i) calls.
  std::vector<std::string> filtered_exts_;
  std::string filtered_exts_str_;

  std::unique_ptr<GLVersionInfo> version_;
};

// Inserts a TRACE for every GL call.
class TraceGLApi : public GLApi {
 public:
  TraceGLApi(GLApi* gl_api) : gl_api_(gl_api) { }
  ~TraceGLApi() override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_gl.h"

 private:
  raw_ptr<GLApi> gl_api_;
};

// Logs debug information for every GL call.
class LogGLApi : public GLApi {
 public:
  LogGLApi(GLApi* gl_api);
  ~LogGLApi() override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_gl.h"

 private:
  raw_ptr<GLApi> gl_api_;
};

// Catches incorrect usage when GL calls are made without a current context.
class NoContextGLApi : public GLApi {
 public:
  NoContextGLApi();
  ~NoContextGLApi() override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_gl.h"
};

}  // namespace gl

#endif  // UI_GL_GL_GL_API_IMPLEMENTATION_H_
