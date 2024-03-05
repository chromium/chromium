// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/gl_surface_cast.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/chromecast_switches.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/cast/gl_ozone_egl_cast.h"

namespace {
// Target fixed 30fps, or 60fps if doing triple-buffer 720p.
// TODO(halliwell): We might need to customize this value on various devices
// or make it dynamic that throttles framerate if device is overheating.
base::TimeDelta GetVSyncInterval() {
  if (chromecast::IsFeatureEnabled(chromecast::kTripleBuffer720)) {
    return base::Seconds(1) / 59.94;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kVSyncInterval)) {
    const std::string interval_str =
        command_line->GetSwitchValueASCII(switches::kVSyncInterval);
    double interval = 0;
    if (base::StringToDouble(interval_str, &interval) && interval > 0) {
      return base::Seconds(1) / interval;
    }
  }

  return base::Seconds(2) / 59.94;
}

}  // namespace

namespace ui {

GLSurfaceCast::GLSurfaceCast(gl::GLDisplayEGL* display,
                             gfx::AcceleratedWidget widget,
                             GLOzoneEglCast* parent)
    : NativeViewGLSurfaceEGL(
          display,
          parent->GetNativeWindow(),
          std::make_unique<gfx::FixedVSyncProvider>(base::TimeTicks(),
                                                    GetVSyncInterval())),
      widget_(widget),
      parent_(parent),
      uses_triple_buffering_(
          chromecast::IsFeatureEnabled(chromecast::kTripleBuffer720)) {
  DCHECK(parent_);
}

bool GLSurfaceCast::Resize(const gfx::Size& size,
                           float scale_factor,
                           const gfx::ColorSpace& color_space,
                           bool has_alpha) {
  return parent_->ResizeDisplay(size) &&
         NativeViewGLSurfaceEGL::Resize(size, scale_factor, color_space,
                                        has_alpha);
}

EGLConfig GLSurfaceCast::GetConfig() {
  if (!config_) {
    EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                               32,
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
                               EGL_WINDOW_BIT,
                               EGL_NONE};
    config_ = ChooseEGLConfig(GetEGLDisplay(), config_attribs);
  }
  return config_;
}

int GLSurfaceCast::GetBufferCount() const {
  return uses_triple_buffering_ ? 3 : 2;
}

GLSurfaceCast::~GLSurfaceCast() {
  Destroy();
}

}  // namespace ui
