// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/skia/skia_renderer_factory.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
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

scoped_refptr<gl::GLSurface> CreateGLSurface(gfx::AcceleratedWidget widget) {
  scoped_refptr<gl::GLSurface> surface;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableSurfaceless))
    surface = gl::init::CreateSurfacelessViewGLSurface(widget);
  if (!surface)
    surface = gl::init::CreateViewGLSurface(widget);
  return surface;
}

}  // namespace

SkiaRendererFactory::SkiaRendererFactory() {}

SkiaRendererFactory::~SkiaRendererFactory() {}

bool SkiaRendererFactory::Initialize() {
  if (!gl::init::InitializeGLOneOff()) {
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
  scoped_refptr<gl::GLSurface> gl_surface = CreateGLSurface(widget);
  if (!gl_surface)
    LOG(FATAL) << "Failed to create GL surface";
  if (gl_surface->IsSurfaceless()) {
    return std::make_unique<SurfacelessSkiaGlRenderer>(
        widget, std::move(window_surface), std::move(gl_surface), size);
  }
  return std::make_unique<SkiaGlRenderer>(widget, std::move(window_surface),
                                          std::move(gl_surface), size);
}

}  // namespace ui
