// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_HDR_METADATA_HELPER_WIN_H_
#define UI_GL_HDR_METADATA_HELPER_WIN_H_

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gl/gl_export.h"

namespace gl {

// This is a very hacky way to get the display characteristics.
// It should be replaced by something that actually knows which
// display is going to be used for, well, display.
class GL_EXPORT HDRMetadataHelperWin {
 public:
  explicit HDRMetadataHelperWin(
      const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device);
  ~HDRMetadataHelperWin();

  // Return the metadata for the display, if available.  Must call
  // CacheDisplayMetadata first.
  base::Optional<DXGI_HDR_METADATA_HDR10> GetDisplayMetadata();

  // Convert |hdr_metadata| to DXGI's metadata format.
  static DXGI_HDR_METADATA_HDR10 HDRMetadataToDXGI(
      const gfx::HDRMetadata& hdr_metadata);

 private:
  void CacheDisplayMetadata(
      const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device);

  base::Optional<DXGI_HDR_METADATA_HDR10> hdr_metadata_;

  DISALLOW_COPY_AND_ASSIGN(HDRMetadataHelperWin);
};

}  // namespace gl

#endif  // UI_GL_HDR_METADATA_HELPER_WIN_H_
