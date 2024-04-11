// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/direct_composition_surface_win.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"

namespace gl {

DirectCompositionSurfaceWin::PendingFrame::PendingFrame(
    Microsoft::WRL::ComPtr<ID3D11Query> query,
    PresentationCallback callback)
    : query(std::move(query)), callback(std::move(callback)) {}
DirectCompositionSurfaceWin::PendingFrame::PendingFrame(PendingFrame&& other) =
    default;
DirectCompositionSurfaceWin::PendingFrame::~PendingFrame() = default;
DirectCompositionSurfaceWin::PendingFrame&
DirectCompositionSurfaceWin::PendingFrame::operator=(PendingFrame&& other) =
    default;

DirectCompositionSurfaceWin::DirectCompositionSurfaceWin(
    GLDisplayEGL* display,
    const Settings& settings)
    : GLSurfaceEGL(display),
      d3d11_device_(GetDirectCompositionD3D11Device()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      max_pending_frames_(settings.max_pending_frames),
      root_surface_(new DirectCompositionChildSurfaceWin(
          display,
          d3d11_device_,
          settings.use_angle_texture_offset)),
      layer_tree_(std::make_unique<DCLayerTree>(
          settings.disable_nv12_dynamic_textures,
          settings.disable_vp_auto_hdr,
          settings.disable_vp_scaling,
          settings.disable_vp_super_resolution,
          settings.force_dcomp_triple_buffer_video_swap_chain,
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

  layer_tree_->Initialize(window(), d3d11_device_);

  if (!root_surface_->Initialize(GLSurfaceFormat()))
    return false;

  return true;
}

void DirectCompositionSurfaceWin::Destroy() {
  for (auto& frame : pending_frames_)
    std::move(frame.callback).Run(gfx::PresentationFeedback::Failure());
  pending_frames_.clear();

  if (observing_vsync_) {
    VSyncThreadWin::GetInstance()->RemoveObserver(this);
  }

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
  if (!child_window_.Resize(size)) {
    return false;
  }
  return root_surface_->Resize(size, scale_factor, color_space, has_alpha);
}

gfx::SwapResult DirectCompositionSurfaceWin::SwapBuffers(
    PresentationCallback callback,
    gfx::FrameData data) {
  TRACE_EVENT0("gpu", "DirectCompositionSurfaceWin::SwapBuffers");

  gfx::Rect swap_rect;
  bool success = root_surface_->EndDraw(&swap_rect);
  // Do not create query for empty damage so that 3D engine is not used when
  // only presenting video in overlay. Callback will be dequeued on next vsync.
  EnqueuePendingFrame(std::move(callback),
                      /*create_query=*/!swap_rect.IsEmpty());
  if (!success)
    return gfx::SwapResult::SWAP_FAILED;

  Microsoft::WRL::ComPtr<IUnknown> root_visual_content;
  if (root_surface_->swap_chain()) {
    root_visual_content = root_surface_->swap_chain();
  } else {
    root_visual_content = root_surface_->dcomp_surface();
  }

  if (root_visual_content) {
    // Add a placeholder overlay for the root surface, at a z-order of 0.
    auto root_params = std::make_unique<DCLayerOverlayParams>();
    root_params->z_order = 0;
    root_params->overlay_image = std::make_optional<DCLayerOverlayImage>(
        root_surface_->GetSize(), std::move(root_visual_content),
        root_surface_->dcomp_surface_serial());
    root_params->content_rect = gfx::RectF(root_params->overlay_image->size());
    root_params->quad_rect = gfx::Rect(root_params->overlay_image->size());
    ScheduleDCLayer(std::move(root_params));
  }

  if (!layer_tree_->CommitAndClearPendingOverlays(
          std::move(pending_overlays_))) {
    return gfx::SwapResult::SWAP_FAILED;
  }

  return gfx::SwapResult::SWAP_ACK;
}

gfx::SwapResult DirectCompositionSurfaceWin::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback,
    gfx::FrameData data) {
  // The arguments are ignored because SetDrawRectangle specified the area to
  // be swapped.
  return SwapBuffers(std::move(callback), data);
}

gfx::VSyncProvider* DirectCompositionSurfaceWin::GetVSyncProvider() {
  return VSyncThreadWin::GetInstance()->vsync_provider();
}

void DirectCompositionSurfaceWin::SetVSyncEnabled(bool enabled) {
  root_surface_->SetVSyncEnabled(enabled);
}

void DirectCompositionSurfaceWin::OnVSync(base::TimeTicks vsync_time,
                                          base::TimeDelta interval) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DirectCompositionSurfaceWin::HandleVSyncOnMainThread,
                     weak_factory_.GetWeakPtr(), vsync_time, interval));
}

void DirectCompositionSurfaceWin::ScheduleDCLayer(
    std::unique_ptr<DCLayerOverlayParams> params) {
  pending_overlays_.push_back(std::move(params));
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

bool DirectCompositionSurfaceWin::SetDrawRectangle(const gfx::Rect& rect) {
  return root_surface_->SetDrawRectangle(rect);
}

gfx::Vector2d DirectCompositionSurfaceWin::GetDrawOffset() const {
  return root_surface_->GetDrawOffset();
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

void DirectCompositionSurfaceWin::HandleVSyncOnMainThread(
    base::TimeTicks vsync_time,
    base::TimeDelta interval) {
  last_vsync_time_ = vsync_time;
  last_vsync_interval_ = interval;
  CheckPendingFrames();
}

void DirectCompositionSurfaceWin::StartOrStopVSyncThread() {
  bool needs_vsync = !pending_frames_.empty();
  if (observing_vsync_ == needs_vsync) {
    return;
  }
  observing_vsync_ = needs_vsync;
  if (needs_vsync) {
    VSyncThreadWin::GetInstance()->AddObserver(this);
  } else {
    VSyncThreadWin::GetInstance()->RemoveObserver(this);
  }
}

void DirectCompositionSurfaceWin::CheckPendingFrames() {
  TRACE_EVENT1("gpu", "DirectCompositionSurfaceWin::CheckPendingFrames",
               "num_pending_frames", pending_frames_.size());

  if (pending_frames_.empty())
    return;

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(&context);
  while (!pending_frames_.empty()) {
    auto& frame = pending_frames_.front();
    // Query isn't created if there was no damage for previous frame.
    if (frame.query) {
      HRESULT hr = context->GetData(frame.query.Get(), nullptr, 0,
                                    D3D11_ASYNC_GETDATA_DONOTFLUSH);
      // When the GPU completes execution past the event query, GetData() will
      // return S_OK, and S_FALSE otherwise.  Do not use SUCCEEDED() because
      // S_FALSE is also a success code.
      if (hr != S_OK)
        break;
    }
    std::move(frame.callback)
        .Run(
            gfx::PresentationFeedback(last_vsync_time_, last_vsync_interval_,
                                      gfx::PresentationFeedback::kVSync |
                                          gfx::PresentationFeedback::kHWClock));
    pending_frames_.pop_front();
  }

  StartOrStopVSyncThread();
}

void DirectCompositionSurfaceWin::EnqueuePendingFrame(
    PresentationCallback callback,
    bool create_query) {
  Microsoft::WRL::ComPtr<ID3D11Query> query;
  if (create_query) {
    D3D11_QUERY_DESC desc = {};
    desc.Query = D3D11_QUERY_EVENT;
    HRESULT hr = d3d11_device_->CreateQuery(&desc, &query);
    if (SUCCEEDED(hr)) {
      Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
      d3d11_device_->GetImmediateContext(&context);
      context->End(query.Get());
      context->Flush();
    } else {
      DLOG(ERROR) << "CreateQuery failed with error 0x" << std::hex << hr;
    }
  }

  pending_frames_.emplace_back(std::move(query), std::move(callback));

  StartOrStopVSyncThread();
}

}  // namespace gl
