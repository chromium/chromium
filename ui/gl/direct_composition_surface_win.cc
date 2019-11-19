// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/direct_composition_surface_win.h"

#include <dxgi1_6.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/win/windows_version.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_surface_presentation_helper.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/vsync_thread_win.h"

#ifndef EGL_ANGLE_flexible_surface_compatibility
#define EGL_ANGLE_flexible_surface_compatibility 1
#define EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6
#endif /* EGL_ANGLE_flexible_surface_compatibility */

namespace gl {
namespace {
// Indicates support for either NV12 or YUY2 hardware overlays.
bool g_supports_overlays = false;

// Used for workaround limiting overlay size to monitor size.
gfx::Size g_overlay_monitor_size;

// Preferred overlay format set when detecting hardware overlay support during
// initialization.  Set to NV12 by default so that it's used when enabling
// overlays using command line flags.
DXGI_FORMAT g_overlay_format_used = DXGI_FORMAT_NV12;

// These are the raw support info, which shouldn't depend on field trial state,
// or command line flags.
UINT g_nv12_overlay_support_flags = 0;
UINT g_yuy2_overlay_support_flags = 0;

bool FlagsSupportsOverlays(UINT flags) {
  return (flags & (DXGI_OVERLAY_SUPPORT_FLAG_DIRECT |
                   DXGI_OVERLAY_SUPPORT_FLAG_SCALING));
}

void InitializeHardwareOverlaySupport() {
  static bool overlay_support_initialized = false;
  if (overlay_support_initialized)
    return;
  overlay_support_initialized = true;

  // Check for DirectComposition support first to prevent likely crashes.
  if (!DirectCompositionSurfaceWin::IsDirectCompositionSupported())
    return;

  // Before Windows 10 Anniversary Update (Redstone 1), overlay planes wouldn't
  // be assigned to non-UWP apps.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Failed to retrieve D3D11 device";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device.As(&dxgi_device))) {
    DLOG(ERROR) << "Failed to retrieve DXGI device";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter))) {
    DLOG(ERROR) << "Failed to retrieve DXGI adapter";
    return;
  }

  // This will fail if the D3D device is "Microsoft Basic Display Adapter".
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  if (FAILED(d3d11_device.As(&video_device))) {
    DLOG(ERROR) << "Failed to retrieve video device";
    return;
  }

  unsigned int i = 0;
  while (true) {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(dxgi_adapter->EnumOutputs(i++, &output)))
      break;
    DCHECK(output);
    Microsoft::WRL::ComPtr<IDXGIOutput3> output3;
    if (FAILED(output.As(&output3)))
      continue;
    DCHECK(output3);
    output3->CheckOverlaySupport(DXGI_FORMAT_NV12, d3d11_device.Get(),
                                 &g_nv12_overlay_support_flags);
    output3->CheckOverlaySupport(DXGI_FORMAT_YUY2, d3d11_device.Get(),
                                 &g_yuy2_overlay_support_flags);
    if (FlagsSupportsOverlays(g_nv12_overlay_support_flags) &&
        base::FeatureList::IsEnabled(
            features::kDirectCompositionPreferNV12Overlays)) {
      // NV12 format is preferred if it's supported.

      // Per Intel's request, use NV12 only when
      // COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 is also supported. Rec 709 is
      // commonly used for H.264 and HEVC. At least one Intel Gen9 SKU will not
      // support NV12 overlays.
      UINT color_space_support_flags = 0;
      Microsoft::WRL::ComPtr<IDXGIOutput4> output4;
      if (SUCCEEDED(output.As(&output4)) &&
          SUCCEEDED(output4->CheckOverlayColorSpaceSupport(
              DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
              d3d11_device.Get(), &color_space_support_flags)) &&
          (color_space_support_flags &
           DXGI_OVERLAY_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
        // Some new Intel drivers only claim to support unscaled overlays, but
        // scaled overlays still work. It's possible DWM works around it by
        // performing an extra scaling Blt before calling the driver. Even when
        // scaled overlays aren't actually supported, presentation using the
        // overlay path should be relatively efficient.
        g_overlay_format_used = DXGI_FORMAT_NV12;
        g_supports_overlays = true;
      }
    }
    if (!g_supports_overlays &&
        FlagsSupportsOverlays(g_yuy2_overlay_support_flags)) {
      // If NV12 isn't supported, fallback to YUY2 if it's supported.
      g_overlay_format_used = DXGI_FORMAT_YUY2;
      g_supports_overlays = true;
    }
    if (g_supports_overlays) {
      DXGI_OUTPUT_DESC monitor_desc = {};
      if (SUCCEEDED(output3->GetDesc(&monitor_desc))) {
        g_overlay_monitor_size =
            gfx::Rect(monitor_desc.DesktopCoordinates).size();
      }
    }
    // Early out after the first output that reports overlay support. All
    // outputs are expected to report the same overlay support according to
    // Microsoft's WDDM documentation:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/display/multiplane-overlay-hardware-requirements
    // TODO(sunnyps): If the above is true, then we can only look at first
    // output instead of iterating over all outputs.
    if (g_supports_overlays)
      break;
  }
  if (g_supports_overlays) {
    base::UmaHistogramSparse("GPU.DirectComposition.OverlayFormatUsed3",
                             g_overlay_format_used);
  }
  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.OverlaysSupported",
                        g_supports_overlays);
}

bool SupportsPresentationFeedback() {
  return base::FeatureList::IsEnabled(
             features::kDirectCompositionPresentationFeedback) &&
         base::FeatureList::IsEnabled(features::kDirectCompositionGpuVSync);
}

bool SupportsLowLatencyPresentation() {
  return base::FeatureList::IsEnabled(
             features::kDirectCompositionLowLatencyPresentation) &&
         SupportsPresentationFeedback();
}
}  // namespace

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
    std::unique_ptr<gfx::VSyncProvider> vsync_provider,
    VSyncCallback vsync_callback,
    HWND parent_window,
    const Settings& settings)
    : GLSurfaceEGL(),
      child_window_(parent_window),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      root_surface_(new DirectCompositionChildSurfaceWin()),
      layer_tree_(std::make_unique<DCLayerTree>(
          settings.disable_nv12_dynamic_textures,
          settings.disable_larger_than_screen_overlays,
          settings.disable_vp_scaling)),
      presentation_helper_(
          std::make_unique<GLSurfacePresentationHelper>(vsync_provider.get())),
      vsync_provider_(std::move(vsync_provider)),
      vsync_callback_(std::move(vsync_callback)),
      max_pending_frames_(settings.max_pending_frames) {
  // Call GetWeakPtr() on main thread before calling on vsync thread so that the
  // internal weak reference is initialized in a thread-safe way.
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

DirectCompositionSurfaceWin::~DirectCompositionSurfaceWin() {
  Destroy();
}

// static
bool DirectCompositionSurfaceWin::IsDirectCompositionSupported() {
  static const bool supported = [] {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kDisableDirectComposition))
      return false;

    // Blacklist direct composition if MCTU.dll or MCTUX.dll are injected. These
    // are user mode drivers for display adapters from Magic Control Technology
    // Corporation.
    if (GetModuleHandle(TEXT("MCTU.dll")) ||
        GetModuleHandle(TEXT("MCTUX.dll"))) {
      DLOG(ERROR) << "Blacklisted due to third party modules";
      return false;
    }

    // Flexible surface compatibility is required to be able to MakeCurrent with
    // the default pbuffer surface.
    if (!GLSurfaceEGL::IsEGLFlexibleSurfaceCompatibilitySupported()) {
      DLOG(ERROR) << "EGL_ANGLE_flexible_surface_compatibility not supported";
      return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        QueryD3D11DeviceObjectFromANGLE();
    if (!d3d11_device) {
      DLOG(ERROR) << "Failed to retrieve D3D11 device";
      return false;
    }

    // This will fail if the D3D device is "Microsoft Basic Display Adapter".
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
    if (FAILED(d3d11_device.As(&video_device))) {
      DLOG(ERROR) << "Failed to retrieve video device";
      return false;
    }

    // This will fail if DirectComposition DLL can't be loaded.
    Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device =
        QueryDirectCompositionDevice(d3d11_device);
    if (!dcomp_device) {
      DLOG(ERROR) << "Failed to retrieve direct composition device";
      return false;
    }

    return true;
  }();
  return supported;
}

// static
bool DirectCompositionSurfaceWin::AreOverlaysSupported() {
  // Always initialize and record overlay support information irrespective of
  // command line flags.
  InitializeHardwareOverlaySupport();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Enable flag should be checked before the disable flag, so we could
  // overwrite GPU driver bug workarounds in testing.
  if (command_line->HasSwitch(switches::kEnableDirectCompositionVideoOverlays))
    return true;
  if (command_line->HasSwitch(switches::kDisableDirectCompositionVideoOverlays))
    return false;

  return g_supports_overlays;
}

// static
bool DirectCompositionSurfaceWin::IsDecodeSwapChainSupported() {
  if (base::FeatureList::IsEnabled(
          features::kDirectCompositionUseNV12DecodeSwapChain)) {
    InitializeHardwareOverlaySupport();
    return GetOverlayFormatUsed() == DXGI_FORMAT_NV12;
  }
  return false;
}

// static
void DirectCompositionSurfaceWin::DisableOverlays() {
  g_supports_overlays = false;
}

// static
bool DirectCompositionSurfaceWin::AreScaledOverlaysSupported() {
  InitializeHardwareOverlaySupport();
  if (g_overlay_format_used == DXGI_FORMAT_NV12)
    return !!(g_nv12_overlay_support_flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);
  DCHECK_EQ(DXGI_FORMAT_YUY2, g_overlay_format_used);
  return !!(g_yuy2_overlay_support_flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);
}

// static
UINT DirectCompositionSurfaceWin::GetOverlaySupportFlags(DXGI_FORMAT format) {
  InitializeHardwareOverlaySupport();
  if (format == DXGI_FORMAT_NV12)
    return g_nv12_overlay_support_flags;
  DCHECK_EQ(DXGI_FORMAT_YUY2, format);
  return g_yuy2_overlay_support_flags;
}

// static
gfx::Size DirectCompositionSurfaceWin::GetOverlayMonitorSize() {
  return g_overlay_monitor_size;
}

// static
DXGI_FORMAT DirectCompositionSurfaceWin::GetOverlayFormatUsed() {
  return g_overlay_format_used;
}

// static
void DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(
    bool supported) {
  InitializeHardwareOverlaySupport();
  if (supported) {
    g_nv12_overlay_support_flags |= DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_yuy2_overlay_support_flags |= DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
  } else {
    g_nv12_overlay_support_flags &= ~DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_yuy2_overlay_support_flags &= ~DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
  }
  DCHECK_EQ(supported, AreScaledOverlaysSupported());
}

// static
void DirectCompositionSurfaceWin::SetOverlayFormatUsedForTesting(
    DXGI_FORMAT format) {
  DCHECK(format == DXGI_FORMAT_NV12 || format == DXGI_FORMAT_YUY2);
  InitializeHardwareOverlaySupport();
  g_overlay_format_used = format;
  DCHECK_EQ(format, GetOverlayFormatUsed());
}

// static
bool DirectCompositionSurfaceWin::IsHDRSupported() {
  // HDR support was introduced in Windows 10 Creators Update.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS2)
    return false;

  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<IDXGIFactory> factory;
  hr = CreateDXGIFactory(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create DXGI factory.";
    return false;
  }

  bool hdr_monitor_found = false;
  for (UINT adapter_index = 0;; ++adapter_index) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = factory->EnumAdapters(adapter_index, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND)
      break;
    if (FAILED(hr)) {
      DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
      break;
    }

    for (UINT output_index = 0;; ++output_index) {
      Microsoft::WRL::ComPtr<IDXGIOutput> output;
      hr = adapter->EnumOutputs(output_index, &output);
      if (hr == DXGI_ERROR_NOT_FOUND)
        break;
      if (FAILED(hr)) {
        DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
        break;
      }

      Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
      hr = output->QueryInterface(IID_PPV_ARGS(&output6));
      if (FAILED(hr)) {
        DLOG(WARNING) << "IDXGIOutput6 is required for HDR detection.";
        continue;
      }

      DXGI_OUTPUT_DESC1 desc;
      if (FAILED(output6->GetDesc1(&desc))) {
        DLOG(ERROR) << "Unexpected error getting output descriptor.";
        continue;
      }

      base::UmaHistogramSparse("GPU.Output.ColorSpace", desc.ColorSpace);
      base::UmaHistogramSparse("GPU.Output.MaxLuminance", desc.MaxLuminance);

      if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
        hdr_monitor_found = true;
      }
    }
  }

  UMA_HISTOGRAM_BOOLEAN("GPU.Output.HDR", hdr_monitor_found);
  return hdr_monitor_found;
}

// static
bool DirectCompositionSurfaceWin::IsSwapChainTearingSupported() {
  static const bool supported = [] {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        QueryD3D11DeviceObjectFromANGLE();
    if (!d3d11_device) {
      DLOG(ERROR) << "Not using swap chain tearing because failed to retrieve "
                     "D3D11 device from ANGLE";
      return false;
    }
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    d3d11_device.As(&dxgi_device);
    DCHECK(dxgi_device);
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    dxgi_device->GetAdapter(&dxgi_adapter);
    DCHECK(dxgi_adapter);
    Microsoft::WRL::ComPtr<IDXGIFactory5> dxgi_factory;
    if (FAILED(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)))) {
      DLOG(ERROR) << "Not using swap chain tearing because failed to retrieve "
                     "IDXGIFactory5 interface";
      return false;
    }

    BOOL present_allow_tearing = FALSE;
    DCHECK(dxgi_factory);
    if (FAILED(dxgi_factory->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING, &present_allow_tearing,
            sizeof(present_allow_tearing)))) {
      DLOG(ERROR)
          << "Not using swap chain tearing because CheckFeatureSupport failed";
      return false;
    }
    return !!present_allow_tearing;
  }();
  return supported;
}

// static
bool DirectCompositionSurfaceWin::AllowTearing() {
  // Swap chain tearing is used only if vsync is disabled explicitly.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableGpuVsync) &&
         DirectCompositionSurfaceWin::IsSwapChainTearingSupported();
}

bool DirectCompositionSurfaceWin::Initialize(GLSurfaceFormat format) {
  d3d11_device_ = QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device_) {
    DLOG(ERROR) << "Failed to retrieve D3D11 device from ANGLE";
    return false;
  }

  dcomp_device_ = QueryDirectCompositionDevice(d3d11_device_);
  if (!dcomp_device_) {
    DLOG(ERROR)
        << "Failed to retrieve direct compostion device from D3D11 device";
    return false;
  }

  if (!child_window_.Initialize()) {
    DLOG(ERROR) << "Failed to initialize native window";
    return false;
  }
  window_ = child_window_.window();

  if (!layer_tree_->Initialize(window_, d3d11_device_, dcomp_device_))
    return false;

  if (!root_surface_->Initialize(GLSurfaceFormat()))
    return false;

  if ((SupportsGpuVSync() && vsync_callback_) || SupportsPresentationFeedback())
    vsync_thread_ = VSyncThreadWin::GetInstance();

  return true;
}

void DirectCompositionSurfaceWin::Destroy() {
  for (auto& frame : pending_frames_)
    std::move(frame.callback).Run(gfx::PresentationFeedback::Failure());
  pending_frames_.clear();

  if (vsync_thread_) {
    vsync_thread_->RemoveObserver(this);
    vsync_thread_ = nullptr;
  }
  // Destroy presentation helper first because its dtor calls GetHandle.
  presentation_helper_ = nullptr;
  root_surface_->Destroy();
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
                                         ColorSpace color_space,
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

  base::Optional<GLSurfacePresentationHelper::ScopedSwapBuffers>
      scoped_swap_buffers;
  if (!SupportsPresentationFeedback()) {
    scoped_swap_buffers.emplace(presentation_helper_.get(),
                                std::move(callback));
  }

  gfx::SwapResult swap_result;
  if (root_surface_->SwapBuffers(PresentationCallback()) ==
          gfx::SwapResult::SWAP_ACK &&
      layer_tree_->CommitAndClearPendingOverlays(root_surface_.get())) {
    swap_result = gfx::SwapResult::SWAP_ACK;
  } else {
    swap_result = gfx::SwapResult::SWAP_FAILED;
  }

  if (scoped_swap_buffers) {
    scoped_swap_buffers->set_result(swap_result);
  } else {
    EnqueuePendingFrame(std::move(callback));
  }

  return swap_result;
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
  return vsync_provider_.get();
}

void DirectCompositionSurfaceWin::SetVSyncEnabled(bool enabled) {
  root_surface_->SetVSyncEnabled(enabled);
}

bool DirectCompositionSurfaceWin::ScheduleDCLayer(
    const ui::DCRendererLayerParams& params) {
  return layer_tree_->ScheduleDCLayer(params);
}

bool DirectCompositionSurfaceWin::SetEnableDCLayers(bool enable) {
  return root_surface_->SetEnableDCLayers(enable);
}

bool DirectCompositionSurfaceWin::FlipsVertically() const {
  return true;
}

bool DirectCompositionSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionSurfaceWin::OnMakeCurrent(GLContext* context) {
  if (presentation_helper_)
    presentation_helper_->OnMakeCurrent(context, this);
  return root_surface_->OnMakeCurrent(context);
}

bool DirectCompositionSurfaceWin::SupportsDCLayers() const {
  return true;
}

bool DirectCompositionSurfaceWin::UseOverlaysForVideo() const {
  return AreOverlaysSupported();
}

bool DirectCompositionSurfaceWin::SupportsProtectedVideo() const {
  // TODO(magchen): Check the gpu driver date (or a function) which we know this
  // new support is enabled.
  return AreOverlaysSupported();
}

bool DirectCompositionSurfaceWin::SetDrawRectangle(const gfx::Rect& rectangle) {
  return root_surface_->SetDrawRectangle(rectangle);
}

gfx::Vector2d DirectCompositionSurfaceWin::GetDrawOffset() const {
  return root_surface_->GetDrawOffset();
}

bool DirectCompositionSurfaceWin::SupportsGpuVSync() const {
  return base::FeatureList::IsEnabled(features::kDirectCompositionGpuVSync);
}

void DirectCompositionSurfaceWin::SetGpuVSyncEnabled(bool enabled) {
  DCHECK(vsync_thread_);
  {
    base::AutoLock lock(vsync_callback_lock_);
    vsync_callback_enabled_ = enabled;
  }
  StartOrStopVSyncThread();
}

void DirectCompositionSurfaceWin::CheckPendingFrames() {
  DCHECK(SupportsPresentationFeedback());

  TRACE_EVENT1("gpu", "DirectCompositionSurfaceWin::CheckPendingFrames",
               "num_pending_frames", pending_frames_.size());

  if (pending_frames_.empty())
    return;

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(&context);
  while (!pending_frames_.empty()) {
    auto& frame = pending_frames_.front();
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
    PresentationCallback callback) {
  DCHECK(SupportsPresentationFeedback());

  Microsoft::WRL::ComPtr<ID3D11Query> query;
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

  pending_frames_.emplace_back(std::move(query), std::move(callback));

  StartOrStopVSyncThread();
}

bool DirectCompositionSurfaceWin::VSyncCallbackEnabled() const {
  base::AutoLock lock(vsync_callback_lock_);
  return vsync_callback_enabled_;
}

void DirectCompositionSurfaceWin::StartOrStopVSyncThread() {
  if (VSyncCallbackEnabled() || !pending_frames_.empty()) {
    vsync_thread_->AddObserver(this);
  } else {
    vsync_thread_->RemoveObserver(this);
  }
}

void DirectCompositionSurfaceWin::OnVSync(base::TimeTicks vsync_time,
                                          base::TimeDelta interval) {
  if (!SupportsLowLatencyPresentation() && VSyncCallbackEnabled()) {
    DCHECK(vsync_callback_);
    vsync_callback_.Run(vsync_time, interval);
  }

  if (SupportsPresentationFeedback()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DirectCompositionSurfaceWin::HandleVSyncOnMainThread,
                   weak_ptr_, vsync_time, interval));
  }
}

void DirectCompositionSurfaceWin::HandleVSyncOnMainThread(
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

}  // namespace gl
