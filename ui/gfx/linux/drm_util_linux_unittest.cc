// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/drm_util_linux.h"

#include <drm_fourcc.h>

#include "base/containers/flat_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(DrmUtilLinuxTest, ConvertsDrmFormats) {
  const base::flat_map<uint32_t, std::string> format_expectations = {
      {DRM_FORMAT_C8, "C8"},
      {DRM_FORMAT_R8, "R8"},
      {DRM_FORMAT_R16, "R16"},
      {DRM_FORMAT_RG88, "RG88"},
      {DRM_FORMAT_GR88, "GR88"},
      {DRM_FORMAT_RG1616, "RG32"},
      {DRM_FORMAT_GR1616, "GR32"},
      {DRM_FORMAT_RGB332, "RGB8"},
      {DRM_FORMAT_BGR233, "BGR8"},
      {DRM_FORMAT_XRGB4444, "XR12"},
      {DRM_FORMAT_XBGR4444, "XB12"},
      {DRM_FORMAT_RGBX4444, "RX12"},
      {DRM_FORMAT_BGRX4444, "BX12"},
      {DRM_FORMAT_ARGB4444, "AR12"},
      {DRM_FORMAT_ABGR4444, "AB12"},
      {DRM_FORMAT_RGBA4444, "RA12"},
      {DRM_FORMAT_BGRA4444, "BA12"},
      {DRM_FORMAT_XRGB1555, "XR15"},
      {DRM_FORMAT_XBGR1555, "XB15"},
      {DRM_FORMAT_RGBX5551, "RX15"},
      {DRM_FORMAT_BGRX5551, "BX15"},
      {DRM_FORMAT_ARGB1555, "AR15"},
      {DRM_FORMAT_ABGR1555, "AB15"},
      {DRM_FORMAT_RGBA5551, "RA15"},
      {DRM_FORMAT_BGRA5551, "BA15"},
      {DRM_FORMAT_RGB565, "RG16"},
      {DRM_FORMAT_BGR565, "BG16"},
      {DRM_FORMAT_RGB888, "RG24"},
      {DRM_FORMAT_BGR888, "BG24"},
      {DRM_FORMAT_XRGB8888, "XR24"},
      {DRM_FORMAT_XBGR8888, "XB24"},
      {DRM_FORMAT_RGBX8888, "RX24"},
      {DRM_FORMAT_BGRX8888, "BX24"},
      {DRM_FORMAT_ARGB8888, "AR24"},
      {DRM_FORMAT_ABGR8888, "AB24"},
      {DRM_FORMAT_RGBA8888, "RA24"},
      {DRM_FORMAT_BGRA8888, "BA24"},
      {DRM_FORMAT_XRGB2101010, "XR30"},
      {DRM_FORMAT_XBGR2101010, "XB30"},
      {DRM_FORMAT_RGBX1010102, "RX30"},
      {DRM_FORMAT_BGRX1010102, "BX30"},
      {DRM_FORMAT_ARGB2101010, "AR30"},
      {DRM_FORMAT_ABGR2101010, "AB30"},
      {DRM_FORMAT_RGBA1010102, "RA30"},
      {DRM_FORMAT_BGRA1010102, "BA30"},
      {DRM_FORMAT_XRGB16161616F, "XR4H"},
      {DRM_FORMAT_XBGR16161616F, "XB4H"},
      {DRM_FORMAT_ARGB16161616F, "AR4H"},
      {DRM_FORMAT_ABGR16161616F, "AB4H"},
      {DRM_FORMAT_AXBXGXRX106106106106, "AB10"},
      {DRM_FORMAT_YUYV, "YUYV"},
      {DRM_FORMAT_YVYU, "YVYU"},
      {DRM_FORMAT_UYVY, "UYVY"},
      {DRM_FORMAT_VYUY, "VYUY"},
      {DRM_FORMAT_AYUV, "AYUV"},
      {DRM_FORMAT_XYUV8888, "XYUV"},
      {DRM_FORMAT_VUY888, "VU24"},
      {DRM_FORMAT_VUY101010, "VU30"},
      {DRM_FORMAT_Y210, "Y210"},
      {DRM_FORMAT_Y212, "Y212"},
      {DRM_FORMAT_Y216, "Y216"},
      {DRM_FORMAT_Y410, "Y410"},
      {DRM_FORMAT_Y412, "Y412"},
      {DRM_FORMAT_Y416, "Y416"},
      {DRM_FORMAT_XVYU2101010, "XV30"},
      {DRM_FORMAT_XVYU12_16161616, "XV36"},
      {DRM_FORMAT_XVYU16161616, "XV48"},
      {DRM_FORMAT_Y0L0, "Y0L0"},
      {DRM_FORMAT_X0L0, "X0L0"},
      {DRM_FORMAT_Y0L2, "Y0L2"},
      {DRM_FORMAT_X0L2, "X0L2"},
      {DRM_FORMAT_YUV420_8BIT, "YU08"},
      {DRM_FORMAT_YUV420_10BIT, "YU10"},
      {DRM_FORMAT_XRGB8888_A8, "XRA8"},
      {DRM_FORMAT_XBGR8888_A8, "XBA8"},
      {DRM_FORMAT_RGBX8888_A8, "RXA8"},
      {DRM_FORMAT_BGRX8888_A8, "BXA8"},
      {DRM_FORMAT_RGB888_A8, "R8A8"},
      {DRM_FORMAT_BGR888_A8, "B8A8"},
      {DRM_FORMAT_RGB565_A8, "R5A8"},
      {DRM_FORMAT_BGR565_A8, "B5A8"},
      {DRM_FORMAT_NV12, "NV12"},
      {DRM_FORMAT_NV21, "NV21"},
      {DRM_FORMAT_NV16, "NV16"},
      {DRM_FORMAT_NV61, "NV61"},
      {DRM_FORMAT_NV24, "NV24"},
      {DRM_FORMAT_NV42, "NV42"},
      {DRM_FORMAT_NV15, "NV15"},
      {DRM_FORMAT_P210, "P210"},
      {DRM_FORMAT_P010, "P010"},
      {DRM_FORMAT_P012, "P012"},
      {DRM_FORMAT_P016, "P016"},
      {DRM_FORMAT_Q410, "Q410"},
      {DRM_FORMAT_Q401, "Q401"},
      {DRM_FORMAT_YUV410, "YUV9"},
      {DRM_FORMAT_YVU410, "YVU9"},
      {DRM_FORMAT_YUV411, "YU11"},
      {DRM_FORMAT_YVU411, "YV11"},
      {DRM_FORMAT_YUV420, "YU12"},
      {DRM_FORMAT_YVU420, "YV12"},
      {DRM_FORMAT_YUV422, "YU16"},
      {DRM_FORMAT_YVU422, "YV16"},
      {DRM_FORMAT_YUV444, "YU24"},
      {DRM_FORMAT_YVU444, "YV24"},
  };

  for (const auto& [drm_format, string] : format_expectations)
    EXPECT_EQ(string, DrmBufferFormatToString(drm_format));
}

TEST(DrmUtilLinuxTest, InvalidFormats) {
  const std::vector<uint32_t> format_expectations = {
      DRM_FORMAT_INVALID /* = 0 */,
      0xFFFFFFFF,
      0x00110011,
  };

  for (auto bad_drm_format : format_expectations)
    EXPECT_EQ("INVALID", DrmBufferFormatToString(bad_drm_format));
}

}  // namespace ui
