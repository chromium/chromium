// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/skia/skia_renderer_factory.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/ozone/demo/skia/skia_gl_renderer.h"
#include "ui/ozone/demo/skia/skia_surfaceless_gl_renderer.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {
namespace {

const char kDisableSurfaceless[] = "disable-surfaceless";

scoped_refptr<gl::Presenter> CreatePresenter(gl::GLDisplay* display,
                                             gfx::AcceleratedWidget widget) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableSurfaceless))
    return gl::init::CreateSurfacelessViewGLSurface(display, widget);
  return nullptr;
}

scoped_refptr<gl::GLSurface> CreateGLSurface(gl::GLDisplay* display,
                                             gfx::AcceleratedWidget widget) {
  return gl::init::CreateViewGLSurface(display, widget);
}
}  // namespace

SkiaRendererFactory::SkiaRendererFactory() {}

SkiaRendererFactory::~SkiaRendererFactory() {
  if (display_) {
    gl::init::ShutdownGL(display_, false);
    display_ = nullptr;
  }
}

bool SkiaRendererFactory::Initialize() {
  display_ = gl::init::InitializeGLOneOff(
      /*gpu_preference=*/gl::GpuPreference::kDefault);
  if (!display_) {
    LOG(FATAL) << "Failed to initialize GL";
  }

  return true;
}

std::unique_ptr<Renderer> SkiaRendererFactory::CreateRenderer(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size) {
  SurfaceFactoryOzone* surface_factory_ozone =
      OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  auto window_surface =
      surface_factory_ozone->CreatePlatformWindowSurface(widget);

  if (auto presenter = CreatePresenter(display_, widget)) {
    return std::make_unique<SurfacelessSkiaGlRenderer>(
        widget, std::move(window_surface),
        gl::init::CreateOffscreenGLSurface(display_, gfx::Size(0, 0)),
        std::move(presenter), size);
  }
  scoped_refptr<gl::GLSurface> gl_surface = CreateGLSurface(display_, widget);
  if (!gl_surface)
    LOG(FATAL) << "Failed to create GL surface";
  return std::make_unique<SkiaGlRenderer>(widget, std::move(window_surface),
                                          std::move(gl_surface), size);
}

}  // namespace ui
