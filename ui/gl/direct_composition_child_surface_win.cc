// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/direct_composition_child_surface_win.h"

#include <d3d11_1.h>
#include <dcomptypes.h>

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/win/windows_version.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/vsync_thread_win.h"

#ifndef EGL_ANGLE_flexible_surface_compatibility
#define EGL_ANGLE_flexible_surface_compatibility 1
#define EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6
#endif /* EGL_ANGLE_flexible_surface_compatibility */

#ifndef EGL_ANGLE_d3d_texture_client_buffer
#define EGL_ANGLE_d3d_texture_client_buffer 1
#define EGL_D3D_TEXTURE_ANGLE 0x33A3
#define EGL_TEXTURE_OFFSET_X_ANGLE 0x3490
#define EGL_TEXTURE_OFFSET_Y_ANGLE 0x3491
#endif /* EGL_ANGLE_d3d_texture_client_buffer */

namespace gl {

namespace {
// Only one DirectComposition surface can be rendered into at a time. Track
// here which IDCompositionSurface is being rendered into. If another context
// is made current, then this surface will be suspended.
IDCompositionSurface* g_current_surface = nullptr;

bool g_direct_composition_swap_chain_failed = false;

bool SupportsLowLatencyPresentation() {
  return base::FeatureList::IsEnabled(
      features::kDirectCompositionLowLatencyPresentation);
}
}  // namespace

DirectCompositionChildSurfaceWin::PendingFrame::PendingFrame(
    Microsoft::WRL::ComPtr<ID3D11Query> query,
    PresentationCallback callback)
    : query(std::move(query)), callback(std::move(callback)) {}
DirectCompositionChildSurfaceWin::PendingFrame::PendingFrame(
    PendingFrame&& other) = default;
DirectCompositionChildSurfaceWin::PendingFrame::~PendingFrame() = default;
DirectCompositionChildSurfaceWin::PendingFrame&
DirectCompositionChildSurfaceWin::PendingFrame::operator=(
    PendingFrame&& other) = default;

DirectCompositionChildSurfaceWin::DirectCompositionChildSurfaceWin(
    VSyncCallback vsync_callback,
    bool use_angle_texture_offset,
    size_t max_pending_frames,
    bool force_full_damage)
    : vsync_callback_(std::move(vsync_callback)),
      use_angle_texture_offset_(use_angle_texture_offset),
      max_pending_frames_(max_pending_frames),
      force_full_damage_(force_full_damage),
      vsync_thread_(VSyncThreadWin::GetInstance()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

DirectCompositionChildSurfaceWin::~DirectCompositionChildSurfaceWin() {
  Destroy();
}

bool DirectCompositionChildSurfaceWin::Initialize(GLSurfaceFormat format) {
  d3d11_device_ = QueryD3D11DeviceObjectFromANGLE();
  dcomp_device_ = QueryDirectCompositionDevice(d3d11_device_);
  if (!dcomp_device_)
    return false;

  EGLDisplay display = GetDisplay();

  EGLint pbuffer_attribs[] = {
      EGL_WIDTH,
      1,
      EGL_HEIGHT,
      1,
      EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE,
      EGL_TRUE,
      EGL_NONE,
  };

  default_surface_ =
      eglCreatePbufferSurface(display, GetConfig(), pbuffer_attribs);
  if (!default_surface_) {
    DLOG(ERROR) << "eglCreatePbufferSurface failed with error "
                << ui::GetLastEGLErrorString();
    return false;
  }

  return true;
}

bool DirectCompositionChildSurfaceWin::ReleaseDrawTexture(bool will_discard) {
  EGLSurface egl_surface = real_surface_;
  real_surface_ = nullptr;

  // We make current with the same surface (could be the parent), but its
  // handle has changed to |default_surface_|.
  gl::GLContext* context = gl::GLContext::GetCurrent();
  DCHECK(context);
  gl::GLSurface* surface = gl::GLSurface::GetCurrent();
  DCHECK(surface);
  bool result = context->MakeCurrent(surface);
  // If MakeCurrent fails (probably lost device), we'll want to return failure,
  // but we still want to reset the rest of the state for consistency.
  DLOG_IF(ERROR, !result) << "Failed to make current in ReleaseDrawTexture";

  if (egl_surface)
    eglDestroySurface(GetDisplay(), egl_surface);

  if (dcomp_surface_.Get() == g_current_surface)
    g_current_surface = nullptr;

  if (draw_texture_) {
    draw_texture_.Reset();
    if (dcomp_surface_) {
      TRACE_EVENT0("gpu", "DirectCompositionChildSurfaceWin::EndDraw");
      HRESULT hr = dcomp_surface_->EndDraw();
      if (FAILED(hr)) {
        DLOG(ERROR) << "EndDraw failed with error " << std::hex << hr;
        return false;
      }
      dcomp_surface_serial_++;
    } else if (!will_discard) {
      const bool use_swap_chain_tearing =
          DirectCompositionSurfaceWin::AllowTearing();
      UINT interval =
          first_swap_ || !vsync_enabled_ || use_swap_chain_tearing ? 0 : 1;
      UINT flags = use_swap_chain_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0;

      TRACE_EVENT2("gpu", "DirectCompositionChildSurfaceWin::PresentSwapChain",
                   "interval", interval, "dirty_rect",
                   force_full_damage_ ? "full_damage" : swap_rect_.ToString());
      HRESULT hr;
      if (force_full_damage_) {
        hr = swap_chain_->Present(interval, flags);
      } else {
        DXGI_PRESENT_PARAMETERS params = {};
        RECT dirty_rect = swap_rect_.ToRECT();
        params.DirtyRectsCount = 1;
        params.pDirtyRects = &dirty_rect;
        hr = swap_chain_->Present1(interval, flags, &params);
      }
      // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only
      // indicates that the window is occluded and we can stop rendering.
      if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
        DLOG(ERROR) << "Present1 failed with error " << std::hex << hr;
        return false;
      }

      Microsoft::WRL::ComPtr<IDXGISwapChainMedia> swap_chain_media;
      if (SUCCEEDED(swap_chain_.As(&swap_chain_media))) {
        DXGI_FRAME_STATISTICS_MEDIA stats = {};
        // GetFrameStatisticsMedia fails with
        // DXGI_ERROR_FRAME_STATISTICS_DISJOINT sometimes, which means an
        // event (such as power cycle) interrupted the gathering of
        // presentation statistics. In this situation, calling the function
        // again succeeds but returns with CompositionMode = NONE.
        // Waiting for the DXGI adapter to finish presenting before calling
        // the function doesn't get rid of the failure.
        if (SUCCEEDED(swap_chain_media->GetFrameStatisticsMedia(&stats))) {
          base::UmaHistogramSparse(
              "GPU.DirectComposition.CompositionMode.MainBuffer",
              stats.CompositionMode);
        }
      }

      if (first_swap_) {
        // Wait for the GPU to finish executing its commands before
        // committing the DirectComposition tree, or else the swapchain
        // may flicker black when it's first presented.
        first_swap_ = false;
        Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
        d3d11_device_.As(&dxgi_device2);
        DCHECK(dxgi_device2);
        base::WaitableEvent event(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED);
        hr = dxgi_device2->EnqueueSetEvent(event.handle());
        DCHECK(SUCCEEDED(hr));
        event.Wait();
      }
    }
  }
  return result;
}

void DirectCompositionChildSurfaceWin::Destroy() {
  for (auto& frame : pending_frames_)
    std::move(frame.callback).Run(gfx::PresentationFeedback::Failure());
  pending_frames_.clear();

  if (vsync_thread_started_)
    vsync_thread_->RemoveObserver(this);

  if (default_surface_) {
    if (!eglDestroySurface(GetDisplay(), default_surface_)) {
      DLOG(ERROR) << "eglDestroySurface failed with error "
                  << ui::GetLastEGLErrorString();
    }
    default_surface_ = nullptr;
  }
  if (real_surface_) {
    if (!eglDestroySurface(GetDisplay(), real_surface_)) {
      DLOG(ERROR) << "eglDestroySurface failed with error "
                  << ui::GetLastEGLErrorString();
    }
    real_surface_ = nullptr;
  }
  if (dcomp_surface_ && (dcomp_surface_.Get() == g_current_surface)) {
    HRESULT hr = dcomp_surface_->EndDraw();
    if (FAILED(hr))
      DLOG(ERROR) << "EndDraw failed with error " << std::hex << hr;
    g_current_surface = nullptr;
  }
  draw_texture_.Reset();
  dcomp_surface_.Reset();
}

gfx::Size DirectCompositionChildSurfaceWin::GetSize() {
  return size_;
}

bool DirectCompositionChildSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionChildSurfaceWin::GetHandle() {
  return real_surface_ ? real_surface_ : default_surface_;
}

gfx::SwapResult DirectCompositionChildSurfaceWin::SwapBuffers(
    PresentationCallback callback) {
  TRACE_EVENT1("gpu", "DirectCompositionChildSurfaceWin::SwapBuffers", "size",
               size_.ToString());

  gfx::SwapResult swap_result = ReleaseDrawTexture(false /* will_discard */)
                                    ? gfx::SwapResult::SWAP_ACK
                                    : gfx::SwapResult::SWAP_FAILED;
  EnqueuePendingFrame(std::move(callback));

  // Reset swap_rect_ since SetDrawRectangle may not be called when the root
  // damage rect is empty.
  swap_rect_ = gfx::Rect();

  return swap_result;
}

gfx::SurfaceOrigin DirectCompositionChildSurfaceWin::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

bool DirectCompositionChildSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionChildSurfaceWin::OnMakeCurrent(GLContext* context) {
  if (g_current_surface != dcomp_surface_.Get()) {
    if (g_current_surface) {
      HRESULT hr = g_current_surface->SuspendDraw();
      if (FAILED(hr)) {
        DLOG(ERROR) << "SuspendDraw failed with error " << std::hex << hr;
        return false;
      }
      g_current_surface = nullptr;
    }
    // We're in the middle of |dcomp_surface_| draw only if |draw_texture_| is
    // not null.
    if (dcomp_surface_ && draw_texture_) {
      HRESULT hr = dcomp_surface_->ResumeDraw();
      if (FAILED(hr)) {
        DLOG(ERROR) << "ResumeDraw failed with error " << std::hex << hr;
        return false;
      }
      g_current_surface = dcomp_surface_.Get();
    }
  }
  return true;
}

bool DirectCompositionChildSurfaceWin::SupportsDCLayers() const {
  return true;
}

bool DirectCompositionChildSurfaceWin::SetDrawRectangle(
    const gfx::Rect& rectangle) {
  if (!gfx::Rect(size_).Contains(rectangle)) {
    DLOG(ERROR) << "Draw rectangle must be contained within size of surface";
    return false;
  }

  if (draw_texture_) {
    DLOG(ERROR) << "SetDrawRectangle must be called only once per swap buffers";
    return false;
  }
  DCHECK(!real_surface_);
  DCHECK(!g_current_surface);

  if (gfx::Rect(size_) != rectangle && !swap_chain_ && !dcomp_surface_) {
    DLOG(ERROR) << "First draw to surface must draw to everything";
    return false;
  }

  gl::GLContext* context = gl::GLContext::GetCurrent();
  if (!context) {
    DLOG(ERROR) << "gl::GLContext::GetCurrent() returned nullptr";
    return false;
  }
  gl::GLSurface* surface = gl::GLSurface::GetCurrent();
  if (!surface) {
    DLOG(ERROR) << "gl::GLSurface::GetCurrent() returned nullptr";
    return false;
  }

  DXGI_FORMAT dxgi_format = gfx::ColorSpaceWin::GetDXGIFormat(color_space_);

  if (!dcomp_surface_ && enable_dc_layers_) {
    TRACE_EVENT2("gpu", "DirectCompositionChildSurfaceWin::CreateSurface",
                 "width", size_.width(), "height", size_.height());
    swap_chain_.Reset();
    // Always treat as premultiplied, because an underlay could cause it to
    // become transparent.
    HRESULT hr = dcomp_device_->CreateSurface(
        size_.width(), size_.height(), dxgi_format,
        DXGI_ALPHA_MODE_PREMULTIPLIED, &dcomp_surface_);
    base::UmaHistogramSparse("GPU.DirectComposition.DcompDeviceCreateSurface",
                             hr);
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateSurface failed with error " << std::hex << hr;
      // Disable direct composition because CreateSurface might fail again next
      // time.
      g_direct_composition_swap_chain_failed = true;
      return false;
    }
  } else if (!swap_chain_ && !enable_dc_layers_) {
    TRACE_EVENT2("gpu", "DirectCompositionChildSurfaceWin::CreateSwapChain",
                 "width", size_.width(), "height", size_.height());
    dcomp_surface_.Reset();

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    d3d11_device_.As(&dxgi_device);
    DCHECK(dxgi_device);
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    dxgi_device->GetAdapter(&dxgi_adapter);
    DCHECK(dxgi_adapter);
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
    dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
    DCHECK(dxgi_factory);

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = size_.width();
    desc.Height = size_.height();
    desc.Format = dxgi_format;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferCount = 2;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode =
        has_alpha_ ? DXGI_ALPHA_MODE_PREMULTIPLIED : DXGI_ALPHA_MODE_IGNORE;
    desc.Flags = DirectCompositionSurfaceWin::AllowTearing()
                     ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
                     : 0;
    HRESULT hr = dxgi_factory->CreateSwapChainForComposition(
        d3d11_device_.Get(), &desc, nullptr, &swap_chain_);
    first_swap_ = true;
    base::UmaHistogramSparse(
        "GPU.DirectComposition.CreateSwapChainForComposition", hr);

    // If CreateSwapChainForComposition fails, we cannot draw to the
    // browser window. Return false after disabling Direct Composition support
    // and let the Renderer handle it. Either the GPU command buffer or the GPU
    // process will be restarted.
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateSwapChainForComposition failed with error "
                  << std::hex << hr;
      // Disable direct composition because SwapChain creation might fail again
      // next time.
      g_direct_composition_swap_chain_failed = true;
      return false;
    }

    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;
    if (SUCCEEDED(swap_chain_.As(&swap_chain))) {
      hr = swap_chain->SetColorSpace1(
          gfx::ColorSpaceWin::GetDXGIColorSpace(color_space_));
      DCHECK(SUCCEEDED(hr))
          << "SetColorSpace1 failed with error " << std::hex << hr;
    }
  }

  swap_rect_ = rectangle;
  draw_offset_ = gfx::Vector2d();

  if (dcomp_surface_) {
    TRACE_EVENT0("gpu", "DirectCompositionChildSurfaceWin::BeginDraw");
    POINT update_offset;
    const RECT rect = rectangle.ToRECT();
    HRESULT hr = dcomp_surface_->BeginDraw(&rect, IID_PPV_ARGS(&draw_texture_),
                                           &update_offset);
    if (FAILED(hr)) {
      DLOG(ERROR) << "BeginDraw failed with error " << std::hex << hr;
      return false;
    }
    draw_offset_ = gfx::Point(update_offset) - rectangle.origin();
  } else {
    TRACE_EVENT0("gpu", "DirectCompositionChildSurfaceWin::GetBuffer");
    swap_chain_->GetBuffer(0, IID_PPV_ARGS(&draw_texture_));
  }
  DCHECK(draw_texture_);

  g_current_surface = dcomp_surface_.Get();

  std::vector<EGLint> pbuffer_attribs;
  pbuffer_attribs.push_back(EGL_WIDTH);
  pbuffer_attribs.push_back(size_.width());
  pbuffer_attribs.push_back(EGL_HEIGHT);
  pbuffer_attribs.push_back(size_.height());
  pbuffer_attribs.push_back(EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE);
  pbuffer_attribs.push_back(EGL_TRUE);
  if (use_angle_texture_offset_) {
    pbuffer_attribs.push_back(EGL_TEXTURE_OFFSET_X_ANGLE);
    pbuffer_attribs.push_back(draw_offset_.x());
    pbuffer_attribs.push_back(EGL_TEXTURE_OFFSET_Y_ANGLE);
    pbuffer_attribs.push_back(draw_offset_.y());
  }
  pbuffer_attribs.push_back(EGL_NONE);

  EGLClientBuffer buffer =
      reinterpret_cast<EGLClientBuffer>(draw_texture_.Get());
  real_surface_ = eglCreatePbufferFromClientBuffer(
      GetDisplay(), EGL_D3D_TEXTURE_ANGLE, buffer, GetConfig(),
      pbuffer_attribs.data());
  if (!real_surface_) {
    DLOG(ERROR) << "eglCreatePbufferFromClientBuffer failed with error "
                << ui::GetLastEGLErrorString();
    return false;
  }

  // We make current with the same surface (could be the parent), but its
  // handle has changed to |real_surface_|.
  if (!context->MakeCurrent(surface)) {
    DLOG(ERROR) << "Failed to make current in SetDrawRectangle";
    return false;
  }

  return true;
}

void DirectCompositionChildSurfaceWin::SetDCompSurfaceForTesting(
    Microsoft::WRL::ComPtr<IDCompositionSurface> surface) {
  dcomp_surface_ = std::move(surface);
}

gfx::Vector2d DirectCompositionChildSurfaceWin::GetDrawOffset() const {
  return use_angle_texture_offset_ ? gfx::Vector2d() : draw_offset_;
}

void DirectCompositionChildSurfaceWin::SetVSyncEnabled(bool enabled) {
  vsync_enabled_ = enabled;
}

bool DirectCompositionChildSurfaceWin::Resize(
    const gfx::Size& size,
    float scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha) {
  if (size_ == size && has_alpha_ == has_alpha && color_space_ == color_space)
    return true;

  // This will release indirect references to swap chain (|real_surface_|) by
  // binding |default_surface_| as the default framebuffer.
  if (!ReleaseDrawTexture(true /* will_discard */))
    return false;

  bool resize_only = has_alpha_ == has_alpha && color_space_ == color_space;

  size_ = size;
  color_space_ = color_space;
  has_alpha_ = has_alpha;

  // ResizeBuffers can't change alpha blending mode.
  if (swap_chain_ && resize_only) {
    DXGI_FORMAT format = gfx::ColorSpaceWin::GetDXGIFormat(color_space_);
    UINT flags = DirectCompositionSurfaceWin::AllowTearing()
                     ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
                     : 0;
    HRESULT hr = swap_chain_->ResizeBuffers(2 /* BufferCount */, size.width(),
                                            size.height(), format, flags);
    UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.SwapChainResizeResult",
                          SUCCEEDED(hr));
    if (SUCCEEDED(hr))
      return true;
    DLOG(ERROR) << "ResizeBuffers failed with error 0x" << std::hex << hr;
  }
  // Next SetDrawRectangle call will recreate the swap chain or surface.
  swap_chain_.Reset();
  dcomp_surface_.Reset();
  return true;
}

bool DirectCompositionChildSurfaceWin::SetEnableDCLayers(bool enable) {
  if (enable_dc_layers_ == enable)
    return true;
  enable_dc_layers_ = enable;
  if (!ReleaseDrawTexture(true /* will_discard */))
    return false;
  // Next SetDrawRectangle call will recreate the swap chain or surface.
  swap_chain_.Reset();
  dcomp_surface_.Reset();
  return true;
}

gfx::VSyncProvider* DirectCompositionChildSurfaceWin::GetVSyncProvider() {
  return vsync_thread_->vsync_provider();
}

bool DirectCompositionChildSurfaceWin::SupportsGpuVSync() const {
  return true;
}

void DirectCompositionChildSurfaceWin::SetGpuVSyncEnabled(bool enabled) {
  {
    base::AutoLock auto_lock(vsync_callback_enabled_lock_);
    vsync_callback_enabled_ = enabled;
  }
  StartOrStopVSyncThread();
}

void DirectCompositionChildSurfaceWin::OnVSync(base::TimeTicks vsync_time,
                                               base::TimeDelta interval) {
  // Main thread will run vsync callback in low latency presentation mode.
  if (VSyncCallbackEnabled() && !SupportsLowLatencyPresentation()) {
    DCHECK(vsync_callback_);
    vsync_callback_.Run(vsync_time, interval);
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DirectCompositionChildSurfaceWin::HandleVSyncOnMainThread,
                     weak_factory_.GetWeakPtr(), vsync_time, interval));
}

void DirectCompositionChildSurfaceWin::HandleVSyncOnMainThread(
    base::TimeTicks vsync_time,
    base::TimeDelta interval) {
  last_vsync_time_ = vsync_time;
  last_vsync_interval_ = interval;

  CheckPendingFrames();

  UMA_HISTOGRAM_COUNTS_100("GPU.DirectComposition.NumPendingFrames",
                           pending_frames_.size());

  if (SupportsLowLatencyPresentation() && VSyncCallbackEnabled() &&
      pending_frames_.size() < max_pending_frames_) {
    DCHECK(vsync_callback_);
    vsync_callback_.Run(vsync_time, interval);
  }
}

void DirectCompositionChildSurfaceWin::StartOrStopVSyncThread() {
  bool start_vsync_thread = VSyncCallbackEnabled() || !pending_frames_.empty();
  if (vsync_thread_started_ == start_vsync_thread)
    return;
  vsync_thread_started_ = start_vsync_thread;
  if (start_vsync_thread) {
    vsync_thread_->AddObserver(this);
  } else {
    vsync_thread_->RemoveObserver(this);
  }
}

bool DirectCompositionChildSurfaceWin::VSyncCallbackEnabled() const {
  base::AutoLock auto_lock(vsync_callback_enabled_lock_);
  return vsync_callback_enabled_;
}

void DirectCompositionChildSurfaceWin::CheckPendingFrames() {
  TRACE_EVENT1("gpu", "DirectCompositionChildSurfaceWin::CheckPendingFrames",
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

void DirectCompositionChildSurfaceWin::EnqueuePendingFrame(
    PresentationCallback callback) {
  Microsoft::WRL::ComPtr<ID3D11Query> query;

  // Do not create query for empty damage so that 3D engine is not used when
  // only presenting video in overlay. Callback will be dequeued on next vsync.
  if (!swap_rect_.IsEmpty()) {
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

// static
bool DirectCompositionChildSurfaceWin::IsDirectCompositionSwapChainFailed() {
  return g_direct_composition_swap_chain_failed;
}

}  // namespace gl
