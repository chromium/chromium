// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/software_renderer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {
namespace {

const int kFrameDelayMilliseconds = 16;

}  // namespace

SoftwareRenderer::SoftwareRenderer(
    gfx::AcceleratedWidget widget,
    std::unique_ptr<PlatformWindowSurface> window_surface,
    const gfx::Size& size)
    : RendererBase(widget, size),
      window_surface_(std::move(window_surface)),
      vsync_period_(
          base::TimeDelta::FromMilliseconds(kFrameDelayMilliseconds)) {}

SoftwareRenderer::~SoftwareRenderer() {}

bool SoftwareRenderer::Initialize() {
  software_surface_ =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateCanvasForWidget(widget_,
                                  base::ThreadTaskRunnerHandle::Get().get());
  if (!software_surface_) {
    LOG(ERROR) << "Failed to create software surface";
    return false;
  }

  software_surface_->ResizeCanvas(size_);
  vsync_provider_ = software_surface_->CreateVSyncProvider();
  RenderFrame();
  return true;
}

void SoftwareRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "SoftwareRenderer::RenderFrame");

  float fraction = NextFraction();

  sk_sp<SkSurface> surface = software_surface_->GetSurface();

  SkColor color =
      SkColorSetARGB(0xff, 0, 0xff * fraction, 0xff * (1 - fraction));

  surface->getCanvas()->clear(color);

  software_surface_->PresentCanvas(gfx::Rect(size_));

  if (vsync_provider_) {
    vsync_provider_->GetVSyncParameters(
        base::BindOnce(&SoftwareRenderer::UpdateVSyncParameters,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  timer_.Start(FROM_HERE, vsync_period_, this, &SoftwareRenderer::RenderFrame);
}

void SoftwareRenderer::UpdateVSyncParameters(const base::TimeTicks timebase,
                                             const base::TimeDelta interval) {
  vsync_period_ = interval;
}

}  // namespace ui
