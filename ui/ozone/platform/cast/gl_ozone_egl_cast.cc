// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/gl_ozone_egl_cast.h"

#include <EGL/egl.h>
#include <dlfcn.h>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/public/cast_egl_platform.h"
#include "chromecast/public/graphics_types.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/cast/gl_surface_cast.h"

using chromecast::CastEglPlatform;

namespace ui {

namespace {

typedef EGLDisplay (*EGLGetDisplayFn)(NativeDisplayType);
typedef EGLBoolean (*EGLTerminateFn)(EGLDisplay);

chromecast::Size FromGfxSize(const gfx::Size& size) {
  return chromecast::Size(size.width(), size.height());
}

// Display resolution, set in browser process and passed by switches.
gfx::Size GetDisplaySize() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  int width, height;
  if (base::StringToInt(
          cmd_line->GetSwitchValueASCII(switches::kCastInitialScreenWidth),
          &width) &&
      base::StringToInt(
          cmd_line->GetSwitchValueASCII(switches::kCastInitialScreenHeight),
          &height)) {
    return gfx::Size(width, height);
  }
  LOG(WARNING) << "Unable to get initial screen resolution from command line,"
               << "using default 720p";
  return gfx::Size(1280, 720);
}

}  // namespace

GLOzoneEglCast::GLOzoneEglCast(std::unique_ptr<CastEglPlatform> egl_platform)
    : display_size_(GetDisplaySize()), egl_platform_(std::move(egl_platform)) {}

GLOzoneEglCast::~GLOzoneEglCast() {
  // eglTerminate must be called first on display before releasing resources
  // and shutting down hardware
  TerminateDisplay();
}

void GLOzoneEglCast::InitializeHardwareIfNeeded() {
  if (hardware_initialized_)
    return;

  CHECK(egl_platform_->InitializeHardware());
  hardware_initialized_ = true;
}

void GLOzoneEglCast::TerminateDisplay() {
  void* egl_lib_handle = egl_platform_->GetEglLibrary();
  if (!egl_lib_handle)
    return;

  EGLGetDisplayFn get_display =
      reinterpret_cast<EGLGetDisplayFn>(dlsym(egl_lib_handle, "eglGetDisplay"));
  EGLTerminateFn terminate =
      reinterpret_cast<EGLTerminateFn>(dlsym(egl_lib_handle, "eglTerminate"));
  DCHECK(get_display);
  DCHECK(terminate);

  EGLDisplay display = get_display(GetNativeDisplay());
  DCHECK_NE(display, EGL_NO_DISPLAY);

  EGLBoolean terminate_result = terminate(display);
  DCHECK_EQ(terminate_result, static_cast<EGLBoolean>(EGL_TRUE));
}

scoped_refptr<gl::GLSurface> GLOzoneEglCast::CreateViewGLSurface(
    gfx::AcceleratedWidget widget) {
  // Verify requested widget dimensions match our current display size.
  DCHECK_EQ(widget >> 16, display_size_.width());
  DCHECK_EQ(widget & 0xffff, display_size_.height());

  return gl::InitializeGLSurface(new GLSurfaceCast(widget, this));
}

scoped_refptr<gl::GLSurface> GLOzoneEglCast::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  return gl::InitializeGLSurface(new gl::PbufferGLSurfaceEGL(size));
}

intptr_t GLOzoneEglCast::GetNativeDisplay() {
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(display_type_);
}

void GLOzoneEglCast::CreateDisplayTypeAndWindowIfNeeded() {
  InitializeHardwareIfNeeded();

  if (!have_display_type_) {
    chromecast::Size create_size = FromGfxSize(display_size_);
    display_type_ = egl_platform_->CreateDisplayType(create_size);
    have_display_type_ = true;
  }
  if (!window_) {
    chromecast::Size create_size = FromGfxSize(display_size_);
    window_ = egl_platform_->CreateWindow(display_type_, create_size);
    CHECK(window_);
  }
}

intptr_t GLOzoneEglCast::GetNativeWindow() {
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(window_);
}

bool GLOzoneEglCast::ResizeDisplay(gfx::Size size) {
  DCHECK_EQ(size.width(), display_size_.width());
  DCHECK_EQ(size.height(), display_size_.height());
  return true;
}

bool GLOzoneEglCast::LoadGLES2Bindings(gl::GLImplementation implementation) {
  InitializeHardwareIfNeeded();

  void* lib_egl = egl_platform_->GetEglLibrary();
  void* lib_gles2 = egl_platform_->GetGles2Library();
  gl::GLGetProcAddressProc gl_proc = reinterpret_cast<gl::GLGetProcAddressProc>(
      egl_platform_->GetGLProcAddressProc());
  if (!lib_egl || !lib_gles2 || !gl_proc) {
    return false;
  }

  gl::SetGLGetProcAddressProc(gl_proc);
  gl::AddGLNativeLibrary(lib_egl);
  gl::AddGLNativeLibrary(lib_gles2);
  return true;
}

}  // namespace ui
