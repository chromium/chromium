// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_x11.h"

#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gl/egl_util.h"

using ui::GetLastEGLErrorString;
using ui::PlatformEvent;
using ui::PlatformEventSource;

namespace gl {

NativeViewGLSurfaceEGLX11::NativeViewGLSurfaceEGLX11(EGLNativeWindowType window)
    : NativeViewGLSurfaceEGL(0, nullptr), parent_window_(window) {}

bool NativeViewGLSurfaceEGLX11::InitializeNativeWindow() {
  Display* x11_display = GetNativeDisplay();
  XWindowAttributes attributes;
  if (!XGetWindowAttributes(x11_display, parent_window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << parent_window_
        << ".";
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
  window_ = XCreateWindow(x11_display, parent_window_, 0, 0, size_.width(),
                          size_.height(), 0, CopyFromParent, InputOutput,
                          CopyFromParent, CWBackPixmap | CWBitGravity, &swa);
  XMapWindow(x11_display, window_);
  XSelectInput(x11_display, window_, ExposureMask);
  XFlush(x11_display);

  return true;
}

void NativeViewGLSurfaceEGLX11::Destroy() {
  NativeViewGLSurfaceEGL::Destroy();

  if (window_) {
    Display* x11_display = GetNativeDisplay();
    XDestroyWindow(x11_display, window_);
    window_ = 0;
    XFlush(x11_display);
  }
}

EGLConfig NativeViewGLSurfaceEGLX11::GetConfig() {
  if (!config_) {
    // Get a config compatible with the window
    DCHECK(window_);
    XWindowAttributes win_attribs;
    if (!XGetWindowAttributes(GetNativeDisplay(), window_, &win_attribs)) {
      return NULL;
    }

    // Try matching the window depth with an alpha channel,
    // because we're worried the destination alpha width could
    // constrain blending precision.
    const int kBufferSizeOffset = 1;
    const int kAlphaSizeOffset = 3;
    EGLint config_attribs[] = {
      EGL_BUFFER_SIZE, ~0,
      EGL_ALPHA_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
      EGL_NONE
    };
    config_attribs[kBufferSizeOffset] = win_attribs.depth;

    EGLDisplay display = GetHardwareDisplay();
    EGLint num_configs;
    if (!eglChooseConfig(display,
                         config_attribs,
                         &config_,
                         1,
                         &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << GetLastEGLErrorString();
      return NULL;
    }

    if (num_configs) {
      EGLint config_depth;
      if (!eglGetConfigAttrib(display,
                              config_,
                              EGL_BUFFER_SIZE,
                              &config_depth)) {
        LOG(ERROR) << "eglGetConfigAttrib failed with error "
                   << GetLastEGLErrorString();
        return NULL;
      }

      if (config_depth == win_attribs.depth) {
        return config_;
      }
    }

    // Try without an alpha channel.
    config_attribs[kAlphaSizeOffset] = 0;
    if (!eglChooseConfig(display,
                         config_attribs,
                         &config_,
                         1,
                         &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << GetLastEGLErrorString();
      return NULL;
    }

    if (num_configs == 0) {
      LOG(ERROR) << "No suitable EGL configs found.";
      return NULL;
    }
  }
  return config_;
}

bool NativeViewGLSurfaceEGLX11::Resize(const gfx::Size& size,
                                       float scale_factor,
                                       ColorSpace color_space,
                                       bool has_alpha) {
  if (size == GetSize())
    return true;

  size_ = size;

  eglWaitGL();
  XResizeWindow(GetNativeDisplay(), window_, size.width(), size.height());
  eglWaitNative(EGL_CORE_NATIVE_ENGINE);

  return true;
}

bool NativeViewGLSurfaceEGLX11::CanDispatchEvent(const PlatformEvent& event) {
  return event->type == Expose && event->xexpose.window == window_;
}

uint32_t NativeViewGLSurfaceEGLX11::DispatchEvent(const PlatformEvent& event) {
  XEvent x_event = *event;
  x_event.xexpose.window = parent_window_;

  Display* x11_display = GetNativeDisplay();
  XSendEvent(x11_display, parent_window_, x11::False, ExposureMask, &x_event);
  XFlush(x11_display);
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

NativeViewGLSurfaceEGLX11::~NativeViewGLSurfaceEGLX11() {
  Destroy();
}

}  // namespace gl
