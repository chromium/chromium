// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_egl_api_implementation.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_implementation_wrapper.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

GL_IMPL_WRAPPER_TYPE(EGL) * g_egl_wrapper = nullptr;

void InitializeStaticGLBindingsEGL() {
  g_driver_egl.InitializeStaticBindings();
  if (!g_egl_wrapper) {
    auto real_api = std::make_unique<RealEGLApi>();
    real_api->Initialize(&g_driver_egl);
    g_egl_wrapper = new GL_IMPL_WRAPPER_TYPE(EGL)(std::move(real_api));
  }

  g_current_egl_context = g_egl_wrapper->api();
}

void ClearBindingsEGL() {
  delete g_egl_wrapper;
  g_egl_wrapper = nullptr;

  g_current_egl_context = nullptr;
  g_driver_egl.ClearBindings();
}

EGLApi::EGLApi() {
}

EGLApi::~EGLApi() {
}

EGLApiBase::EGLApiBase() : driver_(nullptr) {}

EGLApiBase::~EGLApiBase() {
}

void EGLApiBase::InitializeBase(DriverEGL* driver) {
  driver_ = driver;
}

RealEGLApi::RealEGLApi() {
}

RealEGLApi::~RealEGLApi() {
}

void RealEGLApi::Initialize(DriverEGL* driver) {
  InitializeBase(driver);
}

void RealEGLApi::SetDisabledExtensions(const std::string& disabled_extensions) {
  DCHECK(GLContext::TotalGLContexts() == 0);
  disabled_exts_.clear();
  filtered_exts_.clear();
  if (!disabled_extensions.empty()) {
    std::vector<std::string> candidates =
        base::SplitString(disabled_extensions, ", ;", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    for (const auto& ext : candidates) {
      if (!base::StartsWith(ext, "EGL_", base::CompareCase::SENSITIVE))
        continue;
      // For the moment, only the following two extensions can be disabled.
      // See DriverEGL::UpdateConditionalExtensionBindings().
      DCHECK(ext == "EGL_KHR_fence_sync" || ext == "EGL_KHR_wait_sync");
      disabled_exts_.push_back(ext);
    }
  }
}

const char* RealEGLApi::eglQueryStringFn(EGLDisplay dpy, EGLint name) {
  if (name == EGL_EXTENSIONS) {
    auto it = filtered_exts_.find(dpy);
    if (it == filtered_exts_.end()) {
      it = filtered_exts_
               .emplace(dpy, FilterGLExtensionList(
                                 EGLApiBase::eglQueryStringFn(dpy, name),
                                 disabled_exts_))
               .first;
    }
    return (*it).second.c_str();
  }
  return EGLApiBase::eglQueryStringFn(dpy, name);
}

LogEGLApi::LogEGLApi(EGLApi* egl_api) : egl_api_(egl_api) {}

LogEGLApi::~LogEGLApi() {}

void LogEGLApi::SetDisabledExtensions(const std::string& disabled_extensions) {
  if (egl_api_) {
    egl_api_->SetDisabledExtensions(disabled_extensions);
  }
}

TraceEGLApi::~TraceEGLApi() {
}

void TraceEGLApi::SetDisabledExtensions(
    const std::string& disabled_extensions) {
  if (egl_api_) {
    egl_api_->SetDisabledExtensions(disabled_extensions);
  }
}

bool GetGLWindowSystemBindingInfoEGL(GLWindowSystemBindingInfo* info) {
  EGLDisplay display = eglGetCurrentDisplay();
  const char* vendor = eglQueryString(display, EGL_VENDOR);
  const char* version = eglQueryString(display, EGL_VERSION);
  const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
  *info = GLWindowSystemBindingInfo();
  if (vendor)
    info->vendor = vendor;
  if (version)
    info->version = version;
  if (extensions)
    info->extensions = extensions;
  return true;
}

void SetDisabledExtensionsEGL(const std::string& disabled_extensions) {
  DCHECK(g_current_egl_context);
  DCHECK(GLContext::TotalGLContexts() == 0);
  g_current_egl_context->SetDisabledExtensions(disabled_extensions);
}

bool InitializeExtensionSettingsOneOffEGL() {
  return GLSurfaceEGL::InitializeExtensionSettingsOneOff();
}

}  // namespace gl
