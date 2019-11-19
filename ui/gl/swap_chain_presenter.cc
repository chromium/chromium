// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/swap_chain_presenter.h"

#include <d3d11_1.h>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_image_d3d.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_image_memory.h"
#include "ui/gl/gl_switches.h"

namespace gl {
namespace {
// Some drivers fail to correctly handle BT.709 video in overlays. This flag
// converts them to BT.601 in the video processor.
const base::Feature kFallbackBT709VideoToBT601{
    "FallbackBT709VideoToBT601", base::FEATURE_DISABLED_BY_DEFAULT};

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

  ~ScopedReleaseKeyedMutex() {
    HRESULT hr = keyed_mutex_->ReleaseSync(key_);
    DCHECK(SUCCEEDED(hr));
  }

 private:
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
  UINT64 key_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ScopedReleaseKeyedMutex);
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

void RecordOverlayFullScreenTypes(const gfx::Rect& overlay_onscreen_rect) {
  OverlayFullScreenTypes full_screen_type;
  const gfx::Size& screen_size =
      DirectCompositionSurfaceWin::GetOverlayMonitorSize();
  const gfx::Size& overlay_onscreen_size = overlay_onscreen_rect.size();
  const gfx::Point& origin = overlay_onscreen_rect.origin();

  // The kFullScreenInWidthOnly type might be over counted, it's possible the
  // video width fits the screen but it's still in a window mode.
  if (screen_size.IsEmpty()) {
    full_screen_type = OverlayFullScreenTypes::kNotAvailable;
  } else if (origin.IsOrigin() && overlay_onscreen_size == screen_size)
    full_screen_type = OverlayFullScreenTypes::kFullScreenMode;
  else if (overlay_onscreen_size.width() > screen_size.width() ||
           overlay_onscreen_size.height() > screen_size.height()) {
    full_screen_type = OverlayFullScreenTypes::kOverSizedFullScreen;
  } else if (origin.x() == 0 &&
             overlay_onscreen_size.width() == screen_size.width()) {
    full_screen_type = OverlayFullScreenTypes::kFullScreenInWidthOnly;
  } else if (origin.y() == 0 &&
             overlay_onscreen_size.height() == screen_size.height()) {
    full_screen_type = OverlayFullScreenTypes::kFullScreenInHeightOnly;
  } else {
    full_screen_type = OverlayFullScreenTypes::kWindowMode;
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.DirectComposition.OverlayFullScreenTypes",
                            full_screen_type);
}

const char* ProtectedVideoTypeToString(gfx::ProtectedVideoType type) {
  switch (type) {
    case gfx::ProtectedVideoType::kClear:
      return "Clear";
    case gfx::ProtectedVideoType::kSoftwareProtected:
      if (DirectCompositionSurfaceWin::AreOverlaysSupported())
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
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return "BGRA";
    case DXGI_FORMAT_YUY2:
      return "YUY2";
    case DXGI_FORMAT_NV12:
      return "NV12";
    default:
      NOTREACHED();
      return nullptr;
  }
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
      d3d11_device_(d3d11_device),
      dcomp_device_(dcomp_device),
      is_on_battery_power_(true) {
  if (base::PowerMonitor::IsInitialized()) {
    is_on_battery_power_ = base::PowerMonitor::IsOnBatteryPower();
    base::PowerMonitor::AddObserver(this);
  }
}

SwapChainPresenter::~SwapChainPresenter() {
  base::PowerMonitor::RemoveObserver(this);
}

bool SwapChainPresenter::ShouldUseYUVSwapChain(
    gfx::ProtectedVideoType protected_video_type) {
  // TODO(crbug.com/850799): Assess power/perf impact when protected video
  // swap chain is composited by DWM.

  // Always prefer YUV swap chain for hardware protected video for now.
  if (protected_video_type == gfx::ProtectedVideoType::kHardwareProtected)
    return true;

  // For software protected video, BGRA swap chain is preferred if hardware
  // overlay is not supported for better power efficiency.
  // Currently, software protected video is the only case that overlay swap
  // chain is used when hardware overlay is not supported.
  if (protected_video_type == gfx::ProtectedVideoType::kSoftwareProtected &&
      !DirectCompositionSurfaceWin::AreOverlaysSupported())
    return false;

  if (failed_to_create_yuv_swapchain_)
    return false;

  // Start out as YUV.
  if (!presentation_history_.Valid())
    return true;

  int composition_count = presentation_history_.composed_count();

  // It's more efficient to use a BGRA backbuffer instead of YUV if overlays
  // aren't being used, as otherwise DWM will use the video processor a second
  // time to convert it to BGRA before displaying it on screen.

  if (is_yuv_swapchain_) {
    // Switch to BGRA once 3/4 of presents are composed.
    return composition_count < (PresentationHistory::kPresentsToStore * 3 / 4);
  } else {
    // Switch to YUV once 3/4 are using overlays (or unknown).
    return composition_count < (PresentationHistory::kPresentsToStore / 4);
  }
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> SwapChainPresenter::UploadVideoImages(
    GLImageMemory* y_image_memory,
    GLImageMemory* uv_image_memory) {
  gfx::Size texture_size = y_image_memory->GetSize();
  gfx::Size uv_image_size = uv_image_memory->GetSize();
  if (uv_image_size.height() != texture_size.height() / 2 ||
      uv_image_size.width() != texture_size.width() / 2 ||
      y_image_memory->format() != gfx::BufferFormat::R_8 ||
      uv_image_memory->format() != gfx::BufferFormat::RG_88) {
    DLOG(ERROR) << "Invalid NV12 GLImageMemory properties.";
    return nullptr;
  }

  TRACE_EVENT1("gpu", "SwapChainPresenter::UploadVideoImages", "size",
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
      return nullptr;
    }
    DCHECK(staging_texture_);
    staging_texture_size_ = texture_size;
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
  for (int y = 0; y < texture_size.height(); y++) {
    const uint8_t* y_source =
        y_image_memory->memory() + y * y_image_memory->stride();
    uint8_t* dest =
        reinterpret_cast<uint8_t*>(mapped_resource.pData) + dest_stride * y;
    memcpy(dest, y_source, texture_size.width());
  }

  uint8_t* uv_dest_plane_start =
      reinterpret_cast<uint8_t*>(mapped_resource.pData) +
      dest_stride * texture_size.height();
  for (int y = 0; y < uv_image_size.height(); y++) {
    const uint8_t* uv_source =
        uv_image_memory->memory() + y * uv_image_memory->stride();
    uint8_t* dest = uv_dest_plane_start + dest_stride * y;
    memcpy(dest, uv_source, texture_size.width());
  }
  context->Unmap(staging_texture_.Get(), 0);

  if (use_dynamic_texture)
    return staging_texture_;

  if (!copy_texture_) {
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DECODER;
    desc.CPUAccessFlags = 0;
    HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &copy_texture_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Creating D3D11 video upload texture failed: " << std::hex
                  << hr;
      return nullptr;
    }
    DCHECK(copy_texture_);
  }
  TRACE_EVENT0("gpu", "SwapChainPresenter::UploadVideoImages::CopyResource");
  context->CopyResource(copy_texture_.Get(), staging_texture_.Get());
  return copy_texture_;
}

gfx::Size SwapChainPresenter::CalculateSwapChainSize(
    const ui::DCRendererLayerParams& params) {
  // Swap chain size is the minimum of the on-screen size and the source size so
  // the video processor can do the minimal amount of work and the overlay has
  // to read the minimal amount of data. DWM is also less likely to promote a
  // surface to an overlay if it's much larger than its area on-screen.
  gfx::Size swap_chain_size = params.content_rect.size();
  gfx::Size overlay_onscreen_size = swap_chain_size;
  gfx::RectF bounds(params.quad_rect);
  params.transform.TransformRect(&bounds);
  overlay_onscreen_size = gfx::ToEnclosingRect(bounds).size();

  // If transform isn't a scale or translation then swap chain can't be promoted
  // to an overlay so avoid blitting to a large surface unnecessarily.  Also,
  // after the video rotation fix (crbug.com/904035), using rotated size for
  // swap chain size will cause stretching since there's no squashing factor in
  // the transform to counteract.
  // TODO(sunnyps): Support 90/180/270 deg rotations using video context.
  if (params.transform.IsScaleOrTranslation()) {
    swap_chain_size = overlay_onscreen_size;
  }
  if (DirectCompositionSurfaceWin::AreScaledOverlaysSupported() &&
      !ShouldUseVideoProcessorScaling()) {
    // Downscaling doesn't work on Intel display HW, and so DWM will perform an
    // extra BLT to avoid HW downscaling. This prevents the use of hardware
    // overlays especially for protected video.
    swap_chain_size.SetToMin(params.content_rect.size());
  }

  gfx::Size overlay_monitor_size =
      DirectCompositionSurfaceWin::GetOverlayMonitorSize();
  if (layer_tree_->disable_larger_than_screen_overlays() &&
      !overlay_monitor_size.IsEmpty()) {
    // Because of the rounding when converting between pixels and DIPs, a
    // fullscreen video can become slightly larger than the monitor - e.g. on
    // a 3000x2000 monitor with a scale factor of 1.75 a 1920x1079 video can
    // become 3002x1689.
    // On older Intel drivers, swapchains that are bigger than the monitor
    // won't be put into overlays, which will hurt power usage a lot. On those
    // systems, the scaling can be adjusted very slightly so that it's less
    // than the monitor size. This should be close to imperceptible.
    // TODO(jbauman): Remove when http://crbug.com/668278 is fixed.
    const int kOversizeMargin = 3;

    if ((swap_chain_size.width() > overlay_monitor_size.width()) &&
        (swap_chain_size.width() <=
         overlay_monitor_size.width() + kOversizeMargin)) {
      swap_chain_size.set_width(overlay_monitor_size.width());
    }

    if ((swap_chain_size.height() > overlay_monitor_size.height()) &&
        (swap_chain_size.height() <=
         overlay_monitor_size.height() + kOversizeMargin)) {
      swap_chain_size.set_height(overlay_monitor_size.height());
    }
  }
  RecordOverlayFullScreenTypes(gfx::ToEnclosingRect(bounds));

  // 4:2:2 subsampled formats like YUY2 must have an even width, and 4:2:0
  // subsampled formats like NV12 must have an even width and height.
  if (swap_chain_size.width() % 2 == 1)
    swap_chain_size.set_width(swap_chain_size.width() + 1);
  if (swap_chain_size.height() % 2 == 1)
    swap_chain_size.set_height(swap_chain_size.height() + 1);

  return swap_chain_size;
}

void SwapChainPresenter::UpdateVisuals(const ui::DCRendererLayerParams& params,
                                       const gfx::Size& swap_chain_size) {
  if (!content_visual_) {
    DCHECK(!clip_visual_);
    dcomp_device_->CreateVisual(&clip_visual_);
    DCHECK(clip_visual_);
    dcomp_device_->CreateVisual(&content_visual_);
    DCHECK(content_visual_);
    clip_visual_->AddVisual(content_visual_.Get(), FALSE, nullptr);
    layer_tree_->SetNeedsRebuildVisualTree();
  }

  // Visual offset is applied before transform so it behaves similar to how the
  // compositor uses transform to map quad rect in layer space to target space.
  gfx::Point offset = params.quad_rect.origin();
  gfx::Transform transform = params.transform;

  // Transform is correct for scaling up |quad_rect| to on screen bounds, but
  // doesn't include scaling transform from |swap_chain_size| to |quad_rect|.
  // Since |swap_chain_size| could be equal to on screen bounds, and therefore
  // possibly larger than |quad_rect|, this scaling could be downscaling, but
  // only to the extent that it would cancel upscaling already in the transform.
  float swap_chain_scale_x =
      params.quad_rect.width() * 1.0f / swap_chain_size.width();
  float swap_chain_scale_y =
      params.quad_rect.height() * 1.0f / swap_chain_size.height();
  transform.Scale(swap_chain_scale_x, swap_chain_scale_y);

  if (visual_info_.offset != offset || visual_info_.transform != transform) {
    visual_info_.offset = offset;
    visual_info_.transform = transform;
    layer_tree_->SetNeedsRebuildVisualTree();

    content_visual_->SetOffsetX(offset.x());
    content_visual_->SetOffsetY(offset.y());

    Microsoft::WRL::ComPtr<IDCompositionMatrixTransform> dcomp_transform;
    dcomp_device_->CreateMatrixTransform(&dcomp_transform);
    DCHECK(dcomp_transform);
    // SkMatrix44 is column-major, but D2D_MATRIX_3x2_F is row-major.
    D2D_MATRIX_3X2_F d2d_matrix = {
        {{transform.matrix().get(0, 0), transform.matrix().get(1, 0),
          transform.matrix().get(0, 1), transform.matrix().get(1, 1),
          transform.matrix().get(0, 3), transform.matrix().get(1, 3)}}};
    dcomp_transform->SetMatrix(d2d_matrix);
    content_visual_->SetTransform(dcomp_transform.Get());
  }

  if (visual_info_.is_clipped != params.is_clipped ||
      visual_info_.clip_rect != params.clip_rect) {
    visual_info_.is_clipped = params.is_clipped;
    visual_info_.clip_rect = params.clip_rect;
    layer_tree_->SetNeedsRebuildVisualTree();
    // DirectComposition clips happen in the pre-transform visual space, while
    // cc/ clips happen post-transform. So the clip needs to go on a separate
    // parent visual that's untransformed.
    if (params.is_clipped) {
      Microsoft::WRL::ComPtr<IDCompositionRectangleClip> clip;
      dcomp_device_->CreateRectangleClip(&clip);
      DCHECK(clip);
      clip->SetLeft(params.clip_rect.x());
      clip->SetRight(params.clip_rect.right());
      clip->SetBottom(params.clip_rect.bottom());
      clip->SetTop(params.clip_rect.y());
      clip_visual_->SetClip(clip.Get());
    } else {
      clip_visual_->SetClip(nullptr);
    }
  }

  if (visual_info_.z_order != params.z_order) {
    visual_info_.z_order = params.z_order;
    layer_tree_->SetNeedsRebuildVisualTree();
  }
}

bool SwapChainPresenter::TryPresentToDecodeSwapChain(
    GLImageDXGI* nv12_image,
    const gfx::Rect& content_rect,
    const gfx::Size& swap_chain_size) {
  if (!base::FeatureList::IsEnabled(
          features::kDirectCompositionUseNV12DecodeSwapChain))
    return false;

  if (ShouldUseVideoProcessorScaling())
    return false;

  auto not_used_reason = DecodeSwapChainNotUsedReason::kFailedToPresent;

  bool nv12_supported =
      (DXGI_FORMAT_NV12 == DirectCompositionSurfaceWin::GetOverlayFormatUsed());
  // TODO(sunnyps): Try using decode swap chain for uploaded video images.
  if (nv12_image && nv12_supported && !failed_to_present_decode_swapchain_) {
    D3D11_TEXTURE2D_DESC texture_desc = {};
    nv12_image->texture()->GetDesc(&texture_desc);

    bool is_decoder_texture = texture_desc.BindFlags & D3D11_BIND_DECODER;

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
    bool is_overlay_supported_transform =
        visual_info_.transform.IsPositiveScaleOrTranslation();

    // Downscaled video isn't promoted to hardware overlays.  We prefer to
    // blit into the smaller size so that it can be promoted to a hardware
    // overlay.
    float swap_chain_scale_x =
        swap_chain_size.width() * 1.0f / content_rect.width();
    float swap_chain_scale_y =
        swap_chain_size.height() * 1.0f / content_rect.height();

    is_overlay_supported_transform = is_overlay_supported_transform &&
                                     (swap_chain_scale_x >= 1.0f) &&
                                     (swap_chain_scale_y >= 1.0f);

    if (is_decoder_texture && !is_shared_texture && !is_unitary_texture_array &&
        is_overlay_supported_transform) {
      if (PresentToDecodeSwapChain(nv12_image, content_rect, swap_chain_size))
        return true;
      ReleaseSwapChainResources();
      failed_to_present_decode_swapchain_ = true;
      not_used_reason = DecodeSwapChainNotUsedReason::kFailedToPresent;
      DLOG(ERROR)
          << "Present to decode swap chain failed - falling back to blit";
    } else if (!is_decoder_texture) {
      not_used_reason = DecodeSwapChainNotUsedReason::kNonDecoderTexture;
    } else if (is_shared_texture) {
      not_used_reason = DecodeSwapChainNotUsedReason::kSharedTexture;
    } else if (is_unitary_texture_array) {
      not_used_reason = DecodeSwapChainNotUsedReason::kUnitaryTextureArray;
    } else if (!is_overlay_supported_transform) {
      not_used_reason = DecodeSwapChainNotUsedReason::kIncompatibleTransform;
    }
  } else if (!nv12_image) {
    not_used_reason = DecodeSwapChainNotUsedReason::kSoftwareFrame;
  } else if (!nv12_supported) {
    not_used_reason = DecodeSwapChainNotUsedReason::kNv12NotSupported;
  } else if (failed_to_present_decode_swapchain_) {
    not_used_reason = DecodeSwapChainNotUsedReason::kFailedToPresent;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "GPU.DirectComposition.DecodeSwapChainNotUsedReason", not_used_reason);
  return false;
}

bool SwapChainPresenter::PresentToDecodeSwapChain(
    GLImageDXGI* nv12_image,
    const gfx::Rect& content_rect,
    const gfx::Size& swap_chain_size) {
  DCHECK(!swap_chain_size.IsEmpty());

  TRACE_EVENT2("gpu", "SwapChainPresenter::PresentToDecodeSwapChain",
               "content_rect", content_rect.ToString(), "swap_chain_size",
               swap_chain_size.ToString());

  Microsoft::WRL::ComPtr<IDXGIResource> decode_resource;
  nv12_image->texture().As(&decode_resource);
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
    desc.Flags = 0;
    HRESULT hr =
        media_factory->CreateDecodeSwapChainForCompositionSurfaceHandle(
            d3d11_device_.Get(), swap_chain_handle_.Get(), &desc,
            decode_resource_.Get(), nullptr, &decode_swap_chain_);
    base::UmaHistogramSparse(
        "GPU.DirectComposition.DecodeSwapChainCreationResult", hr);
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateDecodeSwapChainForCompositionSurfaceHandle failed "
                     "with error 0x"
                  << std::hex << hr;
      return false;
    }
    DCHECK(decode_swap_chain_);

    Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
    dcomp_device_.As(&desktop_device);
    DCHECK(desktop_device);

    desktop_device->CreateSurfaceFromHandle(swap_chain_handle_.Get(),
                                            &decode_surface_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateSurfaceFromHandle failed with error 0x" << std::hex
                  << hr;
      return false;
    }
    DCHECK(decode_surface_);

    content_visual_->SetContent(decode_surface_.Get());
    layer_tree_->SetNeedsRebuildVisualTree();
  } else if (last_presented_images_[kNV12ImageIndex] == nv12_image &&
             swap_chain_size_ == swap_chain_size) {
    // Early out if we're presenting the same image again.
    return true;
  }

  RECT source_rect = content_rect.ToRECT();
  decode_swap_chain_->SetSourceRect(&source_rect);

  decode_swap_chain_->SetDestSize(swap_chain_size.width(),
                                  swap_chain_size.height());
  RECT target_rect = gfx::Rect(swap_chain_size).ToRECT();
  decode_swap_chain_->SetTargetRect(&target_rect);

  gfx::ColorSpace color_space = nv12_image->color_space();
  if (!color_space.IsValid())
    color_space = gfx::ColorSpace::CreateREC709();

  // TODO(sunnyps): Move this to gfx::ColorSpaceWin helper where we can access
  // internal color space state and do a better job.
  // Common color spaces have primaries and transfer function similar to BT 709
  // and there are no other choices anyway.
  int flags = DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAG_BT709;
  // Proper Rec 709 and 601 have limited or nominal color range.
  if (color_space == gfx::ColorSpace::CreateREC709() ||
      color_space == gfx::ColorSpace::CreateREC601()) {
    flags |= DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAG_NOMINAL_RANGE;
  }
  // xvYCC allows colors outside nominal range to encode negative colors that
  // allows for a wider gamut.
  if (color_space.FullRangeEncodedValues()) {
    flags |= DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAG_xvYCC;
  }
  decode_swap_chain_->SetColorSpace(
      static_cast<DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAGS>(flags));

  HRESULT hr = decode_swap_chain_->PresentBuffer(nv12_image->level(), 1, 0);
  // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only indicates
  // that the window is occluded and we can stop rendering.
  if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
    DLOG(ERROR) << "PresentBuffer failed with error 0x" << std::hex << hr;
    return false;
  }

  last_presented_images_ = ui::DCRendererLayerParams::OverlayImages();
  last_presented_images_[kNV12ImageIndex] = nv12_image;
  swap_chain_size_ = swap_chain_size;
  if (is_yuv_swapchain_) {
    frames_since_color_space_change_++;
  } else {
    UMA_HISTOGRAM_COUNTS_1000(
        "GPU.DirectComposition.FramesSinceColorSpaceChange",
        frames_since_color_space_change_);
    frames_since_color_space_change_ = 0;
    is_yuv_swapchain_ = true;
  }
  RecordPresentationStatistics();
  return true;
}

bool SwapChainPresenter::PresentToSwapChain(
    const ui::DCRendererLayerParams& params) {
  GLImageDXGI* nv12_image =
      GLImageDXGI::FromGLImage(params.images[kNV12ImageIndex].get());
  GLImageMemory* y_image_memory =
      GLImageMemory::FromGLImage(params.images[kYPlaneImageIndex].get());
  GLImageMemory* uv_image_memory =
      GLImageMemory::FromGLImage(params.images[kUVPlaneImageIndex].get());
  GLImageD3D* swap_chain_image =
      GLImageD3D::FromGLImage(params.images[kSwapChainImageIndex].get());

  if (!nv12_image && (!y_image_memory || !uv_image_memory) &&
      !swap_chain_image) {
    DLOG(ERROR) << "Video GLImages are missing";
    ReleaseSwapChainResources();
    // We don't treat this as an error because this could mean that the client
    // sent us invalid overlay candidates which we weren't able to detect prior
    // to this.  This would cause incorrect rendering, but not a failure loop.
    return true;
  }

  std::string image_type = "software video frame";
  if (nv12_image)
    image_type = "hardware video frame";
  if (swap_chain_image)
    image_type = "swap chain";

  gfx::Size swap_chain_size = swap_chain_image ? swap_chain_image->GetSize()
                                               : CalculateSwapChainSize(params);

  TRACE_EVENT2("gpu", "SwapChainPresenter::PresentToSwapChain", "image_type",
               image_type, "swap_chain_size", swap_chain_size.ToString());

  // Do not create a swap chain if swap chain size will be empty.
  if (swap_chain_size.IsEmpty()) {
    swap_chain_size_ = swap_chain_size;
    if (swap_chain_) {
      last_presented_images_ = ui::DCRendererLayerParams::OverlayImages();
      ReleaseSwapChainResources();
      content_visual_->SetContent(nullptr);
      layer_tree_->SetNeedsRebuildVisualTree();
    }
    return true;
  }

  UpdateVisuals(params, swap_chain_size);

  // Swap chain image already has a swap chain that's presented by the client
  // e.g. for webgl/canvas low-latency/desynchronized mode.
  if (swap_chain_image) {
    content_visual_->SetContent(swap_chain_image->swap_chain().Get());
    if (last_presented_images_[kSwapChainImageIndex] != swap_chain_image) {
      last_presented_images_ = params.images;
      ReleaseSwapChainResources();
      layer_tree_->SetNeedsRebuildVisualTree();
    }
    return true;
  }

  if (TryPresentToDecodeSwapChain(nv12_image, params.content_rect,
                                  swap_chain_size)) {
    return true;
  }

  bool swap_chain_resized = swap_chain_size_ != swap_chain_size;
  // Give it another chance to try YUV again when the size changes.
  if (swap_chain_resized) {
    presentation_history_.Clear();
  }
  bool use_yuv_swap_chain = ShouldUseYUVSwapChain(params.protected_video_type);
  bool toggle_yuv_swapchain = use_yuv_swap_chain != is_yuv_swapchain_;
  bool toggle_protected_video =
      protected_video_type_ != params.protected_video_type;

  // Try reallocating swap chain if resizing fails.
  if (!swap_chain_ || swap_chain_resized || toggle_yuv_swapchain ||
      toggle_protected_video) {
    if (!ReallocateSwapChain(swap_chain_size, use_yuv_swap_chain,
                             params.protected_video_type, params.z_order)) {
      ReleaseSwapChainResources();
      return false;
    }
    content_visual_->SetContent(swap_chain_.Get());
    layer_tree_->SetNeedsRebuildVisualTree();
  } else if (last_presented_images_ == params.images) {
    // The swap chain is presenting the same images as last swap, which means
    // that the images were never returned to the video decoder and should
    // have the same contents as last time. It shouldn't need to be redrawn.
    return true;
  }
  last_presented_images_ = params.images;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
  UINT input_level;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  if (nv12_image) {
    input_texture = nv12_image->texture();
    input_level = (UINT)nv12_image->level();
    // Keyed mutex may not exist.
    keyed_mutex = nv12_image->keyed_mutex();
    staging_texture_.Reset();
    copy_texture_.Reset();
  } else {
    DCHECK(y_image_memory);
    DCHECK(uv_image_memory);
    input_texture = UploadVideoImages(y_image_memory, uv_image_memory);
    input_level = 0;
  }

  if (!input_texture) {
    DLOG(ERROR) << "Video image has no texture";
    return false;
  }

  // TODO(sunnyps): Use correct color space for uploaded video frames.
  gfx::ColorSpace src_color_space = gfx::ColorSpace::CreateREC709();
  if (nv12_image && nv12_image->color_space().IsValid())
    src_color_space = nv12_image->color_space();

  if (!VideoProcessorBlt(input_texture, input_level, keyed_mutex,
                         params.content_rect, src_color_space)) {
    return false;
  }

  if (first_present_) {
    first_present_ = false;

    HRESULT hr = swap_chain_->Present(0, 0);
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
    DCHECK(SUCCEEDED(hr));
    event.Wait();
  }
  const bool use_swap_chain_tearing =
      DirectCompositionSurfaceWin::AllowTearing();
  UINT flags = use_swap_chain_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
  UINT interval = use_swap_chain_tearing ? 0 : 1;
  // Ignore DXGI_STATUS_OCCLUDED since that's not an error but only indicates
  // that the window is occluded and we can stop rendering.
  HRESULT hr = swap_chain_->Present(interval, flags);
  if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
    DLOG(ERROR) << "Present failed with error 0x" << std::hex << hr;
    return false;
  }
  frames_since_color_space_change_++;
  RecordPresentationStatistics();
  return true;
}

void SwapChainPresenter::RecordPresentationStatistics() {
  DXGI_FORMAT swap_chain_format =
      is_yuv_swapchain_ ? DirectCompositionSurfaceWin::GetOverlayFormatUsed()
                        : DXGI_FORMAT_B8G8R8A8_UNORM;
  base::UmaHistogramSparse("GPU.DirectComposition.SwapChainFormat3",
                           swap_chain_format);

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

  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.DecodeSwapChainUsed",
                        !!decode_swap_chain_);

  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("gpu.service"),
                       "SwapChain::Present", TRACE_EVENT_SCOPE_THREAD,
                       "PixelFormat", DxgiFormatToString(swap_chain_format),
                       "ZeroCopy", !!decode_swap_chain_);
  HRESULT hr = 0;
  Microsoft::WRL::ComPtr<IDXGISwapChainMedia> swap_chain_media;
  if (decode_swap_chain_) {
    hr = decode_swap_chain_.As(&swap_chain_media);
  } else {
    DCHECK(swap_chain_);
    hr = swap_chain_.As(&swap_chain_media);
  }
  if (SUCCEEDED(hr)) {
    DCHECK(swap_chain_media);
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
      base::UmaHistogramSparse("GPU.DirectComposition.CompositionMode",
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

bool SwapChainPresenter::VideoProcessorBlt(
    Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture,
    UINT input_level,
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
    const gfx::Rect& content_rect,
    const gfx::ColorSpace& src_color_space) {
  TRACE_EVENT2("gpu", "SwapChainPresenter::VideoProcessorBlt", "content_rect",
               content_rect.ToString(), "swap_chain_size",
               swap_chain_size_.ToString());
  if (!layer_tree_->InitializeVideoProcessor(content_rect.size(),
                                             swap_chain_size_)) {
    return false;
  }
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context =
      layer_tree_->video_context();
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor =
      layer_tree_->video_processor();

  gfx::ColorSpace output_color_space =
      is_yuv_swapchain_ ? src_color_space : gfx::ColorSpace::CreateSRGB();

  if (base::FeatureList::IsEnabled(kFallbackBT709VideoToBT601) &&
      (output_color_space == gfx::ColorSpace::CreateREC709())) {
    output_color_space = gfx::ColorSpace::CreateREC601();
  }

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
        gfx::ColorSpaceWin::GetDXGIColorSpace(
            output_color_space, is_yuv_swapchain_ /* force_yuv */);
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

  {
    base::Optional<ScopedReleaseKeyedMutex> release_keyed_mutex;
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
        layer_tree_->video_device();
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
        video_processor_enumerator = layer_tree_->video_processor_enumerator();

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

    hr = video_context->VideoProcessorBlt(video_processor.Get(),
                                          output_view_.Get(), 0, 1, &stream);
    if (FAILED(hr)) {
      DLOG(ERROR) << "VideoProcessorBlt failed with error 0x" << std::hex << hr;
      return false;
    }
  }

  return true;
}

void SwapChainPresenter::ReleaseSwapChainResources() {
  output_view_.Reset();
  swap_chain_.Reset();
  decode_surface_.Reset();
  decode_swap_chain_.Reset();
  decode_resource_.Reset();
  swap_chain_handle_.Close();
  staging_texture_.Reset();
}

bool SwapChainPresenter::ReallocateSwapChain(
    const gfx::Size& swap_chain_size,
    bool use_yuv_swap_chain,
    gfx::ProtectedVideoType protected_video_type,
    bool z_order) {
  TRACE_EVENT2("gpu", "SwapChainPresenter::ReallocateSwapChain", "size",
               swap_chain_size.ToString(), "yuv", use_yuv_swap_chain);

  DCHECK(!swap_chain_size.IsEmpty());
  swap_chain_size_ = swap_chain_size;

  // ResizeBuffers can't change YUV flags so only attempt it when size changes.
  if (swap_chain_ && (is_yuv_swapchain_ == use_yuv_swap_chain) &&
      (protected_video_type_ == protected_video_type)) {
    output_view_.Reset();
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    swap_chain_->GetDesc1(&desc);
    HRESULT hr = swap_chain_->ResizeBuffers(
        desc.BufferCount, swap_chain_size.width(), swap_chain_size.height(),
        desc.Format, desc.Flags);
    if (SUCCEEDED(hr))
      return true;
    DLOG(ERROR) << "ResizeBuffers failed with error 0x" << std::hex << hr;
  }

  protected_video_type_ = protected_video_type;

  if (is_yuv_swapchain_ != use_yuv_swap_chain) {
    UMA_HISTOGRAM_COUNTS_1000(
        "GPU.DirectComposition.FramesSinceColorSpaceChange",
        frames_since_color_space_change_);
    frames_since_color_space_change_ = 0;
  }
  is_yuv_swapchain_ = false;

  ReleaseSwapChainResources();

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
  desc.Format = DirectCompositionSurfaceWin::GetOverlayFormatUsed();
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = 2;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.Flags =
      DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO | DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO;
  if (DirectCompositionSurfaceWin::AllowTearing())
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
  if (IsProtectedVideo(protected_video_type))
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
  if (protected_video_type == gfx::ProtectedVideoType::kHardwareProtected)
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

  const std::string kSwapChainCreationResultByFormatUmaPrefix =
      "GPU.DirectComposition.SwapChainCreationResult2.";

  const std::string kSwapChainCreationResultByVideoTypeUmaPrefix =
      "GPU.DirectComposition.SwapChainCreationResult3.";
  const std::string protected_video_type_string =
      ProtectedVideoTypeToString(protected_video_type);

  DXGI_FORMAT format_used = DirectCompositionSurfaceWin::GetOverlayFormatUsed();
  if (use_yuv_swap_chain) {
    TRACE_EVENT1("gpu", "SwapChainPresenter::ReallocateSwapChain::YUV",
                 "format", DxgiFormatToString(format_used));
    HRESULT hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle_.Get(), &desc, nullptr,
        &swap_chain_);
    is_yuv_swapchain_ = SUCCEEDED(hr);
    failed_to_create_yuv_swapchain_ = !is_yuv_swapchain_;

    base::UmaHistogramSparse(kSwapChainCreationResultByFormatUmaPrefix +
                                 DxgiFormatToString(format_used),
                             hr);
    base::UmaHistogramSparse(kSwapChainCreationResultByVideoTypeUmaPrefix +
                                 protected_video_type_string,
                             hr);

    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create " << DxgiFormatToString(format_used)
                  << " swap chain of size " << swap_chain_size.ToString()
                  << " with error 0x" << std::hex << hr
                  << "\nFalling back to BGRA";
    }
  }
  if (!is_yuv_swapchain_) {
    TRACE_EVENT0("gpu", "SwapChainPresenter::ReallocateSwapChain::BGRA");
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Flags = 0;
    if (IsProtectedVideo(protected_video_type))
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    if (protected_video_type == gfx::ProtectedVideoType::kHardwareProtected)
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    if (DirectCompositionSurfaceWin::AllowTearing())
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    HRESULT hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle_.Get(), &desc, nullptr,
        &swap_chain_);

    base::UmaHistogramSparse(kSwapChainCreationResultByFormatUmaPrefix +
                                 DxgiFormatToString(DXGI_FORMAT_B8G8R8A8_UNORM),
                             hr);
    base::UmaHistogramSparse(kSwapChainCreationResultByVideoTypeUmaPrefix +
                                 protected_video_type_string,
                             hr);

    if (FAILED(hr)) {
      // Disable overlay support so dc_layer_overlay will stop sending down
      // overlay frames here and uses GL Composition instead.
      DirectCompositionSurfaceWin::DisableOverlays();
      DLOG(ERROR) << "Failed to create BGRA swap chain of size "
                  << swap_chain_size.ToString() << " with error 0x" << std::hex
                  << hr << ". Disable overlay swap chains";
      return false;
    }
  }
  return true;
}

void SwapChainPresenter::OnPowerStateChange(bool on_battery_power) {
  is_on_battery_power_ = on_battery_power;
}

bool SwapChainPresenter::ShouldUseVideoProcessorScaling() {
  return (!is_on_battery_power_ && !layer_tree_->disable_vp_scaling());
}

}  // namespace gl
