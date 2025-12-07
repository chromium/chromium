// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_x11_gles2.h"

#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/visual_manager.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"

using ui::GetLastEGLErrorString;

namespace gl {

NativeViewGLSurfaceEGLX11GLES2::NativeViewGLSurfaceEGLX11GLES2(
    gl::GLDisplayEGL* display,
    x11::Window window)
    : NativeViewGLSurfaceEGLX11(display, x11::Window::None),
      parent_window_(window) {}

bool NativeViewGLSurfaceEGLX11GLES2::InitializeNativeWindow() {
  auto* connection = GetXNativeConnection();
  auto geometry = connection->GetGeometry(parent_window_).Sync();
  if (!geometry) {
    LOG(ERROR) << "GetGeometry failed for window "
               << static_cast<uint32_t>(parent_window_) << ".";
    return false;
  }

  size_ = gfx::Size(geometry->width, geometry->height);

  // Create a child window, with a CopyFromParent visual (to avoid inducing
  // extra blits in the driver), that we can resize exactly in Resize(),
  // correctly ordered with GL, so that we don't have invalid transient states.
  // See https://crbug.com/326995.
  set_window(connection->GenerateId<x11::Window>());
  connection->CreateWindow(x11::CreateWindowRequest{
      .wid = window(),
      .parent = parent_window_,
      .width = static_cast<uint16_t>(size_.width()),
      .height = static_cast<uint16_t>(size_.height()),
      .c_class = x11::WindowClass::InputOutput,
      .background_pixmap = x11::Pixmap::None,
      .bit_gravity = x11::Gravity::NorthWest,
      .event_mask = x11::EventMask::Exposure,
  });
  connection->MapWindow({window()});
  connection->Flush();

  return true;
}

void NativeViewGLSurfaceEGLX11GLES2::Destroy() {
  NativeViewGLSurfaceEGLX11::Destroy();

  if (window_) {
    auto* connection = GetXNativeConnection();
    connection->DestroyWindow({window()});
    window_ = 0;
    connection->Flush();
  }
}

EGLConfig NativeViewGLSurfaceEGLX11GLES2::GetConfig() {
  if (!config_) {
    // Get a config compatible with the window
    DCHECK(window_);
    auto* connection = GetXNativeConnection();
    auto geometry = connection->GetGeometry(window()).Sync();
    if (!geometry)
      return nullptr;

    // Try matching the window depth with an alpha channel,
    // because we're worried the destination alpha width could
    // constrain blending precision.
    const int kBufferSizeOffset = 1;
    const int kAlphaSizeOffset = 3;
    std::array<EGLint, 15> config_attribs{
        {EGL_BUFFER_SIZE, ~0, EGL_ALPHA_SIZE, 8, EGL_BLUE_SIZE, 8,
         EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE,
         EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
         EGL_NONE}};
    config_attribs[kBufferSizeOffset] = geometry->depth;

    EGLDisplay display = GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay();
    x11::VisualId visual_id;
    connection->GetOrCreateVisualManager().ChooseVisualForWindow(
        true, &visual_id, nullptr, nullptr, nullptr);
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs.data(), nullptr, 0,
                         &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << GetLastEGLErrorString();
      return nullptr;
    }
    std::vector<EGLConfig> configs(num_configs);

    if (num_configs) {
      if (!eglChooseConfig(display, config_attribs.data(), &configs.front(),
                           num_configs, &num_configs)) {
        LOG(ERROR) << "eglChooseConfig failed with error "
                   << GetLastEGLErrorString();
        return nullptr;
      }
      for (EGLConfig config : configs) {
        EGLint config_depth;
        if (!eglGetConfigAttrib(display, config, EGL_BUFFER_SIZE,
                                &config_depth)) {
          LOG(ERROR) << "eglGetConfigAttrib failed with error "
                     << GetLastEGLErrorString();
          return nullptr;
        }
        EGLint config_visual_id;
        if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID,
                                &config_visual_id)) {
          LOG(ERROR) << "eglGetConfigAttrib failed with error "
                     << GetLastEGLErrorString();
          return nullptr;
        }
        if (config_depth == geometry->depth &&
            config_visual_id == static_cast<EGLint>(visual_id)) {
          config_ = config;
          return config_;
        }
      }
    }

    // Try without an alpha channel.
    config_attribs[kAlphaSizeOffset] = 0;
    if (!eglChooseConfig(display, config_attribs.data(), &config_, 1,
                         &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << GetLastEGLErrorString();
      return nullptr;
    }

    if (num_configs == 0) {
      LOG(ERROR) << "No suitable EGL configs found.";
      return nullptr;
    }
  }
  return config_;
}

bool NativeViewGLSurfaceEGLX11GLES2::Resize(const gfx::Size& size,
                                            float scale_factor,
                                            const gfx::ColorSpace& color_space,
                                            bool has_alpha) {
  if (size == GetSize())
    return true;

  size_ = size;

  eglWaitGL();
  auto* connection = GetXNativeConnection();
  connection->ConfigureWindow({
      .window = window(),
      .width = size.width(),
      .height = size.height(),
  });
  connection->Flush();
  eglWaitNative(EGL_CORE_NATIVE_ENGINE);

  return true;
}

void NativeViewGLSurfaceEGLX11GLES2::OnEvent(const x11::Event& x11_event) {
  auto* expose = x11_event.As<x11::ExposeEvent>();
  auto window = static_cast<x11::Window>(window_);
  if (!expose || expose->window != window)
    return;

  auto expose_copy = *expose;
  expose_copy.window = parent_window_;
  x11::Connection::Get()->SendEvent(expose_copy, parent_window_,
                                    x11::EventMask::Exposure);
  x11::Connection::Get()->Flush();
}

NativeViewGLSurfaceEGLX11GLES2::~NativeViewGLSurfaceEGLX11GLES2() {
  InvalidateWeakPtrs();
  Destroy();
}

}  // namespace gl
