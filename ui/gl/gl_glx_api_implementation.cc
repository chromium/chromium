// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_glx_api_implementation.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_implementation_wrapper.h"
#include "ui/gl/gl_surface_glx.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

GL_IMPL_WRAPPER_TYPE(GLX) * g_glx_wrapper = nullptr;

void InitializeStaticGLBindingsGLX() {
  g_driver_glx.InitializeStaticBindings();
  if (!g_glx_wrapper) {
    auto real_api = std::make_unique<RealGLXApi>();
    real_api->Initialize(&g_driver_glx);
    g_glx_wrapper = new GL_IMPL_WRAPPER_TYPE(GLX)(std::move(real_api));
  }

  g_current_glx_context = g_glx_wrapper->api();
}

void ClearBindingsGLX() {
  delete g_glx_wrapper;
  g_glx_wrapper = nullptr;

  g_current_glx_context = nullptr;
  g_driver_glx.ClearBindings();
}

GLXApi::GLXApi() = default;

GLXApi::~GLXApi() = default;

GLXApiBase::GLXApiBase() : driver_(nullptr) {}

GLXApiBase::~GLXApiBase() = default;

void GLXApiBase::InitializeBase(DriverGLX* driver) {
  driver_ = driver;
}

RealGLXApi::RealGLXApi() = default;

RealGLXApi::~RealGLXApi() = default;

void RealGLXApi::Initialize(DriverGLX* driver) {
  InitializeBase(driver);
}

void RealGLXApi::SetDisabledExtensions(const std::string& disabled_extensions) {
  disabled_exts_.clear();
  filtered_exts_ = "";
  if (!disabled_extensions.empty()) {
    disabled_exts_ =
        base::SplitString(disabled_extensions, ", ;", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
  }
}

const char* RealGLXApi::glXQueryExtensionsStringFn(Display* dpy, int screen) {
  if (filtered_exts_.size())
    return filtered_exts_.c_str();

  if (!driver_->fn.glXQueryExtensionsStringFn)
    return nullptr;
  const char* str = GLXApiBase::glXQueryExtensionsStringFn(dpy, screen);
  if (!str)
    return nullptr;

  filtered_exts_ = FilterGLExtensionList(str, disabled_exts_);
  return filtered_exts_.c_str();
}

LogGLXApi::LogGLXApi(GLXApi* glx_api) : glx_api_(glx_api) {}

LogGLXApi::~LogGLXApi() = default;

void LogGLXApi::SetDisabledExtensions(const std::string& disabled_extensions) {
  if (glx_api_) {
    glx_api_->SetDisabledExtensions(disabled_extensions);
  }
}

TraceGLXApi::~TraceGLXApi() = default;

void TraceGLXApi::SetDisabledExtensions(
    const std::string& disabled_extensions) {
  if (glx_api_) {
    glx_api_->SetDisabledExtensions(disabled_extensions);
  }
}

bool GetGLWindowSystemBindingInfoGLX(const GLVersionInfo& gl_info,
                                     GLWindowSystemBindingInfo* info) {
  auto* connection = x11::Connection::Get();
  auto* display = connection->GetXlibDisplay().display();
  const int screen = connection->DefaultScreenId();
  const char* vendor = glXQueryServerString(display, screen, GLX_VENDOR);
  const char* version = glXQueryServerString(display, screen, GLX_VERSION);
  const char* extensions = glXQueryExtensionsString(display, screen);
  *info = GLWindowSystemBindingInfo();
  if (vendor)
    info->vendor = vendor;
  if (version)
    info->version = version;
  if (extensions)
    info->extensions = extensions;
  if (glXIsDirect(display, glXGetCurrentContext())) {
    info->direct_rendering_version = "2";
    bool using_mesa = gl_info.driver_vendor.find("Mesa") != std::string::npos ||
                      gl_info.driver_version.find("Mesa") != std::string::npos;
    if (using_mesa) {
      std::vector<std::string> split_version =
          base::SplitString(gl_info.driver_version, ".", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_ALL);
      unsigned major_num = 0;
      base::StringToUint(split_version[0], &major_num);
      // Mesa after version 17 will reliably use DRI3 when available.

      if (major_num >= 17 && connection->QueryExtension("DRI3").Sync())
        info->direct_rendering_version = "2.3";
      else if (connection->QueryExtension("DRI2").Sync())
        info->direct_rendering_version = "2.2";
      else if (connection->QueryExtension("DRI").Sync())
        info->direct_rendering_version = "2.1";
    }
  } else {
    info->direct_rendering_version = "1";
  }
  return true;
}

void SetDisabledExtensionsGLX(const std::string& disabled_extensions) {
  DCHECK(g_current_glx_context);
  DCHECK(GLContext::TotalGLContexts() == 0);
  g_current_glx_context->SetDisabledExtensions(disabled_extensions);
}

bool InitializeExtensionSettingsOneOffGLX() {
  return GLSurfaceGLX::InitializeExtensionSettingsOneOff();
}

}  // namespace gl
