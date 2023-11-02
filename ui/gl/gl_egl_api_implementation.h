// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_EGL_API_IMPLEMENTATION_H_
#define UI_GL_GL_EGL_API_IMPLEMENTATION_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_export.h"

namespace gl {

struct GLWindowSystemBindingInfo;

GL_EXPORT void InitializeStaticGLBindingsEGL();
GL_EXPORT void ClearBindingsEGL();
GL_EXPORT bool GetGLWindowSystemBindingInfoEGL(GLWindowSystemBindingInfo* info);
GL_EXPORT void SetDisabledExtensionsEGL(const std::string& disabled_extensions);
GL_EXPORT bool InitializeExtensionSettingsOneOffEGL(GLDisplayEGL* display);

class GL_EXPORT EGLApiBase : public EGLApi {
 public:
  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_egl.h"

 protected:
  EGLApiBase();
  ~EGLApiBase() override;
  void InitializeBase(DriverEGL* driver);

  raw_ptr<DriverEGL> driver_;
};

class GL_EXPORT RealEGLApi : public EGLApiBase {
 public:
  RealEGLApi();
  ~RealEGLApi() override;
  void Initialize(DriverEGL* driver);
  void SetDisabledExtensions(const std::string& disabled_extensions) override;

  const char* eglQueryStringFn(EGLDisplay dpy, EGLint name) override;

 private:
  // Filtered EGL_EXTENSIONS we return to eglQueryStringFn() calls.
  std::vector<std::string> disabled_exts_;
  std::map<EGLDisplay, std::string> filtered_exts_;
};

// Logs debug information for every EGL call.
class GL_EXPORT LogEGLApi : public EGLApi {
 public:
  LogEGLApi(EGLApi* egl_api);
  ~LogEGLApi() override;
  void SetDisabledExtensions(const std::string& disabled_extensions) override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_egl.h"

 private:
  raw_ptr<EGLApi> egl_api_;
};

// Inserts a TRACE for every EGL call.
class GL_EXPORT TraceEGLApi : public EGLApi {
 public:
  TraceEGLApi(EGLApi* egl_api) : egl_api_(egl_api) { }
  ~TraceEGLApi() override;
  void SetDisabledExtensions(const std::string& disabled_extensions) override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_egl.h"

 private:
  raw_ptr<EGLApi> egl_api_;
};

}  // namespace gl

#endif  // UI_GL_GL_EGL_API_IMPLEMENTATION_H_
