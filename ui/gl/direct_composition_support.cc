// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/direct_composition_support.h"

#include <d3d11on12.h>
#include <dcomp.h>
#include <dxgi1_6.h>

#include <set>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/win/windows_version.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gpu_switching_manager.h"

namespace gl {
namespace {
// Whether the overlay caps are valid or not. GUARDED_BY GetOverlayLock().
bool g_overlay_caps_valid = false;
// Indicates support for either NV12 or YUY2 overlays. GUARDED_BY
// GetOverlayLock().
bool g_supports_overlays = false;
// Whether the GPU can support hardware overlays or not.
bool g_supports_hardware_overlays = false;
// Whether video processor auto HDR is supported.
bool g_supports_vp_auto_hdr = false;
// Whether the DecodeSwapChain is disabled or not.
bool g_disable_decode_swap_chain = false;
// Whether to force the nv12 overlay support.
bool g_force_nv12_overlay_support = false;
// Whether software overlays have been disabled.
bool g_disable_sw_overlays = false;

// The lock to guard g_overlay_caps_valid and g_supports_overlays.
base::Lock& GetOverlayLock() {
  static base::NoDestructor<base::Lock> overlay_lock;
  return *overlay_lock;
}

bool SupportsOverlays() {
  base::AutoLock auto_lock(GetOverlayLock());
  return g_supports_overlays;
}

bool SupportsHardwareOverlays() {
  base::AutoLock auto_lock(GetOverlayLock());
  return g_supports_hardware_overlays;
}

bool SupportsVideoProcessorAutoHDR() {
  base::AutoLock auto_lock(GetOverlayLock());
  return g_supports_vp_auto_hdr;
}

void SetSupportsOverlays(bool support) {
  base::AutoLock auto_lock(GetOverlayLock());
  g_supports_overlays = support;
}

void SetSupportsHardwareOverlays(bool support) {
  base::AutoLock auto_lock(GetOverlayLock());
  g_supports_hardware_overlays = support;
}

void SetSupportsVideoProcessorAutoHDR(bool support) {
  base::AutoLock auto_lock(GetOverlayLock());
  g_supports_vp_auto_hdr = support;
}

bool SupportsSoftwareOverlays() {
  return base::FeatureList::IsEnabled(
             features::kDirectCompositionSoftwareOverlays) &&
         !g_disable_sw_overlays;
}

bool OverlayCapsValid() {
  base::AutoLock auto_lock(GetOverlayLock());
  return g_overlay_caps_valid;
}

void SetOverlayCapsValid(bool valid) {
  base::AutoLock auto_lock(GetOverlayLock());
  g_overlay_caps_valid = valid;
}

// A wrapper of IDXGIOutput4::CheckOverlayColorSpaceSupport()
bool CheckOverlayColorSpaceSupport(
    DXGI_FORMAT dxgi_format,
    DXGI_COLOR_SPACE_TYPE dxgi_color_space,
    Microsoft::WRL::ComPtr<IDXGIOutput> output,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  UINT color_space_support_flags = 0;
  Microsoft::WRL::ComPtr<IDXGIOutput4> output4;
  if (FAILED(output.As(&output4)) ||
      FAILED(output4->CheckOverlayColorSpaceSupport(
          dxgi_format, dxgi_color_space, d3d11_device.Get(),
          &color_space_support_flags)))
    return false;
  return (color_space_support_flags &
          DXGI_OVERLAY_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
}

// Used for adjusting overlay size to monitor size.
gfx::Size g_primary_monitor_size;

// The number of all visible display monitors on a desktop.
int g_num_monitors = 0;

// Whether there is a HDR capable display monitor being connected.
bool g_system_hdr_enabled = false;

// Per-monitor HDR capability
std::set<HMONITOR>* GetHDRMonitors() {
  static base::NoDestructor<std::set<HMONITOR>> hdr_monitors;
  return hdr_monitors.get();
}

// Global direct composition device.
IDCompositionDevice3* g_dcomp_device = nullptr;
// Global d3d11 device used by direct composition.
ID3D11Device* g_d3d11_device = nullptr;
// Whether swap chain present failed and direct composition should be disabled.
bool g_direct_composition_swap_chain_failed = false;

// Preferred overlay format set when detecting overlay support during
// initialization.  Set to NV12 by default so that it's used when enabling
// overlays using command line flags.
DXGI_FORMAT g_overlay_format_used = DXGI_FORMAT_NV12;
DXGI_FORMAT g_overlay_format_used_hdr = DXGI_FORMAT_UNKNOWN;

// These are the raw support info, which shouldn't depend on field trial state,
// or command line flags. GUARDED_BY GetOverlayLock().
UINT g_nv12_overlay_support_flags = 0;
UINT g_yuy2_overlay_support_flags = 0;
UINT g_bgra8_overlay_support_flags = 0;
UINT g_rgb10a2_overlay_support_flags = 0;
UINT g_p010_overlay_support_flags = 0;

// When this is set, if NV12 or YUY2 overlays are supported, set BGRA8 overlays
// as supported as well.
bool g_enable_bgra8_overlays_with_yuv_overlay_support = false;

// Force enabling DXGI_FORMAT_R10G10B10A2_UNORM format for overlay. Intel
// Icelake and Tigerlake fail to report the cap of this HDR overlay format.
// TODO(magchen@): Remove this workaround when this cap is fixed in the Intel
// drivers.
bool g_force_rgb10a2_overlay_support = false;

// Per Intel's request, only use NV12 for overlay when
// COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 is also supported. At least one Intel
// Gen9 SKU does not support NV12 overlays and it cannot be screened by the
// device id.
bool g_check_ycbcr_studio_g22_left_p709_for_nv12_support = false;

void SetOverlaySupportFlagsForFormats(UINT nv12_flags,
                                      UINT yuy2_flags,
                                      UINT bgra8_flags,
                                      UINT rgb10a2_flags,
                                      UINT p010_flags) {
  base::AutoLock auto_lock(GetOverlayLock());
  g_nv12_overlay_support_flags = nv12_flags;
  g_yuy2_overlay_support_flags = yuy2_flags;
  g_bgra8_overlay_support_flags = bgra8_flags;
  g_rgb10a2_overlay_support_flags = rgb10a2_flags;
  g_p010_overlay_support_flags = p010_flags;
}

bool FlagsSupportsOverlays(UINT flags) {
  return (flags & (DXGI_OVERLAY_SUPPORT_FLAG_DIRECT |
                   DXGI_OVERLAY_SUPPORT_FLAG_SCALING));
}

void GetGpuDriverOverlayInfo(bool* supports_overlays,
                             bool* supports_hardware_overlays,
                             DXGI_FORMAT* overlay_format_used,
                             DXGI_FORMAT* overlay_format_used_hdr,
                             UINT* nv12_overlay_support_flags,
                             UINT* yuy2_overlay_support_flags,
                             UINT* bgra8_overlay_support_flags,
                             UINT* rgb10a2_overlay_support_flags,
                             UINT* p010_overlay_support_flags) {
  // Initialization
  *supports_overlays = false;
  *supports_hardware_overlays = false;
  *overlay_format_used = DXGI_FORMAT_NV12;
  *overlay_format_used_hdr = DXGI_FORMAT_R10G10B10A2_UNORM;
  *nv12_overlay_support_flags = 0;
  *yuy2_overlay_support_flags = 0;
  *bgra8_overlay_support_flags = 0;
  *rgb10a2_overlay_support_flags = 0;
  *p010_overlay_support_flags = 0;

  // Check for DirectComposition support first to prevent likely crashes.
  if (!DirectCompositionSupported())
    return;

  // Before Windows 10 Anniversary Update (Redstone 1), overlay planes wouldn't
  // be assigned to non-UWP apps.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device = g_d3d11_device;
  if (!d3d11_device) {
    LOG(ERROR) << __func__ << ": Failed to retrieve D3D11 device";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  CHECK_EQ(d3d11_device.As(&dxgi_device), S_OK);

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  CHECK_EQ(dxgi_device->GetAdapter(&dxgi_adapter), S_OK);

  // This will fail if the D3D device is "Microsoft Basic Display Adapter".
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  if (FAILED(d3d11_device.As(&video_device))) {
    LOG(ERROR) << __func__ << ": Failed to retrieve video device";
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
                                 nv12_overlay_support_flags);
    output3->CheckOverlaySupport(DXGI_FORMAT_YUY2, d3d11_device.Get(),
                                 yuy2_overlay_support_flags);
    output3->CheckOverlaySupport(DXGI_FORMAT_B8G8R8A8_UNORM, d3d11_device.Get(),
                                 bgra8_overlay_support_flags);
    // Today it still returns false, which blocks Chrome from using HDR
    // overlays.
    output3->CheckOverlaySupport(DXGI_FORMAT_R10G10B10A2_UNORM,
                                 d3d11_device.Get(),
                                 rgb10a2_overlay_support_flags);
    output3->CheckOverlaySupport(DXGI_FORMAT_P010, d3d11_device.Get(),
                                 p010_overlay_support_flags);
    if (FlagsSupportsOverlays(*nv12_overlay_support_flags)) {
      // NV12 format is preferred if it's supported.
      *overlay_format_used = DXGI_FORMAT_NV12;
      *supports_hardware_overlays = true;

      if (g_check_ycbcr_studio_g22_left_p709_for_nv12_support &&
          !CheckOverlayColorSpaceSupport(
              DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
              output, d3d11_device)) {
        // Some new Intel drivers only claim to support unscaled overlays, but
        // scaled overlays still work. It's possible DWM works around it by
        // performing an extra scaling Blt before calling the driver. Even when
        // scaled overlays aren't actually supported, presentation using the
        // overlay path should be relatively efficient.
        *supports_hardware_overlays = false;
      }
    }
    if (!*supports_hardware_overlays &&
        FlagsSupportsOverlays(*yuy2_overlay_support_flags)) {
      // If NV12 isn't supported, fallback to YUY2 if it's supported.
      *overlay_format_used = DXGI_FORMAT_YUY2;
      *supports_hardware_overlays = true;
    }
    if (g_enable_bgra8_overlays_with_yuv_overlay_support) {
      if (FlagsSupportsOverlays(*nv12_overlay_support_flags))
        *bgra8_overlay_support_flags = *nv12_overlay_support_flags;
      else if (FlagsSupportsOverlays(*yuy2_overlay_support_flags))
        *bgra8_overlay_support_flags = *yuy2_overlay_support_flags;
    }

    // RGB10A2 overlay is used for displaying HDR content. In Intel's
    // platform, RGB10A2 overlay is enabled only when
    // DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 is supported.
    if (FlagsSupportsOverlays(*rgb10a2_overlay_support_flags)) {
      if (!CheckOverlayColorSpaceSupport(
              DXGI_FORMAT_R10G10B10A2_UNORM,
              DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, output, d3d11_device))
        *rgb10a2_overlay_support_flags = 0;
    }
    if (g_force_rgb10a2_overlay_support) {
      *rgb10a2_overlay_support_flags = DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    }

    // Early out after the first output that reports overlay support. All
    // outputs are expected to report the same overlay support according to
    // Microsoft's WDDM documentation:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/display/multiplane-overlay-hardware-requirements
    // TODO(sunnyps): If the above is true, then we can only look at first
    // output instead of iterating over all outputs.
    if (*supports_hardware_overlays)
      break;
  }

  *supports_overlays = *supports_hardware_overlays;
  if (*supports_hardware_overlays || !SupportsSoftwareOverlays()) {
    return;
  }

  // If no devices with hardware overlay support were found use software ones.
  *supports_overlays = true;
  *nv12_overlay_support_flags = 0;
  *yuy2_overlay_support_flags = 0;
  *bgra8_overlay_support_flags = 0;
  *rgb10a2_overlay_support_flags = 0;
  *p010_overlay_support_flags = 0;

  // Software overlays always use NV12 because it's slightly more efficient and
  // YUY2 was only used because Skylake doesn't support NV12 hardware overlays.
  *overlay_format_used = DXGI_FORMAT_NV12;
}

void UpdateOverlaySupport() {
  if (OverlayCapsValid())
    return;
  SetOverlayCapsValid(true);

  bool supports_overlays = false;
  bool supports_hardware_overlays = false;
  DXGI_FORMAT overlay_format_used = DXGI_FORMAT_NV12;
  DXGI_FORMAT overlay_format_used_hdr = DXGI_FORMAT_R10G10B10A2_UNORM;
  UINT nv12_overlay_support_flags = 0;
  UINT yuy2_overlay_support_flags = 0;
  UINT bgra8_overlay_support_flags = 0;
  UINT rgb10a2_overlay_support_flags = 0;
  UINT p010_overlay_support_flags = 0;

  GetGpuDriverOverlayInfo(
      &supports_overlays, &supports_hardware_overlays, &overlay_format_used,
      &overlay_format_used_hdr, &nv12_overlay_support_flags,
      &yuy2_overlay_support_flags, &bgra8_overlay_support_flags,
      &rgb10a2_overlay_support_flags, &p010_overlay_support_flags);

  if (g_force_nv12_overlay_support) {
    supports_overlays = true;
    nv12_overlay_support_flags = DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    overlay_format_used = DXGI_FORMAT_NV12;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDirectCompositionVideoSwapChainFormat)) {
    std::string override_format =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kDirectCompositionVideoSwapChainFormat);
    if (override_format == kSwapChainFormatNV12) {
      overlay_format_used = DXGI_FORMAT_NV12;
    } else if (override_format == kSwapChainFormatYUY2) {
      overlay_format_used = DXGI_FORMAT_YUY2;
    } else if (override_format == kSwapChainFormatBGRA) {
      overlay_format_used = DXGI_FORMAT_B8G8R8A8_UNORM;
    } else {
      LOG(ERROR) << "Invalid value for switch "
                 << switches::kDirectCompositionVideoSwapChainFormat;
    }
  }

  // Record histograms.
  if (supports_overlays) {
    base::UmaHistogramSparse("GPU.DirectComposition.OverlayFormatUsed3",
                             overlay_format_used);
  }
  base::UmaHistogramBoolean("GPU.DirectComposition.OverlaysSupported",
                            supports_overlays);
  base::UmaHistogramBoolean("GPU.DirectComposition.HardwareOverlaysSupported",
                            supports_hardware_overlays);

  // Update global caps.
  SetSupportsOverlays(supports_overlays);
  SetSupportsHardwareOverlays(supports_hardware_overlays);
  SetOverlaySupportFlagsForFormats(
      nv12_overlay_support_flags, yuy2_overlay_support_flags,
      bgra8_overlay_support_flags, rgb10a2_overlay_support_flags,
      p010_overlay_support_flags);
  g_overlay_format_used = overlay_format_used;
  g_overlay_format_used_hdr = overlay_format_used_hdr;
}

std::vector<DXGI_OUTPUT_DESC1> GetDirectCompositionOutputDescs() {
  std::vector<DXGI_OUTPUT_DESC1> output_descs;
  // HDR support was introduced in Windows 10 Creators Update.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS2) {
    return output_descs;
  }

  // Only direct composition surface can allocate HDR swap chains.
  if (!DirectCompositionSupported()) {
    return output_descs;
  }

  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateDXGIFactory1 failed: "
               << logging::SystemErrorCodeToString(hr);
    return output_descs;
  }

  for (UINT adapter_index = 0;; ++adapter_index) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = factory->EnumAdapters(adapter_index, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(hr)) {
      LOG(ERROR)
          << "Unexpected error creating DXGI adapter, EnumAdapters failed: "
          << logging::SystemErrorCodeToString(hr);
      break;
    }

    for (UINT output_index = 0;; ++output_index) {
      Microsoft::WRL::ComPtr<IDXGIOutput> output;
      hr = adapter->EnumOutputs(output_index, &output);
      if (hr == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      if (FAILED(hr)) {
        LOG(ERROR)
            << "Unexpected error creating DXGI adapter, EnumOutputs failed: "
            << logging::SystemErrorCodeToString(hr);
        break;
      }

      Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
      hr = output->QueryInterface(IID_PPV_ARGS(&output6));
      if (FAILED(hr)) {
        LOG(WARNING) << "IDXGIOutput6 is required for HDR detection.";
        continue;
      }

      DXGI_OUTPUT_DESC1 desc;
      hr = output6->GetDesc1(&desc);
      if (FAILED(hr)) {
        LOG(ERROR) << "Unexpected error getting output descriptor: "
                   << logging::SystemErrorCodeToString(hr);
        continue;
      }

      output_descs.push_back(std::move(desc));
    }
  }

  return output_descs;
}

void UpdateMonitorInfo() {
  g_num_monitors = GetSystemMetrics(SM_CMONITORS);

  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  if (GetMonitorInfo(MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY),
                     &monitor_info)) {
    g_primary_monitor_size = gfx::Rect(monitor_info.rcMonitor).size();
  } else {
    g_primary_monitor_size = gfx::Size();
  }

  GetHDRMonitors()->clear();
  g_system_hdr_enabled = false;
  for (const auto& desc : GetDirectCompositionOutputDescs()) {
    if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
      GetHDRMonitors()->insert(desc.Monitor);
      g_system_hdr_enabled = true;
    }
  }
  UMA_HISTOGRAM_BOOLEAN("GPU.Output.HDR", g_system_hdr_enabled);
}

// Update video processor auto HDR feature support status.
// Note that NVIDIA GPU is the only one that supports Auto HDR feature
// currently.
// Must be called on GpuMain thread.
void UpdateVideoProcessorAutoHDRSupport() {
  if (GetGlWorkarounds().disable_vp_auto_hdr) {
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kNvidiaVpTrueHDR)) {
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device = g_d3d11_device;
  if (!d3d11_device) {
    LOG(ERROR) << __func__ << ": Failed to retrieve D3D11 device";
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device.As(&dxgi_device))) {
    DLOG(ERROR) << "Failed to retrieve DXGI device";
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter))) {
    DLOG(ERROR) << "Failed to retrieve DXGI adapter";
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  DXGI_ADAPTER_DESC adapter_desc;
  if (FAILED(dxgi_adapter->GetDesc(&adapter_desc))) {
    DLOG(ERROR) << "Failed to get adapter desc";
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  // Check the vendor ID to make sure it's NVIDIA.
  if (adapter_desc.VendorId != 0x10de) {
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
  // D3D11 immediate context isn't allowed to be accessed simultaneously on two
  // threads, and all other callers are using this on the GpuMain thread, so
  // this function must be called on GpuMain thread.
  d3d11_device->GetImmediateContext(&d3d11_context);
  if (!d3d11_context) {
    LOG(ERROR) << __func__ << ": Failed to get immediate context";
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11VideoContext> d3d11_video_context;
  if (FAILED(d3d11_context.As(&d3d11_video_context))) {
    LOG(ERROR) << __func__ << ": Failed to retrieve video context";
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11VideoDevice> d3d11_video_device;
  if (FAILED(d3d11_device.As(&d3d11_video_device))) {
    LOG(ERROR) << __func__ << ": Failed to retrieve video device";
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = 1920;
  desc.InputHeight = 1080;
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = 1920;
  desc.OutputHeight = 1080;
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  HRESULT hr = S_OK;

  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> d3d11_video_enumerator;
  hr = d3d11_video_device->CreateVideoProcessorEnumerator(
      &desc, &d3d11_video_enumerator);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateVideoProcessorEnumerator failed: "
               << logging::SystemErrorCodeToString(hr);
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> d3d11_video_processor;
  hr = d3d11_video_device->CreateVideoProcessor(d3d11_video_enumerator.Get(), 0,
                                                &d3d11_video_processor);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateVideoProcessor failed: "
               << logging::SystemErrorCodeToString(hr);
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  constexpr GUID kNvidiaTrueHDRInterfaceGUID = {
      0xfdd62bb4,
      0x620b,
      0x4fd7,
      {0x9a, 0xb3, 0x1e, 0x59, 0xd0, 0xd5, 0x44, 0xb3}};

  UINT driver_supports_true_hdr = 0;
  hr = d3d11_video_context->VideoProcessorGetStreamExtension(
      d3d11_video_processor.Get(), 0, &kNvidiaTrueHDRInterfaceGUID,
      sizeof(driver_supports_true_hdr), &driver_supports_true_hdr);
  if (FAILED(hr)) {
    LOG(ERROR) << "VideoProcessorGetStreamExtension failed: "
               << logging::SystemErrorCodeToString(hr);
    SetSupportsVideoProcessorAutoHDR(false);
    return;
  }

  d3d11_video_processor.Reset();
  d3d11_video_enumerator.Reset();
  d3d11_video_context.Reset();
  d3d11_video_device.Reset();
  d3d11_context.Reset();
  d3d11_device.Reset();

  SetSupportsVideoProcessorAutoHDR(driver_supports_true_hdr == 1);
}

}  // namespace

void InitializeDirectComposition(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  DCHECK(!g_dcomp_device);
  if (GetGlWorkarounds().disable_direct_composition) {
    return;
  }

  // Blocklist direct composition if MCTU.dll or MCTUX.dll are injected. These
  // are user mode drivers for display adapters from Magic Control Technology
  // Corporation.
  if (GetModuleHandle(TEXT("MCTU.dll")) || GetModuleHandle(TEXT("MCTUX.dll"))) {
    LOG(ERROR) << "Blocklisted due to third party modules";
    return;
  }

  // Load DLL at runtime since older Windows versions don't have dcomp.
  HMODULE dcomp_module = ::GetModuleHandle(L"dcomp.dll");
  if (!dcomp_module) {
    LOG(ERROR) << "Failed to load dcomp.dll";
    return;
  }

  using PFN_DCOMPOSITION_CREATE_DEVICE3 = HRESULT(WINAPI*)(
      IUnknown * renderingDevice, REFIID iid, void** dcompositionDevice);
  PFN_DCOMPOSITION_CREATE_DEVICE3 create_device3_function =
      reinterpret_cast<PFN_DCOMPOSITION_CREATE_DEVICE3>(
          ::GetProcAddress(dcomp_module, "DCompositionCreateDevice3"));
  if (!create_device3_function) {
    LOG(ERROR) << "GetProcAddress failed for DCompositionCreateDevice3";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);

  Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
  HRESULT hr =
      create_device3_function(dxgi_device.Get(), IID_PPV_ARGS(&desktop_device));
  if (FAILED(hr)) {
    LOG(ERROR) << "DCompositionCreateDevice3 failed: "
               << logging::SystemErrorCodeToString(hr);
    return;
  }

  Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device;
  hr = desktop_device.As(&dcomp_device);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to retrieve IDCompositionDevice3: "
               << logging::SystemErrorCodeToString(hr);
    return;
  }

  g_dcomp_device = dcomp_device.Detach();
  DCHECK(g_dcomp_device);

  g_d3d11_device = d3d11_device.Detach();

  UpdateVideoProcessorAutoHDRSupport();
}

void ShutdownDirectComposition() {
  if (g_dcomp_device) {
    g_dcomp_device->Release();
    g_dcomp_device = nullptr;
    g_d3d11_device->Release();
    g_d3d11_device = nullptr;
  }
}

IDCompositionDevice3* GetDirectCompositionDevice() {
  return g_dcomp_device;
}

ID3D11Device* GetDirectCompositionD3D11Device() {
  return g_d3d11_device;
}

bool DirectCompositionSupported() {
  return g_dcomp_device && !g_direct_composition_swap_chain_failed;
}

bool DirectCompositionOverlaysSupported() {
  // Always initialize and record overlay support information irrespective of
  // command line flags.
  UpdateOverlaySupport();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Enable flag should be checked before the disable workaround, so we could
  // overwrite GPU driver bug workarounds in testing.
  if (command_line->HasSwitch(
          switches::kEnableDirectCompositionVideoOverlays)) {
    return true;
  }
  if (GetGlWorkarounds().disable_direct_composition_video_overlays) {
    return false;
  }

  return SupportsOverlays();
}

bool DirectCompositionHardwareOverlaysSupported() {
  UpdateOverlaySupport();
  return SupportsHardwareOverlays();
}

bool DirectCompositionDecodeSwapChainSupported() {
  if (!g_disable_decode_swap_chain) {
    UpdateOverlaySupport();
    return GetDirectCompositionSDROverlayFormat() == DXGI_FORMAT_NV12;
  }
  return false;
}

void DisableDirectCompositionOverlays() {
  SetSupportsOverlays(false);
  DirectCompositionOverlayCapsMonitor::GetInstance()
      ->NotifyOverlayCapsChanged();
}

bool DirectCompositionScaledOverlaysSupported() {
  UpdateOverlaySupport();
  if (g_overlay_format_used == DXGI_FORMAT_NV12) {
    return (g_nv12_overlay_support_flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING) ||
           (SupportsOverlays() && SupportsSoftwareOverlays());
  } else if (g_overlay_format_used == DXGI_FORMAT_YUY2) {
    return !!(g_yuy2_overlay_support_flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);
  } else {
    DCHECK_EQ(g_overlay_format_used, DXGI_FORMAT_B8G8R8A8_UNORM);
    // Assume scaling is supported for BGRA overlays.
    return true;
  }
}

bool VideoProcessorAutoHDRSupported() {
  return SupportsVideoProcessorAutoHDR();
}

bool CheckVideoProcessorFormatSupport(DXGI_FORMAT dxgi_format) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device = g_d3d11_device;
  if (!d3d11_device) {
    LOG(ERROR) << __func__ << ": Failed to retrieve D3D11 device";
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  if (FAILED(d3d11_device.As(&video_device))) {
    LOG(ERROR) << __func__ << ": Failed to retrieve video device";
    return false;
  }

  HRESULT hr = S_OK;

  UINT device = 0;
  hr = d3d11_device->CheckFormatSupport(dxgi_format, &device);
  if (FAILED(hr)) {
    LOG(ERROR) << "CheckFormatSupport failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }

  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = 1920;
  desc.InputHeight = 1080;
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = 1920;
  desc.OutputHeight = 1080;
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> video_enumerator;
  hr = video_device->CreateVideoProcessorEnumerator(&desc, &video_enumerator);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateVideoProcessorEnumerator failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }

  if (!video_enumerator) {
    LOG(ERROR) << "Failed to locate video enumerator";
    return false;
  }

  UINT enumerator = 0;
  hr = video_enumerator->CheckVideoProcessorFormat(dxgi_format, &enumerator);
  if (FAILED(hr)) {
    LOG(ERROR) << "CheckVideoProcessorFormat failed: "
               << logging::SystemErrorCodeToString(hr);
    video_enumerator.Reset();
    return false;
  }

  video_enumerator.Reset();
  return (enumerator & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) &&
         (device & D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT);
}

UINT GetDirectCompositionOverlaySupportFlags(DXGI_FORMAT format) {
  UpdateOverlaySupport();
  base::AutoLock auto_lock(GetOverlayLock());
  UINT support_flag = 0;
  switch (format) {
    case DXGI_FORMAT_NV12:
      support_flag = g_nv12_overlay_support_flags;
      break;
    case DXGI_FORMAT_YUY2:
      support_flag = g_yuy2_overlay_support_flags;
      break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      support_flag = g_bgra8_overlay_support_flags;
      break;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      support_flag = g_rgb10a2_overlay_support_flags;
      break;
    case DXGI_FORMAT_P010:
      support_flag = g_p010_overlay_support_flags;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return support_flag;
}

gfx::Size GetDirectCompositionPrimaryMonitorSize() {
  if (g_primary_monitor_size.IsEmpty())
    UpdateMonitorInfo();
  return g_primary_monitor_size;
}

int GetDirectCompositionNumMonitors() {
  if (g_num_monitors == 0)
    UpdateMonitorInfo();
  return g_num_monitors;
}

bool DirectCompositionSystemHDREnabled() {
  if (g_num_monitors == 0)
    UpdateMonitorInfo();
  return g_system_hdr_enabled;
}

bool DirectCompositionMonitorHDREnabled(HWND window) {
  if (g_num_monitors == 0) {
    UpdateMonitorInfo();
  }

  return GetHDRMonitors()->find(MonitorFromWindow(
             window, MONITOR_DEFAULTTONEAREST)) != GetHDRMonitors()->end();
}

DXGI_FORMAT GetDirectCompositionSDROverlayFormat() {
  return g_overlay_format_used;
}

void SetDirectCompositionScaledOverlaysSupportedForTesting(bool supported) {
  UpdateOverlaySupport();
  if (supported) {
    g_nv12_overlay_support_flags |= DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_yuy2_overlay_support_flags |= DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_rgb10a2_overlay_support_flags |= DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_p010_overlay_support_flags |= DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
  } else {
    g_nv12_overlay_support_flags &= ~DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_yuy2_overlay_support_flags &= ~DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_rgb10a2_overlay_support_flags &= ~DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    g_p010_overlay_support_flags &= ~DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
  }
  g_disable_sw_overlays = !supported;
  SetSupportsHardwareOverlays(supported);
  DCHECK_EQ(supported, DirectCompositionScaledOverlaysSupported());
}

void SetDirectCompositionOverlayFormatUsedForTesting(DXGI_FORMAT format) {
  DCHECK(format == DXGI_FORMAT_NV12 || format == DXGI_FORMAT_YUY2 ||
         format == DXGI_FORMAT_B8G8R8A8_UNORM);
  UpdateOverlaySupport();
  g_overlay_format_used = format;
  DCHECK_EQ(format, GetDirectCompositionSDROverlayFormat());
}

gfx::mojom::DXGIInfoPtr GetDirectCompositionHDRMonitorDXGIInfo() {
  auto result_info = gfx::mojom::DXGIInfo::New();

  for (const auto& desc : GetDirectCompositionOutputDescs()) {
    auto result_output = gfx::mojom::DXGIOutputDesc::New();
    result_output->device_name = desc.DeviceName;
    result_output->hdr_enabled =
        desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    result_output->primaries.fRX = desc.RedPrimary[0];
    // SAFETY: required from Windows API.
    result_output->primaries.fRY = UNSAFE_BUFFERS(desc.RedPrimary[1]);
    result_output->primaries.fGX = desc.GreenPrimary[0];
    result_output->primaries.fGY = UNSAFE_BUFFERS(desc.GreenPrimary[1]);
    result_output->primaries.fBX = desc.BluePrimary[0];
    result_output->primaries.fBY = UNSAFE_BUFFERS(desc.BluePrimary[1]);
    result_output->primaries.fWX = desc.WhitePoint[0];
    result_output->primaries.fWY = UNSAFE_BUFFERS(desc.WhitePoint[1]);
    result_output->min_luminance = desc.MinLuminance;
    result_output->max_luminance = desc.MaxLuminance;
    result_output->max_full_frame_luminance = desc.MaxFullFrameLuminance;
    result_info->output_descs.push_back(std::move(result_output));
  }

  return result_info;
}

bool DXGISwapChainTearingSupported() {
  static const bool supported = [] {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device = g_d3d11_device;
    if (!d3d11_device) {
      LOG(ERROR) << "Not using swap chain tearing because failed to retrieve "
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
      LOG(ERROR) << "Not using swap chain tearing because failed to retrieve "
                    "IDXGIFactory5 interface";
      return false;
    }

    BOOL present_allow_tearing = FALSE;
    DCHECK(dxgi_factory);
    if (FAILED(dxgi_factory->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING, &present_allow_tearing,
            sizeof(present_allow_tearing)))) {
      LOG(ERROR)
          << "Not using swap chain tearing because CheckFeatureSupport failed";
      return false;
    }
    return !!present_allow_tearing;
  }();
  return supported;
}

bool DirectCompositionSwapChainTearingEnabled() {
  return DXGISwapChainTearingSupported() && !features::UseGpuVsync();
}

bool DXGIWaitableSwapChainEnabled() {
  return base::FeatureList::IsEnabled(features::kDXGIWaitableSwapChain);
}

UINT GetDXGIWaitableSwapChainMaxQueuedFrames() {
  return static_cast<UINT>(
      features::kDXGIWaitableSwapChainMaxQueuedFrames.Get());
}

void SetDirectCompositionOverlayWorkarounds(
    const DirectCompositionOverlayWorkarounds& workarounds) {
  // This has to be set before initializing overlay caps.
  CHECK(!OverlayCapsValid());
  g_disable_sw_overlays = workarounds.disable_sw_video_overlays;
  g_disable_decode_swap_chain = workarounds.disable_decode_swap_chain;
  g_enable_bgra8_overlays_with_yuv_overlay_support =
      workarounds.enable_bgra8_overlays_with_yuv_overlay_support;
  g_force_nv12_overlay_support = workarounds.force_nv12_overlay_support;
  g_force_rgb10a2_overlay_support = workarounds.force_rgb10a2_overlay_support;
  g_check_ycbcr_studio_g22_left_p709_for_nv12_support =
      workarounds.check_ycbcr_studio_g22_left_p709_for_nv12_support;
}

void SetDirectCompositionSwapChainFailed() {
  if (!g_direct_composition_swap_chain_failed) {
    g_direct_composition_swap_chain_failed = true;
    DirectCompositionOverlayCapsMonitor::GetInstance()
        ->NotifyOverlayCapsChanged();
  }
}

void SetDirectCompositionMonitorInfoForTesting(
    int num_monitors,
    const gfx::Size& primary_monitor_size) {
  g_num_monitors = num_monitors;
  g_primary_monitor_size = primary_monitor_size;
}

std::optional<bool> g_direct_composition_texture_supported;

bool DirectCompositionTextureSupported() {
  if (g_direct_composition_texture_supported.has_value()) {
    return g_direct_composition_texture_supported.value();
  }

  if (!g_dcomp_device || !g_d3d11_device) {
    // We don't support DComp textures if we haven't initialized Direct
    // Composition. This can happen if Direct Composition is disabled, e.g.
    // during software rendering mode.
    return false;
  }

  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device = g_dcomp_device;
  CHECK(dcomp_device);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device = g_d3d11_device;
  CHECK(d3d11_device);

  // Set the result to false early in case any of the following conditions fail.
  // We don't set this earlier in case this function is called before
  // |InitializeDirectComposition|.
  g_direct_composition_texture_supported = false;

  Microsoft::WRL::ComPtr<IDCompositionDevice4> dcomp_device4;
  HRESULT hr = dcomp_device.As(&dcomp_device4);
  if (FAILED(hr)) {
    // Not a recent enough Windows system
    LOG(ERROR) << "QueryInterface to IDCompositionDevice4 failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }

  BOOL supports_composition_textures = FALSE;
  hr = dcomp_device4->CheckCompositionTextureSupport(
      d3d11_device.Get(), &supports_composition_textures);
  if (FAILED(hr)) {
    LOG(ERROR) << "CheckCompositionTextureSupport failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }

  if (supports_composition_textures == FALSE) {
    LOG(ERROR) << "CheckCompositionTextureSupport reported unsupported";
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11on12_device;
  if (SUCCEEDED(d3d11_device.As(&d3d11on12_device))) {
    // IDCompositionTexture is not implemented on an 11on12, even though the
    // device will claim support for it.
    LOG(WARNING) << "IDCompositionTexture is not supported on 11on12 devices.";
    return false;
  }

  g_direct_composition_texture_supported = true;
  return true;
}

// For DirectComposition Display Monitor.
DirectCompositionOverlayCapsMonitor::DirectCompositionOverlayCapsMonitor()
    : observer_list_(new base::ObserverListThreadSafe<
                     DirectCompositionOverlayCapsObserver>()) {
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
}

DirectCompositionOverlayCapsMonitor::~DirectCompositionOverlayCapsMonitor() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
}

// static
DirectCompositionOverlayCapsMonitor*
DirectCompositionOverlayCapsMonitor::GetInstance() {
  static base::NoDestructor<DirectCompositionOverlayCapsMonitor>
      direct_compoisition_overlay_cap_monitor;
  return direct_compoisition_overlay_cap_monitor.get();
}

void DirectCompositionOverlayCapsMonitor::AddObserver(
    DirectCompositionOverlayCapsObserver* observer) {
  observer_list_->AddObserver(observer);
}

void DirectCompositionOverlayCapsMonitor::RemoveObserver(
    DirectCompositionOverlayCapsObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

void DirectCompositionOverlayCapsMonitor::NotifyOverlayCapsChanged() {
  observer_list_->Notify(
      FROM_HERE, &DirectCompositionOverlayCapsObserver::OnOverlayCapsChanged);
}

// Called from GpuSwitchingObserver on the GPU main thread.
void DirectCompositionOverlayCapsMonitor::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {}

// Called from GpuSwitchingObserver on the GPU main thread.
void DirectCompositionOverlayCapsMonitor::OnDisplayAdded() {
  SetOverlayCapsValid(false);
  UpdateOverlaySupport();
  UpdateVideoProcessorAutoHDRSupport();
  UpdateMonitorInfo();

  NotifyOverlayCapsChanged();
}

// Called from GpuSwitchingObserver on the GPU main thread.
void DirectCompositionOverlayCapsMonitor::OnDisplayRemoved() {
  SetOverlayCapsValid(false);
  UpdateOverlaySupport();
  UpdateVideoProcessorAutoHDRSupport();
  UpdateMonitorInfo();

  NotifyOverlayCapsChanged();
}

// Called from GpuSwitchingObserver on the GPU main thread.
void DirectCompositionOverlayCapsMonitor::OnDisplayMetricsChanged() {
  UpdateMonitorInfo();

  NotifyOverlayCapsChanged();
}

}  // namespace gl
