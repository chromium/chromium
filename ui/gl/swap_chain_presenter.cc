// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/swap_chain_presenter.h"

#include <d3d11_1.h>
#include <d3d11_4.h>

#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/dc_layer_overlay_image.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/debug_utils.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/hdr_metadata_helper_win.h"

namespace gl {
namespace {

// When in BGRA888 overlay format, wait for this time delta before retrying
// YUV format.
constexpr base::TimeDelta kDelayForRetryingYUVFormat = base::Minutes(10);

// Some drivers fail to correctly handle BT.709 video in overlays. This flag
// converts them to BT.601 in the video processor.
BASE_FEATURE(kFallbackBT709VideoToBT601,
             "FallbackBT709VideoToBT601",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisableVPBLTUpscale,
             "DisableVPBLTUpscale",
             base::FEATURE_DISABLED_BY_DEFAULT);

gfx::ColorSpace GetOutputColorSpace(const gfx::ColorSpace& input_color_space,
                                    bool is_yuv_swapchain) {
  gfx::ColorSpace output_color_space =
      is_yuv_swapchain ? input_color_space : gfx::ColorSpace::CreateSRGB();
  if (base::FeatureList::IsEnabled(kFallbackBT709VideoToBT601) &&
      (output_color_space == gfx::ColorSpace::CreateREC709())) {
    output_color_space = gfx::ColorSpace::CreateREC601();
  }
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

bool CreateSurfaceHandleHelper(HANDLE* handle) {
  using PFN_DCOMPOSITION_CREATE_SURFACE_HANDLE =
      HRESULT(WINAPI*)(DWORD, SECURITY_ATTRIBUTES*, HANDLE*);
  static PFN_DCOMPOSITION_CREATE_SURFACE_HANDLE create_surface_handle_function =
      nullptr;

  if (!create_surface_handle_function) {
    HMODULE dcomp = ::GetModuleHandleA("dcomp.dll");
    if (!dcomp) {
      DLOG(ERROR) << "Failed to get handle for dcomp.dll";
      return false;
    }
    create_surface_handle_function =
        reinterpret_cast<PFN_DCOMPOSITION_CREATE_SURFACE_HANDLE>(
            ::GetProcAddress(dcomp, "DCompositionCreateSurfaceHandle"));
    if (!create_surface_handle_function) {
      DLOG(ERROR)
          << "Failed to get address for DCompositionCreateSurfaceHandle";
      return false;
    }
  }

  HRESULT hr = create_surface_handle_function(COMPOSITIONOBJECT_ALL_ACCESS,
                                              nullptr, handle);
  if (FAILED(hr)) {
    DLOG(ERROR) << "DCompositionCreateSurfaceHandle failed with error 0x"
                << std::hex << hr;
    return false;
  }

  return true;
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
    default:
      NOTREACHED_IN_MIGRATION();
      return "UNKNOWN";
  }
}

bool IsYUVSwapChainFormat(DXGI_FORMAT format) {
  if (format == DXGI_FORMAT_NV12 || format == DXGI_FORMAT_YUY2)
    return true;
  return false;
}

UINT BufferCount(bool force_triple_buffer) {
  return force_triple_buffer || base::FeatureList::IsEnabled(
                                    features::kDCompTripleBufferVideoSwapChain)
             ? 3u
             : 2u;
}

// Transform is correct for scaling up |quad_rect| to on screen bounds, but
// doesn't include scaling transform from |swap_chain_size| to |quad_rect|.
// Since |swap_chain_size| could be equal to on screen bounds, and therefore
// possibly larger than |quad_rect|, this scaling could be downscaling, but
// only to the extent that it would cancel upscaling already in the transform.
void UpdateSwapChainTransform(const gfx::Size& quad_size,
                               const gfx::SizeF& swap_chain_size,
                               gfx::Transform* visual_transform) {
  float swap_chain_scale_x = quad_size.width() * 1.0f / swap_chain_size.width();
  float swap_chain_scale_y =
      quad_size.height() * 1.0f / swap_chain_size.height();
  visual_transform->Scale(swap_chain_scale_x, swap_chain_scale_y);
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
    DLOG(ERROR) << "VideoProcessorSetOutputExtension failed with error 0x"
                << std::hex << hr;
    return hr;
  }

  ext.function = kIntelVpeFnMode;
  param = enable ? kIntelVpeModePreproc : kIntelVpeModeNone;
  hr = video_context->VideoProcessorSetOutputExtension(
      video_processor, &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetOutputExtension failed with error 0x"
                << std::hex << hr;
    return hr;
  }

  ext.function = kIntelVpeFnScaling;
  param = enable ? kIntelVpeScalingSuperResolution : kIntelVpeScalingDefault;

  hr = video_context->VideoProcessorSetStreamExtension(
      video_processor, 0, &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetStreamExtension failed with error 0x"
                << std::hex << hr;
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

  base::UmaHistogramSparse(enable
                               ? "GPU.NvidiaVpSuperResolution.On.SetStreamExt"
                               : "GPU.NvidiaVpSuperResolution.Off.SetStreamExt",
                           hr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetStreamExtension failed with error 0x"
                << std::hex << hr;
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

  if (gpu_vendor_id == 0x10de &&
      base::FeatureList::IsEnabled(features::kNvidiaVpSuperResolution)) {
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
    DLOG(ERROR) << "VideoProcessorGetStreamExtension failed with error 0x"
                << std::hex << hr;
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

  base::UmaHistogramSparse(enable ? "GPU.NvidiaVpTrueHDR.On.SetStreamExt"
                                  : "GPU.NvidiaVpTrueHDR.Off.SetStreamExt",
                           hr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetStreamExtension failed with error 0x"
                << std::hex << hr;
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
  if (gpu_vendor_id == 0x10de &&
      base::FeatureList::IsEnabled(features::kNvidiaVpTrueHDR)) {
    return true;
  }

  return false;
}

bool IsWithinMargin(float i, float j) {
  constexpr float kFullScreenMargin = 10.0;
  return (std::abs(i - j) < kFullScreenMargin);
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
    DLOG(ERROR) << "GetDestSize failed with error 0x" << std::hex << hr;
    return false;
  }

  // Set the destination surface size if necessary.
  if (dest_size.width() != (int)original_dest_width ||
      dest_size.height() != (int)original_dest_height) {
    hr = decode_swap_chain->SetDestSize(dest_size.width(), dest_size.height());
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetDestSize failed with error 0x" << std::hex << hr;
      return false;
    }
  }

  // Get the original target rect in case of restoring.
  RECT original_target_rect;
  hr = decode_swap_chain->GetTargetRect(&original_target_rect);
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetTargetRect failed with error 0x" << std::hex << hr;
    decode_swap_chain->SetDestSize(original_dest_width, original_dest_height);
    return false;
  }

  // Set the target region to the specified rectangle if necessary.
  RECT target_region = target_rect.ToRECT();
  if (target_region != original_target_rect) {
    hr = decode_swap_chain->SetTargetRect(&target_region);
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetTargetRect failed with error 0x" << std::hex << hr;
      decode_swap_chain->SetDestSize(original_dest_width, original_dest_height);
      decode_swap_chain->SetTargetRect(&original_target_rect);
      return false;
    }
  }

  return true;
}

}  // namespace

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
    Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device)
    : layer_tree_(layer_tree),
      swap_chain_buffer_count_(BufferCount(
          layer_tree->force_dcomp_triple_buffer_video_swap_chain())),
      switched_to_BGRA8888_time_tick_(base::TimeTicks::Now()),
      d3d11_device_(d3d11_device),
      dcomp_device_(dcomp_device),
      is_on_battery_power_(
          base::PowerMonitor::GetInstance()
              ->AddPowerStateObserverAndReturnBatteryPowerStatus(this) ==
          base::PowerStateObserver::BatteryPowerStatus::kBatteryPower) {}

SwapChainPresenter::~SwapChainPresenter() {
  base::PowerMonitor::GetInstance()->RemovePowerStateObserver(this);
}

DXGI_FORMAT SwapChainPresenter::GetSwapChainFormat(
    gfx::ProtectedVideoType protected_video_type,
    bool content_is_hdr) {
  // Prefer RGB10A2 swapchain when playing HDR content.
  // Only use rgb10a2 overlay when the hdr monitor is available.
  if (content_is_hdr && DirectCompositionSystemHDREnabled()) {
    return DXGI_FORMAT_R10G10B10A2_UNORM;
  }

  DXGI_FORMAT yuv_overlay_format = GetDirectCompositionSDROverlayFormat();
  // Always prefer YUV swap chain for hardware protected video for now.
  if (protected_video_type == gfx::ProtectedVideoType::kHardwareProtected)
    return yuv_overlay_format;

  if (failed_to_create_yuv_swapchain_ ||
      !DirectCompositionHardwareOverlaysSupported()) {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  }

  // Start out as YUV.
  if (!presentation_history_.Valid())
    return yuv_overlay_format;

  int composition_count = presentation_history_.composed_count();

  // It's more efficient to use a BGRA backbuffer instead of YUV if overlays
  // aren't being used, as otherwise DWM will use the video processor a second
  // time to convert it to BGRA before displaying it on screen.

  if (swap_chain_format_ == yuv_overlay_format) {
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
      return yuv_overlay_format;
    }
  }
  return swap_chain_format_;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> SwapChainPresenter::UploadVideoImage(
    const gfx::Size& texture_size,
    const uint8_t* nv12_pixmap,
    size_t pixmap_stride) {
  if (!nv12_pixmap) {
    DLOG(ERROR) << "Invalid NV12 pixmap data.";
    return nullptr;
  }

  if (texture_size.width() % 2 != 0 || texture_size.height() % 2 != 0) {
    DLOG(ERROR) << "Invalid NV12 pixmap size.";
    return nullptr;
  }

  if (pixmap_stride < static_cast<size_t>(texture_size.width())) {
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
      DLOG(ERROR) << "Creating D3D11 video staging texture failed: " << std::hex
                  << hr;
      DisableDirectCompositionOverlays();
      return nullptr;
    }
    DCHECK(staging_texture_);
    staging_texture_size_ = texture_size;
    hr = SetDebugName(staging_texture_.Get(), "SwapChainPresenter_Staging");
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to label D3D11 texture: " << std::hex << hr;
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
    DLOG(ERROR) << "Mapping D3D11 video staging texture failed: " << std::hex
                << hr;
    return nullptr;
  }

  size_t dest_stride = mapped_resource.RowPitch;
  DCHECK_GE(dest_stride, static_cast<size_t>(texture_size.width()));
  // y-plane size.
  size_t src_size = pixmap_stride * texture_size.height();
  size_t dest_size = dest_stride * texture_size.height();
  if (texture_size.height() / 2 > 0) {
    // uv-plane size. Note that the last row is actual texture width, not
    // the stride.
    src_size +=
        pixmap_stride * (texture_size.height() / 2 - 1) + texture_size.width();
    dest_size +=
        dest_stride * (texture_size.height() / 2 - 1) + texture_size.width();
  }
  base::span<const uint8_t> src =
      UNSAFE_TODO(base::span(nv12_pixmap, src_size));
  // SAFETY: required from Map() call result.
  base::span<uint8_t> dest = UNSAFE_BUFFERS(
      base::span(reinterpret_cast<uint8_t*>(mapped_resource.pData), dest_size));
  for (int y = 0; y < texture_size.height(); y++) {
    auto src_row = src.subspan(pixmap_stride * y, texture_size.width());
    auto dest_row = dest.subspan(dest_stride * y, texture_size.width());
    dest_row.copy_prefix_from(src_row);
  }

  auto uv_src = src.subspan(pixmap_stride * texture_size.height());
  auto uv_dest = dest.subspan(dest_stride * texture_size.height());
  for (int y = 0; y < texture_size.height() / 2; y++) {
    auto src_row = uv_src.subspan(pixmap_stride * y, texture_size.width());
    auto dest_row = uv_dest.subspan(dest_stride * y, texture_size.width());
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
      DLOG(ERROR) << "Creating D3D11 video upload texture failed: " << std::hex
                  << hr;
      DisableDirectCompositionOverlays();
      return nullptr;
    }
    DCHECK(copy_texture_);
    hr = SetDebugName(copy_texture_.Get(), "SwapChainPresenter_Copy");
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to label D3D11 texture: " << std::hex << hr;
    }
  }
  TRACE_EVENT0("gpu", "SwapChainPresenter::UploadVideoImages::CopyResource");
  context->CopyResource(copy_texture_.Get(), staging_texture_.Get());
  return copy_texture_;
}

gfx::Size SwapChainPresenter::GetMonitorSize() const {
  if (GetDirectCompositionNumMonitors() == 1) {
    // Only one monitor. Return the size of this monitor.
    return GetDirectCompositionPrimaryMonitorSize();
  } else {
    gfx::Size monitor_size;
    // Get the monitor on which the overlay is displayed.
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    if (GetMonitorInfo(
            MonitorFromWindow(layer_tree_->window(), MONITOR_DEFAULTTONEAREST),
            &monitor_info)) {
      monitor_size = gfx::Rect(monitor_info.rcMonitor).size();
    }

    return monitor_size;
  }
}

void SwapChainPresenter::SetTargetToFullScreen(
    gfx::Transform* visual_transform,
    gfx::Rect* visual_clip_rect,
    const std::optional<gfx::Rect>& target_rect) {
  if (base::FeatureList::IsEnabled(kDisableVPBLTUpscale) &&
      (std::abs(visual_transform->rc(0, 0)) > 1.0f) &&
      (std::abs(visual_transform->rc(1, 1)) > 1.0f) &&
      target_rect.has_value()) {
    // Reset the horizontal/vertical shift according to the target_rect and
    // original transform, since DWM will do the positioning in case of overlay.
    visual_transform->set_rc(
        0, 3,
        visual_transform->rc(0, 3) -
            target_rect.value().x() * visual_transform->rc(0, 0));
    visual_transform->set_rc(
        1, 3,
        visual_transform->rc(1, 3) -
            target_rect.value().y() * visual_transform->rc(1, 1));
  } else {
    // Reset the horizontal/vertical shift according to the visual clip and
    // original transform, since DWM will do the positioning in case of overlay.
    visual_transform->set_rc(
        0, 3,
        visual_clip_rect->x() -
            visual_transform->rc(0, 3) * visual_transform->rc(0, 0));
    visual_transform->set_rc(
        1, 3,
        visual_clip_rect->y() -
            visual_transform->rc(1, 3) * visual_transform->rc(1, 1));
  }

  // Expand the clip rect for swap chain to the whole screen.
  *visual_clip_rect = gfx::Rect(GetMonitorSize());

  last_desktop_plane_removed_ = true;
}

void SwapChainPresenter::AdjustTargetToOptimalSizeIfNeeded(
    const DCLayerOverlayParams& params,
    const gfx::RectF& overlay_onscreen_rect,
    gfx::SizeF* swap_chain_size,
    gfx::Transform* visual_transform,
    gfx::RectF* visual_clip_rect,
    std::optional<gfx::SizeF>* dest_size,
    std::optional<gfx::RectF>* target_rect) const {
  // First try to adjust the full screen overlay that can fit the whole
  // screen. If it cannot fit the whole screen and we know it's in
  // letterboxing mode, try to center the overlay and adjust only x or only y.
  gfx::Size monitor_size = GetMonitorSize();
  gfx::SizeF monitor_size_float(monitor_size.width(), monitor_size.height());
  bool size_adjusted = AdjustTargetToFullScreenSizeIfNeeded(
      monitor_size_float, params, overlay_onscreen_rect, swap_chain_size,
      visual_transform, visual_clip_rect);

  // Adjustment for the full screen letterboxing scenario.
  if (!size_adjusted &&
      params.video_params.possible_video_fullscreen_letterboxing) {
    AdjustTargetForFullScreenLetterboxing(
        monitor_size_float, params, overlay_onscreen_rect, swap_chain_size,
        visual_transform, visual_clip_rect, dest_size, target_rect);
  }
}

bool SwapChainPresenter::AdjustTargetToFullScreenSizeIfNeeded(
    const gfx::SizeF& monitor_size,
    const DCLayerOverlayParams& params,
    const gfx::RectF& overlay_onscreen_rect,
    gfx::SizeF* swap_chain_size,
    gfx::Transform* visual_transform,
    gfx::RectF* visual_clip_rect) const {
  if (monitor_size.IsEmpty()) {
    return false;
  }

  gfx::RectF clipped_onscreen_rect = overlay_onscreen_rect;
  if (params.clip_rect.has_value()) {
    clipped_onscreen_rect.Intersect(*visual_clip_rect);
  }

  // Skip adjustment if the current swap chain size is already correct.
  if (clipped_onscreen_rect == gfx::RectF(monitor_size) &&
      overlay_onscreen_rect == gfx::RectF(monitor_size)) {
    return true;
  }

  // Because of the rounding when converting between pixels and DIPs, a
  // fullscreen video can become slightly larger than the monitor - e.g. on
  // a 3000x2000 monitor with a scale factor of 1.75 a 1920x1079 video can
  // become 3002x1689.
  // Swapchains that are bigger than the monitor won't be put into overlays,
  // which will hurt power usage a lot. On those systems, the scaling can be
  // adjusted very slightly so that it's less than the monitor size. This
  // should be close to imperceptible. http://crbug.com/668278
  // The overlay must be positioned at (0, 0) in fullscreen mode.
  if (!IsWithinMargin(clipped_onscreen_rect.x(), 0.0) ||
      !IsWithinMargin(clipped_onscreen_rect.y(), 0.0)) {
    // Not fullscreen mode.
    return false;
  }

  // Check whether the on-screen overlay is near the full screen size.
  // If yes, adjust the overlay size so it can fit the screen. This allows the
  // application of fullscreen optimizations like dynamic backlighting or
  // dynamic refresh rates (24hz/48hz). Note: The DWM optimizations works for
  // both hardware and software overlays.
  // If no, do nothing.
  if (!IsWithinMargin(clipped_onscreen_rect.width(), monitor_size.width()) ||
      !IsWithinMargin(clipped_onscreen_rect.height(), monitor_size.height())) {
    // Not fullscreen mode.
    return false;
  }

  // For most video playbacks, |clip_rect| is the same as
  // |overlay_onscreen_rect| or close to it. If |clipped_onscreen_rect| has the
  // size of the monitor but |overlay_onscreen_rect| is much bigger than the
  // monitor size, we don't get the benefit of this optimization in this case.
  // We should do nothing here. e.g. |overlay_onscreen_rect| is ~7680 x 4320 and
  // it's clipped to ~3840 x 2160 to fit the monitor. Check
  // |overlay_onscreen_rect| only if it's different from |clipped_onscreen_rect|
  // when clipping is enabled. https://crbug.com/1213035
  if (params.clip_rect.has_value()) {
    if (!IsWithinMargin(overlay_onscreen_rect.width(), monitor_size.width()) ||
        !IsWithinMargin(overlay_onscreen_rect.height(),
                        monitor_size.height())) {
      return false;
    }
  }

  //
  // Adjust the clip rect.
  //
  if (params.clip_rect.has_value()) {
    *visual_clip_rect = gfx::RectF(monitor_size);
  }

  //
  // Adjust the swap chain size if needed.
  //
  // Change the swap chain size so the scaling is performed by video processor.
  // Make the final |visual_transform| after this function an Identity if
  // possible.
  // The swap chain is either the size of overlay_onscreen_rect or
  // min(overlay_onscreen_rect, content_rect). The swap chain might not need to
  // be updated if it's the content size.
  // |visual_transform| transforms the swap chain to the on-screen rect.
  // (See UpdateSwapChainTransform() in CalculateSwapChainSize().) Now update
  // |visual_transform| so it still produces the same on-screen rect
  // after changing the swapchain.
  float scale_x;
  float scale_y;
  if (*swap_chain_size == overlay_onscreen_rect.size()) {
    scale_x = swap_chain_size->width() * 1.0f / monitor_size.width();
    scale_y = swap_chain_size->height() * 1.0f / monitor_size.height();
    visual_transform->Scale(scale_x, scale_y);
    *swap_chain_size = monitor_size;
  }

  //
  // Adjust the transform matrix.
  //
  // Add the new scale that scales |overlay_onscreen_rect| to |monitor_size|.
  // The new |visual_transform| will produce a rect of the monitor size.
  scale_x = monitor_size.width() * 1.0f / overlay_onscreen_rect.width();
  scale_y = monitor_size.height() * 1.0f / overlay_onscreen_rect.height();
  visual_transform->Scale(scale_x, scale_y);

  // Origin is probably (0,0) all the time. If not, adjust the origin.
  gfx::RectF unmapped_rect =
      gfx::RectF(params.quad_rect.x(), params.quad_rect.y(),
                 swap_chain_size->width(), swap_chain_size->height());
  gfx::RectF mapped_rect = visual_transform->MapRect(unmapped_rect);
  visual_transform->PostTranslate(-mapped_rect.OffsetFromOrigin());

#if DCHECK_IS_ON()
  // Verify if the new transform matrix transforms the swap chain to the
  // monitor rect.
  gfx::RectF new_rect = visual_transform->MapRect(unmapped_rect);
  if (params.clip_rect.has_value()) {
    new_rect.Intersect(*visual_clip_rect);
  }

  // https://crbug.com/1517344: "DCHECK_EQ(new_rect, gfx::RectF(monitor_size))"
  // sometimes failed in the field. But here we collect possible crashes in
  // general.
  base::debug::Alias(&visual_transform);
  base::debug::Alias(&new_rect);

  // Here we use 0.01f as the check tolerance for floating-point numbers, since
  // eventually the size adjustment for overlay will be rounded to be integral.
  constexpr float kTolerance = 0.01f;
  bool sufficiently_equal = new_rect.ApproximatelyEqual(
      gfx::RectF(monitor_size), kTolerance, kTolerance);
  DCHECK(sufficiently_equal)
      << ", params.quad_rect: " << params.quad_rect.ToString()
      << ", params.content_rect: " << params.content_rect.ToString()
      << ", clipped_onscreen_rect: " << clipped_onscreen_rect.ToString()
      << ", overlay_onscreen_rect: " << overlay_onscreen_rect.ToString()
      << ", params.transform: " << params.transform.ToString()
      << ", visual_transform: " << visual_transform->ToString();
#endif

  return true;
}

void SwapChainPresenter::AdjustTargetForFullScreenLetterboxing(
    const gfx::SizeF& monitor_size,
    const DCLayerOverlayParams& params,
    const gfx::RectF& overlay_onscreen_rect,
    gfx::SizeF* swap_chain_size,
    gfx::Transform* visual_transform,
    gfx::RectF* visual_clip_rect,
    std::optional<gfx::SizeF>* dest_size,
    std::optional<gfx::RectF>* target_rect) const {
  if (!base::FeatureList::IsEnabled(
          features::kDirectCompositionLetterboxVideoOptimization)) {
    return;
  }

  if (monitor_size.IsEmpty()) {
    return;
  }

  gfx::RectF clipped_onscreen_rect = overlay_onscreen_rect;
  if (params.clip_rect.has_value()) {
    clipped_onscreen_rect.Intersect(*visual_clip_rect);
  }

  bool is_onscreen_rect_x_near_0 =
      IsWithinMargin(clipped_onscreen_rect.x(), 0.0);
  bool is_onscreen_rect_y_near_0 =
      IsWithinMargin(clipped_onscreen_rect.y(), 0.0);
  if (!is_onscreen_rect_x_near_0 && !is_onscreen_rect_y_near_0) {
    // Not fullscreen letterboxing mode.
    return;
  }

  if (!IsWithinMargin(clipped_onscreen_rect.width(), monitor_size.width()) &&
      !IsWithinMargin(clipped_onscreen_rect.height(), monitor_size.height())) {
    // Not fullscreen letterboxing mode.
    return;
  }

  // Scrolling down during video fullscreen letterboxing will change the
  // position of the whole clipped_onscreen_rect, which makes it not cover
  // the whole screen with its black bar surroundings. In this case, the
  // adjustment should be stopped. (http://crbug.com/1371976)
  if (is_onscreen_rect_x_near_0 &&
      !IsWithinMargin(
          clipped_onscreen_rect.y() * 2.0 + clipped_onscreen_rect.height(),
          monitor_size.height())) {
    // Not fullscreen letterboxing mode.
    return;
  }

  if (is_onscreen_rect_y_near_0 &&
      !IsWithinMargin(
          clipped_onscreen_rect.x() * 2.0 + clipped_onscreen_rect.width(),
          monitor_size.width())) {
    // Not fullscreen letterboxing mode.
    return;
  }

  if (params.clip_rect.has_value()) {
    if (is_onscreen_rect_x_near_0 &&
        !IsWithinMargin(overlay_onscreen_rect.width(), monitor_size.width())) {
      // Not fullscreen letterboxing mode.
      return;
    }
    if (is_onscreen_rect_y_near_0 &&
        !IsWithinMargin(overlay_onscreen_rect.height(),
                        monitor_size.height())) {
      // Not fullscreen letterboxing mode.
      return;
    }
  }

  //
  // Adjust the on-screen rect.
  //
  // Make sure the on-screen rect touches both the screen borders, and the
  // on-screen rect is right in the center. At the same time, make sure the
  // origin position for |new_onscreen_rect| with round-up integer so that no
  // extra blank bar shows up.
  gfx::Rect new_onscreen_rect = gfx::ToNearestRect(clipped_onscreen_rect);
  if (is_onscreen_rect_x_near_0) {
    new_onscreen_rect.set_x(0);
    new_onscreen_rect.set_width(monitor_size.width());
    int new_y = (monitor_size.height() - new_onscreen_rect.height()) / 2;
    if (new_y < new_onscreen_rect.y()) {
      // If new_onscreen_rect needs to be moved up by n lines, we add n
      // lines to the video onscreen rect height.
      new_onscreen_rect.set_height(new_onscreen_rect.height() +
                                   new_onscreen_rect.y() - new_y);
      new_onscreen_rect.set_y(new_y);
    } else if (new_y > new_onscreen_rect.y()) {
      // If new_onscreen_rect needs to be moved down by n lines, we keep
      // the original point of the video onscreen rect. Meanwhile, increase its
      // size to make it symmetrical around the monitor center.
      new_onscreen_rect.set_height(monitor_size.height() -
                                   new_onscreen_rect.y() * 2);
    }

    // Make new_onscreen_rect height even.
    if (new_onscreen_rect.height() % 2 == 1) {
      new_onscreen_rect.set_height(new_onscreen_rect.height() + 1);
    }
  }

  if (is_onscreen_rect_y_near_0) {
    new_onscreen_rect.set_y(0);
    new_onscreen_rect.set_height(monitor_size.height());
    int new_x = (monitor_size.width() - new_onscreen_rect.width()) / 2;
    if (new_x < new_onscreen_rect.x()) {
      // If new_onscreen_rect needs to be moved left by n lines, we add n
      // lines to the video onscreen rect width.
      new_onscreen_rect.set_width(new_onscreen_rect.width() +
                                  new_onscreen_rect.x() - new_x);
      new_onscreen_rect.set_x(new_x);
    } else if (new_x > new_onscreen_rect.x()) {
      // If new_onscreen_rect needs to be moved right by n lines, we keep
      // the original point of the video onscreen rect. Meanwhile, increase its
      // size to make it symmetrical around the monitor center.
      new_onscreen_rect.set_width(monitor_size.width() -
                                  new_onscreen_rect.x() * 2);
    }

    // Make new_onscreen_rect width even.
    if (new_onscreen_rect.width() % 2 == 1) {
      new_onscreen_rect.set_width(new_onscreen_rect.width() + 1);
    }
  }

  gfx::RectF new_onscreen_rect_float = gfx::RectF(new_onscreen_rect);

  // Skip adjustment if the current swap chain size is already correct.
  if (new_onscreen_rect_float != clipped_onscreen_rect) {
    //
    // Adjust the clip rect.
    //
    if (params.clip_rect.has_value()) {
      *visual_clip_rect = new_onscreen_rect_float;
    }

    //
    // Adjust the swap chain size if needed.
    //
    // The swap chain is either the size of overlay_onscreen_rect or
    // min(overlay_onscreen_rect, content_rect). The swap chain might not need
    // to be updated if it's the content size. After UpdateSwapChainTransform()
    // in CalculateSwapChainSize(), |visual_transform| transforms the swap
    // chain to the on-screen rect. Now update |visual_transform| so it still
    // produces the same on-screen rect after changing the swapchain.
    float scale_x;
    float scale_y;
    if (*swap_chain_size == overlay_onscreen_rect.size()) {
      scale_x =
          swap_chain_size->width() * 1.0f / new_onscreen_rect_float.width();
      scale_y =
          swap_chain_size->height() * 1.0f / new_onscreen_rect_float.height();
      visual_transform->Scale(scale_x, scale_y);

      *swap_chain_size = new_onscreen_rect_float.size();
    }

    //
    // Adjust the transform matrix.
    //
    // Add the new scale that scales |overlay_onscreen_rect| to
    // |new_onscreen_rect|. The new |visual_transform| will produce a new width
    // or a new height of the monitor size.
    scale_x =
        new_onscreen_rect_float.width() * 1.0f / overlay_onscreen_rect.width();
    scale_y = new_onscreen_rect_float.height() * 1.0f /
              overlay_onscreen_rect.height();
    visual_transform->Scale(scale_x, scale_y);

    // Update the origin.
    gfx::RectF unmapped_rect = gfx::RectF(
        gfx::PointF(params.quad_rect.origin()), gfx::SizeF(*swap_chain_size));
    gfx::RectF mapped_rect = visual_transform->MapRect(unmapped_rect);

    auto offset = new_onscreen_rect_float.OffsetFromOrigin() -
                  mapped_rect.OffsetFromOrigin();
    visual_transform->PostTranslate(offset);
  }

  // Full screen letterboxing overlay scenario can be optimized by DWM, like to
  // turn off the topmost desktop plane to save power.
  // Here the destination surface size is set to the whole monitor, while the
  // target region is set to the visual clip rectangle on the screen.
  if (params.z_order > 0) {
    if (base::FeatureList::IsEnabled(kDisableVPBLTUpscale) &&
        (std::abs(visual_transform->rc(0, 0)) > 1.0f) &&
        (std::abs(visual_transform->rc(1, 1)) > 1.0f)) {
      // Since DWM will perform the transform scaling on dest_size/target_rect
      // when display, so the inverse scaling ratio should be applied in the
      // process of calculating dest_size/target_rect than directly using
      // the monitor size.
      float inverse_scale_x = 1.0f / std::abs(visual_transform->rc(0, 0));
      float inverse_scale_y = 1.0f / std::abs(visual_transform->rc(1, 1));
      *dest_size =
          gfx::ScaleSize(monitor_size, inverse_scale_x, inverse_scale_y);
      *target_rect =
          gfx::ScaleRect(*visual_clip_rect, inverse_scale_x, inverse_scale_y);
    } else {
      *dest_size = monitor_size;
      *target_rect = *visual_clip_rect;
    }
  } else {
    // For underlay scenario, keep the destination surface size and target
    // region according to swap chain size.
    *dest_size = *swap_chain_size;
    *target_rect = gfx::RectF(*swap_chain_size);
  }

#if DCHECK_IS_ON()
  {
    // Verify if the new transform matrix transforms the swap chain correctly.
    gfx::RectF new_swap_chain_rect = gfx::RectF(
        gfx::PointF(params.quad_rect.origin()), gfx::SizeF(*swap_chain_size));

    gfx::RectF result_rect = visual_transform->MapRect(new_swap_chain_rect);
    if (params.clip_rect.has_value()) {
      result_rect.Intersect(*visual_clip_rect);
    }
    gfx::RectF new_onscreen_rect_local = new_onscreen_rect_float;

    // TODO(crbug.com/40866962): Remove these crash keys.
    gfx::Transform new_visual_transform = *visual_transform;
    base::debug::Alias(&new_swap_chain_rect);
    base::debug::Alias(&result_rect);
    base::debug::Alias(&new_onscreen_rect_local);
    base::debug::Alias(&new_visual_transform);
    // https://crbug.com/1366493: "DCHECK_EQ(result_rect.x(), 0);" sometimes
    // failed in the field. But here we collect possible crashes in general.
    // https://crbug.com/1517344 might also be triggered similarly.
    static auto* new_swap_chain_rect_key = base::debug::AllocateCrashKeyString(
        "new-swap-chain-rect", base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString scoped_crash_key_1(
        new_swap_chain_rect_key, new_swap_chain_rect.ToString());
    static auto* visual_transform_key = base::debug::AllocateCrashKeyString(
        "visual-transform", base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString scoped_crash_key_2(
        visual_transform_key, visual_transform->ToString());
    static auto* result_rect_key = base::debug::AllocateCrashKeyString(
        "result-rect", base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString scoped_crash_key_3(
        result_rect_key, result_rect.ToString());

    // Here we use 0.01f as the check tolerance for floating-point numbers,
    // since eventually the size adjustment for overlay will be rounded to be
    // integral.
    constexpr float kTolerance = 0.01f;
    if (is_onscreen_rect_x_near_0) {
      DCHECK_LE(std::abs(result_rect.x()), kTolerance);
      DCHECK_LE(std::abs(result_rect.width() - monitor_size.width()),
                kTolerance);
    }

    if (is_onscreen_rect_y_near_0) {
      DCHECK_LE(std::abs(result_rect.y()), kTolerance);
      DCHECK_LE(std::abs(result_rect.height() - monitor_size.height()),
                kTolerance);
    }
  }
#endif
}

gfx::Size SwapChainPresenter::CalculateSwapChainSize(
    const DCLayerOverlayParams& params,
    gfx::Transform* visual_transform,
    gfx::Rect* visual_clip_rect,
    std::optional<gfx::Size>* dest_size,
    std::optional<gfx::Rect>* target_rect) const {
  gfx::RectF visual_clip_rect_float = gfx::RectF(*visual_clip_rect);
  std::optional<gfx::SizeF> dest_size_float;
  std::optional<gfx::RectF> target_rect_float;

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
  gfx::RectF overlay_onscreen_rect = visual_transform->MapRect(quad_rect_float);

  // If transform isn't a scale or translation then swap chain can't be promoted
  // to an overlay so avoid blitting to a large surface unnecessarily.  Also,
  // after the video rotation fix (crbug.com/904035), using rotated size for
  // swap chain size will cause stretching since there's no squashing factor in
  // the transform to counteract.
  // Downscaling doesn't work on Intel display HW, and so DWM will perform an
  // extra BLT to avoid HW downscaling. This prevents the use of hardware
  // overlays especially for protected video. Use the onscreen size (scale==1)
  // for overlay can avoid this problem.
  // TODO(sunnyps): Support 90/180/270 deg rotations using video context.

  // On battery_power mode, set swap_chain_size to the source content size when
  // the swap chain presents upscaled overlay, multi-plane overlay hardware will
  // perform an upscaling operation instead of video processor(VP). Disabling VP
  // upscaled BLT is more power saving as the video processor can do the minimal
  // amount of work and the overlay has to read the minimal amount of data.
  bool can_disable_vp_upscaling_blt =
      base::FeatureList::IsEnabled(kDisableVPBLTUpscale) &&
      is_on_battery_power_ && std::abs(params.transform.rc(0, 0)) > 1.0f &&
      std::abs(params.transform.rc(1, 1)) > 1.0f;

  if (visual_transform->IsScaleOrTranslation() &&
      !can_disable_vp_upscaling_blt) {
    swap_chain_size = overlay_onscreen_rect.size();
  }

  // 4:2:2 subsampled formats like YUY2 must have an even width, and 4:2:0
  // subsampled formats like NV12 must have an even width and height..
  gfx::Size swap_chain_size_rounded = gfx::ToRoundedSize(swap_chain_size);
  if (swap_chain_size_rounded.width() % 2 == 1) {
    swap_chain_size.set_width(swap_chain_size.width() + 1);
  }
  if (swap_chain_size_rounded.height() % 2 == 1) {
    swap_chain_size.set_height(swap_chain_size.height() + 1);
  }

  // Adjust the transform matrix.
  UpdateSwapChainTransform(params.quad_rect.size(), swap_chain_size,
                           visual_transform);

  // In order to get the fullscreen DWM optimizations, the overlay onscreen rect
  // must fit the monitor when in non-letterboxing fullscreen mode. Adjust
  // |swap_chain_size|, |visual_transform| and |visual_clip_rect| so
  // |overlay_onscreen_rect| is the same as the monitor rect.
  // Specially for fullscreen overlays with letterboxing effect,
  // |overlay_onscreen_rect| will be placed in the center of the screen, and
  // either left/right edges or top/bottom edges will touch the monitor edges.
  if (visual_transform->IsScaleOrTranslation()) {
    AdjustTargetToOptimalSizeIfNeeded(
        params, overlay_onscreen_rect, &swap_chain_size, visual_transform,
        &visual_clip_rect_float, &dest_size_float, &target_rect_float);

    *visual_clip_rect = gfx::ToNearestRect(visual_clip_rect_float);

    if (target_rect_float.has_value()) {
      gfx::RectF temp = target_rect_float.value();
      *target_rect = gfx::ToNearestRect(temp);
    }

    if (dest_size_float.has_value()) {
      gfx::SizeF temp = dest_size_float.value();
      *dest_size = gfx::ToRoundedSize(temp);
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
    const gfx::Transform& transform_to_root,
    const std::optional<gfx::Size> dest_size,
    const std::optional<gfx::Rect> target_rect) {
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
                                   content_rect, swap_chain_size, dest_size,
                                   target_rect)) {
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
    const gfx::Size& swap_chain_size,
    const std::optional<gfx::Size> dest_size,
    const std::optional<gfx::Rect> target_rect) {
  DCHECK(!swap_chain_size.IsEmpty());

  TRACE_EVENT2("gpu", "SwapChainPresenter::PresentToDecodeSwapChain",
               "content_rect", content_rect.ToString(), "swap_chain_size",
               swap_chain_size.ToString());

  Microsoft::WRL::ComPtr<IDXGIResource> decode_resource;
  texture.As(&decode_resource);
  DCHECK(decode_resource);

  HRESULT hr = S_OK;
  if (!decode_swap_chain_ || decode_resource_ != decode_resource) {
    TRACE_EVENT0(
        "gpu",
        "SwapChainPresenter::PresentToDecodeSwapChain::CreateDecodeSwapChain");
    ReleaseSwapChainResources();

    decode_resource_ = decode_resource;

    HANDLE handle = INVALID_HANDLE_VALUE;
    if (!CreateSurfaceHandleHelper(&handle))
      return false;
    swap_chain_handle_.Set(handle);

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    d3d11_device_.As(&dxgi_device);
    DCHECK(dxgi_device);
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    dxgi_device->GetAdapter(&dxgi_adapter);
    DCHECK(dxgi_adapter);
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
        d3d11_device_.Get(), swap_chain_handle_.Get(), &desc,
        decode_resource_.Get(), nullptr, &decode_swap_chain_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateDecodeSwapChainForCompositionSurfaceHandle failed "
                     "with error 0x"
                  << std::hex << hr;
      return false;
    }
    DCHECK(decode_swap_chain_);
    SetSwapChainPresentDuration();

    Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
    dcomp_device_.As(&desktop_device);
    DCHECK(desktop_device);

    hr = desktop_device->CreateSurfaceFromHandle(swap_chain_handle_.Get(),
                                                 &decode_surface_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateSurfaceFromHandle failed with error 0x" << std::hex
                  << hr;
      return false;
    }
    DCHECK(decode_surface_);

    content_ = decode_surface_.Get();
  }

  RECT source_rect = content_rect.ToRECT();
  hr = decode_swap_chain_->SetSourceRect(&source_rect);
  if (FAILED(hr)) {
    DLOG(ERROR) << "SetSourceRect failed with error 0x" << std::hex << hr;
    return false;
  }

  gfx::Size swap_chain_dest_size =
      dest_size.has_value() ? dest_size.value() : swap_chain_size;
  hr = decode_swap_chain_->SetDestSize(swap_chain_dest_size.width(),
                                       swap_chain_dest_size.height());
  if (FAILED(hr)) {
    DLOG(ERROR) << "SetDestSize failed with error 0x" << std::hex << hr;
    return false;
  }

  RECT swap_chain_target_rect = target_rect.has_value()
                                    ? (target_rect.value()).ToRECT()
                                    : gfx::Rect(swap_chain_size).ToRECT();
  hr = decode_swap_chain_->SetTargetRect(&swap_chain_target_rect);
  if (FAILED(hr)) {
    DLOG(ERROR) << "SetTargetRect failed with error 0x" << std::hex << hr;
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
    DLOG(ERROR) << "SetColorSpace failed with error 0x" << std::hex << hr;
    return false;
  }

  UINT present_flags = DXGI_PRESENT_USE_DURATION;
  hr = decode_swap_chain_->PresentBuffer(array_slice, 1, present_flags);
  // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only indicates
  // that the window is occluded and we can stop rendering.
  if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
    DLOG(ERROR) << "PresentBuffer failed with error 0x" << std::hex << hr;
    return false;
  }

  swap_chain_size_ = swap_chain_size;
  content_size_ = swap_chain_size;
  swap_chain_format_ = DXGI_FORMAT_NV12;
  RecordPresentationStatistics();
  return true;
}

bool SwapChainPresenter::PresentToSwapChain(DCLayerOverlayParams& params,
                                            gfx::Transform* visual_transform,
                                            gfx::Rect* visual_clip_rect) {
  DCHECK(params.overlay_image);
  DCHECK_NE(params.overlay_image->type(),
            DCLayerOverlayType::kDCompVisualContent);
  CHECK(gfx::IsNearestRectWithinDistance(params.content_rect, 0.01f));

  DCLayerOverlayType overlay_type = params.overlay_image->type();

  *visual_transform = params.transform;
  *visual_clip_rect = params.clip_rect.value_or(gfx::Rect());

  if (overlay_type == DCLayerOverlayType::kDCompSurfaceProxy) {
    return PresentDCOMPSurface(params, visual_transform, visual_clip_rect);
  }

  // SwapChainPresenter can be reused when switching between MediaFoundation
  // (MF) video content and non-MF content; in such cases, the DirectComposition
  // (DCOMP) surface handle associated with the MF content needs to be cleared.
  // Doing so allows a DCOMP surface to be reset on the visual when MF
  // content is shown again.
  ReleaseDCOMPSurfaceResourcesIfNeeded();

  // Optional |dest_size| and |target_rect| are only calculated for full screen
  // letterboxing in |AdjustTargetForFullScreenLetterboxing|, which is guarded
  // by flag of DirectCompositionLetterboxVideoOptimization for now.
  std::optional<gfx::Size> dest_size;
  std::optional<gfx::Rect> target_rect;
  gfx::Size swap_chain_size = CalculateSwapChainSize(
      params, visual_transform, visual_clip_rect, &dest_size, &target_rect);

  if (overlay_type == DCLayerOverlayType::kNV12Texture &&
      !params.overlay_image->nv12_texture()) {
    // We can't proceed if overlay image has no underlying d3d11 texture.  It's
    // unclear how we get into this state, but we do observe crashes due to it.
    // Just stop here instead, and render incorrectly.
    // https://crbug.com/1077645
    DLOG(ERROR) << "Video NV12 texture is missing";
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

  // Enable VideoProcessor-HDR for SDR content if the monitor supports it and
  // the GPU driver version is not blocked (enable_vp_auto_hdr_). The actual GPU
  // driver support will be queried right after InitializeVideoProcessor() and
  // is checked in ToggleVpAutoHDR().
  bool use_vp_auto_hdr =
      !content_is_hdr &&
      DirectCompositionMonitorHDREnabled(layer_tree_->window()) &&
      enable_vp_auto_hdr_ && !is_on_battery_power_;

  bool use_hdr_swap_chain =
      ((content_is_hdr && params.video_params.hdr_metadata.IsValid()) ||
       use_vp_auto_hdr);

  DXGI_FORMAT swap_chain_format = GetSwapChainFormat(
      params.video_params.protected_video_type, use_hdr_swap_chain);

  bool swap_chain_format_changed = swap_chain_format != swap_chain_format_;
  bool toggle_protected_video = swap_chain_protected_video_type_ !=
                                params.video_params.protected_video_type;

  bool contents_changed = last_overlay_image_ != params.overlay_image;

  if (swap_chain_ && !swap_chain_resized && !swap_chain_format_changed &&
      !toggle_protected_video && !contents_changed) {
    // The swap chain is presenting the same images as last swap, which means
    // that the images were never returned to the video decoder and should
    // have the same contents as last time. It shouldn't need to be redrawn.
    // But the visual transform and clip rectangle for DCLayerTree update need
    // to keep the same as the last presentation when desktop plane was removed.
    if (last_desktop_plane_removed_) {
      SetTargetToFullScreen(visual_transform, visual_clip_rect, target_rect);
    }

    return true;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture =
      params.overlay_image->nv12_texture();
  unsigned input_level = params.overlay_image->texture_array_slice();

  if (TryPresentToDecodeSwapChain(input_texture, input_level, input_color_space,
                                  gfx::ToNearestRect(params.content_rect),
                                  swap_chain_size, swap_chain_format,
                                  params.transform, dest_size, target_rect)) {
    last_overlay_image_ = std::move(params.overlay_image);
    // Only NV12 format is supported in zero copy presentation path.
    if (dest_size.has_value() && target_rect.has_value() &&
        params.z_order > 0) {
      SetTargetToFullScreen(visual_transform, visual_clip_rect, target_rect);
    } else {
      last_desktop_plane_removed_ = false;
    }

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
    content_ = swap_chain_.Get();
    swap_chain_size_ = swap_chain_size;
    content_size_ = swap_chain_size;
  }

  if (input_texture) {
    staging_texture_.Reset();
    copy_texture_.Reset();
  } else {
    input_texture = UNSAFE_TODO(UploadVideoImage(
        params.overlay_image->size(), params.overlay_image->nv12_pixmap(),
        params.overlay_image->pixmap_stride()));
    input_level = 0;
  }

  std::optional<DXGI_HDR_METADATA_HDR10> stream_metadata;
  if (params.video_params.hdr_metadata.IsValid()) {
    stream_metadata = HDRMetadataHelperWin::HDRMetadataToDXGI(
        params.video_params.hdr_metadata);
  }

  if (!VideoProcessorBlt(std::move(input_texture), input_level,
                         gfx::ToNearestRect(params.content_rect),
                         input_color_space, stream_metadata, use_vp_auto_hdr)) {
    return false;
  }

  HRESULT hr, device_removed_reason;
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
        DLOG(ERROR) << "Present failed with error 0x" << std::hex << hr;
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
    d3d11_device_.As(&dxgi_device2);
    DCHECK(dxgi_device2);
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    hr = dxgi_device2->EnqueueSetEvent(event.handle());
    if (SUCCEEDED(hr)) {
      event.Wait();
    } else {
      device_removed_reason = d3d11_device_->GetDeviceRemovedReason();
      base::debug::Alias(&hr);
      base::debug::Alias(&device_removed_reason);
      base::debug::DumpWithoutCrashing();
    }
  }
  UINT flags = DXGI_PRESENT_USE_DURATION;
  UINT interval = 1;
  if (DirectCompositionSwapChainTearingEnabled()) {
    flags |= DXGI_PRESENT_ALLOW_TEARING;
    interval = 0;
  } else if (base::FeatureList::IsEnabled(
                 features::kDXGISwapChainPresentInterval0)) {
    interval = 0;
  }

  // DWM can turn off the desktop plane if this is a YUV swap chain and the
  // overlay candidate covers the whole screen with letterboxing.
  bool is_letterboxing_overlay_ready = false;
  if (IsYUVSwapChainFormat(swap_chain_format_) && dest_size.has_value() &&
      target_rect.has_value()) {
    // Try to QI IDXGIDecodeSwapChain and set the DXGI properties properly, in
    // order to turn off the desktop plane in case of overlay.
    bool succeeded = false;
    Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain;

    // Note that QI IDXGIDecodeSwapChain from an RGB swap chain will always
    // fail.
    hr = swap_chain_->QueryInterface(IID_PPV_ARGS(&decode_swap_chain));
    if (SUCCEEDED(hr)) {
      succeeded = TryDisableDesktopPlane(decode_swap_chain.Get(), *dest_size,
                                         *target_rect);
    } else {
      DLOG(ERROR)
          << "QueryInterface for IDXGIDecodeSwapChain failed with error 0x"
          << std::hex << hr;
    }

    // There should be no other UI content overtop of the video, so that the
    // letterboxing and positioning can be carried out by DWM. In case of
    // underlay, both |dest_size| and |target_rect| are initialized according to
    // swap_chain_size, thus no extra target transform and clip adjustment is
    // needed as follow-ups.
    if (succeeded && params.z_order > 0) {
      is_letterboxing_overlay_ready = true;
    }
  }

  // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only indicates
  // that the window is occluded and we can stop rendering.
  hr = swap_chain_->Present(interval, flags);
  if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
    DLOG(ERROR) << "Present failed with error 0x" << std::hex << hr;
    return false;
  }

  // Update |visual_transform| and |visual_clip_rect| for the full screen
  // letterboxing overlay presentation.
  if (is_letterboxing_overlay_ready) {
    SetTargetToFullScreen(visual_transform, visual_clip_rect, target_rect);
  } else {
    last_desktop_plane_removed_ = false;
  }

  last_overlay_image_ = std::move(params.overlay_image);
  RecordPresentationStatistics();
  return true;
}

void SwapChainPresenter::SetFrameRate(float frame_rate) {
  frame_rate_ = frame_rate;
  SetSwapChainPresentDuration();
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

bool SwapChainPresenter::PresentDCOMPSurface(DCLayerOverlayParams& params,
                                             gfx::Transform* visual_transform,
                                             gfx::Rect* visual_clip_rect) {
  auto* dcomp_surface_proxy = params.overlay_image->dcomp_surface_proxy();
  last_overlay_image_ = std::move(params.overlay_image);

  dcomp_surface_proxy->SetParentWindow(layer_tree_->window());
  gfx::Rect mapped_rect;

  // Apply fullscreen rounding and transform to video and notify DCOMPTexture.
  // For the DCOMP Surface presentation path we don't create a swap chain, but
  // we expect the Media Engine to use the on screen rect as its
  // representation.
  gfx::RectF overlay_onscreen_rect =
      visual_transform->MapRect(gfx::RectF(params.quad_rect));
  gfx::SizeF on_screen_size_float = overlay_onscreen_rect.size();

  gfx::RectF visual_clip_rect_float = gfx::RectF(*visual_clip_rect);
  std::optional<gfx::SizeF> dest_size;
  std::optional<gfx::RectF> target_rect;

  // In order to get the fullscreen DWM optimizations, the overlay onscreen
  // rect must fit the monitor when in non-letterboxing fullscreen mode.
  // Adjust |swap_chain_size|, |visual_transform| and |visual_clip_rect| so
  // |overlay_onscreen_rect| is the same as the monitor rect.
  // Specially for fullscreen overlays with letterboxing effect,
  // |overlay_onscreen_rect| will be placed in the center of the screen, and
  // either left/right edges or top/bottom edges will touch the monitor edges.
  if (visual_transform->IsScaleOrTranslation()) {
    AdjustTargetToOptimalSizeIfNeeded(
        params, overlay_onscreen_rect, &on_screen_size_float, visual_transform,
        &visual_clip_rect_float, &dest_size, &target_rect);
  }

  mapped_rect = visual_transform->MapRect(params.quad_rect);

  // Note: do not intersect clip rect w/ mapped_rect. This will result
  // in Media Foundation scaling the full video to the clipped region,
  // instead of allowing clipping to a portion of the video.

  dcomp_surface_proxy->SetRect(mapped_rect);

  dcomp_surface_proxy->SetProtectedVideoType(
      params.video_params.protected_video_type);

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

  // TODO(crbug.com/40642952): Call UpdateVisuals() here.

  // Scaling is handled by the MF video renderer, so we only need the
  // translation component.
  gfx::Vector2dF visual_transform_offset = visual_transform->To2dTranslation();
  visual_transform->MakeIdentity();
  visual_transform->Translate(visual_transform_offset);

#if DCHECK_IS_ON()
  TRACE_EVENT2("gpu", "PresentDCOMPSurface", "finalized transform",
               visual_transform->ToString(), "finalized mapped rect",
               mapped_rect.ToString());
#endif  // DCHECK_IS_ON()

  // This visual's content was a different DC surface.
  HANDLE surface_handle = dcomp_surface_proxy->GetSurfaceHandle();
  if (dcomp_surface_handle_ != surface_handle) {
    DVLOG(2) << "Update visual's content. " << __func__ << "(" << this << ")";

    ReleaseSwapChainResources();

    Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface;
    Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_device1;
    HRESULT hr = dcomp_device_.As(&dcomp_device1);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get DCOMP device. hr=" << hr;
      return false;
    }

    hr = dcomp_device1->CreateSurfaceFromHandle(surface_handle, &dcomp_surface);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create DCOMP surface. hr=" << hr;
      return false;
    }

    content_ = dcomp_surface.Get();
    content_size_ = content_size;
    // Don't take ownership of handle as the DCOMPSurfaceProxy instance owns it.
    dcomp_surface_handle_ = surface_handle;
  }

  return true;
}

void SwapChainPresenter::ReleaseDCOMPSurfaceResourcesIfNeeded() {
  if (dcomp_surface_handle_ != INVALID_HANDLE_VALUE) {
    dcomp_surface_handle_ = INVALID_HANDLE_VALUE;
    last_overlay_image_.reset();
    content_.Reset();
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
      DLOG(ERROR) << " SetColorSpace1 failed with error: " << hr;
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
      DLOG(ERROR) << "CreateVideoProcessorInputView failed with error 0x"
                  << std::hex << hr;
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
        DLOG(ERROR) << "CreateVideoProcessorOutputView failed with error 0x"
                    << std::hex << hr;
        return false;
      }
      DCHECK(output_view_);
    }

    if (enable_vp_auto_hdr_) {
      hr = ToggleVpAutoHDR(gpu_vendor_id_, driver_supports_vp_auto_hdr,
                           video_context.Get(), video_processor.Get(),
                           use_vp_auto_hdr);
      if (FAILED(hr)) {
        enable_vp_auto_hdr_ = false;

        if (use_vp_auto_hdr) {
          if (!RevertSwapChainToSDR(video_device, video_processor,
                                    video_processor_enumerator, swap_chain3,
                                    context1, src_color_space)) {
            return false;
          }

          use_vp_auto_hdr = false;
        }
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
    base::UmaHistogramSparse(
        (use_vp_auto_hdr ? "GPU.VideoProcessorBlt.VpAutoHDR.On"
                         : "GPU.VideoProcessorBlt.VpAutoHDR.Off"),
        hr);
    base::UmaHistogramSparse(
        (use_vp_super_resolution
             ? "GPU.VideoProcessorBlt.VpSuperResolution.On"
             : "GPU.VideoProcessorBlt.VpSuperResolution.Off"),
        hr);

    // Retry VideoProcessorBlt with VpSuperResolution off if it was on.
    if (FAILED(hr) && use_vp_super_resolution) {
      DLOG(ERROR) << "Retry VideoProcessorBlt with VpSuperResolution off "
                     "after it failed with error 0x"
                  << std::hex << hr;

      ToggleVpSuperResolution(gpu_vendor_id_, video_context.Get(),
                              video_processor.Get(), false);
      {
        TRACE_EVENT0("gpu", "ID3D11VideoContext::VideoProcessorBlt");
        hr = video_context->VideoProcessorBlt(
            video_processor.Get(), output_view_.Get(), 0, 1, &stream);
      }
      base::UmaHistogramSparse(
          "GPU.VideoProcessorBlt.VpSuperResolution.RetryOffAfterError", hr);

      // We shouldn't use VpSuperResolution if it was the reason that caused
      // the VideoProcessorBlt failure.
      if (SUCCEEDED(hr)) {
        enable_vp_super_resolution_ = false;
      }
    }

    if (FAILED(hr) && use_vp_auto_hdr) {
      DLOG(ERROR) << "Retry VideoProcessorBlt with VpAutoHDR off "
                     "after it failed with error 0x"
                  << std::hex << hr;

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
      base::UmaHistogramSparse(
          "GPU.VideoProcessorBlt.VpAutoHDR.RetryOffAfterError", hr);

      // We shouldn't use VpAutoHDR if it was the reason that caused
      // the VideoProcessorBlt failure.
      if (SUCCEEDED(hr)) {
        enable_vp_auto_hdr_ = false;
      }
    }

    if (FAILED(hr)) {
      DLOG(ERROR) << "VideoProcessorBlt failed with error 0x" << std::hex << hr;

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
    output_view_.Reset();
    swap_chain_.Reset();
    swap_chain_handle_.Close();
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
  d3d11_device_.As(&dxgi_device);
  DCHECK(dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  DCHECK(dxgi_adapter);
  Microsoft::WRL::ComPtr<IDXGIFactoryMedia> media_factory;
  dxgi_adapter->GetParent(IID_PPV_ARGS(&media_factory));
  DCHECK(media_factory);

  // The composition surface handle is only used to create YUV swap chains since
  // CreateSwapChainForComposition can't do that.
  HANDLE handle = INVALID_HANDLE_VALUE;
  if (!CreateSurfaceHandleHelper(&handle))
    return false;
  swap_chain_handle_.Set(handle);

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
    HRESULT hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle_.Get(), &desc, nullptr,
        &swap_chain_);
    failed_to_create_yuv_swapchain_ = FAILED(hr);

    base::UmaHistogramSparse(kSwapChainCreationResultByVideoTypeUmaPrefix +
                                 protected_video_type_string,
                             hr);

    if (failed_to_create_yuv_swapchain_) {
      DLOG(ERROR) << "Failed to create "
                  << DxgiFormatToString(swap_chain_format)
                  << " swap chain of size " << swap_chain_size.ToString()
                  << " with error 0x" << std::hex << hr
                  << "\nFalling back to BGRA";
      use_yuv_swap_chain = false;
      swap_chain_format = DXGI_FORMAT_B8G8R8A8_UNORM;
    }
  }
  if (!use_yuv_swap_chain) {
    TRACE_EVENT1("gpu", "SwapChainPresenter::ReallocateSwapChain::BGRA",
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

    HRESULT hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle_.Get(), &desc, nullptr,
        &swap_chain_);

    base::UmaHistogramSparse(kSwapChainCreationResultByVideoTypeUmaPrefix +
                                 protected_video_type_string,
                             hr);

    if (FAILED(hr)) {
      // Disable overlay support so dc_layer_overlay will stop sending down
      // overlay frames here and uses GL Composition instead.
      DisableDirectCompositionOverlays();
      DLOG(ERROR) << "Failed to create "
                  << DxgiFormatToString(swap_chain_format)
                  << " swap chain of size " << swap_chain_size.ToString()
                  << " with error 0x" << std::hex << hr
                  << ". Disable overlay swap chains";
      return false;
    }
  }

  if (DXGIWaitableSwapChainEnabled()) {
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3;
    if (SUCCEEDED(swap_chain_.As(&swap_chain3))) {
      HRESULT hr = swap_chain3->SetMaximumFrameLatency(
          GetDXGIWaitableSwapChainMaxQueuedFrames());
      DCHECK(SUCCEEDED(hr)) << "SetMaximumFrameLatency failed with error "
                            << logging::SystemErrorCodeToString(hr);
    }
  }

  LabelSwapChainAndBuffers(swap_chain_.Get(), "SwapChainPresenter");

  swap_chain_format_ = swap_chain_format;
  SetSwapChainPresentDuration();

  DXGI_ADAPTER_DESC adapter_desc;
  HRESULT hr = dxgi_adapter->GetDesc(&adapter_desc);
  if (SUCCEEDED(hr)) {
    gpu_vendor_id_ = adapter_desc.VendorId;
  } else {
    DLOG(ERROR) << "Failed to get adapter desc with error 0x" << std::hex << hr;
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
    UINT duration_100ns = FrameRateToPresentDuration(frame_rate_);
    UINT requested_duration = 0u;
    if (duration_100ns > 0) {
      UINT smaller_duration = 0u, larger_duration = 0u;
      HRESULT hr = swap_chain_media->CheckPresentDurationSupport(
          duration_100ns, &smaller_duration, &larger_duration);
      if (FAILED(hr)) {
        DLOG(ERROR) << "CheckPresentDurationSupport failed with error "
                    << std::hex << hr;
        return;
      }
      constexpr UINT kDurationThreshold = 1000u;
      // Smaller duration should be used to avoid frame loss. However, we want
      // to take into consideration the larger duration is the same as the
      // requested duration but was slightly different due to frame rate
      // estimation errors.
      if (larger_duration > 0 &&
          larger_duration - duration_100ns < kDurationThreshold) {
        requested_duration = larger_duration;
      } else if (smaller_duration > 0) {
        requested_duration = smaller_duration;
      }
    }
    HRESULT hr = swap_chain_media->SetPresentDuration(requested_duration);
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetPresentDuration failed with error " << std::hex << hr;
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
          GetSwapChainFormat(swap_chain_protected_video_type_, /*hdr=*/false),
          swap_chain_protected_video_type_)) {
    ReleaseSwapChainResources();
    return false;
  }
  content_ = swap_chain_.Get();

  Microsoft::WRL::ComPtr<ID3D11Texture2D> swap_chain_buffer;
  swap_chain_->GetBuffer(0, IID_PPV_ARGS(&swap_chain_buffer));
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_desc = {};
  output_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  output_desc.Texture2D.MipSlice = 0;
  HRESULT hr = video_device->CreateVideoProcessorOutputView(
      swap_chain_buffer.Get(), video_processor_enumerator.Get(), &output_desc,
      &output_view_);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateVideoProcessorOutputView failed with error 0x"
                << std::hex << hr;
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
    DLOG(ERROR) << "SetColorSpace1 failed with error 0x" << std::hex << hr;
    return false;
  }

  return true;
}

}  // namespace gl
