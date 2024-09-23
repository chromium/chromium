// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/presenter.h"

#include "ui/gfx/gpu_fence.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gl/dc_layer_overlay_params.h"
#else
namespace gl {
struct DCLayerOverlayParams {};
}  // namespace gl
#endif

namespace gl {

Presenter::Presenter() = default;
Presenter::~Presenter() = default;

bool Presenter::SupportsOverridePlatformSize() const {
  return false;
}

bool Presenter::SupportsViewporter() const {
  return false;
}

bool Presenter::SupportsPlaneGpuFences() const {
  return false;
}

bool Presenter::ScheduleOverlayPlane(
    OverlayImage image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  NOTIMPLEMENTED();
  return false;
}

bool Presenter::ScheduleCALayer(const ui::CARendererLayerParams& params) {
  NOTIMPLEMENTED();
  return false;
}

void Presenter::ScheduleDCLayer(std::unique_ptr<DCLayerOverlayParams> params) {
  NOTIMPLEMENTED();
}

bool Presenter::Resize(const gfx::Size& size,
                       float scale_factor,
                       const gfx::ColorSpace& color_space,
                       bool has_alpha) {
  return true;
}

}  // namespace gl
