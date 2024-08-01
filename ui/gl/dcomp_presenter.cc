// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dcomp_presenter.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/vsync_thread_win.h"

namespace gl {

DCompPresenter::PendingFrame::PendingFrame(
    Microsoft::WRL::ComPtr<ID3D11Query> query,
    PresentationCallback callback)
    : query(std::move(query)), callback(std::move(callback)) {}
DCompPresenter::PendingFrame::PendingFrame(PendingFrame&& other) = default;
DCompPresenter::PendingFrame::~PendingFrame() = default;
DCompPresenter::PendingFrame& DCompPresenter::PendingFrame::operator=(
    PendingFrame&& other) = default;

DCompPresenter::DCompPresenter(const Settings& settings)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      layer_tree_(std::make_unique<DCLayerTree>(
          settings.disable_nv12_dynamic_textures,
          settings.disable_vp_auto_hdr,
          settings.disable_vp_scaling,
          settings.disable_vp_super_resolution,
          settings.force_dcomp_triple_buffer_video_swap_chain,
          settings.no_downscaled_overlay_promotion)),
      use_gpu_vsync_(features::UseGpuVsync()) {
  CHECK(DirectCompositionSupported());
  d3d11_device_ = GetDirectCompositionD3D11Device();
  child_window_.Initialize();
  layer_tree_->Initialize(child_window_.window(), d3d11_device_);
}

DCompPresenter::~DCompPresenter() {
  Destroy();
}

void DCompPresenter::Destroy() {
  for (auto& frame : pending_frames_)
    std::move(frame.callback).Run(gfx::PresentationFeedback::Failure());
  pending_frames_.clear();

  if (observing_vsync_) {
    VSyncThreadWin::GetInstance()->RemoveObserver(this);
  }

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

bool DCompPresenter::Resize(const gfx::Size& size,
                            float scale_factor,
                            const gfx::ColorSpace& color_space,
                            bool has_alpha) {
  if (!Presenter::Resize(size, scale_factor, color_space, has_alpha)) {
    return false;
  }

  child_window_.Resize(size);
  return true;
}

gfx::VSyncProvider* DCompPresenter::GetVSyncProvider() {
  return VSyncThreadWin::GetInstance()->vsync_provider();
}

void DCompPresenter::OnVSync(base::TimeTicks vsync_time,
                             base::TimeDelta interval) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DCompPresenter::HandleVSyncOnMainThread,
                     weak_factory_.GetWeakPtr(), vsync_time, interval));
}

void DCompPresenter::ScheduleDCLayer(
    std::unique_ptr<DCLayerOverlayParams> params) {
  pending_overlays_.push_back(std::move(params));
}

void DCompPresenter::SetFrameRate(float frame_rate) {
  // Only try to reduce vsync frequency through the video swap chain.
  // This allows us to experiment UseSetPresentDuration optimization to
  // fullscreen video overlays only and avoid compromising
  // UsePreferredIntervalForVideo optimization where we skip compositing
  // every other frame when fps <= half the vsync frame rate.
  layer_tree_->SetFrameRate(frame_rate);
}

void DCompPresenter::Present(SwapCompletionCallback completion_callback,
                             PresentationCallback presentation_callback,
                             gfx::FrameData data) {
  TRACE_EVENT0("gpu", "DCompPresenter::Present");

  // Callback will be dequeued on next vsync.
  EnqueuePendingFrame(std::move(presentation_callback),
                      /*create_query=*/create_query_this_frame_);
  create_query_this_frame_ = false;

  if (!layer_tree_->CommitAndClearPendingOverlays(
          std::move(pending_overlays_))) {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    return;
  }

  std::move(completion_callback)
      .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
}

bool DCompPresenter::SupportsViewporter() const {
  return true;
}

bool DCompPresenter::SupportsDelegatedInk() {
  return layer_tree_->SupportsDelegatedInk();
}

void DCompPresenter::SetDelegatedInkTrailStartPoint(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  layer_tree_->SetDelegatedInkTrailStartPoint(std::move(metadata));
}

void DCompPresenter::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  layer_tree_->InitDelegatedInkPointRendererReceiver(
      std::move(pending_receiver));
}

scoped_refptr<base::TaskRunner>
DCompPresenter::GetWindowTaskRunnerForTesting() {
  return child_window_.GetTaskRunnerForTesting();  // IN-TEST
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DCompPresenter::GetLayerSwapChainForTesting(size_t index) const {
  return layer_tree_->GetLayerSwapChainForTesting(index);  // IN-TEST
}

void DCompPresenter::GetSwapChainVisualInfoForTesting(
    size_t index,
    gfx::Transform* transform,
    gfx::Point* offset,
    gfx::Rect* clip_rect) const {
  layer_tree_->GetSwapChainVisualInfoForTesting(  // IN-TEST
      index, transform, offset, clip_rect);
}

void DCompPresenter::HandleVSyncOnMainThread(base::TimeTicks vsync_time,
                                             base::TimeDelta interval) {
  last_vsync_time_ = vsync_time;
  last_vsync_interval_ = interval;
  CheckPendingFrames();
}

void DCompPresenter::StartOrStopVSyncThread() {
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

void DCompPresenter::CheckPendingFrames() {
  TRACE_EVENT1("gpu", "DCompPresenter::CheckPendingFrames",
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

  if (use_gpu_vsync_) {
    StartOrStopVSyncThread();
  }
}

void DCompPresenter::EnqueuePendingFrame(PresentationCallback callback,
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

  if (use_gpu_vsync_) {
    StartOrStopVSyncThread();
  } else {
    last_vsync_time_ = base::TimeTicks::Now();
    last_vsync_interval_ = VSyncThreadWin::GetInstance()->GetVsyncInterval();
    // Handle pending frames asynchronously to avoid reentrancy issues in the
    // caller.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&DCompPresenter::CheckPendingFrames,
                                          weak_factory_.GetWeakPtr()));
  }
}

HWND DCompPresenter::GetWindow() const {
  return child_window_.window();
}

}  // namespace gl
