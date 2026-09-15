// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dcomp_presenter.h"

#include <winerror.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/dc_commit_error.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/vsync_thread_win.h"

namespace gl {

namespace {

constexpr std::string_view CommitErrorReasonToString(
    CommitError::Reason reason) {
  switch (reason) {
    case CommitError::Reason::kUnknown:
      return "Unknown";
    case CommitError::Reason::kIDCompositionDeviceCommit:
      return "IDCompositionDeviceCommit";
    case CommitError::Reason::kSolidColorSurfacePoolCreateSurface:
      return "SolidColorSurfacePoolCreateSurface";
    case CommitError::Reason::kSolidColorSurfaceBeginDraw:
      return "SolidColorSurfaceBeginDraw";
    case CommitError::Reason::kSolidColorSurfaceEndDraw:
      return "SolidColorSurfaceEndDraw";
    case CommitError::Reason::kSolidColorSurfaceCreateRenderTargetView:
      return "SolidColorSurfaceCreateRenderTargetView";
    case CommitError::Reason::kSolidColorTexturePoolCreateD3D12Resource:
      return "SolidColorTexturePoolCreateD3D12Resource";
    case CommitError::Reason::kSolidColorTexturePoolCreateSharedTextureMemory:
      return "SolidColorTexturePoolCreateSharedTextureMemory";
    case CommitError::Reason::kSolidColorTexturePoolCreateDawnSharedTexture:
      return "SolidColorTexturePoolCreateDawnSharedTexture";
    case CommitError::Reason::kSolidColorTexturePoolBeginAccess:
      return "SolidColorTexturePoolBeginAccess";
    case CommitError::Reason::kIDCompositionDevice6PresentCompositionTextures:
      return "IDCompositionDevice6PresentCompositionTextures";
    case CommitError::Reason::kInitializeVideoProcessorD3D11DeviceAsVideoDevice:
      return "InitializeVideoProcessorD3D11DeviceAsVideoDevice";
    case CommitError::Reason::
        kInitializeVideoProcessorCreateVideoProcessorEnumerator:
      return "InitializeVideoProcessorCreateVideoProcessorEnumerator";
    case CommitError::Reason::kInitializeVideoProcessorCreateVideoProcessor:
      return "InitializeVideoProcessorCreateVideoProcessor";
    case CommitError::Reason::kPresentToSwapChainCreateSurfaceFromHandle:
      return "PresentToSwapChainCreateSurfaceFromHandle";
    case CommitError::Reason::kPresentToSwapChainCreateVideoProcessorInputView:
      return "PresentToSwapChainCreateVideoProcessorInputView";
    case CommitError::Reason::kPresentToSwapChainCreateVideoProcessorOutputView:
      return "PresentToSwapChainCreateVideoProcessorOutputView";
    case CommitError::Reason::kPresentToSwapChainVideoProcessorBlt:
      return "PresentToSwapChainVideoProcessorBlt";
    case CommitError::Reason::
        kPresentToSwapChainCreateSwapChainForCompositionSurfaceHandle:
      return "PresentToSwapChainCreateSwapChainForCompositionSurfaceHandle";
    case CommitError::Reason::kPresentToSwapChainFirstPresent:
      return "PresentToSwapChainFirstPresent";
    case CommitError::Reason::kPresentToSwapChainPresentBuffer:
      return "PresentToSwapChainPresentBuffer";
    case CommitError::Reason::kPresentToSwapChainPresent:
      return "PresentToSwapChainPresent";
    case CommitError::Reason::kPresentToSwapChainSdrRevertMissingVideoProcessor:
      return "PresentToSwapChainSdrRevertMissingVideoProcessor";
    case CommitError::Reason::
        kPresentToSwapChainSdrRevertCreateVideoProcessorOutputView:
      return "PresentToSwapChainSdrRevertCreateVideoProcessorOutputView";
    case CommitError::Reason::kPresentToSwapChainSdrRevertSetColorSpace:
      return "PresentToSwapChainSdrRevertSetColorSpace";
    case CommitError::Reason::kUploadVideoImageInvalidPixmapData:
      return "UploadVideoImageInvalidPixmapData";
    case CommitError::Reason::kUploadVideoImageInvalidPixmapSize:
      return "UploadVideoImageInvalidPixmapSize";
    case CommitError::Reason::kUploadVideoImageInvalidPixmapStride:
      return "UploadVideoImageInvalidPixmapStride";
    case CommitError::Reason::kUploadVideoImageCreateStagingTexture:
      return "UploadVideoImageCreateStagingTexture";
    case CommitError::Reason::kUploadVideoImageMapStagingTexture:
      return "UploadVideoImageMapStagingTexture";
    case CommitError::Reason::kUploadVideoImageCreateCopyTexture:
      return "UploadVideoImageCreateCopyTexture";
  }
}

constexpr bool CommitErrorReasonStringsFitInCrashKey() {
  // Leave room for the NUL byte in the crash key value.
  constexpr size_t kMaxLength =
      static_cast<size_t>(base::debug::CrashKeySize::Size64) - 1;
  for (int i = 0; i <= static_cast<int>(CommitError::Reason::kMaxValue); i++) {
    if (CommitErrorReasonToString(static_cast<CommitError::Reason>(i)).size() >
        kMaxLength) {
      return false;
    }
  }
  return true;
}

static_assert(CommitErrorReasonStringsFitInCrashKey(),
              "Commit error reason strings must fit in a Size64 crash key.");

}  // namespace

DCompPresenter::PendingFrame::PendingFrame(PresentationCallback callback)
    : callback(std::move(callback)), creation_time(base::TimeTicks::Now()) {}
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
          settings.disable_dc_letterbox_video_optimization,
          settings.force_dcomp_triple_buffer_video_swap_chain,
          settings.no_downscaled_overlay_promotion)),
      use_gpu_vsync_(features::UseGpuVsync()) {
  CHECK(DirectCompositionSupported());
  d3d11_device_ = GetDirectCompositionD3D11Device();
  child_window_.Initialize();
  layer_tree_->Initialize(child_window_.window(), d3d11_device_);
}

DCompPresenter::~DCompPresenter() {
  for (auto& frame : pending_frames_)
    std::move(frame.callback).Run(gfx::PresentationFeedback::Failure());
  pending_frames_.clear();

  if (observing_vsync_) {
    VSyncThreadWin::GetInstance()->RemoveObserver(this);
  }
}

bool DCompPresenter::DestroyDCLayerTree() {
  CHECK(layer_tree_);

  // Freeing DComp resources such as visuals and surfaces causes the device to
  // become 'dirty'. We must commit the changes to the device in order for the
  // objects to actually be destroyed.
  // Leaving the device in the dirty state for long periods of time means that
  // if DWM.exe crashes, the Chromium window will become black until the next
  // Commit.
  layer_tree_.reset();
  if (auto* dcomp_device = GetDirectCompositionDevice()) {
    HRESULT hr = dcomp_device->Commit();
    if (FAILED(hr)) {
      return false;
    }
  }

  return true;
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

void DCompPresenter::ScheduleDCLayers(
    std::vector<DCLayerOverlayParams> overlays) {
  // We expect alternating calls to `ScheduleDCLayers` and `Present`.
  DCHECK_EQ(0u, pending_overlays_.size());
  pending_overlays_ = std::move(overlays);
}

void DCompPresenter::Present(SwapCompletionCallback completion_callback,
                             PresentationCallback presentation_callback,
                             gfx::FrameData data) {
  TRACE_EVENT0("gpu", "DCompPresenter::Present");

  // Callback will be dequeued on next vsync.
  EnqueuePendingFrame(std::move(presentation_callback));

  base::expected<void, CommitError> result =
      layer_tree_->CommitAndClearPendingOverlays(std::move(pending_overlays_));
  if (!result.has_value()) {
    const HRESULT device_removed_reason =
        gl::GetDirectCompositionD3D11Device()->GetDeviceRemovedReason();
    const bool not_device_removed = SUCCEEDED(device_removed_reason);
    if (not_device_removed && result.error().hr != PRESENTATION_ERROR_LOST) {
      SCOPED_CRASH_KEY_STRING64(
          "gpu", "DCompPresenter.SWAP_FAILED.reason",
          CommitErrorReasonToString(result.error().reason));
      SCOPED_CRASH_KEY_STRING32(
          "gpu", "DCompPresenter.SWAP_FAILED.hr?",
          result.error()
              .hr.transform([](HRESULT hr) { return base::NumberToString(hr); })
              .value_or("nullopt"));
      base::debug::DumpWithoutCrashing();
    } else {
      // Ignore device removed cases as they don't usually indicate a problem
      // originating from viz. `PRESENTATION_ERROR_LOST` usually happens when
      // device removed is caught internally in DWM when using DComp textures.
    }

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
DCompPresenter::GetLayerSwapChainForTesting(
    const gfx::OverlayLayerId& layer_id) const {
  return layer_tree_->GetLayerSwapChainForTesting(layer_id);  // IN-TEST
}

void DCompPresenter::GetSwapChainVisualInfoForTesting(
    const gfx::OverlayLayerId& layer_id,
    gfx::Transform* out_transform,
    gfx::Point* out_offset,
    gfx::Rect* out_clip_rect) const {
  layer_tree_->GetSwapChainVisualInfoForTesting(  // IN-TEST
      layer_id, out_transform, out_offset, out_clip_rect);
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


  while (!pending_frames_.empty()) {
    auto& frame = pending_frames_.front();
    // Break from the loop if we encounter a pending frame that was enqueued
    // after the last vsync since these cannot possibly be completed before
    // they were enqueued. We will process them on subsequent vsyncs.
    if (frame.creation_time > last_vsync_time_) {
      break;
    }
    // TODO: crbug.com/513024708 - reintroduce D3D11 query based GPU work
    // tracking so we mark a frame as presented only after its GPU work is done.
    std::move(frame.callback)
        .Run(
            gfx::PresentationFeedback(last_vsync_time_, last_vsync_interval_,
                                      gfx::PresentationFeedback::kVSync));
    pending_frames_.pop_front();
  }

  if (use_gpu_vsync_) {
    StartOrStopVSyncThread();
  }
}

void DCompPresenter::EnqueuePendingFrame(PresentationCallback callback) {
  pending_frames_.emplace_back(std::move(callback));

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
