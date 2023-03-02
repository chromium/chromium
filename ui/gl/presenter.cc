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

Presenter::Presenter(GLDisplayEGL* display, const gfx::Size& size) {}
Presenter::~Presenter() = default;

bool Presenter::SupportsAsyncSwap() {
  return true;
}

bool Presenter::SupportsPostSubBuffer() {
  return true;
}

bool Presenter::SupportsCommitOverlayPlanes() {
  return false;
}

bool Presenter::SupportsOverridePlatformSize() const {
  return false;
}

bool Presenter::SupportsViewporter() const {
  return false;
}

bool Presenter::SupportsPlaneGpuFences() const {
  return false;
}

bool Presenter::IsOffscreen() {
  return false;
}

bool Presenter::IsSurfaceless() {
  return true;
}

bool Presenter::SupportsDCLayers() const {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

bool Presenter::SupportsGpuVSync() const {
  return false;
}

gfx::SurfaceOrigin Presenter::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

void Presenter::SwapBuffersAsync(SwapCompletionCallback completion_callback,
                                 PresentationCallback presentation_callback,
                                 gfx::FrameData data) {
  Present(std::move(completion_callback), std::move(presentation_callback),
          data);
}

void Presenter::PostSubBufferAsync(int x,
                                   int y,
                                   int width,
                                   int height,
                                   SwapCompletionCallback completion_callback,
                                   PresentationCallback presentation_callback,
                                   gfx::FrameData data) {
  Present(std::move(completion_callback), std::move(presentation_callback),
          data);
}

gfx::SwapResult Presenter::SwapBuffers(PresentationCallback callback,
                                       gfx::FrameData data) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::SwapResult Presenter::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::SwapResult Presenter::CommitOverlayPlanes(PresentationCallback callback,
                                               gfx::FrameData data) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void Presenter::CommitOverlayPlanesAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  Present(std::move(completion_callback), std::move(presentation_callback),
          data);
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

bool Presenter::ScheduleDCLayer(std::unique_ptr<DCLayerOverlayParams> params) {
  NOTIMPLEMENTED();
  return false;
}

bool Presenter::Resize(const gfx::Size& size,
                       float scale_factor,
                       const gfx::ColorSpace& color_space,
                       bool has_alpha) {
  return true;
}

bool Presenter::Initialize(GLSurfaceFormat format) {
  return true;
}

bool Presenter::OnMakeCurrent(GLContext* context) {
  return true;
}

}  // namespace gl