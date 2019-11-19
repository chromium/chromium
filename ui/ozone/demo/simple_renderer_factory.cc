// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/simple_renderer_factory.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/ozone/demo/gl_renderer.h"
#include "ui/ozone/demo/software_renderer.h"
#include "ui/ozone/demo/surfaceless_gl_renderer.h"
#include "ui/ozone/public/overlay_surface.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/ozone/demo/vulkan_overlay_renderer.h"
#include "ui/ozone/demo/vulkan_renderer.h"
#endif

namespace ui {
namespace {

const char kDisableSurfaceless[] = "disable-surfaceless";
const char kDisableGpu[] = "disable-gpu";
#if BUILDFLAG(ENABLE_VULKAN)
const char kEnableVulkan[] = "enable-vulkan";
#endif

scoped_refptr<gl::GLSurface> CreateGLSurface(gfx::AcceleratedWidget widget) {
  scoped_refptr<gl::GLSurface> surface;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableSurfaceless))
    surface = gl::init::CreateSurfacelessViewGLSurface(widget);
  if (!surface)
    surface = gl::init::CreateViewGLSurface(widget);
  return surface;
}

}  // namespace

SimpleRendererFactory::SimpleRendererFactory() {}

SimpleRendererFactory::~SimpleRendererFactory() {}

bool SimpleRendererFactory::Initialize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

#if BUILDFLAG(ENABLE_VULKAN)
  if (command_line->HasSwitch(kEnableVulkan)) {
    vulkan_implementation_ = gpu::CreateVulkanImplementation();
    if (vulkan_implementation_ &&
        vulkan_implementation_->InitializeVulkanInstance()) {
      type_ = VULKAN;
      return true;
    } else {
      vulkan_implementation_.reset();
    }
  }
#endif
  if (!command_line->HasSwitch(kDisableGpu) && gl::init::InitializeGLOneOff()) {
    type_ = GL;
  } else {
    type_ = SOFTWARE;
  }

  return true;
}

std::unique_ptr<Renderer> SimpleRendererFactory::CreateRenderer(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size) {
  SurfaceFactoryOzone* surface_factory_ozone =
      OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  auto window_surface =
      surface_factory_ozone->CreatePlatformWindowSurface(widget);
  switch (type_) {
    case GL: {
      scoped_refptr<gl::GLSurface> surface = CreateGLSurface(widget);
      if (!surface)
        LOG(FATAL) << "Failed to create GL surface";
      if (surface->IsSurfaceless()) {
        return std::make_unique<SurfacelessGlRenderer>(
            widget, std::move(window_surface), surface, size);
      }
      return std::make_unique<GlRenderer>(widget, std::move(window_surface),
                                          surface, size);
    }
#if BUILDFLAG(ENABLE_VULKAN)
    case VULKAN: {
      std::unique_ptr<OverlaySurface> overlay_surface =
          surface_factory_ozone->CreateOverlaySurface(widget);
      if (overlay_surface) {
        return std::make_unique<VulkanOverlayRenderer>(
            std::move(window_surface), std::move(overlay_surface),
            surface_factory_ozone, vulkan_implementation_.get(), widget, size);
      }
      std::unique_ptr<gpu::VulkanSurface> vulkan_surface =
          vulkan_implementation_->CreateViewSurface(widget);
      return std::make_unique<VulkanRenderer>(
          std::move(window_surface), std::move(vulkan_surface),
          vulkan_implementation_.get(), widget, size);
    }
#endif
    case SOFTWARE:
      return std::make_unique<SoftwareRenderer>(
          widget, std::move(window_surface), size);
  }

  return nullptr;
}

}  // namespace ui
