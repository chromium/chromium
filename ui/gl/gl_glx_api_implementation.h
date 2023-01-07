// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_GLX_API_IMPLEMENTATION_H_
#define UI_GL_GL_GLX_API_IMPLEMENTATION_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace gl {

struct GLVersionInfo;
struct GLWindowSystemBindingInfo;

GL_EXPORT void InitializeStaticGLBindingsGLX();
GL_EXPORT void ClearBindingsGLX();
GL_EXPORT bool GetGLWindowSystemBindingInfoGLX(const GLVersionInfo& gl_info,
                                               GLWindowSystemBindingInfo* info);
GL_EXPORT void SetDisabledExtensionsGLX(const std::string& disabled_extensions);
GL_EXPORT bool InitializeExtensionSettingsOneOffGLX();

class GL_EXPORT GLXApiBase : public GLXApi {
 public:
  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_glx.h"

 protected:
  GLXApiBase();
  ~GLXApiBase() override;
  void InitializeBase(DriverGLX* driver);

  raw_ptr<DriverGLX> driver_;
};

class GL_EXPORT RealGLXApi : public GLXApiBase {
 public:
  RealGLXApi();
  ~RealGLXApi() override;
  void Initialize(DriverGLX* driver);
  void SetDisabledExtensions(const std::string& disabled_extensions) override;

  const char* glXQueryExtensionsStringFn(Display* dpy, int screen) override;
 private:

  std::vector<std::string> disabled_exts_;
  std::string filtered_exts_;
};

// Logs debug information for every GLX call.
class GL_EXPORT LogGLXApi : public GLXApi {
 public:
  LogGLXApi(GLXApi* glx_api);
  ~LogGLXApi() override;

  void SetDisabledExtensions(const std::string& disabled_extensions) override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_glx.h"

 private:
  raw_ptr<GLXApi> glx_api_;
};

// Inserts a TRACE for every GLX call.
class GL_EXPORT TraceGLXApi : public GLXApi {
 public:
  TraceGLXApi(GLXApi* glx_api) : glx_api_(glx_api) { }
  ~TraceGLXApi() override;

  void SetDisabledExtensions(const std::string& disabled_extensions) override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_glx.h"

 private:
  raw_ptr<GLXApi> glx_api_;
};

}  // namespace gl

#endif  // UI_GL_GL_GLX_API_IMPLEMENTATION_H_
