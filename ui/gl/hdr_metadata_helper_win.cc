// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/hdr_metadata_helper_win.h"
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

absl::optional<DXGI_HDR_METADATA_HDR10>
HDRMetadataHelperWin::GetDisplayMetadata() {
  return hdr_metadata_;
}

void HDRMetadataHelperWin::UpdateDisplayMetadata() {
  hdr_metadata_.reset();

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

  DXGI_OUTPUT_DESC1 desc_best{};
  bool found_monitor = false;

  // Enumerate all the monitors attached to all the adapters.  Pick the
  // brightest monitor as the one we want, which makes no sense really.
  // TODO(liberato): figure out what monitor we're actually using, or get that
  // from the renderer.
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

      if (desc_best.MaxLuminance < desc1.MaxLuminance) {
        desc_best = desc1;
        found_monitor = true;
      }
    }
  }

  if (!found_monitor)
    return;

  DXGI_HDR_METADATA_HDR10 metadata{};

  auto& primary_r = desc_best.RedPrimary;
  metadata.RedPrimary[0] = primary_r[0] * kPrimariesFixedPoint;
  metadata.RedPrimary[1] = primary_r[1] * kPrimariesFixedPoint;
  auto& primary_g = desc_best.GreenPrimary;
  metadata.GreenPrimary[0] = primary_g[0] * kPrimariesFixedPoint;
  metadata.GreenPrimary[1] = primary_g[1] * kPrimariesFixedPoint;
  auto& primary_b = desc_best.BluePrimary;
  metadata.BluePrimary[0] = primary_b[0] * kPrimariesFixedPoint;
  metadata.BluePrimary[1] = primary_b[1] * kPrimariesFixedPoint;
  auto& white_point = desc_best.WhitePoint;
  metadata.WhitePoint[0] = white_point[0] * kPrimariesFixedPoint;
  metadata.WhitePoint[1] = white_point[1] * kPrimariesFixedPoint;
  metadata.MaxMasteringLuminance = desc_best.MaxLuminance;
  metadata.MinMasteringLuminance =
      desc_best.MinLuminance * kMinLuminanceFixedPoint;
  // It's unclear how to set these properly, so this is a guess.
  // Also note that these are not fixed-point.
  metadata.MaxContentLightLevel = desc_best.MaxFullFrameLuminance;
  metadata.MaxFrameAverageLightLevel = desc_best.MaxFullFrameLuminance;

  hdr_metadata_ = metadata;
}

// static
DXGI_HDR_METADATA_HDR10 HDRMetadataHelperWin::HDRMetadataToDXGI(
    const gfx::HDRMetadata& hdr_metadata) {
  DXGI_HDR_METADATA_HDR10 metadata{};
  const auto& primaries = hdr_metadata.color_volume_metadata.primaries;
  metadata.RedPrimary[0] = primaries.fRX * kPrimariesFixedPoint;
  metadata.RedPrimary[1] = primaries.fRY * kPrimariesFixedPoint;
  metadata.GreenPrimary[0] = primaries.fGX * kPrimariesFixedPoint;
  metadata.GreenPrimary[1] = primaries.fGY * kPrimariesFixedPoint;
  metadata.BluePrimary[0] = primaries.fBX * kPrimariesFixedPoint;
  metadata.BluePrimary[1] = primaries.fBY * kPrimariesFixedPoint;
  metadata.WhitePoint[0] = primaries.fWX * kPrimariesFixedPoint;
  metadata.WhitePoint[1] = primaries.fWY * kPrimariesFixedPoint;
  metadata.MaxMasteringLuminance =
      hdr_metadata.color_volume_metadata.luminance_max;
  metadata.MinMasteringLuminance =
      hdr_metadata.color_volume_metadata.luminance_min *
      kMinLuminanceFixedPoint;
  metadata.MaxContentLightLevel = hdr_metadata.max_content_light_level;
  metadata.MaxFrameAverageLightLevel =
      hdr_metadata.max_frame_average_light_level;

  return metadata;
}

void HDRMetadataHelperWin::OnDisplayAdded() {
  UpdateDisplayMetadata();
}

void HDRMetadataHelperWin::OnDisplayRemoved() {
  UpdateDisplayMetadata();
}
}  // namespace gl
