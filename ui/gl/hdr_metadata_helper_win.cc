// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/hdr_metadata_helper_win.h"

namespace {

// Magic constants to convert to fixed point.
// https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_5/ns-dxgi1_5-dxgi_hdr_metadata_hdr10
static constexpr int kPrimariesFixedPoint = 50000;
static constexpr int kLuminanceFixedPoint = 10000;

}  // namespace

namespace gl {

HDRMetadataHelperWin::HDRMetadataHelperWin(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device) {
  CacheDisplayMetadata(d3d11_device);
}

HDRMetadataHelperWin::~HDRMetadataHelperWin() = default;

base::Optional<DXGI_HDR_METADATA_HDR10>
HDRMetadataHelperWin::GetDisplayMetadata() {
  return hdr_metadata_;
}

void HDRMetadataHelperWin::CacheDisplayMetadata(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device) {
  hdr_metadata_.reset();

  if (!d3d11_device)
    return;

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device.As(&dxgi_device)))
    return;

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter)))
    return;

  Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
  if (FAILED(dxgi_adapter->GetParent(__uuidof(IDXGIFactory), &dxgi_factory)))
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
  metadata.MaxMasteringLuminance =
      desc_best.MaxLuminance * kLuminanceFixedPoint;
  metadata.MinMasteringLuminance =
      desc_best.MinLuminance * kLuminanceFixedPoint;
  // It's unclear how to set these properly, so this is a guess.
  // Also note that these are not fixed-point.
  metadata.MaxContentLightLevel = desc_best.MaxFullFrameLuminance;
  metadata.MaxFrameAverageLightLevel = desc_best.MaxFullFrameLuminance;

  hdr_metadata_ = metadata;
}

// static
DXGI_HDR_METADATA_HDR10 HDRMetadataHelperWin::HDRMetadataToDXGI(
    const HDRMetadata& hdr_metadata) {
  DXGI_HDR_METADATA_HDR10 metadata{};

  auto& primary_r = hdr_metadata.mastering_metadata.primary_r;
  metadata.RedPrimary[0] = primary_r.x() * kPrimariesFixedPoint;
  metadata.RedPrimary[1] = primary_r.y() * kPrimariesFixedPoint;
  auto& primary_g = hdr_metadata.mastering_metadata.primary_g;
  metadata.GreenPrimary[0] = primary_g.x() * kPrimariesFixedPoint;
  metadata.GreenPrimary[1] = primary_g.y() * kPrimariesFixedPoint;
  auto& primary_b = hdr_metadata.mastering_metadata.primary_b;
  metadata.BluePrimary[0] = primary_b.x() * kPrimariesFixedPoint;
  metadata.BluePrimary[1] = primary_b.y() * kPrimariesFixedPoint;
  auto& white_point = hdr_metadata.mastering_metadata.white_point;
  metadata.WhitePoint[0] = white_point.x() * kPrimariesFixedPoint;
  metadata.WhitePoint[1] = white_point.y() * kPrimariesFixedPoint;
  metadata.MaxMasteringLuminance =
      hdr_metadata.mastering_metadata.luminance_max * kLuminanceFixedPoint;
  metadata.MinMasteringLuminance =
      hdr_metadata.mastering_metadata.luminance_min * kLuminanceFixedPoint;
  metadata.MaxContentLightLevel = hdr_metadata.max_content_light_level;
  metadata.MaxFrameAverageLightLevel =
      hdr_metadata.max_frame_average_light_level;

  return metadata;
}

}  // namespace gl
