// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/swap_chain_presenter.h"

#include <d3d11_1.h>
#include <d3d11_4.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gl/dc_layer_overlay_image.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/debug_utils.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/hdr_metadata_helper_win.h"

namespace gl {
namespace {

// When in BGRA8888 overlay format, wait for this time delta before retrying
// YUV format.
constexpr base::TimeDelta kDelayForRetryingYUVFormat = base::Minutes(10);

// TODO(crbug.com/397907161): When this feature is enabled, it will cause
// `AdjustTargetForFullScreenLetterboxing` to return `dest_size` and
// `target_rect` in terms of the unscaled video rect. This lets DWM scale up the
// video (via the visual transform) rather than allocating a swap chain at the
// target size and letting VP BLT do the scaling. Ensure that this does not
// break DWM optimizations for MF fullscreen letterboxing in
// `PresentDCOMPSurface`. These optimizations require `dest_size` to match the
// monitor size in order for MF to handle fullscreen letterboxing of videos.
BASE_FEATURE(kDisableVPBLTUpscale, base::FEATURE_DISABLED_BY_DEFAULT);

// Limit the video swap chain size that we request from Media Foundation, opting
// to do upscaling via DWM, rather than VPBLT.
//
// This is necessary in the case of very large onscreen size (particularly with
// scaled up videos that are clipped), where large MF swap chain sizes can
// negatively affect performance and memory usage.
BASE_FEATURE(kLimitMFSwapChainSize, base::FEATURE_ENABLED_BY_DEFAULT);

// This flag attempts to enable MPO for P010 SDR video content. The feature
// should only be enabled when P010 MPO is detected as supported.
BASE_FEATURE(kP010MPOForSDR, base::FEATURE_ENABLED_BY_DEFAULT);

gfx::ColorSpace GetOutputColorSpace(const gfx::ColorSpace& input_color_space,
                                    bool is_yuv_swapchain) {
  gfx::ColorSpace output_color_space =
      is_yuv_swapchain ? input_color_space : gfx::ColorSpace::CreateSRGB();
  if (input_color_space.IsHDR()) {
    output_color_space = gfx::ColorSpace::CreateHDR10();
  }

  return output_color_space;
}

bool IsProtectedVideo(gfx::ProtectedVideoType protected_video_type) {
  return protected_video_type != gfx::ProtectedVideoType::kClear;
}

const char* ProtectedVideoTypeToString(gfx::ProtectedVideoType type) {
  switch (type) {
    case gfx::ProtectedVideoType::kClear:
      return "Clear";
    case gfx::ProtectedVideoType::kSoftwareProtected:
      if (DirectCompositionOverlaysSupported())
        return "SoftwareProtected.HasOverlaySupport";
      else
        return "SoftwareProtected.NoOverlaySupport";
    case gfx::ProtectedVideoType::kHardwareProtected:
      return "HardwareProtected";
  }
}

base::win::ScopedHandle CreateDCompSurfaceHandle() {
  HANDLE handle = INVALID_HANDLE_VALUE;
  const HRESULT hr = ::DCompositionCreateSurfaceHandle(
      COMPOSITIONOBJECT_ALL_ACCESS, nullptr, &handle);
  CHECK_EQ(hr, S_OK);
  return base::win::ScopedHandle(handle);
}

const char* DxgiFormatToString(DXGI_FORMAT format) {
  // Please also modify histogram enum and trace integration tests if new
  // formats are added.
  switch (format) {
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return "RGB10A2";
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return "BGRA";
    case DXGI_FORMAT_YUY2:
      return "YUY2";
    case DXGI_FORMAT_NV12:
      return "NV12";
    case DXGI_FORMAT_P010:
      return "P010";
    default:
      NOTREACHED();
  }
}

bool IsYUVSwapChainFormat(DXGI_FORMAT format) {
  if (format == DXGI_FORMAT_NV12 || format == DXGI_FORMAT_YUY2 ||
      format == DXGI_FORMAT_P010) {
    return true;
  }
  return false;
}

UINT BufferCount(bool force_triple_buffer) {
  return force_triple_buffer || base::FeatureList::IsEnabled(
                                    features::kDCompTripleBufferVideoSwapChain)
             ? 3u
             : 2u;
}

const GUID GUID_INTEL_VPE_INTERFACE = {
    0xedd1d4b9,
    0x8659,
    0x4cbc,
    {0xa4, 0xd6, 0x98, 0x31, 0xa2, 0x16, 0x3a, 0xc3}};

enum : UINT {
  kIntelVpeFnVersion = 0x01,
  kIntelVpeFnMode = 0x20,
  kIntelVpeFnScaling = 0x37,
};

enum : UINT {
  kIntelVpeVersion3 = 0x0003,
};

enum : UINT {
  kIntelVpeModeNone = 0x0,
  kIntelVpeModePreproc = 0x01,
};

enum : UINT {
  kIntelVpeScalingDefault = 0x0,
  kIntelVpeScalingSuperResolution = 0x2,
};

struct IntelVpeExt {
  UINT function;
  raw_ptr<void> param;
};

HRESULT ToggleIntelVpSuperResolution(ID3D11VideoContext* video_context,
                                     ID3D11VideoProcessor* video_processor,
                                     bool enable) {
  TRACE_EVENT1("gpu", "ToggleIntelVpSuperResolution", "on", enable);

  IntelVpeExt ext = {};
  UINT param = 0;
  ext.param = &param;

  ext.function = kIntelVpeFnVersion;
  param = kIntelVpeVersion3;
  HRESULT hr = video_context->VideoProcessorSetOutputExtension(
      video_processor, &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetOutputExtension failed: "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  ext.function = kIntelVpeFnMode;
  param = enable ? kIntelVpeModePreproc : kIntelVpeModeNone;
  hr = video_context->VideoProcessorSetOutputExtension(
      video_processor, &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetOutputExtension failed: "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  ext.function = kIntelVpeFnScaling;
  param = enable ? kIntelVpeScalingSuperResolution : kIntelVpeScalingDefault;

  hr = video_context->VideoProcessorSetStreamExtension(
      video_processor, 0, &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetStreamExtension failed: "
                << logging::SystemErrorCodeToString(hr);
  }

  return hr;
}

HRESULT ToggleNvidiaVpSuperResolution(ID3D11VideoContext* video_context,
                                      ID3D11VideoProcessor* video_processor,
                                      bool enable) {
  TRACE_EVENT1("gpu", "ToggleNvidiaVpSuperResolution", "on", enable);

  constexpr GUID kNvidiaPPEInterfaceGUID = {
      0xd43ce1b3,
      0x1f4b,
      0x48ac,
      {0xba, 0xee, 0xc3, 0xc2, 0x53, 0x75, 0xe6, 0xf7}};
  constexpr UINT kStreamExtensionVersionV1 = 0x1;
  constexpr UINT kStreamExtensionMethodSuperResolution = 0x2;

  struct {
    UINT version;
    UINT method;
    UINT enable;
  } stream_extension_info = {kStreamExtensionVersionV1,
                             kStreamExtensionMethodSuperResolution,
                             enable ? 1u : 0u};

  HRESULT hr = video_context->VideoProcessorSetStreamExtension(
      video_processor, 0, &kNvidiaPPEInterfaceGUID,
      sizeof(stream_extension_info), &stream_extension_info);

  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetStreamExtension failed: "
                << logging::SystemErrorCodeToString(hr);
  }

  return hr;
}

HRESULT ToggleVpSuperResolution(UINT gpu_vendor_id,
                                ID3D11VideoContext* video_context,
                                ID3D11VideoProcessor* video_processor,
                                bool enable) {
  if (gpu_vendor_id == 0x8086 &&
      base::FeatureList::IsEnabled(features::kIntelVpSuperResolution)) {
    return ToggleIntelVpSuperResolution(video_context, video_processor, enable);
  }

  if (gpu_vendor_id == 0x10de) {
    return ToggleNvidiaVpSuperResolution(video_context, video_processor,
                                         enable);
  }

  return E_NOTIMPL;
}

constexpr GUID kNvidiaTrueHDRInterfaceGUID = {
    0xfdd62bb4,
    0x620b,
    0x4fd7,
    {0x9a, 0xb3, 0x1e, 0x59, 0xd0, 0xd5, 0x44, 0xb3}};

bool NvidiaDriverSupportsTrueHDR(ID3D11VideoContext* video_context,
                                 ID3D11VideoProcessor* video_processor) {
  UINT driver_supports_true_hdr = 0;
  HRESULT hr = video_context->VideoProcessorGetStreamExtension(
      video_processor, 0, &kNvidiaTrueHDRInterfaceGUID,
      sizeof(driver_supports_true_hdr), &driver_supports_true_hdr);

  // The runtime never fails the GetStreamExtension hr unless a bad memory size
  // is provided.
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorGetStreamExtension failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return (driver_supports_true_hdr == 1);
}

bool GpuDriverSupportsVpAutoHDR(UINT gpu_vendor_id,
                                ID3D11VideoContext* video_context,
                                ID3D11VideoProcessor* video_processor) {
  if (gpu_vendor_id == 0x10de) {
    return NvidiaDriverSupportsTrueHDR(video_context, video_processor);
  }

  return false;
}

HRESULT ToggleNvidiaVpTrueHDR(bool driver_supports_vp_auto_hdr,
                              ID3D11VideoContext* video_context,
                              ID3D11VideoProcessor* video_processor,
                              bool enable) {
  TRACE_EVENT1("gpu", "ToggleNvidiaVpTrueHDR", "on", enable);

  if (enable && !driver_supports_vp_auto_hdr) {
    return E_NOTIMPL;
  }

  constexpr UINT kStreamExtensionVersionV4 = 0x4;
  constexpr UINT kStreamExtensionMethodTrueHDR = 0x3;
  struct {
    UINT version;
    UINT method;
    UINT enable : 1;
    UINT reserved : 31;
  } stream_extension_info = {kStreamExtensionVersionV4,
                             kStreamExtensionMethodTrueHDR, enable ? 1u : 0u,
                             0u};

  HRESULT hr = video_context->VideoProcessorSetStreamExtension(
      video_processor, 0, &kNvidiaTrueHDRInterfaceGUID,
      sizeof(stream_extension_info), &stream_extension_info);

  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetStreamExtension failed: "
                << logging::SystemErrorCodeToString(hr);
  }

  return hr;
}

HRESULT ToggleVpAutoHDR(UINT gpu_vendor_id,
                        bool driver_supports_vp_auto_hdr,
                        ID3D11VideoContext* video_context,
                        ID3D11VideoProcessor* video_processor,
                        bool enable) {
  if (gpu_vendor_id == 0x10de) {
    return ToggleNvidiaVpTrueHDR(driver_supports_vp_auto_hdr, video_context,
                                 video_processor, enable);
  }

  return E_NOTIMPL;
}

bool IsVpAutoHDREnabled(UINT gpu_vendor_id) {
  return gpu_vendor_id == 0x10de;
}

// Try disabling the topmost desktop plane for a decode swap chain in the case
// of full screen. Otherwise, swap chain size is used to set destination size
// and target rectangle for the decode swap chain. In DWM, the desktop plane
// can be turned off if the letterboxing info is set up properly for YUV
// swapchains, meaning that when the size of the window and the size of the
// monitor are the same and there is no other UI component overtop of the
// video. Otherwise, set the letterboxing info with swap chain size in order
// to restore the topmost desktop plane, which happens in scenarios like
// switching to underlay.
// Returns true on successful settings.
bool TryDisableDesktopPlane(IDXGIDecodeSwapChain* decode_swap_chain,
                            const gfx::Size& dest_size,
                            const gfx::Rect& target_rect) {
  // Get the original dest size in case of restoring.
  uint32_t original_dest_width, original_dest_height;
  HRESULT hr = decode_swap_chain->GetDestSize(&original_dest_width,
                                              &original_dest_height);
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetDestSize failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  // Set the destination surface size if necessary.
  if (dest_size.width() != static_cast<int>(original_dest_width) ||
      dest_size.height() != static_cast<int>(original_dest_height)) {
    hr = decode_swap_chain->SetDestSize(dest_size.width(), dest_size.height());
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetDestSize failed: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }
  }

  // Get the original target rect in case of restoring.
  RECT original_target_rect;
  hr = decode_swap_chain->GetTargetRect(&original_target_rect);
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetTargetRect failed: "
                << logging::SystemErrorCodeToString(hr);
    decode_swap_chain->SetDestSize(original_dest_width, original_dest_height);
    return false;
  }

  // Set the target region to the specified rectangle if necessary.
  RECT target_region = target_rect.ToRECT();
  if (target_region != original_target_rect) {
    hr = decode_swap_chain->SetTargetRect(&target_region);
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetTargetRect failed: "
                  << logging::SystemErrorCodeToString(hr);
      decode_swap_chain->SetDestSize(original_dest_width, original_dest_height);
      decode_swap_chain->SetTargetRect(&original_target_rect);
      return false;
    }
  }

  return true;
}

bool IsCompatibleHDRMetadata(const gfx::HDRMetadata& hdr_metadata) {
  return (
      (hdr_metadata.smpte_st_2086 && hdr_metadata.smpte_st_2086->IsValid()) ||
      (hdr_metadata.cta_861_3 && hdr_metadata.cta_861_3->IsValid()));
}

}  // namespace

// static
gfx::Size GetMonitorSizeForWindow(HWND window) {
  if (GetDirectCompositionNumMonitors() == 1) {
    // Only one monitor. Return the size of this monitor.
    return GetDirectCompositionPrimaryMonitorSize();
  } else {
    gfx::Size monitor_size;
    // Get the monitor on which the overlay is displayed.
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    if (GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                       &monitor_info)) {
      monitor_size = gfx::Rect(monitor_info.rcMonitor).size();
    }

    return monitor_size;
  }
}

SwapChainPresenter::PresentationHistory::PresentationHistory() = default;
SwapChainPresenter::PresentationHistory::~PresentationHistory() = default;

void SwapChainPresenter::PresentationHistory::AddSample(
    DXGI_FRAME_PRESENTATION_MODE mode) {
  if (mode == DXGI_FRAME_PRESENTATION_MODE_COMPOSED)
    composed_count_++;

  presents_.push_back(mode);
  if (presents_.size() > kPresentsToStore) {
    DXGI_FRAME_PRESENTATION_MODE first_mode = presents_.front();
    if (first_mode == DXGI_FRAME_PRESENTATION_MODE_COMPOSED)
      composed_count_--;
    presents_.pop_front();
  }
}

void SwapChainPresenter::PresentationHistory::Clear() {
  presents_.clear();
  composed_count_ = 0;
}

bool SwapChainPresenter::PresentationHistory::Valid() const {
  return presents_.size() >= kPresentsToStore;
}

int SwapChainPresenter::PresentationHistory::composed_count() const {
  return composed_count_;
}

SwapChainPresenter::SwapChainPresenter(
    DCLayerTree* layer_tree,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device)
    : layer_tree_(layer_tree),
      swap_chain_buffer_count_(BufferCount(
          layer_tree->force_dcomp_triple_buffer_video_swap_chain())),
      switched_to_BGRA8888_time_tick_(base::TimeTicks::Now()),
      d3d11_device_(d3d11_device),
      is_on_battery_power_(
          base::PowerMonitor::GetInstance()
              ->AddPowerStateObserverAndReturnBatteryPowerStatus(this) ==
          base::PowerStateObserver::BatteryPowerStatus::kBatteryPower) {
  DVLOG(1) << __func__ << "(" << this << ")";
  CHECK_EQ(dcomp_device.As(&dcomp_device_), S_OK);
}

SwapChainPresenter::~SwapChainPresenter() {
  DVLOG(1) << __func__ << "(" << this << ")";
  base::PowerMonitor::GetInstance()->RemovePowerStateObserver(this);
}

DXGI_FORMAT SwapChainPresenter::GetSwapChainFormat(
    gfx::ProtectedVideoType protected_video_type,
    bool use_hdr_swap_chain,
    bool use_p010_for_sdr_swap_chain) {
  // Prefer RGB10A2 swapchain when playing HDR content and system HDR being
  // enabled. Another scenario is that AutoHDR is enabled even with SDR
  // content, RGB10A2 is also preferred.
  // Note that only use RGB10A2 overlay when the hdr monitor is available.
  if (use_hdr_swap_chain) {
    return DXGI_FORMAT_R10G10B10A2_UNORM;
  }

  if (failed_to_create_yuv_swapchain_ ||
      !DirectCompositionHardwareOverlaysSupported()) {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  }

  DXGI_FORMAT sdr_yuv_overlay_format =
      use_p010_for_sdr_swap_chain ? DXGI_FORMAT_P010
                                  : GetDirectCompositionSDROverlayFormat();
  // Always prefer YUV swap chain for hardware protected video for now.
  if (protected_video_type == gfx::ProtectedVideoType::kHardwareProtected) {
    return sdr_yuv_overlay_format;
  }

  if (!presentation_history_.Valid() || IsSwapChainYuvFormatForced()) {
    // Prefer P010 swapchain when playing P010 SDR content on SDR system with
    // P010 MPO supported.
    return sdr_yuv_overlay_format;
  }

  int composition_count = presentation_history_.composed_count();

  // It's more efficient to use a BGRA backbuffer instead of YUV if overlays
  // aren't being used, as otherwise DWM will use the video processor a second
  // time to convert it to BGRA before displaying it on screen.
  if (swap_chain_format_ != DXGI_FORMAT_B8G8R8A8_UNORM) {
    // Switch to BGRA once 3/4 of presents are composed.
    if (composition_count >= (PresentationHistory::kPresentsToStore * 3 / 4)) {
      switched_to_BGRA8888_time_tick_ = base::TimeTicks::Now();
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    }
  } else {
    // To prevent it from switching back and forth between YUV and BGRA8888,
    // Wait for at least 10 minutes before we re-try YUV. On a system that
    // can promote BGRA8888 but not YUV, the format change might cause
    // flickers.
    base::TimeDelta time_delta =
        base::TimeTicks::Now() - switched_to_BGRA8888_time_tick_;
    if (time_delta >= kDelayForRetryingYUVFormat) {
      presentation_history_.Clear();
      return sdr_yuv_overlay_format;
    }
  }
  return swap_chain_format_;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> SwapChainPresenter::UploadVideoImage(
    const gfx::Size& texture_size,
    base::span<const uint8_t> shm_video_pixmap,
    size_t pixmap_stride) {
  if (!shm_video_pixmap.data()) {
    DLOG(ERROR) << "Invalid NV12 pixmap data.";
    return nullptr;
  }

  if (texture_size.width() % 2 != 0 || texture_size.height() % 2 != 0) {
    DLOG(ERROR) << "Invalid NV12 pixmap size.";
    return nullptr;
  }

  const auto cols = static_cast<size_t>(texture_size.width());
  const auto rows = static_cast<size_t>(texture_size.height());
  if (pixmap_stride < cols) {
    DLOG(ERROR) << "Invalid NV12 pixmap stride.";
    return nullptr;
  }

  TRACE_EVENT1("gpu", "SwapChainPresenter::UploadVideoImage", "size",
               texture_size.ToString());

  bool use_dynamic_texture = !layer_tree_->disable_nv12_dynamic_textures();

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = texture_size.width();
  desc.Height = texture_size.height();
  desc.Format = DXGI_FORMAT_NV12;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Usage = use_dynamic_texture ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_STAGING;
  // This isn't actually bound to a decoder, but dynamic textures need
  // BindFlags to be nonzero and D3D11_BIND_DECODER also works when creating
  // a VideoProcessorInputView.
  desc.BindFlags = use_dynamic_texture ? D3D11_BIND_DECODER : 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  desc.MiscFlags = 0;
  desc.SampleDesc.Count = 1;

  if (!staging_texture_ || (staging_texture_size_ != texture_size)) {
    staging_texture_.Reset();
    copy_texture_.Reset();
    HRESULT hr =
        d3d11_device_->CreateTexture2D(&desc, nullptr, &staging_texture_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Creating D3D11 video staging texture failed: "
                  << logging::SystemErrorCodeToString(hr);
      DisableDirectCompositionOverlays();
      return nullptr;
    }
    DCHECK(staging_texture_);
    staging_texture_size_ = texture_size;
    hr = SetDebugName(staging_texture_.Get(), "SwapChainPresenter_Staging");
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to label D3D11 texture: "
                  << logging::SystemErrorCodeToString(hr);
    }
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(&context);
  DCHECK(context);

  D3D11_MAP map_type =
      use_dynamic_texture ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
  D3D11_MAPPED_SUBRESOURCE mapped_resource;
  HRESULT hr =
      context->Map(staging_texture_.Get(), 0, map_type, 0, &mapped_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Mapping D3D11 video staging texture failed: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  size_t dest_stride = mapped_resource.RowPitch;
  DCHECK_GE(dest_stride, cols);
  // y-plane size.

  size_t dest_size = dest_stride * rows;
  if (rows / 2 > 0) {
    // uv-plane size. Note that the last row is actual texture width, not
    // the stride.
    dest_size += dest_stride * (rows / 2 - 1) + cols;
  }

  // SAFETY: required from Map() call result.
  base::span<uint8_t> dest = UNSAFE_BUFFERS(
      base::span(reinterpret_cast<uint8_t*>(mapped_resource.pData), dest_size));
  for (size_t y = 0; y < rows; ++y) {
    auto src_row = shm_video_pixmap.subspan(pixmap_stride * y, cols);
    auto dest_row = dest.subspan(dest_stride * y, cols);
    dest_row.copy_prefix_from(src_row);
  }

  auto uv_src = shm_video_pixmap.subspan(pixmap_stride * rows);
  auto uv_dest = dest.subspan(dest_stride * rows);
  for (size_t y = 0; y < rows / 2; ++y) {
    auto src_row = uv_src.subspan(pixmap_stride * y, cols);
    auto dest_row = uv_dest.subspan(dest_stride * y, cols);
    dest_row.copy_prefix_from(src_row);
  }
  context->Unmap(staging_texture_.Get(), 0);

  if (use_dynamic_texture)
    return staging_texture_;

  if (!copy_texture_) {
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DECODER;
    desc.CPUAccessFlags = 0;
    hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &copy_texture_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Creating D3D11 video upload texture failed: "
                  << logging::SystemErrorCodeToString(hr);
      DisableDirectCompositionOverlays();
      return nullptr;
    }
    DCHECK(copy_texture_);
    hr = SetDebugName(copy_texture_.Get(), "SwapChainPresenter_Copy");
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to label D3D11 texture: "
                  << logging::SystemErrorCodeToString(hr);
    }
  }
  TRACE_EVENT0("gpu", "SwapChainPresenter::UploadVideoImages::CopyResource");
  context->CopyResource(copy_texture_.Get(), staging_texture_.Get());
  return copy_texture_;
}

gfx::Size SwapChainPresenter::CalculateSwapChainSize(
    const DCLayerOverlayParams& params) const {
  // Swap chain size is the minimum of the on-screen size and the source size so
  // the video processor can do the minimal amount of work and the overlay has
  // to read the minimal amount of data. DWM is also less likely to promote a
  // surface to an overlay if it's much larger than its area on-screen.
  gfx::SizeF swap_chain_size = params.content_rect.size();
  if (swap_chain_size.IsEmpty()) {
    return gfx::Size();
  }
  if (params.quad_rect.IsEmpty()) {
    return gfx::Size();
  }

  gfx::RectF quad_rect_float = gfx::RectF(params.quad_rect);
  gfx::RectF overlay_onscreen_rect = params.transform.MapRect(quad_rect_float);

  // If transform isn't a scale or translation then swap chain can't be promoted
  // to an overlay so avoid blitting to a large surface unnecessarily.  Also,
  // after the video rotation fix (crbug.com/904035), using rotated size for
  // swap chain size will cause stretching since there's no squashing factor in
  // the transform to counteract.
  // Downscaling doesn't work on Intel display HW, and so DWM will perform an
  // extra BLT to avoid HW downscaling. This prevents the use of hardware
  // overlays especially for protected video. Use the onscreen size (scale==1)
  // for overlay can avoid this problem.
  // TODO(crbug.com/474398418): Support 90/180/270 deg rotations using video
  // context.

  // On battery_power mode, set swap_chain_size to the source content size when
  // the swap chain presents upscaled overlay, multi-plane overlay hardware will
  // perform an upscaling operation instead of video processor(VP). Disabling VP
  // upscaled BLT is more power saving as the video processor can do the minimal
  // amount of work and the overlay has to read the minimal amount of data.
  bool can_disable_vp_upscaling_blt =
      base::FeatureList::IsEnabled(kDisableVPBLTUpscale) &&
      is_on_battery_power_ && std::abs(params.transform.rc(0, 0)) > 1.0f &&
      std::abs(params.transform.rc(1, 1)) > 1.0f;

  if (params.transform.IsScaleOrTranslation() &&
      !can_disable_vp_upscaling_blt) {
    swap_chain_size = overlay_onscreen_rect.size();
  }

  // 4:2:2 subsampled formats like YUY2 must have an even width, and 4:2:0
  // subsampled formats like NV12 or P010 must have an even width and height.
  gfx::Size swap_chain_size_rounded = gfx::ToRoundedSize(swap_chain_size);
  if (swap_chain_size_rounded.width() % 2 == 1) {
    swap_chain_size.set_width(swap_chain_size.width() + 1);
  }
  if (swap_chain_size_rounded.height() % 2 == 1) {
    swap_chain_size.set_height(swap_chain_size.height() + 1);
  }

  // Adjust `swap_chain_size` to fit into the max texture size.
  const gfx::SizeF max_texture_size(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
                                    D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION);
  if (swap_chain_size.width() > max_texture_size.width() ||
      swap_chain_size.height() > max_texture_size.height()) {
    if (max_texture_size.AspectRatio() > swap_chain_size.AspectRatio()) {
      swap_chain_size =
          gfx::SizeF(max_texture_size.height() * swap_chain_size.AspectRatio(),
                     max_texture_size.height());
    } else {
      swap_chain_size =
          gfx::SizeF(max_texture_size.width(),
                     max_texture_size.width() / swap_chain_size.AspectRatio());
    }
  }

  return gfx::ToRoundedSize(swap_chain_size);
}

bool SwapChainPresenter::TryPresentToDecodeSwapChain(
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
    unsigned array_slice,
    const gfx::ColorSpace& color_space,
    const gfx::Rect& content_rect,
    const gfx::Size& swap_chain_size,
    DXGI_FORMAT swap_chain_format,
    const gfx::Transform& transform_to_root) {
  if (ShouldUseVideoProcessorScaling())
    return false;

  bool nv12_supported =
      (swap_chain_format == DXGI_FORMAT_NV12) &&
      (DXGI_FORMAT_NV12 == GetDirectCompositionSDROverlayFormat());
  // TODO(sunnyps): Try using decode swap chain for uploaded video images.
  if (texture && nv12_supported && !failed_to_present_decode_swapchain_) {
    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture->GetDesc(&texture_desc);

    bool is_decoder_texture = (texture_desc.Format == DXGI_FORMAT_NV12) &&
                              (texture_desc.BindFlags & D3D11_BIND_DECODER);

    // Decode swap chains do not support shared resources.
    // TODO(sunnyps): Find a workaround for when the decoder moves to its own
    // thread and D3D device.  See https://crbug.com/911847
    bool is_shared_texture =
        texture_desc.MiscFlags &
        (D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
         D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    // DXVA decoder (or rather MFT) sometimes gives texture arrays with one
    // element, which constitutes most of decode swap chain creation failures.
    bool is_unitary_texture_array = texture_desc.ArraySize <= 1;

    // Rotated videos are not promoted to overlays.  We plan to implement
    // rotation using video processor instead of via direct composition.  Also
    // check for skew and any downscaling specified to direct composition.
    bool compatible_transform =
        transform_to_root.IsPositiveScaleOrTranslation();

    // Downscaled video isn't promoted to hardware overlays.  We prefer to
    // blit into the smaller size so that it can be promoted to a hardware
    // overlay.
    float swap_chain_scale_x =
        swap_chain_size.width() * 1.0f / content_rect.width();
    float swap_chain_scale_y =
        swap_chain_size.height() * 1.0f / content_rect.height();

    if (layer_tree_->no_downscaled_overlay_promotion()) {
      compatible_transform = compatible_transform &&
                             (swap_chain_scale_x >= 1.0f) &&
                             (swap_chain_scale_y >= 1.0f);
    }
    if (!DirectCompositionScaledOverlaysSupported()) {
      compatible_transform = compatible_transform &&
                             (swap_chain_scale_x == 1.0f) &&
                             (swap_chain_scale_y == 1.0f);
    }

    if (is_decoder_texture && !is_shared_texture && !is_unitary_texture_array &&
        compatible_transform) {
      if (PresentToDecodeSwapChain(texture, array_slice, color_space,
                                   content_rect, swap_chain_size)) {
        return true;
      }
      ReleaseSwapChainResources();
      failed_to_present_decode_swapchain_ = true;
      DLOG(ERROR)
          << "Present to decode swap chain failed - falling back to blit";
    }
  }
  return false;
}

bool SwapChainPresenter::PresentToDecodeSwapChain(
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
    unsigned array_slice,
    const gfx::ColorSpace& color_space,
    const gfx::Rect& content_rect,
    const gfx::Size& swap_chain_size) {
  DCHECK(!swap_chain_size.IsEmpty());

  TRACE_EVENT2("gpu", "SwapChainPresenter::PresentToDecodeSwapChain",
               "content_rect", content_rect.ToString(), "swap_chain_size",
               swap_chain_size.ToString());

  Microsoft::WRL::ComPtr<IDXGIResource> decode_resource;
  HRESULT hr = texture.As(&decode_resource);
  CHECK_EQ(hr, S_OK);

  if (!decode_swap_chain_ || decode_resource_ != decode_resource) {
    TRACE_EVENT0(
        "gpu",
        "SwapChainPresenter::PresentToDecodeSwapChain::CreateDecodeSwapChain");
    ReleaseSwapChainResources();

    decode_resource_ = decode_resource;

    base::win::ScopedHandle swap_chain_handle = CreateDCompSurfaceHandle();

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    hr = d3d11_device_.As(&dxgi_device);
    CHECK_EQ(hr, S_OK);
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    hr = dxgi_device->GetAdapter(&dxgi_adapter);
    CHECK_EQ(hr, S_OK);
    Microsoft::WRL::ComPtr<IDXGIFactoryMedia> media_factory;
    dxgi_adapter->GetParent(IID_PPV_ARGS(&media_factory));
    DCHECK(media_factory);

    DXGI_DECODE_SWAP_CHAIN_DESC desc = {};
    // Set the DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO flag to mark this surface
    // as a candidate for full screen video optimizations. If the surface
    // does not qualify as fullscreen by DWM's logic then the flag will have
    // no effects.
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO;
    hr = media_factory->CreateDecodeSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle.Get(), &desc,
        decode_resource_.Get(), nullptr, &decode_swap_chain_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateDecodeSwapChainForCompositionSurfaceHandle failed: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }
    DCHECK(decode_swap_chain_);
    DVLOG(2) << "Update visual's content. " << __func__ << "(" << this << ")";
    SetSwapChainPresentDuration();

    Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
    dcomp_device_.As(&desktop_device);
    DCHECK(desktop_device);

    hr = desktop_device->CreateSurfaceFromHandle(swap_chain_handle.Get(),
                                                 &decode_surface_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateSurfaceFromHandle failed: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }
    DCHECK(decode_surface_);

    content_ = decode_surface_.Get();
  }

  RECT source_rect = content_rect.ToRECT();
  hr = decode_swap_chain_->SetSourceRect(&source_rect);
  if (FAILED(hr)) {
    DLOG(ERROR) << "SetSourceRect failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = decode_swap_chain_->SetDestSize(swap_chain_size.width(),
                                       swap_chain_size.height());
  if (FAILED(hr)) {
    DLOG(ERROR) << "SetDestSize failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  RECT swap_chain_target_rect = gfx::Rect(swap_chain_size).ToRECT();
  hr = decode_swap_chain_->SetTargetRect(&swap_chain_target_rect);
  if (FAILED(hr)) {
    DLOG(ERROR) << "SetTargetRect failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  // TODO(sunnyps): Move this to gfx::ColorSpaceWin helper where we can access
  // internal color space state and do a better job.
  // Common color spaces have primaries and transfer function similar to BT 709
  // and there are no other choices anyway.
  int color_space_flags = DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAG_BT709;
  // Proper Rec 709 and 601 have limited or nominal color range.
  if (color_space == gfx::ColorSpace::CreateREC709() ||
      color_space == gfx::ColorSpace::CreateREC601() ||
      !color_space.IsValid()) {
    color_space_flags |= DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAG_NOMINAL_RANGE;
  }
  // xvYCC allows colors outside nominal range to encode negative colors that
  // allows for a wider gamut.
  if (color_space.FullRangeEncodedValues()) {
    color_space_flags |= DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAG_xvYCC;
  }
  hr = decode_swap_chain_->SetColorSpace(
      static_cast<DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAGS>(color_space_flags));
  if (FAILED(hr)) {
    DLOG(ERROR) << "SetColorSpace failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  pending_swap_buffer_ = array_slice;

  swap_chain_size_ = swap_chain_size;
  content_size_ = swap_chain_size;
  swap_chain_format_ = DXGI_FORMAT_NV12;
  return true;
}

std::optional<DCLayerOverlayImage> SwapChainPresenter::PresentToSwapChain(
    DCLayerOverlayParams& overlay,
    std::optional<OverlayPositionAdjustment>& overlay_position_adjustment) {
  if (!SetupPresentToSwapChain(overlay)) {
    return std::nullopt;
  }

  if (overlay.video_params.is_full_screen_video) {
    const gfx::Size monitor_size =
        GetMonitorSizeForWindow(layer_tree_->window());
    if (TryDisablePrimaryPlane(monitor_size, overlay)) {
      // If we successfully disable the primary plane, it means DWM's internal
      // swap chain is now the size of the monitor. In this case we want to just
      // treat it as an unscaled image that completely fills the screen.
      overlay_position_adjustment =
          OverlayPositionAdjustment{.monitor_size = monitor_size};
    }
  }

  if (!FinishPresentToSwapChain()) {
    return std::nullopt;
  }

  return DCLayerOverlayImage(content_size_, content_);
}

bool SwapChainPresenter::SetupPresentToSwapChain(DCLayerOverlayParams& params) {
  DCHECK(params.overlay_image);
  DCHECK_NE(params.overlay_image->type(),
            DCLayerOverlayType::kDCompVisualContent);
  CHECK(gfx::IsNearestRectWithinDistance(params.content_rect, 0.01f));

  DCLayerOverlayType overlay_type = params.overlay_image->type();

  if (overlay_type == DCLayerOverlayType::kDCompSurfaceProxy) {
    return PresentDCOMPSurface(params);
  }

  // SwapChainPresenter can be reused when switching between MediaFoundation
  // (MF) video content and non-MF content; in such cases, the DirectComposition
  // (DCOMP) surface handle associated with the MF content needs to be cleared.
  // Doing so allows a DCOMP surface to be reset on the visual when MF
  // content is shown again.
  ReleaseDCOMPSurfaceResourcesIfNeeded();

  const gfx::Size swap_chain_size = CalculateSwapChainSize(params);

  if (overlay_type == DCLayerOverlayType::kD3D11Texture &&
      !params.overlay_image->d3d11_video_texture()) {
    // We can't proceed if overlay image has no underlying d3d11 texture.  It's
    // unclear how we get into this state, but we do observe crashes due to it.
    // Just stop here instead, and render incorrectly.
    // https://crbug.com/1077645
    DLOG(ERROR) << "Video D3D11 texture is missing";
    ReleaseSwapChainResources();
    return true;
  }

  // Do not create a swap chain if swap chain size will be empty.
  if (swap_chain_size.IsEmpty()) {
    ReleaseSwapChainResources();
    swap_chain_size_ = swap_chain_size;
    content_size_ = swap_chain_size;
    return true;
  }

  bool swap_chain_resized = swap_chain_size_ != swap_chain_size;

  gfx::ColorSpace input_color_space = params.video_params.color_space;
  if (!input_color_space.IsValid()) {
    input_color_space = gfx::ColorSpace::CreateREC709();
  }

  bool content_is_hdr = input_color_space.IsHDR();

  // Enable VideoProcessor-HDR for SDR content if the monitor supports it
  // and the GPU driver version is not blocked (enable_vp_auto_hdr_). The
  // actual GPU driver support will be queried right after
  // InitializeVideoProcessor() and is checked in ToggleVpAutoHDR().
  bool use_vp_auto_hdr =
      !content_is_hdr &&
      DirectCompositionMonitorHDREnabled(layer_tree_->window()) &&
      enable_vp_auto_hdr_ && !is_on_battery_power_;

  // We allow HDR10 swap chains to be created without metadata if the input
  // stream is BT.2020 and the transfer function is PQ (Perceptual Quantizer).
  // For this combination, the corresponding DXGI color space is
  // DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 (full range RGB),
  // DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020 (studio range RGB)
  // DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020 (studio range YUV)
  bool content_is_pq10 =
      (input_color_space.GetPrimaryID() ==
       gfx::ColorSpace::PrimaryID::BT2020) &&
      (input_color_space.GetTransferID() == gfx::ColorSpace::TransferID::PQ);

  bool use_hdr_swap_chain =
      DirectCompositionMonitorHDREnabled(layer_tree_->window()) &&
      (content_is_pq10 || use_vp_auto_hdr);

  // Try to use P010 swapchain when playing 10-bit content on SDR monitor where
  // P010 MPO support is detected, due to the better quality over 8-bit
  // swapchain.
  bool use_p010_for_sdr_swap_chain =
      base::FeatureList::IsEnabled(kP010MPOForSDR) &&
      (gl::GetDirectCompositionOverlaySupportFlags(DXGI_FORMAT_P010) != 0) &&
      !DirectCompositionMonitorHDREnabled(layer_tree_->window()) &&
      params.video_params.is_p010_content;

  DXGI_FORMAT swap_chain_format =
      GetSwapChainFormat(params.video_params.protected_video_type,
                         use_hdr_swap_chain, use_p010_for_sdr_swap_chain);

  bool swap_chain_format_changed = swap_chain_format != swap_chain_format_;
  bool toggle_protected_video = swap_chain_protected_video_type_ !=
                                params.video_params.protected_video_type;

  bool contents_changed = last_overlay_image_ != params.overlay_image;

  if (swap_chain_ && !swap_chain_resized && !swap_chain_format_changed &&
      !toggle_protected_video && !contents_changed) {
    return true;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture =
      params.overlay_image->d3d11_video_texture();
  unsigned input_level = params.overlay_image->texture_array_slice();

  if (TryPresentToDecodeSwapChain(input_texture, input_level, input_color_space,
                                  gfx::ToNearestRect(params.content_rect),
                                  swap_chain_size, swap_chain_format,
                                  params.transform)) {
    last_overlay_image_ = std::move(params.overlay_image);
    return true;
  }

  // Reallocate swap chain if contents or properties change.
  if (!swap_chain_ || swap_chain_resized || swap_chain_format_changed ||
      toggle_protected_video) {
    if (!ReallocateSwapChain(swap_chain_size, swap_chain_format,
                             params.video_params.protected_video_type)) {
      ReleaseSwapChainResources();
      return false;
    }
  }

  if (input_texture) {
    staging_texture_.Reset();
    copy_texture_.Reset();
  } else {
    // TODO: Add P010 overlay for software decoder frame pixmap from
    // crbug.com/338686911.
    input_texture = UNSAFE_TODO(UploadVideoImage(
        params.overlay_image->size(), params.overlay_image->shm_video_pixmap(),
        params.overlay_image->pixmap_stride()));
    input_level = 0;
  }

  std::optional<DXGI_HDR_METADATA_HDR10> stream_metadata;
  if (content_is_pq10) {
    gfx::HDRMetadata hdr_metadata = params.video_params.hdr_metadata;
    // Potential parser bug (https://crbug.com/1362288) if HDR metadata is
    // incompatible. Missing `smpte_st_2086` or `cta_861_3` can cause Intel
    // driver crashes in HDR overlay mode. Having at least one of
    // `smpte_st_2086` or `cta_861_3` can prevent crashes. If HDR metadata is
    // invalid, set up default metadata (HdrMetadataSmpteSt2086) to avoid
    // crashes.
    if (!IsCompatibleHDRMetadata(hdr_metadata)) {
      hdr_metadata = gfx::HDRMetadata::PopulateUnspecifiedWithDefaults(
          params.video_params.hdr_metadata);
    }
    stream_metadata = HDRMetadataHelperWin::HDRMetadataToDXGI(hdr_metadata);
  }

  if (!VideoProcessorBlt(std::move(input_texture), input_level,
                         gfx::ToNearestRect(params.content_rect),
                         input_color_space, stream_metadata, use_vp_auto_hdr)) {
    return false;
  }

  HRESULT hr;
  if (first_present_) {
    first_present_ = false;
    UINT flags = DXGI_PRESENT_USE_DURATION;
    // DirectComposition can display black for a swap chain between the first
    // and second time it's presented to - maybe the first Present can get lost
    // somehow and it shows the wrong buffer. In that case copy the buffers so
    // all have the correct contents, which seems to help. The first Present()
    // after this needs to have SyncInterval > 0, or else the workaround doesn't
    // help.
    for (size_t i = 0; i < swap_chain_buffer_count_ - 1; ++i) {
      hr = swap_chain_->Present(0, flags);
      // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only
      // indicates that the window is occluded and we can stop rendering.
      if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
        LOG(ERROR) << "Present failed: "
                   << logging::SystemErrorCodeToString(hr);
        return false;
      }

      Microsoft::WRL::ComPtr<ID3D11Texture2D> dest_texture;
      swap_chain_->GetBuffer(0, IID_PPV_ARGS(&dest_texture));
      DCHECK(dest_texture);
      Microsoft::WRL::ComPtr<ID3D11Texture2D> src_texture;
      hr = swap_chain_->GetBuffer(1, IID_PPV_ARGS(&src_texture));
      DCHECK(src_texture);
      Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
      d3d11_device_->GetImmediateContext(&context);
      DCHECK(context);
      context->CopyResource(dest_texture.Get(), src_texture.Get());
    }

    // Additionally wait for the GPU to finish executing its commands, or
    // there still may be a black flicker when presenting expensive content
    // (e.g. 4k video).
    Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
    hr = d3d11_device_.As(&dxgi_device2);
    CHECK_EQ(hr, S_OK);
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    hr = dxgi_device2->EnqueueSetEvent(event.handle());
    if (SUCCEEDED(hr)) {
      event.Wait();
    }
  }

  pending_swap_buffer_ = 0;

  last_overlay_image_ = std::move(params.overlay_image);
  return true;
}

bool SwapChainPresenter::TryDisablePrimaryPlane(
    const gfx::Size& monitor_size,
    const DCLayerOverlayParams& overlay) {
  CHECK(overlay.video_params.is_full_screen_video);

  const gfx::RectF target_rect =
      overlay.transform.MapRect(gfx::RectF(overlay.quad_rect));

  if (swap_chain_) {
    // Note that QI IDXGIDecodeSwapChain from an RGB swap chain will always
    // fail.
    if (Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain;
        SUCCEEDED(
            swap_chain_->QueryInterface(IID_PPV_ARGS(&decode_swap_chain)))) {
      if (TryDisableDesktopPlane(decode_swap_chain.Get(), monitor_size,
                                 gfx::ToRoundedRect(target_rect))) {
        content_size_ = monitor_size;
        return true;
      }
    }
    return false;
  }

  if (IsMediaFoundationSurfaceProxy() &&
      base::FeatureList::IsEnabled(
          features::kDesktopPlaneRemovalForMFFullScreenLetterbox)) {
    // The ideal rect is video size scaled to fit and centered inside
    // `monitor_size`.
    gfx::RectF ideal_full_screen_rect = gfx::RectF(overlay.content_rect);
    ideal_full_screen_rect.Scale(
        std::min(monitor_size.width() / ideal_full_screen_rect.width(),
                 monitor_size.height() / ideal_full_screen_rect.height()));
    ideal_full_screen_rect.Offset(
        (monitor_size.width() - ideal_full_screen_rect.width()) / 2.0,
        (monitor_size.height() - ideal_full_screen_rect.height()) / 2.0);

    // Reject videos with non-uniform scaling since `DCOMPSurfaceProxy::SetRect`
    // always uniformly scales to fit and centers the video within the rect.
    constexpr float tolerance = 1.0f;
    if (target_rect.ApproximatelyEqual(ideal_full_screen_rect, tolerance,
                                       tolerance)) {
      pending_dcomp_surface_rect_in_window_ = gfx::Rect(monitor_size);
      content_size_ = monitor_size;
      return true;
    }

    return false;
  }

  return false;
}

bool SwapChainPresenter::FinishPresentToSwapChain() {
  if (IsMediaFoundationSurfaceProxy()) {
    CHECK(last_overlay_image_->dcomp_surface_proxy());
    CHECK(pending_dcomp_surface_rect_in_window_);
    last_overlay_image_->dcomp_surface_proxy()->SetRect(
        pending_dcomp_surface_rect_in_window_.value());
    pending_dcomp_surface_rect_in_window_.reset();
  } else if (pending_swap_buffer_) {
    if (decode_swap_chain_) {
      constexpr UINT kPresentFlags = DXGI_PRESENT_USE_DURATION;
      HRESULT hr = decode_swap_chain_->PresentBuffer(
          pending_swap_buffer_.value(), 1, kPresentFlags);
      // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only
      // indicates that the window is occluded and we can stop rendering.
      if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
        LOG(ERROR) << "PresentBuffer failed: "
                   << logging::SystemErrorCodeToString(hr);
        return false;
      }
    } else {
      CHECK_EQ(pending_swap_buffer_.value(), 0u);
      UINT flags = DXGI_PRESENT_USE_DURATION;
      UINT interval = 1;
      if (DirectCompositionSwapChainTearingEnabled()) {
        flags |= DXGI_PRESENT_ALLOW_TEARING;
        interval = 0;
      } else if (base::FeatureList::IsEnabled(
                     features::kDXGISwapChainPresentInterval0)) {
        interval = 0;
      }
      // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only
      // indicates that the window is occluded and we can stop rendering.
      HRESULT hr = swap_chain_->Present(interval, flags);
      if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
        LOG(ERROR) << "Present failed: "
                   << logging::SystemErrorCodeToString(hr);
        return false;
      }
    }
    pending_swap_buffer_.reset();
    RecordPresentationStatistics();
  }

  return true;
}
// static
base::win::ScopedHandle
SwapChainPresenter::CreateDCompSurfaceHandleForTesting() {
  return CreateDCompSurfaceHandle();
}

SwapChainPresenter::PresentationMode
SwapChainPresenter::GetLastPresentationMode() const {
  if (IsMediaFoundationSurfaceProxy()) {
    return PresentationMode::kMfSurfaceProxy;
  } else if (decode_swap_chain_) {
    return PresentationMode::kDecodeSwapChain;
  } else if (staging_texture_) {
    return PresentationMode::kVpBltWithStagingTexture;
  } else {
    return PresentationMode::kVpBlt;
  }
}

void SwapChainPresenter::RecordPresentationStatistics() {
  base::UmaHistogramSparse("GPU.DirectComposition.SwapChainFormat3",
                           swap_chain_format_);

  VideoPresentationMode presentation_mode;
  if (decode_swap_chain_) {
    presentation_mode = VideoPresentationMode::kZeroCopyDecodeSwapChain;
  } else if (staging_texture_) {
    presentation_mode = VideoPresentationMode::kUploadAndVideoProcessorBlit;
  } else {
    presentation_mode = VideoPresentationMode::kBindAndVideoProcessorBlit;
  }
  UMA_HISTOGRAM_ENUMERATION("GPU.DirectComposition.VideoPresentationMode",
                            presentation_mode);

  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("gpu.service"),
                       "SwapChain::Present", TRACE_EVENT_SCOPE_THREAD,
                       "PixelFormat", DxgiFormatToString(swap_chain_format_),
                       "ZeroCopy", !!decode_swap_chain_);
  Microsoft::WRL::ComPtr<IDXGISwapChainMedia> swap_chain_media =
      GetSwapChainMedia();
  if (swap_chain_media) {
    DXGI_FRAME_STATISTICS_MEDIA stats = {};
    // GetFrameStatisticsMedia fails with DXGI_ERROR_FRAME_STATISTICS_DISJOINT
    // sometimes, which means an event (such as power cycle) interrupted the
    // gathering of presentation statistics. In this situation, calling the
    // function again succeeds but returns with CompositionMode = NONE.
    // Waiting for the DXGI adapter to finish presenting before calling the
    // function doesn't get rid of the failure.
    HRESULT hr = swap_chain_media->GetFrameStatisticsMedia(&stats);
    int mode = -1;
    if (SUCCEEDED(hr)) {
      base::UmaHistogramSparse(
          "GPU.DirectComposition.CompositionMode2.VideoOrCanvas",
          stats.CompositionMode);
      presentation_history_.AddSample(stats.CompositionMode);
      mode = stats.CompositionMode;
    }
    // Record CompositionMode as -1 if GetFrameStatisticsMedia() fails.
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("gpu.service"),
                         "GetFrameStatisticsMedia", TRACE_EVENT_SCOPE_THREAD,
                         "CompositionMode", mode);
  }
}

bool SwapChainPresenter::PresentDCOMPSurface(DCLayerOverlayParams& params) {
  auto* dcomp_surface_proxy = params.overlay_image->dcomp_surface_proxy();
  last_overlay_image_ = std::move(params.overlay_image);

  dcomp_surface_proxy->SetParentWindow(layer_tree_->window());

  const gfx::RectF overlay_onscreen_rect =
      params.transform.MapRect(gfx::RectF(params.quad_rect));

  // Note: do not intersect clip rect w/ mapped_rect. This will result
  // in Media Foundation scaling the full video to the clipped region,
  // instead of allowing clipping to a portion of the video.
  gfx::Rect mapped_rect = gfx::ToEnclosingRect(overlay_onscreen_rect);

  if (base::FeatureList::IsEnabled(kLimitMFSwapChainSize)) {
    // We somewhat arbitrarily choose a combination of the monitor size and
    // video natural size as the upper limit.
    // - The monitor size upper limit ensures that if the video is a lower
    //   resolution than the screen and Media Foundation can do better scaling
    //   than DWM, full screen videos will continue to be upscaled nicely.
    // - The video natural size upper limit ensures that if a video is a higher
    //   resolution than the screen size, we will not limit the max scale factor
    //   to less than 1x.
    const gfx::SizeF monitor_size =
        gfx::SizeF(GetMonitorSizeForWindow(layer_tree_->window()));
    // Note: we assume that the video has an unclipped UV rect, so the
    // `content_rect` represents the resource size in pixels.
    const gfx::SizeF video_natural_size =
        gfx::SizeF(params.content_rect.size());
    const gfx::SizeF max_swap_chain_size = gfx::SizeF(
        std::max(monitor_size.width(), video_natural_size.width()),
        std::max(monitor_size.height(), video_natural_size.height()));

    // Since Chromium's MF renderer assumes that a MF video will be centered and
    // scaled (maintaining aspect ratio) to fit its quad rect, we must expand
    // one dimension of our chosen max size to match the onscreen aspect ratio.
    // The resulting size is the smallest size that encloses
    // `max_swap_chain_size` while maintaining the aspect ratio of
    // `overlay_onscreen_rect`.
    const double onscreen_to_max_size_scale =
        std::max(max_swap_chain_size.width() / overlay_onscreen_rect.width(),
                 max_swap_chain_size.height() / overlay_onscreen_rect.height());
    const gfx::SizeF adjusted_max_swap_chain_size = gfx::ScaleSize(
        overlay_onscreen_rect.size(), onscreen_to_max_size_scale);

    if (overlay_onscreen_rect.width() > adjusted_max_swap_chain_size.width() ||
        overlay_onscreen_rect.height() >
            adjusted_max_swap_chain_size.height()) {
      TRACE_EVENT("gpu", "PresentDCOMPSurface LimitMFSwapChainSize",
                  "overlay_onscreen_rect", overlay_onscreen_rect.ToString(),
                  "adjusted_max_swap_chain_size",
                  adjusted_max_swap_chain_size.ToString());
      mapped_rect.set_size(gfx::ToCeiledSize(adjusted_max_swap_chain_size));
    }
  }

  pending_dcomp_surface_rect_in_window_ = mapped_rect;
  content_size_ = mapped_rect.size();

  // If |dcomp_surface_proxy| size is {1, 1}, the texture was initialized
  // without knowledge of output size; reset |content_| so it's not added to the
  // visual tree.
  const gfx::Size content_size = dcomp_surface_proxy->GetSize();
  if (content_size == gfx::Size(1, 1)) {
    // If |content_visual_| is not updated, empty the visual and clear the DComp
    // surface to prevent stale content from being displayed.
    ReleaseDCOMPSurfaceResourcesIfNeeded();
    DVLOG(2) << __func__ << " this=" << this
             << " dcomp_surface_proxy size (1x1) path.";
    return true;
  }

  // This visual's content was a different DC surface.
  HANDLE surface_handle = dcomp_surface_proxy->GetSurfaceHandle();
  if (dcomp_surface_handle_ != surface_handle) {
    DVLOG(2) << "Update visual's content. " << __func__ << "(" << this << ")";

    ReleaseSwapChainResources();

    Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface;
    const HRESULT hr =
        dcomp_device_->CreateSurfaceFromHandle(surface_handle, &dcomp_surface);
    if (FAILED(hr)) {
      LOG(ERROR) << "CreateSurfaceFromHandle failed: "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    content_ = dcomp_surface.Get();
    // Don't take ownership of handle as the DCOMPSurfaceProxy instance owns it.
    dcomp_surface_handle_ = surface_handle;
  }

  return true;
}

void SwapChainPresenter::ReleaseDCOMPSurfaceResourcesIfNeeded() {
  if (dcomp_surface_handle_ != INVALID_HANDLE_VALUE) {
    DVLOG(2) << __func__ << "(" << this << ")";
    dcomp_surface_handle_ = INVALID_HANDLE_VALUE;
    pending_dcomp_surface_rect_in_window_.reset();
    last_overlay_image_.reset();
    content_.Reset();
    content_size_ = gfx::Size();
  }
}

bool SwapChainPresenter::VideoProcessorBlt(
    Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture,
    UINT input_level,
    const gfx::Rect& content_rect,
    const gfx::ColorSpace& src_color_space,
    std::optional<DXGI_HDR_METADATA_HDR10> stream_hdr_metadata,
    bool use_vp_auto_hdr) {
  TRACE_EVENT2("gpu", "SwapChainPresenter::VideoProcessorBlt", "content_rect",
               content_rect.ToString(), "swap_chain_size",
               swap_chain_size_.ToString());

  // TODO(sunnyps): Ensure output color space for YUV swap chains is Rec709 or
  // Rec601 so that the conversion from gfx::ColorSpace to DXGI_COLOR_SPACE
  // doesn't need a |force_yuv| parameter (and the associated plumbing).
  bool is_yuv_swapchain = IsYUVSwapChainFormat(swap_chain_format_);
  gfx::ColorSpace output_color_space =
      GetOutputColorSpace(src_color_space, is_yuv_swapchain);
  bool video_processor_recreated = false;
  VideoProcessorWrapper* video_processor_wrapper =
      layer_tree_->InitializeVideoProcessor(
          content_rect.size(), swap_chain_size_, output_color_space.IsHDR(),
          video_processor_recreated);
  if (!video_processor_wrapper)
    return false;

  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context =
      video_processor_wrapper->video_context;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor =
      video_processor_wrapper->video_processor;

  if (video_processor_recreated) {
    bool supports_vp_auto_hdr = GpuDriverSupportsVpAutoHDR(
        gpu_vendor_id_, video_context.Get(), video_processor.Get());
    video_processor_wrapper->SetDriverSupportsVpAutoHdr(supports_vp_auto_hdr);
  }
  bool driver_supports_vp_auto_hdr =
      video_processor_wrapper->GetDriverSupportsVpAutoHdr();

  Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3;
  Microsoft::WRL::ComPtr<ID3D11VideoContext1> context1;
  if (SUCCEEDED(swap_chain_.As(&swap_chain3)) &&
      SUCCEEDED(video_context.As(&context1))) {
    DCHECK(swap_chain3);
    DCHECK(context1);
    // Set input color space.
    context1->VideoProcessorSetStreamColorSpace1(
        video_processor.Get(), 0,
        gfx::ColorSpaceWin::GetDXGIColorSpace(src_color_space));
    // Set output color space.
    DXGI_COLOR_SPACE_TYPE output_dxgi_color_space =
        gfx::ColorSpaceWin::GetDXGIColorSpace(output_color_space,
                                              /*force_yuv=*/is_yuv_swapchain);
    DXGI_COLOR_SPACE_TYPE swap_dxgi_color_space =
        use_vp_auto_hdr ? gfx::ColorSpaceWin::GetDXGIColorSpace(
                              gfx::ColorSpace::CreateHDR10())
                        : output_dxgi_color_space;

    // Can fail with E_INVALIDARG if the swap chain does not support the
    // DXGI color space. We should still set the output color space as
    // best effort.
    HRESULT hr = swap_chain3->SetColorSpace1(swap_dxgi_color_space);
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetColorSpace1 failed: "
                  << logging::SystemErrorCodeToString(hr);
    }
    context1->VideoProcessorSetOutputColorSpace1(video_processor.Get(),
                                                 output_dxgi_color_space);
  } else {
    // This can't handle as many different types of color spaces, so use it
    // only if ID3D11VideoContext1 isn't available.
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE src_d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(src_color_space);
    video_context->VideoProcessorSetStreamColorSpace(video_processor.Get(), 0,
                                                     &src_d3d11_color_space);
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE output_d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(output_color_space);
    video_context->VideoProcessorSetOutputColorSpace(video_processor.Get(),
                                                     &output_d3d11_color_space);
  }

  Microsoft::WRL::ComPtr<ID3D11VideoContext2> context2;
  std::optional<DXGI_HDR_METADATA_HDR10> display_metadata =
      layer_tree_->GetHDRMetadataHelper()->GetDisplayMetadata(
          layer_tree_->window());
  if (display_metadata.has_value() && SUCCEEDED(video_context.As(&context2))) {
    if (stream_hdr_metadata.has_value()) {
      context2->VideoProcessorSetStreamHDRMetaData(
          video_processor.Get(), 0, DXGI_HDR_METADATA_TYPE_HDR10,
          sizeof(DXGI_HDR_METADATA_HDR10), &(*stream_hdr_metadata));
    }

    context2->VideoProcessorSetOutputHDRMetaData(
        video_processor.Get(), DXGI_HDR_METADATA_TYPE_HDR10,
        sizeof(DXGI_HDR_METADATA_HDR10), &(*display_metadata));
  }

  {
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device =
        video_processor_wrapper->video_device;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
        video_processor_enumerator =
            video_processor_wrapper->video_processor_enumerator;

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_desc = {};
    input_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    input_desc.Texture2D.ArraySlice = input_level;

    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> input_view;
    HRESULT hr = video_device->CreateVideoProcessorInputView(
        input_texture.Get(), video_processor_enumerator.Get(), &input_desc,
        &input_view);
    if (FAILED(hr)) {
      LOG(ERROR) << "CreateVideoProcessorInputView failed: "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = true;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.PastFrames = 0;
    stream.FutureFrames = 0;
    stream.pInputSurface = input_view.Get();
    RECT dest_rect = gfx::Rect(swap_chain_size_).ToRECT();
    video_context->VideoProcessorSetOutputTargetRect(video_processor.Get(),
                                                     TRUE, &dest_rect);
    video_context->VideoProcessorSetStreamDestRect(video_processor.Get(), 0,
                                                   TRUE, &dest_rect);
    RECT source_rect = content_rect.ToRECT();
    video_context->VideoProcessorSetStreamSourceRect(video_processor.Get(), 0,
                                                     TRUE, &source_rect);

    if (!output_view_) {
      Microsoft::WRL::ComPtr<ID3D11Texture2D> swap_chain_buffer;
      swap_chain_->GetBuffer(0, IID_PPV_ARGS(&swap_chain_buffer));

      D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_desc = {};
      output_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
      output_desc.Texture2D.MipSlice = 0;

      hr = video_device->CreateVideoProcessorOutputView(
          swap_chain_buffer.Get(), video_processor_enumerator.Get(),
          &output_desc, &output_view_);
      if (FAILED(hr)) {
        LOG(ERROR) << "CreateVideoProcessorOutputView failed: "
                   << logging::SystemErrorCodeToString(hr);
        return false;
      }
      DCHECK(output_view_);
    }

    if (enable_vp_auto_hdr_) {
      hr = ToggleVpAutoHDR(gpu_vendor_id_, driver_supports_vp_auto_hdr,
                           video_context.Get(), video_processor.Get(),
                           use_vp_auto_hdr);
      if (FAILED(hr)) {
        if (use_vp_auto_hdr) {
          if (!RevertSwapChainToSDR(video_device, video_processor,
                                    video_processor_enumerator, swap_chain3,
                                    context1, src_color_space)) {
            return false;
          }

          use_vp_auto_hdr = false;
        }
        enable_vp_auto_hdr_ = false;
      }
    }

    bool use_vp_super_resolution =
        enable_vp_super_resolution_ && !is_on_battery_power_;
    if (enable_vp_super_resolution_) {
      hr = ToggleVpSuperResolution(gpu_vendor_id_, video_context.Get(),
                                   video_processor.Get(),
                                   use_vp_super_resolution);
      if (FAILED(hr)) {
        enable_vp_super_resolution_ = false;
        use_vp_super_resolution = false;
      }
    }

    {
      TRACE_EVENT0("gpu", "ID3D11VideoContext::VideoProcessorBlt");
      hr = video_context->VideoProcessorBlt(video_processor.Get(),
                                            output_view_.Get(), 0, 1, &stream);
    }

    // Retry VideoProcessorBlt with VpSuperResolution off if it was on.
    if (FAILED(hr) && use_vp_super_resolution) {
      DLOG(ERROR) << "Retry VideoProcessorBlt with VpSuperResolution off "
                     "after it failed with: "
                  << logging::SystemErrorCodeToString(hr);

      ToggleVpSuperResolution(gpu_vendor_id_, video_context.Get(),
                              video_processor.Get(), false);
      {
        TRACE_EVENT0("gpu", "ID3D11VideoContext::VideoProcessorBlt");
        hr = video_context->VideoProcessorBlt(
            video_processor.Get(), output_view_.Get(), 0, 1, &stream);
      }

      // We shouldn't use VpSuperResolution if it was the reason that caused
      // the VideoProcessorBlt failure.
      if (SUCCEEDED(hr)) {
        enable_vp_super_resolution_ = false;
      }
    }

    if (FAILED(hr) && use_vp_auto_hdr) {
      DLOG(ERROR) << "Retry VideoProcessorBlt with VpAutoHDR off "
                     "after it failed with: "
                  << logging::SystemErrorCodeToString(hr);

      ToggleVpAutoHDR(gpu_vendor_id_, driver_supports_vp_auto_hdr,
                      video_context.Get(), video_processor.Get(), false);

      if (!RevertSwapChainToSDR(video_device, video_processor,
                                video_processor_enumerator, swap_chain3,
                                context1, src_color_space)) {
        return false;
      }

      {
        TRACE_EVENT0("gpu", "ID3D11VideoContext::VideoProcessorBlt");
        hr = video_context->VideoProcessorBlt(
            video_processor.Get(), output_view_.Get(), 0, 1, &stream);
      }

      // We shouldn't use VpAutoHDR if it was the reason that caused
      // the VideoProcessorBlt failure.
      if (SUCCEEDED(hr)) {
        enable_vp_auto_hdr_ = false;
      }
    }

    if (FAILED(hr)) {
      LOG(ERROR) << "VideoProcessorBlt failed: "
                 << logging::SystemErrorCodeToString(hr);

      // To prevent it from failing in all coming frames, disable overlay if
      // VideoProcessorBlt is not implemented in the GPU driver.
      if (hr == E_NOTIMPL) {
        DisableDirectCompositionOverlays();
      }
      return false;
    }
  }

  return true;
}

void SwapChainPresenter::ReleaseSwapChainResources() {
  if (swap_chain_ || decode_swap_chain_) {
    DVLOG(2) << __func__ << "(" << this << ")";
    output_view_.Reset();
    swap_chain_.Reset();
    staging_texture_.Reset();
    swap_chain_size_ = gfx::Size();

    decode_surface_.Reset();
    decode_swap_chain_.Reset();
    decode_resource_.Reset();

    // Only release these if we were previously using a swap chain, otherwise it
    // might interfere with dcomp surface path.
    content_.Reset();
    content_size_ = gfx::Size();
    last_overlay_image_.reset();
  }
}

bool SwapChainPresenter::ReallocateSwapChain(
    const gfx::Size& swap_chain_size,
    DXGI_FORMAT swap_chain_format,
    gfx::ProtectedVideoType protected_video_type) {
  bool use_yuv_swap_chain = IsYUVSwapChainFormat(swap_chain_format);

  TRACE_EVENT2("gpu", "SwapChainPresenter::ReallocateSwapChain", "size",
               swap_chain_size.ToString(), "yuv", use_yuv_swap_chain);

  ReleaseSwapChainResources();

  DCHECK(!swap_chain_size.IsEmpty());
  swap_chain_size_ = swap_chain_size;
  swap_chain_protected_video_type_ = protected_video_type;
  gpu_vendor_id_ = 0;

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  HRESULT hr = d3d11_device_.As(&dxgi_device);
  CHECK_EQ(hr, S_OK);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  hr = dxgi_device->GetAdapter(&dxgi_adapter);
  CHECK_EQ(hr, S_OK);
  Microsoft::WRL::ComPtr<IDXGIFactoryMedia> media_factory;
  dxgi_adapter->GetParent(IID_PPV_ARGS(&media_factory));
  DCHECK(media_factory);

  // The composition surface handle is only used to create YUV swap chains since
  // CreateSwapChainForComposition can't do that.
  base::win::ScopedHandle swap_chain_handle = CreateDCompSurfaceHandle();

  first_present_ = true;

  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = swap_chain_size_.width();
  desc.Height = swap_chain_size_.height();
  desc.Format = swap_chain_format;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = swap_chain_buffer_count_;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.Flags =
      DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO | DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO;
  if (DirectCompositionSwapChainTearingEnabled()) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
  }
  if (DXGIWaitableSwapChainEnabled()) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
  }
  if (IsProtectedVideo(protected_video_type)) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
  }
  if (protected_video_type == gfx::ProtectedVideoType::kHardwareProtected) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
  }
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

  const std::string kSwapChainCreationResultByVideoTypeUmaPrefix =
      "GPU.DirectComposition.SwapChainCreationResult3.";
  const std::string protected_video_type_string =
      ProtectedVideoTypeToString(protected_video_type);

  if (use_yuv_swap_chain) {
    TRACE_EVENT1("gpu", "SwapChainPresenter::ReallocateSwapChain::YUV",
                 "format", DxgiFormatToString(swap_chain_format));
    hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle.Get(), &desc, nullptr,
        &swap_chain_);
    failed_to_create_yuv_swapchain_ = FAILED(hr);

    base::UmaHistogramSparse(kSwapChainCreationResultByVideoTypeUmaPrefix +
                                 protected_video_type_string,
                             hr);

    if (failed_to_create_yuv_swapchain_) {
      DLOG(ERROR) << "Failed to create "
                  << DxgiFormatToString(swap_chain_format)
                  << " swap chain of size " << swap_chain_size.ToString()
                  << ": " << logging::SystemErrorCodeToString(hr)
                  << "\nFalling back to BGRA";
      use_yuv_swap_chain = false;
      swap_chain_format = DXGI_FORMAT_B8G8R8A8_UNORM;
    } else {
      DVLOG(2) << "Update visual's content (yuv). " << __func__ << "(" << this
               << ")";
    }
  }
  if (!use_yuv_swap_chain) {
    TRACE_EVENT1("gpu", "SwapChainPresenter::ReallocateSwapChain::RGB",
                 "format", DxgiFormatToString(swap_chain_format));

    desc.Format = swap_chain_format;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO;
    if (DirectCompositionSwapChainTearingEnabled()) {
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }
    if (DXGIWaitableSwapChainEnabled()) {
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    }
    if (IsProtectedVideo(protected_video_type)) {
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    }
    if (protected_video_type == gfx::ProtectedVideoType::kHardwareProtected) {
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    }

    hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle.Get(), &desc, nullptr,
        &swap_chain_);

    base::UmaHistogramSparse(kSwapChainCreationResultByVideoTypeUmaPrefix +
                                 protected_video_type_string,
                             hr);

    if (FAILED(hr)) {
      // Disable overlay support so dc_layer_overlay will stop sending down
      // overlay frames here and uses GL Composition instead.
      DisableDirectCompositionOverlays();
      LOG(ERROR) << "Failed to create " << DxgiFormatToString(swap_chain_format)
                 << " swap chain of size " << swap_chain_size.ToString() << ": "
                 << logging::SystemErrorCodeToString(hr)
                 << ". Disable overlay swap chains";
      return false;
    }

    DVLOG(2) << "Update visual's content. " << __func__ << "(" << this << ")";
  }

  if (DXGIWaitableSwapChainEnabled()) {
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3;
    if (SUCCEEDED(swap_chain_.As(&swap_chain3))) {
      hr = swap_chain3->SetMaximumFrameLatency(
          GetDXGIWaitableSwapChainMaxQueuedFrames());
      DCHECK(SUCCEEDED(hr)) << "SetMaximumFrameLatency failed: "
                            << logging::SystemErrorCodeToString(hr);
    }
  }

  LabelSwapChainAndBuffers(swap_chain_.Get(), "SwapChainPresenter");

  content_ = swap_chain_.Get();
  content_size_ = swap_chain_size;
  swap_chain_size_ = swap_chain_size;
  swap_chain_format_ = swap_chain_format;
  SetSwapChainPresentDuration();

  DXGI_ADAPTER_DESC adapter_desc;
  hr = dxgi_adapter->GetDesc(&adapter_desc);
  if (SUCCEEDED(hr)) {
    gpu_vendor_id_ = adapter_desc.VendorId;
  } else {
    DLOG(ERROR) << "Failed to get adapter desc: "
                << logging::SystemErrorCodeToString(hr);
  }

  enable_vp_auto_hdr_ =
      !layer_tree_->disable_vp_auto_hdr() && IsVpAutoHDREnabled(gpu_vendor_id_);
  enable_vp_super_resolution_ = !layer_tree_->disable_vp_super_resolution();

  return true;
}

void SwapChainPresenter::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  is_on_battery_power_ =
      (battery_power_status ==
       base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
}

bool SwapChainPresenter::ShouldUseVideoProcessorScaling() {
  return (!is_on_battery_power_ && !layer_tree_->disable_vp_scaling());
}

void SwapChainPresenter::SetSwapChainPresentDuration() {
  Microsoft::WRL::ComPtr<IDXGISwapChainMedia> swap_chain_media =
      GetSwapChainMedia();
  if (swap_chain_media) {
    UINT requested_duration = 0u;
    HRESULT hr = swap_chain_media->SetPresentDuration(requested_duration);
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetPresentDuration failed: "
                  << logging::SystemErrorCodeToString(hr);
    }
  }
}

Microsoft::WRL::ComPtr<IDXGISwapChainMedia>
SwapChainPresenter::GetSwapChainMedia() const {
  Microsoft::WRL::ComPtr<IDXGISwapChainMedia> swap_chain_media;
  HRESULT hr = S_OK;
  if (decode_swap_chain_) {
    hr = decode_swap_chain_.As(&swap_chain_media);
  } else {
    DCHECK(swap_chain_);
    hr = swap_chain_.As(&swap_chain_media);
  }
  if (SUCCEEDED(hr))
    return swap_chain_media;
  return nullptr;
}

bool SwapChainPresenter::RevertSwapChainToSDR(
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device,
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor,
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
        video_processor_enumerator,
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3,
    Microsoft::WRL::ComPtr<ID3D11VideoContext1> context1,
    const gfx::ColorSpace& input_color_space) {
  if (!video_device || !video_processor || !video_processor_enumerator ||
      !swap_chain3 || !context1) {
    return false;
  }

  // Restore the SDR swap chain and output view
  if (!ReallocateSwapChain(
          gfx::Size(swap_chain_size_),
          GetSwapChainFormat(swap_chain_protected_video_type_,
                             /*use_hdr_swap_chain=*/false,
                             /*use_p010_for_sdr_swap_chain=*/false),
          swap_chain_protected_video_type_)) {
    ReleaseSwapChainResources();
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> swap_chain_buffer;
  swap_chain_->GetBuffer(0, IID_PPV_ARGS(&swap_chain_buffer));
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_desc = {};
  output_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  output_desc.Texture2D.MipSlice = 0;
  HRESULT hr = video_device->CreateVideoProcessorOutputView(
      swap_chain_buffer.Get(), video_processor_enumerator.Get(), &output_desc,
      &output_view_);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateVideoProcessorOutputView failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  DCHECK(output_view_);

  // Reset the output color space for the swap chain and video processor
  bool is_yuv_swapchain = IsYUVSwapChainFormat(swap_chain_format_);
  gfx::ColorSpace output_color_space =
      GetOutputColorSpace(input_color_space, is_yuv_swapchain);
  DXGI_COLOR_SPACE_TYPE output_dxgi_color_space =
      gfx::ColorSpaceWin::GetDXGIColorSpace(output_color_space,
                                            is_yuv_swapchain);
  context1->VideoProcessorSetOutputColorSpace1(video_processor.Get(),
                                               output_dxgi_color_space);
  hr = swap_chain3->SetColorSpace1(output_dxgi_color_space);
  if (FAILED(hr)) {
    LOG(ERROR) << "SetColorSpace1 failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return true;
}

bool SwapChainPresenter::IsMediaFoundationSurfaceProxy() const {
  if (last_overlay_image_ &&
      last_overlay_image_->type() == DCLayerOverlayType::kDCompSurfaceProxy) {
    CHECK(last_overlay_image_->dcomp_surface_proxy());
    return true;
  }
  return false;
}

}  // namespace gl
