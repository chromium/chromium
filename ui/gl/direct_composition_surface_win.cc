// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/direct_composition_surface_win.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/direct_composition_support.h"

namespace gl {

DirectCompositionSurfaceWin::DirectCompositionSurfaceWin(
    GLDisplayEGL* display,
    HWND parent_window,
    VSyncCallback vsync_callback,
    const Settings& settings)
    : GLSurfaceEGL(display),
      child_window_(parent_window),
      root_surface_(new DirectCompositionChildSurfaceWin(
          display,
          std::move(vsync_callback),
          settings.use_angle_texture_offset,
          settings.max_pending_frames,
          settings.force_root_surface_full_damage,
          settings.force_root_surface_full_damage_always)),
      layer_tree_(std::make_unique<DCLayerTree>(
          settings.disable_nv12_dynamic_textures,
          settings.disable_vp_scaling,
          settings.disable_vp_super_resolution,
          settings.no_downscaled_overlay_promotion)) {}

DirectCompositionSurfaceWin::~DirectCompositionSurfaceWin() {
  Destroy();
}

bool DirectCompositionSurfaceWin::Initialize(GLSurfaceFormat format) {
  if (!DirectCompositionSupported()) {
    DLOG(ERROR) << "Direct composition not supported";
    return false;
  }

  child_window_.Initialize();

  window_ = child_window_.window();

  if (!layer_tree_->Initialize(window_))
    return false;

  if (!root_surface_->Initialize(GLSurfaceFormat()))
    return false;

  return true;
}

void DirectCompositionSurfaceWin::Destroy() {
  root_surface_->Destroy();
  // Freeing DComp resources such as visuals and surfaces causes the
  // device to become 'dirty'. We must commit the changes to the device
  // in order for the objects to actually be destroyed.
  // Leaving the device in the dirty state for long periods of time means
  // that if DWM.exe crashes, the Chromium window will become black until
  // the next Commit.
  layer_tree_.reset();
  if (auto* dcomp_device = GetDirectCompositionDevice())
    dcomp_device->Commit();
}

gfx::Size DirectCompositionSurfaceWin::GetSize() {
  return root_surface_->GetSize();
}

bool DirectCompositionSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionSurfaceWin::GetHandle() {
  return root_surface_->GetHandle();
}

bool DirectCompositionSurfaceWin::Resize(const gfx::Size& size,
                                         float scale_factor,
                                         const gfx::ColorSpace& color_space,
                                         bool has_alpha) {
  // Force a resize and redraw (but not a move, activate, etc.).
  if (!SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                        SWP_NOOWNERZORDER | SWP_NOZORDER)) {
    return false;
  }
  return root_surface_->Resize(size, scale_factor, color_space, has_alpha);
}

gfx::SwapResult DirectCompositionSurfaceWin::SwapBuffers(
    PresentationCallback callback) {
  TRACE_EVENT0("gpu", "DirectCompositionSurfaceWin::SwapBuffers");

  if (root_surface_->SwapBuffers(std::move(callback)) !=
      gfx::SwapResult::SWAP_ACK)
    return gfx::SwapResult::SWAP_FAILED;

  if (!layer_tree_->CommitAndClearPendingOverlays(root_surface_.get()))
    return gfx::SwapResult::SWAP_FAILED;

  return gfx::SwapResult::SWAP_ACK;
}

gfx::SwapResult DirectCompositionSurfaceWin::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback) {
  // The arguments are ignored because SetDrawRectangle specified the area to
  // be swapped.
  return SwapBuffers(std::move(callback));
}

gfx::VSyncProvider* DirectCompositionSurfaceWin::GetVSyncProvider() {
  return root_surface_->GetVSyncProvider();
}

void DirectCompositionSurfaceWin::SetVSyncEnabled(bool enabled) {
  root_surface_->SetVSyncEnabled(enabled);
}

bool DirectCompositionSurfaceWin::ScheduleDCLayer(
    std::unique_ptr<ui::DCRendererLayerParams> params) {
  return layer_tree_->ScheduleDCLayer(std::move(params));
}

void DirectCompositionSurfaceWin::SetFrameRate(float frame_rate) {
  // Only try to reduce vsync frequency through the video swap chain.
  // This allows us to experiment UseSetPresentDuration optimization to
  // fullscreen video overlays only and avoid compromising
  // UsePreferredIntervalForVideo optimization where we skip compositing
  // every other frame when fps <= half the vsync frame rate.
  layer_tree_->SetFrameRate(frame_rate);
}

bool DirectCompositionSurfaceWin::SetEnableDCLayers(bool enable) {
  return root_surface_->SetEnableDCLayers(enable);
}

gfx::SurfaceOrigin DirectCompositionSurfaceWin::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

bool DirectCompositionSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionSurfaceWin::OnMakeCurrent(GLContext* context) {
  return root_surface_->OnMakeCurrent(context);
}

bool DirectCompositionSurfaceWin::SupportsDCLayers() const {
  return true;
}

bool DirectCompositionSurfaceWin::SupportsProtectedVideo() const {
  // TODO(magchen): Check the gpu driver date (or a function) which we know this
  // new support is enabled.
  return DirectCompositionOverlaysSupported();
}

bool DirectCompositionSurfaceWin::SetDrawRectangle(const gfx::Rect& rect) {
  return root_surface_->SetDrawRectangle(rect);
}

gfx::Vector2d DirectCompositionSurfaceWin::GetDrawOffset() const {
  return root_surface_->GetDrawOffset();
}

bool DirectCompositionSurfaceWin::SupportsGpuVSync() const {
  return true;
}

void DirectCompositionSurfaceWin::SetGpuVSyncEnabled(bool enabled) {
  root_surface_->SetGpuVSyncEnabled(enabled);
}

bool DirectCompositionSurfaceWin::SupportsDelegatedInk() {
  return layer_tree_->SupportsDelegatedInk();
}

void DirectCompositionSurfaceWin::SetDelegatedInkTrailStartPoint(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  layer_tree_->SetDelegatedInkTrailStartPoint(std::move(metadata));
}

void DirectCompositionSurfaceWin::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  layer_tree_->InitDelegatedInkPointRendererReceiver(
      std::move(pending_receiver));
}

scoped_refptr<base::TaskRunner>
DirectCompositionSurfaceWin::GetWindowTaskRunnerForTesting() {
  return child_window_.GetTaskRunnerForTesting();
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::GetLayerSwapChainForTesting(size_t index) const {
  return layer_tree_->GetLayerSwapChainForTesting(index);
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::GetBackbufferSwapChainForTesting() const {
  return root_surface_->swap_chain();
}

scoped_refptr<DirectCompositionChildSurfaceWin>
DirectCompositionSurfaceWin::GetRootSurfaceForTesting() const {
  return root_surface_;
}

void DirectCompositionSurfaceWin::GetSwapChainVisualInfoForTesting(
    size_t index,
    gfx::Transform* transform,
    gfx::Point* offset,
    gfx::Rect* clip_rect) const {
  layer_tree_->GetSwapChainVisualInfoForTesting(  // IN-TEST
      index, transform, offset, clip_rect);
}

}  // namespace gl
