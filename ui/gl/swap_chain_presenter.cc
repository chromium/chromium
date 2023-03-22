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

bool IsProtectedVideo(gfx::ProtectedVideoType protected_video_type) {
  return protected_video_type != gfx::ProtectedVideoType::kClear;
}

class ScopedReleaseKeyedMutex {
 public:
  ScopedReleaseKeyedMutex(Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
                          UINT64 key)
      : keyed_mutex_(keyed_mutex), key_(key) {
    DCHECK(keyed_mutex);
  }

  ScopedReleaseKeyedMutex(const ScopedReleaseKeyedMutex&) = delete;
  ScopedReleaseKeyedMutex& operator=(const ScopedReleaseKeyedMutex&) = delete;

  ~ScopedReleaseKeyedMutex() {
    HRESULT hr = keyed_mutex_->ReleaseSync(key_);
    DCHECK(SUCCEEDED(hr));
  }

 private:
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
  UINT64 key_ = 0;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OverlayFullScreenTypes {
  kWindowMode,
  kFullScreenMode,
  kFullScreenInWidthOnly,
  kFullScreenInHeightOnly,
  kOverSizedFullScreen,
  kNotAvailable,
  kMaxValue = kNotAvailable,
};

enum : size_t {
  kSwapChainImageIndex = 0,
  kNV12ImageIndex = 0,
  kYPlaneImageIndex = 0,
  kUVPlaneImageIndex = 1,
};

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
      NOTREACHED();
      return "UNKNOWN";
  }
}

bool IsYUVSwapChainFormat(DXGI_FORMAT format) {
  if (format == DXGI_FORMAT_NV12 || format == DXGI_FORMAT_YUY2)
    return true;
  return false;
}

UINT BufferCount() {
  return base::FeatureList::IsEnabled(
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
                              const gfx::Size& swap_chain_size,
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

// Return true if VpSuperResolution has been set successfully.
bool ToggleIntelVpSuperResolution(ID3D11VideoContext* video_context,
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
  base::UmaHistogramSparse(enable
                               ? "GPU.IntelVpSuperResolution.On.VpeFnVersion"
                               : "GPU.IntelVpSuperResolution.Off.VpeFnVersion",
                           hr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetOutputExtension failed with error 0x"
                << std::hex << hr;
    return false;
  }

  ext.function = kIntelVpeFnMode;
  param = enable ? kIntelVpeModePreproc : kIntelVpeModeNone;
  hr = video_context->VideoProcessorSetOutputExtension(
      video_processor, &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
  base::UmaHistogramSparse(enable ? "GPU.IntelVpSuperResolution.On.VpeFnMode"
                                  : "GPU.IntelVpSuperResolution.Off.VpeFnMode",
                           hr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetOutputExtension failed with error 0x"
                << std::hex << hr;
    return false;
  }

  ext.function = kIntelVpeFnScaling;
  param = enable ? kIntelVpeScalingSuperResolution : kIntelVpeScalingDefault;

  hr = video_context->VideoProcessorSetStreamExtension(
      video_processor, 0, &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
  base::UmaHistogramSparse(enable
                               ? "GPU.IntelVpSuperResolution.On.VpeFnScaling"
                               : "GPU.IntelVpSuperResolution.Off.VpeFnScaling",
                           hr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "VideoProcessorSetStreamExtension failed with error 0x"
                << std::hex << hr;
    return false;
  }
  return enable;
}

// Return true if VpSuperResolution has been set successfully.
bool ToggleNvidiaVpSuperResolution(ID3D11VideoContext* video_context,
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
    return false;
  }
  return enable;
}

bool ToggleVpSuperResolution(UINT gpu_vendor_id,
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
  return false;
}

bool IsWithinMargin(int i, int j) {
  constexpr int kFullScreenMargin = 10;
  return (std::abs(i - j) < kFullScreenMargin);
}

// TODO(sunnyps): Move to DCLayerOverlayType header and make consistent with the
// type names after changing trace tests which depend on this.
std::string OverlayTypeToString(DCLayerOverlayType overlay_type) {
  std::string overlay_type_str;
  if (overlay_type == gl::DCLayerOverlayType::kDCompVisualContent) {
    overlay_type_str = "swap chain";
  } else if (overlay_type == gl::DCLayerOverlayType::kNV12Texture) {
    overlay_type_str = "hardware video frame";
  } else {
    overlay_type_str = "software video frame";
  }
  return overlay_type_str;
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
    HWND window,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device)
    : layer_tree_(layer_tree),
      window_(window),
      switched_to_BGRA8888_time_tick_(base::TimeTicks::Now()),
      d3d11_device_(d3d11_device),
      dcomp_device_(dcomp_device),
      is_on_battery_power_(
          base::PowerMonitor::AddPowerStateObserverAndReturnOnBatteryState(
              this)) {}

SwapChainPresenter::~SwapChainPresenter() {
  base::PowerMonitor::RemovePowerStateObserver(this);
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
  for (int y = 0; y < texture_size.height(); y++) {
    const uint8_t* src_row = nv12_pixmap + pixmap_stride * y;
    uint8_t* dest_row =
        reinterpret_cast<uint8_t*>(mapped_resource.pData) + dest_stride * y;
    memcpy(dest_row, src_row, texture_size.width());
  }

  const uint8_t* uv_src_start =
      nv12_pixmap + pixmap_stride * texture_size.height();
  uint8_t* uv_dst_start = reinterpret_cast<uint8_t*>(mapped_resource.pData) +
                          dest_stride * texture_size.height();
  for (int y = 0; y < texture_size.height() / 2; y++) {
    const uint8_t* src_row = uv_src_start + pixmap_stride * y;
    uint8_t* dest_row = uv_dst_start + dest_stride * y;
    memcpy(dest_row, src_row, texture_size.width());
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

gfx::Size SwapChainPresenter::GetMonitorSize() {
  if (GetDirectCompositionNumMonitors() == 1) {
    // Only one monitor. Return the size of this monitor.
    return GetDirectCompositionPrimaryMonitorSize();
  } else {
    gfx::Size monitor_size;
    // Get the monitor on which the overlay is displayed.
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    if (GetMonitorInfo(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST),
                       &monitor_info)) {
      monitor_size = gfx::Rect(monitor_info.rcMonitor).size();
    }

    return monitor_size;
  }
}

void SwapChainPresenter::AdjustTargetToOptimalSizeIfNeeded(
    const DCLayerOverlayParams& params,
    const gfx::Rect& overlay_onscreen_rect,
    gfx::Size* swap_chain_size,
    gfx::Transform* visual_transform,
    gfx::Rect* visual_clip_rect) {
  // First try to adjust the full screen overlay that can fit the whole
  // screen. If it cannot fit the whole screen and we know it's in
  // letterboxing mode, try to center the overlay and adjust only x or only y.
  gfx::Size monitor_size = GetMonitorSize();
  bool size_adjusted = AdjustTargetToFullScreenSizeIfNeeded(
      monitor_size, params, overlay_onscreen_rect, swap_chain_size,
      visual_transform, visual_clip_rect);
  if (!size_adjusted && params.maybe_video_fullscreen_letterboxing) {
    AdjustTargetForFullScreenLetterboxing(
        monitor_size, params, overlay_onscreen_rect, swap_chain_size,
        visual_transform, visual_clip_rect);
  }
}

bool SwapChainPresenter::AdjustTargetToFullScreenSizeIfNeeded(
    const gfx::Size& monitor_size,
    const DCLayerOverlayParams& params,
    const gfx::Rect& overlay_onscreen_rect,
    gfx::Size* swap_chain_size,
    gfx::Transform* visual_transform,
    gfx::Rect* visual_clip_rect) {
  if (monitor_size.IsEmpty())
    return false;

  gfx::Rect clipped_onscreen_rect = overlay_onscreen_rect;
  if (params.clip_rect.has_value())
    clipped_onscreen_rect.Intersect(*visual_clip_rect);

  // Because of the rounding when converting between pixels and DIPs, a
  // fullscreen video can become slightly larger than the monitor - e.g. on
  // a 3000x2000 monitor with a scale factor of 1.75 a 1920x1079 video can
  // become 3002x1689.
  // Swapchains that are bigger than the monitor won't be put into overlays,
  // which will hurt power usage a lot. On those systems, the scaling can be
  // adjusted very slightly so that it's less than the monitor size. This
  // should be close to imperceptible. http://crbug.com/668278
  // The overlay must be positioned at (0, 0) in fullscreen mode.
  if (!IsWithinMargin(clipped_onscreen_rect.x(), 0) ||
      !IsWithinMargin(clipped_onscreen_rect.y(), 0)) {
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

  // Adjust the clip rect.
  if (params.clip_rect.has_value()) {
    *visual_clip_rect = gfx::Rect(monitor_size);
  }

  // Adjust the swap chain size.
  // The swap chain is either the size of overlay_onscreen_rect or
  // min(overlay_onscreen_rect, content_rect). It might not need to update if it
  // has the content size.
  if (IsWithinMargin(swap_chain_size->width(), monitor_size.width()) &&
      IsWithinMargin(swap_chain_size->height(), monitor_size.height())) {
    *swap_chain_size = monitor_size;
  }

  // Adjust the transform matrix.
  float scale_x = monitor_size.width() * 1.0f / swap_chain_size->width();
  float scale_y = monitor_size.height() * 1.0f / swap_chain_size->height();
  visual_transform->MakeIdentity();
  visual_transform->Scale(scale_x, scale_y);

  // Origin is probably (0,0) all the time. If not, adjust the origin.
  if (!params.quad_rect.origin().IsOrigin()) {
    auto new_origin = visual_transform->MapPoint(params.quad_rect.origin());
    visual_transform->PostTranslate(-new_origin.OffsetFromOrigin());
  }

  // The new transform matrix should transform the swap chain to the monitor
  // rect.
  DCHECK_EQ(visual_transform->MapRect(
                gfx::Rect(params.quad_rect.origin(), *swap_chain_size)),
            gfx::Rect(monitor_size));

  return true;
}

void SwapChainPresenter::AdjustTargetForFullScreenLetterboxing(
    const gfx::Size& monitor_size,
    const DCLayerOverlayParams& params,
    const gfx::Rect& overlay_onscreen_rect,
    gfx::Size* swap_chain_size,
    gfx::Transform* visual_transform,
    gfx::Rect* visual_clip_rect) {
  if (!base::FeatureList::IsEnabled(
          features::kDirectCompositionLetterboxVideoOptimization)) {
    return;
  }

  if (monitor_size.IsEmpty())
    return;

  gfx::Rect clipped_onscreen_rect = overlay_onscreen_rect;
  if (params.clip_rect.has_value())
    clipped_onscreen_rect.Intersect(*visual_clip_rect);

  bool is_onscreen_rect_x_near_0 = IsWithinMargin(clipped_onscreen_rect.x(), 0);
  bool is_onscreen_rect_y_near_0 = IsWithinMargin(clipped_onscreen_rect.y(), 0);
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
          clipped_onscreen_rect.y() * 2 + clipped_onscreen_rect.height(),
          monitor_size.height())) {
    // Not fullscreen letterboxing mode.
    return;
  }

  if (is_onscreen_rect_y_near_0 &&
      !IsWithinMargin(
          clipped_onscreen_rect.x() * 2 + clipped_onscreen_rect.width(),
          monitor_size.width())) {
    // Not fullscreen letterboxing mode.
    return;
  }

  // Adjust the onscreen rect to touch two screen borders, and also make sure
  // the onscreen rect be right in the center.
  // At the same time, make sure the origin position for clipped_onscreen_rect
  // with round-up integer so that no extra blank bar shows up.
  if (is_onscreen_rect_x_near_0) {
    clipped_onscreen_rect.set_x(0);
    clipped_onscreen_rect.set_width(monitor_size.width());
    int new_y = (monitor_size.height() - clipped_onscreen_rect.height()) / 2;
    if (new_y < clipped_onscreen_rect.y()) {
      // If clipped_onscreen_rect needs to be moved up by n lines, we add n
      // lines to the video onscreen rect height.
      clipped_onscreen_rect.set_height(clipped_onscreen_rect.height() +
                                       clipped_onscreen_rect.y() - new_y);
      clipped_onscreen_rect.set_y(new_y);
    } else if (new_y > clipped_onscreen_rect.y()) {
      // If clipped_onscreen_rect needs to be moved down by n lines, we keep
      // the original point of the video onscreen rect. Meanwhile, increase its
      // size to make it symmetrical around the monitor center.
      clipped_onscreen_rect.set_height(monitor_size.height() -
                                       clipped_onscreen_rect.y() * 2);
    }

    // Make clipped_onscreen_rect height even.
    if (clipped_onscreen_rect.height() % 2 == 1)
      clipped_onscreen_rect.set_height(clipped_onscreen_rect.height() + 1);
  }

  if (is_onscreen_rect_y_near_0) {
    clipped_onscreen_rect.set_y(0);
    clipped_onscreen_rect.set_height(monitor_size.height());
    int new_x = (monitor_size.width() - clipped_onscreen_rect.width()) / 2;
    if (new_x < clipped_onscreen_rect.x()) {
      // If clipped_onscreen_rect needs to be moved left by n lines, we add n
      // lines to the video onscreen rect width.
      clipped_onscreen_rect.set_width(clipped_onscreen_rect.width() +
                                      clipped_onscreen_rect.x() - new_x);
      clipped_onscreen_rect.set_x(new_x);
    } else if (new_x > clipped_onscreen_rect.x()) {
      // If clipped_onscreen_rect needs to be moved right by n lines, we keep
      // the original point of the video onscreen rect. Meanwhile, increase its
      // size to make it symmetrical around the monitor center.
      clipped_onscreen_rect.set_width(monitor_size.width() -
                                      clipped_onscreen_rect.x() * 2);
    }

    // Make clipped_onscreen_rect width even.
    if (clipped_onscreen_rect.width() % 2 == 1)
      clipped_onscreen_rect.set_width(clipped_onscreen_rect.width() + 1);
  }

  // Adjust the clip rect.
  if (params.clip_rect.has_value())
    *visual_clip_rect = clipped_onscreen_rect;

  // Swap chain size has been updated before. Do not update it if it is not
  // necessary.
  if (!IsWithinMargin(swap_chain_size->width(),
                      clipped_onscreen_rect.width()) ||
      !IsWithinMargin(swap_chain_size->height(),
                      clipped_onscreen_rect.height())) {
    *swap_chain_size = clipped_onscreen_rect.size();
  }

  // Adjust the transform matrix.
  float scale_x =
      clipped_onscreen_rect.width() * 1.0f / swap_chain_size->width();
  float scale_y =
      clipped_onscreen_rect.height() * 1.0f / swap_chain_size->height();
  visual_transform->set_rc(0, 3, clipped_onscreen_rect.x());
  visual_transform->set_rc(1, 3, clipped_onscreen_rect.y());
  visual_transform->set_rc(0, 0, scale_x);
  visual_transform->set_rc(1, 1, scale_y);

#if DCHECK_IS_ON()
  {
    // The new transform matrix should transform the swap chain correctly
    gfx::Rect new_swap_chain_rect(params.quad_rect.origin(), *swap_chain_size);
    gfx::Rect result_rect = visual_transform->MapRect(new_swap_chain_rect);
    gfx::Rect new_clipped_onscreen_rect = clipped_onscreen_rect;
    gfx::Transform new_visual_transform = *visual_transform;
    base::debug::Alias(&new_swap_chain_rect);
    base::debug::Alias(&result_rect);
    base::debug::Alias(&new_clipped_onscreen_rect);
    base::debug::Alias(&new_visual_transform);
    // https://crbug.com/1366493: "DCHECK_EQ(result_rect.x(), 0);" sometimes
    // failed in the field. But here we collect possible crashes in general.
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

    if (IsWithinMargin(clipped_onscreen_rect.x(), 0)) {
      DCHECK_EQ(result_rect.x(), 0);
      DCHECK_EQ(result_rect.width(), monitor_size.width());
    }

    if (IsWithinMargin(clipped_onscreen_rect.y(), 0)) {
      DCHECK_EQ(result_rect.y(), 0);
      DCHECK_EQ(result_rect.height(), monitor_size.height());
    }
  }
#endif
}

gfx::Size SwapChainPresenter::CalculateSwapChainSize(
    const DCLayerOverlayParams& params,
    gfx::Transform* visual_transform,
    gfx::Rect* visual_clip_rect) {
  // Swap chain size is the minimum of the on-screen size and the source size so
  // the video processor can do the minimal amount of work and the overlay has
  // to read the minimal amount of data. DWM is also less likely to promote a
  // surface to an overlay if it's much larger than its area on-screen.
  gfx::Size swap_chain_size = params.content_rect.size();
  if (swap_chain_size.IsEmpty())
    return gfx::Size();
  if (params.quad_rect.IsEmpty())
    return gfx::Size();
  gfx::Rect overlay_onscreen_rect = params.transform.MapRect(params.quad_rect);

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
  if (params.transform.IsScaleOrTranslation()) {
    swap_chain_size = overlay_onscreen_rect.size();
  }

  // 4:2:2 subsampled formats like YUY2 must have an even width, and 4:2:0
  // subsampled formats like NV12 must have an even width and height.
  if (swap_chain_size.width() % 2 == 1)
    swap_chain_size.set_width(swap_chain_size.width() + 1);
  if (swap_chain_size.height() % 2 == 1)
    swap_chain_size.set_height(swap_chain_size.height() + 1);

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
    AdjustTargetToOptimalSizeIfNeeded(params, overlay_onscreen_rect,
                                      &swap_chain_size, visual_transform,
                                      visual_clip_rect);
  }

  return swap_chain_size;
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
  texture.As(&decode_resource);
  DCHECK(decode_resource);

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
    HRESULT hr =
        media_factory->CreateDecodeSwapChainForCompositionSurfaceHandle(
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
  decode_swap_chain_->SetSourceRect(&source_rect);

  decode_swap_chain_->SetDestSize(swap_chain_size.width(),
                                  swap_chain_size.height());
  RECT target_rect = gfx::Rect(swap_chain_size).ToRECT();
  decode_swap_chain_->SetTargetRect(&target_rect);

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
  decode_swap_chain_->SetColorSpace(
      static_cast<DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAGS>(color_space_flags));

  UINT present_flags = DXGI_PRESENT_USE_DURATION;
  HRESULT hr = decode_swap_chain_->PresentBuffer(array_slice, 1, present_flags);
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

  gl::DCLayerOverlayType overlay_type = params.overlay_image->type();

  *visual_transform = params.transform;
  *visual_clip_rect = params.clip_rect.value_or(gfx::Rect());

  if (overlay_type == gl::DCLayerOverlayType::kDCompSurfaceProxy) {
    return PresentDCOMPSurface(params, visual_transform, visual_clip_rect);
  }

  // SwapChainPresenter can be reused when switching between MediaFoundation
  // (MF) video content and non-MF content; in such cases, the DirectComposition
  // (DCOMP) surface handle associated with the MF content needs to be cleared.
  // Doing so allows a DCOMP surface to be reset on the visual when MF
  // content is shown again.
  ReleaseDCOMPSurfaceResourcesIfNeeded();

  gfx::Size content_size;
  gfx::Size swap_chain_size;
  if (overlay_type == gl::DCLayerOverlayType::kDCompVisualContent) {
    content_size = params.overlay_image->size();
    // |visual_transform| now scales from |content_size| to on screen bounds.
    UpdateSwapChainTransform(params.quad_rect.size(), content_size,
                             visual_transform);
  } else {
    swap_chain_size =
        CalculateSwapChainSize(params, visual_transform, visual_clip_rect);
    content_size = swap_chain_size;
  }

  TRACE_EVENT2("gpu", "SwapChainPresenter::PresentToSwapChain", "image_type",
               OverlayTypeToString(overlay_type), "size",
               content_size.ToString());

  // Swap chain image already has a swap chain that's presented by the client
  // e.g. for webgl/canvas low-latency/desynchronized mode.
  if (overlay_type == gl::DCLayerOverlayType::kDCompVisualContent) {
    DCHECK(params.overlay_image->dcomp_visual_content());
    if (last_overlay_image_ != params.overlay_image) {
      ReleaseSwapChainResources();
      content_ = params.overlay_image->dcomp_visual_content();
      content_size_ = content_size;
      last_overlay_image_ = std::move(params.overlay_image);
    }
    return true;
  }

  if (overlay_type == gl::DCLayerOverlayType::kNV12Texture &&
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

  gfx::ColorSpace input_color_space = params.color_space;
  if (!input_color_space.IsValid()) {
    input_color_space = gfx::ColorSpace::CreateREC709();
  }

  bool content_is_hdr = input_color_space.IsHDR();
  bool use_hdr_swap_chain = content_is_hdr && params.hdr_metadata.IsValid();

  DXGI_FORMAT swap_chain_format =
      GetSwapChainFormat(params.protected_video_type, use_hdr_swap_chain);
  bool swap_chain_format_changed = swap_chain_format != swap_chain_format_;
  bool toggle_protected_video =
      swap_chain_protected_video_type_ != params.protected_video_type;

  bool contents_changed = last_overlay_image_ != params.overlay_image;

  if (swap_chain_ && !swap_chain_resized && !swap_chain_format_changed &&
      !toggle_protected_video && !contents_changed) {
    // The swap chain is presenting the same images as last swap, which means
    // that the images were never returned to the video decoder and should
    // have the same contents as last time. It shouldn't need to be redrawn.
    return true;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture =
      params.overlay_image->nv12_texture();
  unsigned input_level = params.overlay_image->texture_array_slice();

  if (TryPresentToDecodeSwapChain(input_texture, input_level, input_color_space,
                                  params.content_rect, swap_chain_size,
                                  swap_chain_format, params.transform)) {
    last_overlay_image_ = std::move(params.overlay_image);
    return true;
  }

  // Reallocate swap chain if contents or properties change.
  if (!swap_chain_ || swap_chain_resized || swap_chain_format_changed ||
      toggle_protected_video) {
    if (!ReallocateSwapChain(swap_chain_size, swap_chain_format,
                             params.protected_video_type)) {
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
    input_texture = UploadVideoImage(params.overlay_image->size(),
                                     params.overlay_image->nv12_pixmap(),
                                     params.overlay_image->pixmap_stride());
    input_level = 0;
  }

  // Keyed mutex is not present if access is synchronized by the shared image.
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex =
      params.overlay_image->keyed_mutex();

  absl::optional<DXGI_HDR_METADATA_HDR10> stream_metadata;
  if (params.hdr_metadata.IsValid()) {
    stream_metadata =
        gl::HDRMetadataHelperWin::HDRMetadataToDXGI(params.hdr_metadata);
  }

  if (!VideoProcessorBlt(std::move(input_texture), input_level,
                         std::move(keyed_mutex), params.content_rect,
                         input_color_space, content_is_hdr, stream_metadata)) {
    return false;
  }

  HRESULT hr, device_removed_reason;
  if (first_present_) {
    first_present_ = false;
    UINT flags = DXGI_PRESENT_USE_DURATION;
    hr = swap_chain_->Present(0, flags);
    // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only indicates
    // that the window is occluded and we can stop rendering.
    if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
      DLOG(ERROR) << "Present failed with error 0x" << std::hex << hr;
      return false;
    }

    // DirectComposition can display black for a swap chain between the first
    // and second time it's presented to - maybe the first Present can get
    // lost somehow and it shows the wrong buffer. In that case copy the
    // buffers so both have the correct contents, which seems to help. The
    // first Present() after this needs to have SyncInterval > 0, or else the
    // workaround doesn't help.
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
  // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only indicates
  // that the window is occluded and we can stop rendering.
  hr = swap_chain_->Present(interval, flags);
  if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
    DLOG(ERROR) << "Present failed with error 0x" << std::hex << hr;
    return false;
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
      if (frame_rate_ != 0) {
        // [1ms, 10s] covers the fps between [0.1hz, 1000hz].
        base::UmaHistogramTimes(
            "GPU.DirectComposition.ApprovedPresentDuration",
            base::Milliseconds(stats.ApprovedPresentDuration / 10000));
      }
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
  // TODO(crbug.com/999747): Include an early out path in case the same dcomp
  // surface is being presented.

  auto* dcomp_surface_proxy = params.overlay_image->dcomp_surface_proxy();
  last_overlay_image_ = std::move(params.overlay_image);

  dcomp_surface_proxy->SetParentWindow(layer_tree_->window());

  // Apply fullscreen rounding and transform to video and notify DCOMPTexture.
  gfx::Rect overlay_onscreen_rect = params.quad_rect;
  gfx::Size on_screen_size = overlay_onscreen_rect.size();
  AdjustTargetToOptimalSizeIfNeeded(params, overlay_onscreen_rect,
                                    &on_screen_size, visual_transform,
                                    visual_clip_rect);
  dcomp_surface_proxy->SetRect(visual_transform->MapRect(
      gfx::Rect(params.quad_rect.origin(), on_screen_size)));

  dcomp_surface_proxy->SetProtectedVideoType(params.protected_video_type);

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

  // TODO(crbug.com/999747): Call UpdateVisuals() here.

  // Scaling is handled by the MF video renderer, so we only need the
  // translation component.
  gfx::Vector2dF visual_transform_offset = visual_transform->To2dTranslation();
  visual_transform->MakeIdentity();
  visual_transform->Translate(visual_transform_offset);

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
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
    const gfx::Rect& content_rect,
    const gfx::ColorSpace& src_color_space,
    bool content_is_hdr,
    absl::optional<DXGI_HDR_METADATA_HDR10> stream_hdr_metadata) {
  TRACE_EVENT2("gpu", "SwapChainPresenter::VideoProcessorBlt", "content_rect",
               content_rect.ToString(), "swap_chain_size",
               swap_chain_size_.ToString());

  // TODO(sunnyps): Ensure output color space for YUV swap chains is Rec709 or
  // Rec601 so that the conversion from gfx::ColorSpace to DXGI_COLOR_SPACE
  // doesn't need a |force_yuv| parameter (and the associated plumbing).
  bool is_yuv_swapchain = IsYUVSwapChainFormat(swap_chain_format_);
  gfx::ColorSpace output_color_space =
      is_yuv_swapchain ? src_color_space : gfx::ColorSpace::CreateSRGB();
  if (base::FeatureList::IsEnabled(kFallbackBT709VideoToBT601) &&
      (output_color_space == gfx::ColorSpace::CreateREC709())) {
    output_color_space = gfx::ColorSpace::CreateREC601();
  }
  if (content_is_hdr) {
    output_color_space = gfx::ColorSpace::CreateHDR10();
  }

  VideoProcessorWrapper* video_processor_wrapper =
      layer_tree_->InitializeVideoProcessor(
          content_rect.size(), swap_chain_size_, output_color_space.IsHDR());
  if (!video_processor_wrapper)
    return false;

  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context =
      video_processor_wrapper->video_context;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor =
      video_processor_wrapper->video_processor;

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

    if (SUCCEEDED(swap_chain3->SetColorSpace1(output_dxgi_color_space))) {
      context1->VideoProcessorSetOutputColorSpace1(video_processor.Get(),
                                                   output_dxgi_color_space);
    }
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
  absl::optional<DXGI_HDR_METADATA_HDR10> display_metadata =
      layer_tree_->GetHDRMetadataHelper()->GetDisplayMetadata();
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
    absl::optional<ScopedReleaseKeyedMutex> release_keyed_mutex;
    if (keyed_mutex) {
      // The producer may still be using this texture for a short period of
      // time, so wait long enough to hopefully avoid glitches. For example,
      // all levels of the texture share the same keyed mutex, so if the
      // hardware decoder acquired the mutex to decode into a different array
      // level then it still may block here temporarily.
      const int kMaxSyncTimeMs = 1000;
      HRESULT hr = keyed_mutex->AcquireSync(0, kMaxSyncTimeMs);
      if (FAILED(hr)) {
        DLOG(ERROR) << "Error acquiring keyed mutex: " << std::hex << hr;
        return false;
      }
      release_keyed_mutex.emplace(keyed_mutex, 0);
    }

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

    bool use_vp_super_resolution = false;
    if (!layer_tree_->disable_vp_super_resolution() &&
        !force_vp_super_resolution_off_) {
      use_vp_super_resolution =
          ToggleVpSuperResolution(gpu_vendor_id_, video_context.Get(),
                                  video_processor.Get(), !is_on_battery_power_);
    }

    hr = video_context->VideoProcessorBlt(video_processor.Get(),
                                          output_view_.Get(), 0, 1, &stream);
    base::UmaHistogramSparse(
        (use_vp_super_resolution
             ? "GPU.VideoProcessorBlt.VpSuperResolution.On"
             : "GPU.VideoProcessorBlt.VpSuperResolution.Off"),
        hr);

    if (FAILED(hr)) {
      // Retry VideoProcessorBlt with vp super resolution off if it was on.
      if (use_vp_super_resolution) {
        DLOG(ERROR) << "Retry VideoProcessorBlt with VpSuperResolution off "
                       "after it failed with error 0x"
                    << std::hex << hr;

        ToggleVpSuperResolution(gpu_vendor_id_, video_context.Get(),
                                video_processor.Get(), false);
        hr = video_context->VideoProcessorBlt(
            video_processor.Get(), output_view_.Get(), 0, 1, &stream);

        base::UmaHistogramSparse(
            "GPU.VideoProcessorBlt.VpSuperResolution.RetryOffAfterError", hr);

        // We shouldn't use VpSuperResolution if it was the reason that caused
        // the VideoProcessorBlt failure.
        force_vp_super_resolution_off_ = SUCCEEDED(hr);
      }

      if (FAILED(hr)) {
        DLOG(ERROR) << "VideoProcessorBlt failed with error 0x" << std::hex
                    << hr;

        // To prevent it from failing in all coming frames, disable overlay if
        // VideoProcessorBlt is not implemented in the GPU driver.
        if (hr == E_NOTIMPL) {
          DisableDirectCompositionOverlays();
        }
        return false;
      }
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
  desc.BufferCount = BufferCount();
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
    std::ostringstream trace_event_stream;
    trace_event_stream << "SwapChainPresenter::ReallocateSwapChain::"
                       << DxgiFormatToString(swap_chain_format);
    TRACE_EVENT0("gpu", trace_event_stream.str().c_str());

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

  gl::LabelSwapChainAndBuffers(swap_chain_.Get(), "SwapChainPresenter");

  swap_chain_format_ = swap_chain_format;
  SetSwapChainPresentDuration();

  DXGI_ADAPTER_DESC adapter_desc;
  HRESULT hr = dxgi_adapter->GetDesc(&adapter_desc);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get adapter desc with error 0x" << std::hex << hr;
  } else {
    gpu_vendor_id_ = adapter_desc.VendorId;
  }

  return true;
}

void SwapChainPresenter::OnPowerStateChange(bool on_battery_power) {
  is_on_battery_power_ = on_battery_power;
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
  HRESULT hr = 0;
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

}  // namespace gl
