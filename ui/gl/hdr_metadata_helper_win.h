// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_HDR_METADATA_HELPER_WIN_H_
#define UI_GL_HDR_METADATA_HELPER_WIN_H_

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <optional>
#include <unordered_map>

#include "ui/gfx/hdr_metadata.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gpu_switching_observer.h"

namespace gl {

class GL_EXPORT HDRMetadataHelperWin : ui::GpuSwitchingObserver {
 public:
  explicit HDRMetadataHelperWin(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  HDRMetadataHelperWin(const HDRMetadataHelperWin&) = delete;
  HDRMetadataHelperWin& operator=(const HDRMetadataHelperWin&) = delete;

  ~HDRMetadataHelperWin() override;

  // Return the metadata of the brightest monitor, if available.  Must call
  // UpdateDisplayMetadata first.
  std::optional<DXGI_HDR_METADATA_HDR10> GetDisplayMetadata();

  // Return the metadata of a given window's monitor, if available.  Must call
  // UpdateDisplayMetadata first.
  std::optional<DXGI_HDR_METADATA_HDR10> GetDisplayMetadata(HWND window);

  // Query the display metadata from all monitors. In the event of monitor
  // hot plugging, the metadata should be updated again.
  void UpdateDisplayMetadata();

  // Convert |hdr_metadata| to DXGI's metadata format.
  static DXGI_HDR_METADATA_HDR10 HDRMetadataToDXGI(
      const gfx::HDRMetadata& hdr_metadata);

  // Convert |desc1| to DXGI's metadata format.
  static DXGI_HDR_METADATA_HDR10 OutputDESC1ToDXGI(
      const DXGI_OUTPUT_DESC1& desc1);

  // Implements GpuSwitchingObserver
  void OnDisplayAdded() override;
  void OnDisplayRemoved() override;

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  HMONITOR brightest_monitor_ = nullptr;
  std::unordered_map<HMONITOR, DXGI_HDR_METADATA_HDR10> hdr_metadatas_;
};

}  // namespace gl

#endif  // UI_GL_HDR_METADATA_HELPER_WIN_H_
