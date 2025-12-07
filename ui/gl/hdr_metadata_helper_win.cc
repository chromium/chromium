// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/hdr_metadata_helper_win.h"

#include "base/compiler_specific.h"
#include "ui/gl/gpu_switching_manager.h"

namespace {

// Magic constants to convert to fixed point.
// https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_5/ns-dxgi1_5-dxgi_hdr_metadata_hdr10
static constexpr int kPrimariesFixedPoint = 50000;
static constexpr int kMinLuminanceFixedPoint = 10000;

}  // namespace

namespace gl {

HDRMetadataHelperWin::HDRMetadataHelperWin(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device)
    : d3d11_device_(std::move(d3d11_device)) {
  UpdateDisplayMetadata();
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
}

HDRMetadataHelperWin::~HDRMetadataHelperWin() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
}

std::optional<DXGI_HDR_METADATA_HDR10>
HDRMetadataHelperWin::GetDisplayMetadata() {
  if (!brightest_monitor_) {
    return std::nullopt;
  }
  auto it = hdr_metadatas_.find(brightest_monitor_);
  if (it == hdr_metadatas_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<DXGI_HDR_METADATA_HDR10> HDRMetadataHelperWin::GetDisplayMetadata(
    HWND window) {
  auto it =
      hdr_metadatas_.find(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST));
  if (it == hdr_metadatas_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void HDRMetadataHelperWin::UpdateDisplayMetadata() {
  brightest_monitor_ = nullptr;
  hdr_metadatas_.clear();

  if (!d3d11_device_)
    return;

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device_.As(&dxgi_device)))
    return;

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter)))
    return;

  Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
  if (FAILED(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory))))
    return;

  FLOAT max_luminance = 0;
  HMONITOR brightest_monitor = nullptr;
  std::unordered_map<HMONITOR, DXGI_HDR_METADATA_HDR10> hdr_metadatas;

  // Enumerate all the monitors attached to all the adapters.  Pick the
  // brightest monitor as the one we want as default.
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  for (unsigned int i = 0;
       dxgi_factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    for (unsigned int u = 0;
         adapter->EnumOutputs(u, &output) != DXGI_ERROR_NOT_FOUND; u++) {
      Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
      if (FAILED(output.As(&output6)))
        continue;

      DXGI_OUTPUT_DESC1 desc1{};
      if (FAILED(output6->GetDesc1(&desc1)))
        continue;

      if (max_luminance < desc1.MaxLuminance) {
        max_luminance = desc1.MaxLuminance;
        brightest_monitor = desc1.Monitor;
      }

      hdr_metadatas[desc1.Monitor] = OutputDESC1ToDXGI(desc1);
    }
  }

  if (!brightest_monitor) {
    return;
  }

  brightest_monitor_ = brightest_monitor;
  hdr_metadatas_ = std::move(hdr_metadatas);
}

// static
DXGI_HDR_METADATA_HDR10 HDRMetadataHelperWin::HDRMetadataToDXGI(
    const gfx::HDRMetadata& hdr_metadata) {
  DXGI_HDR_METADATA_HDR10 metadata{};

  const auto smpte_st_2086 =
      hdr_metadata.smpte_st_2086.value_or(gfx::HdrMetadataSmpteSt2086());
  const auto& primaries = smpte_st_2086.primaries;
  metadata.RedPrimary[0] = primaries.fRX * kPrimariesFixedPoint;
  // SAFETY: required from Windows API.
  UNSAFE_BUFFERS(metadata.RedPrimary[1]) = primaries.fRY * kPrimariesFixedPoint;
  metadata.GreenPrimary[0] = primaries.fGX * kPrimariesFixedPoint;
  UNSAFE_BUFFERS(metadata.GreenPrimary[1]) =
      primaries.fGY * kPrimariesFixedPoint;
  metadata.BluePrimary[0] = primaries.fBX * kPrimariesFixedPoint;
  UNSAFE_BUFFERS(metadata.BluePrimary[1]) =
      primaries.fBY * kPrimariesFixedPoint;
  metadata.WhitePoint[0] = primaries.fWX * kPrimariesFixedPoint;
  UNSAFE_BUFFERS(metadata.WhitePoint[1]) = primaries.fWY * kPrimariesFixedPoint;
  metadata.MaxMasteringLuminance = smpte_st_2086.luminance_max;
  metadata.MinMasteringLuminance =
      smpte_st_2086.luminance_min * kMinLuminanceFixedPoint;

  const auto cta_861_3 =
      hdr_metadata.cta_861_3.value_or(gfx::HdrMetadataCta861_3());
  metadata.MaxContentLightLevel = cta_861_3.max_content_light_level;
  metadata.MaxFrameAverageLightLevel = cta_861_3.max_frame_average_light_level;

  return metadata;
}

DXGI_HDR_METADATA_HDR10 HDRMetadataHelperWin::OutputDESC1ToDXGI(
    const DXGI_OUTPUT_DESC1& desc1) {
  DXGI_HDR_METADATA_HDR10 metadata{};

  auto& primary_r = desc1.RedPrimary;
  metadata.RedPrimary[0] = primary_r[0] * kPrimariesFixedPoint;
  // SAFETY: required from Windows API.
  UNSAFE_BUFFERS(metadata.RedPrimary[1]) =
      UNSAFE_BUFFERS(primary_r[1]) * kPrimariesFixedPoint;
  auto& primary_g = desc1.GreenPrimary;
  metadata.GreenPrimary[0] = primary_g[0] * kPrimariesFixedPoint;
  UNSAFE_BUFFERS(metadata.GreenPrimary[1]) =
      UNSAFE_BUFFERS(primary_g[1]) * kPrimariesFixedPoint;
  auto& primary_b = desc1.BluePrimary;
  metadata.BluePrimary[0] = primary_b[0] * kPrimariesFixedPoint;
  UNSAFE_BUFFERS(metadata.BluePrimary[1]) =
      UNSAFE_BUFFERS(primary_b[1]) * kPrimariesFixedPoint;
  auto& white_point = desc1.WhitePoint;
  metadata.WhitePoint[0] = white_point[0] * kPrimariesFixedPoint;
  UNSAFE_BUFFERS(metadata.WhitePoint[1]) =
      UNSAFE_BUFFERS(white_point[1]) * kPrimariesFixedPoint;
  metadata.MaxMasteringLuminance = desc1.MaxLuminance;
  metadata.MinMasteringLuminance = desc1.MinLuminance * kMinLuminanceFixedPoint;
  // It's unclear how to set these properly, so this is a guess.
  // Also note that these are not fixed-point.
  metadata.MaxContentLightLevel = desc1.MaxFullFrameLuminance;
  metadata.MaxFrameAverageLightLevel = desc1.MaxFullFrameLuminance;

  return metadata;
}

void HDRMetadataHelperWin::OnDisplayAdded() {
  UpdateDisplayMetadata();
}

void HDRMetadataHelperWin::OnDisplayRemoved() {
  UpdateDisplayMetadata();
}
}  // namespace gl
