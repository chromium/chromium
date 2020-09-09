// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_x11_gles2.h"

#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/gl/egl_util.h"

using ui::GetLastEGLErrorString;
using ui::X11EventSource;

namespace gl {

NativeViewGLSurfaceEGLX11GLES2::NativeViewGLSurfaceEGLX11GLES2(
    x11::Window window)
    : NativeViewGLSurfaceEGLX11(x11::Window::None), parent_window_(window) {}

bool NativeViewGLSurfaceEGLX11GLES2::InitializeNativeWindow() {
  Display* x11_display = GetXNativeDisplay();
  XWindowAttributes attributes;
  if (!XGetWindowAttributes(x11_display, static_cast<uint32_t>(parent_window_),
                            &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window "
               << static_cast<uint32_t>(parent_window_) << ".";
    return false;
  }

  size_ = gfx::Size(attributes.width, attributes.height);

  // Create a child window, with a CopyFromParent visual (to avoid inducing
  // extra blits in the driver), that we can resize exactly in Resize(),
  // correctly ordered with GL, so that we don't have invalid transient states.
  // See https://crbug.com/326995.
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = 0;
  swa.bit_gravity = NorthWestGravity;
  window_ = XCreateWindow(x11_display, static_cast<uint32_t>(parent_window_), 0,
                          0, size_.width(), size_.height(), 0,
                          static_cast<int>(x11::WindowClass::CopyFromParent),
                          static_cast<int>(x11::WindowClass::InputOutput),
                          nullptr, CWBackPixmap | CWBitGravity, &swa);
  XMapWindow(x11_display, window_);
  XSelectInput(x11_display, window_, ExposureMask);
  XFlush(x11_display);

  return true;
}

void NativeViewGLSurfaceEGLX11GLES2::Destroy() {
  NativeViewGLSurfaceEGLX11::Destroy();

  if (window_) {
    Display* x11_display = GetXNativeDisplay();
    XDestroyWindow(x11_display, window_);
    window_ = 0;
    XFlush(x11_display);
  }
}

EGLConfig NativeViewGLSurfaceEGLX11GLES2::GetConfig() {
  if (!config_) {
    // Get a config compatible with the window
    DCHECK(window_);
    XWindowAttributes win_attribs;
    if (!XGetWindowAttributes(GetXNativeDisplay(), window_, &win_attribs)) {
      return nullptr;
    }

    // Try matching the window depth with an alpha channel,
    // because we're worried the destination alpha width could
    // constrain blending precision.
    const int kBufferSizeOffset = 1;
    const int kAlphaSizeOffset = 3;
    EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                               ~0,
                               EGL_ALPHA_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_RED_SIZE,
                               8,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                               EGL_NONE};
    config_attribs[kBufferSizeOffset] = win_attribs.depth;

    EGLDisplay display = GetHardwareDisplay();
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &config_, 1, &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << GetLastEGLErrorString();
      return nullptr;
    }

    if (num_configs) {
      EGLint config_depth;
      if (!eglGetConfigAttrib(display, config_, EGL_BUFFER_SIZE,
                              &config_depth)) {
        LOG(ERROR) << "eglGetConfigAttrib failed with error "
                   << GetLastEGLErrorString();
        return nullptr;
      }

      if (config_depth == win_attribs.depth) {
        return config_;
      }
    }

    // Try without an alpha channel.
    config_attribs[kAlphaSizeOffset] = 0;
    if (!eglChooseConfig(display, config_attribs, &config_, 1, &num_configs)) {
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
  XResizeWindow(GetXNativeDisplay(), window_, size.width(), size.height());
  eglWaitNative(EGL_CORE_NATIVE_ENGINE);

  return true;
}

bool NativeViewGLSurfaceEGLX11GLES2::DispatchXEvent(x11::Event* x11_event) {
  auto* expose = x11_event->As<x11::ExposeEvent>();
  auto window = static_cast<x11::Window>(window_);
  if (!expose || expose->window != window)
    return false;

  auto expose_copy = *expose;
  expose_copy.window = parent_window_;
  x11::SendEvent(expose_copy, parent_window_, x11::EventMask::Exposure);
  x11::Connection::Get()->Flush();
  return true;
}

NativeViewGLSurfaceEGLX11GLES2::~NativeViewGLSurfaceEGLX11GLES2() {
  Destroy();
}

}  // namespace gl
