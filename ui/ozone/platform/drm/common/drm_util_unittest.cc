// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <map>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

bool operator==(const ui::DisplayMode_Params& a,
                const ui::DisplayMode_Params& b) {
  return a.size == b.size && a.is_interlaced == b.is_interlaced &&
         a.refresh_rate == b.refresh_rate;
}

bool operator==(const ui::DisplaySnapshot_Params& a,
                const ui::DisplaySnapshot_Params& b) {
  return a.display_id == b.display_id && a.origin == b.origin &&
         a.physical_size == b.physical_size && a.type == b.type &&
         a.is_aspect_preserving_scaling == b.is_aspect_preserving_scaling &&
         a.has_overscan == b.has_overscan &&
         a.has_color_correction_matrix == b.has_color_correction_matrix &&
         a.display_name == b.display_name && a.sys_path == b.sys_path &&
         std::equal(a.modes.cbegin(), a.modes.cend(), b.modes.cbegin()) &&
         std::equal(a.edid.cbegin(), a.edid.cend(), a.edid.cbegin()) &&
         a.has_current_mode == b.has_current_mode &&
         a.current_mode == b.current_mode &&
         a.has_native_mode == b.has_native_mode &&
         a.native_mode == b.native_mode && a.product_code == b.product_code &&
         a.year_of_manufacture == b.year_of_manufacture &&
         a.maximum_cursor_size == b.maximum_cursor_size;
}

namespace {

DisplayMode_Params MakeDisplay(float refresh) {
  DisplayMode_Params params;
  params.size = gfx::Size(101, 42);
  params.is_interlaced = true;
  params.refresh_rate = refresh;
  return params;
}

void DetailedCompare(const ui::DisplaySnapshot_Params& a,
                     const ui::DisplaySnapshot_Params& b) {
  EXPECT_EQ(a.display_id, b.display_id);
  EXPECT_EQ(a.origin, b.origin);
  EXPECT_EQ(a.physical_size, b.physical_size);
  EXPECT_EQ(a.type, b.type);
  EXPECT_EQ(a.is_aspect_preserving_scaling, b.is_aspect_preserving_scaling);
  EXPECT_EQ(a.has_overscan, b.has_overscan);
  EXPECT_EQ(a.has_color_correction_matrix, b.has_color_correction_matrix);
  EXPECT_EQ(a.color_space, b.color_space);
  EXPECT_EQ(a.bits_per_channel, b.bits_per_channel);
  EXPECT_EQ(a.display_name, b.display_name);
  EXPECT_EQ(a.sys_path, b.sys_path);
  EXPECT_EQ(a.modes, b.modes);
  EXPECT_EQ(a.edid, b.edid);
  EXPECT_EQ(a.has_current_mode, b.has_current_mode);
  EXPECT_EQ(a.current_mode, b.current_mode);
  EXPECT_EQ(a.has_native_mode, b.has_native_mode);
  EXPECT_EQ(a.native_mode, b.native_mode);
  EXPECT_EQ(a.product_code, b.product_code);
  EXPECT_EQ(a.year_of_manufacture, b.year_of_manufacture);
  EXPECT_EQ(a.maximum_cursor_size, b.maximum_cursor_size);
}

}  // namespace

class DrmUtilTest : public testing::Test {};

TEST_F(DrmUtilTest, RoundTripDisplayMode) {
  DisplayMode_Params orig_params = MakeDisplay(3.14);

  auto udm = CreateDisplayModeFromParams(orig_params);
  auto roundtrip_params = GetDisplayModeParams(*udm.get());

  EXPECT_EQ(orig_params.size.width(), roundtrip_params.size.width());
  EXPECT_EQ(orig_params.size.height(), roundtrip_params.size.height());
  EXPECT_EQ(orig_params.is_interlaced, roundtrip_params.is_interlaced);
  EXPECT_EQ(orig_params.refresh_rate, roundtrip_params.refresh_rate);
}

TEST_F(DrmUtilTest, RoundTripDisplaySnapshot) {
  std::vector<DisplaySnapshot_Params> orig_params;
  DisplaySnapshot_Params fp, sp, ep;

  fp.display_id = 101;
  fp.origin = gfx::Point(101, 42);
  fp.physical_size = gfx::Size(102, 43);
  fp.type = display::DISPLAY_CONNECTION_TYPE_INTERNAL;
  fp.is_aspect_preserving_scaling = true;
  fp.has_overscan = true;
  fp.has_color_correction_matrix = true;
  fp.color_space = gfx::ColorSpace::CreateREC709();
  fp.bits_per_channel = 8u;
  fp.display_name = "bending glass";
  fp.sys_path = base::FilePath("/bending");
  fp.modes =
      std::vector<DisplayMode_Params>({MakeDisplay(1.1), MakeDisplay(1.2)});
  fp.edid = std::vector<uint8_t>({'f', 'p'});
  fp.has_current_mode = true;
  fp.current_mode = MakeDisplay(1.2);
  fp.has_native_mode = true;
  fp.native_mode = MakeDisplay(1.1);
  fp.product_code = 7;
  fp.year_of_manufacture = 1776;
  fp.maximum_cursor_size = gfx::Size(103, 44);

  sp.display_id = 1002;
  sp.origin = gfx::Point(500, 42);
  sp.physical_size = gfx::Size(500, 43);
  sp.type = display::DISPLAY_CONNECTION_TYPE_INTERNAL;
  sp.is_aspect_preserving_scaling = true;
  sp.has_overscan = true;
  sp.has_color_correction_matrix = true;
  sp.color_space = gfx::ColorSpace::CreateExtendedSRGB();
  sp.bits_per_channel = 8u;
  sp.display_name = "rigid glass";
  sp.sys_path = base::FilePath("/bending");
  sp.modes =
      std::vector<DisplayMode_Params>({MakeDisplay(500.1), MakeDisplay(500.2)});
  sp.edid = std::vector<uint8_t>({'s', 'p'});
  sp.has_current_mode = false;
  sp.has_native_mode = true;
  sp.native_mode = MakeDisplay(500.2);
  sp.product_code = 8;
  sp.year_of_manufacture = 2018;
  sp.maximum_cursor_size = gfx::Size(500, 44);

  ep.display_id = 2002;
  ep.origin = gfx::Point(1000, 42);
  ep.physical_size = gfx::Size(1000, 43);
  ep.type = display::DISPLAY_CONNECTION_TYPE_INTERNAL;
  ep.is_aspect_preserving_scaling = false;
  ep.has_overscan = false;
  ep.has_color_correction_matrix = false;
  ep.color_space = gfx::ColorSpace::CreateDisplayP3D65();
  sp.bits_per_channel = 9u;
  ep.display_name = "fluted glass";
  ep.sys_path = base::FilePath("/bending");
  ep.modes = std::vector<DisplayMode_Params>(
      {MakeDisplay(1000.1), MakeDisplay(1000.2)});
  ep.edid = std::vector<uint8_t>({'s', 'p'});
  ep.has_current_mode = true;
  ep.current_mode = MakeDisplay(1000.2);
  ep.has_native_mode = false;
  ep.product_code = 9;
  ep.year_of_manufacture = 2000;
  ep.maximum_cursor_size = gfx::Size(1000, 44);

  orig_params.push_back(fp);
  orig_params.push_back(sp);
  orig_params.push_back(ep);

  MovableDisplaySnapshots intermediate_snapshots;
  for (const auto& snapshot_params : orig_params)
    intermediate_snapshots.push_back(CreateDisplaySnapshot(snapshot_params));

  std::vector<DisplaySnapshot_Params> roundtrip_params =
      CreateDisplaySnapshotParams(intermediate_snapshots);

  DetailedCompare(fp, roundtrip_params[0]);

  EXPECT_EQ(fp, roundtrip_params[0]);
  EXPECT_EQ(sp, roundtrip_params[1]);
  EXPECT_EQ(ep, roundtrip_params[2]);
}

TEST_F(DrmUtilTest, OverlaySurfaceCandidate) {
  OverlaySurfaceCandidateList input;

  OverlaySurfaceCandidate input_osc;
  input_osc.transform = gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
  input_osc.format = gfx::BufferFormat::YUV_420_BIPLANAR;
  input_osc.buffer_size = gfx::Size(100, 50);
  input_osc.display_rect = gfx::RectF(1., 2., 3., 4.);
  input_osc.crop_rect = gfx::RectF(10., 20., 30., 40.);
  input_osc.clip_rect = gfx::Rect(10, 20, 30, 40);
  input_osc.is_clipped = true;
  input_osc.plane_z_order = 42;
  input_osc.overlay_handled = true;

  input.push_back(input_osc);

  // Roundtrip the conversions.
  auto output = CreateOverlaySurfaceCandidateListFrom(
      CreateParamsFromOverlaySurfaceCandidate(input));

  EXPECT_EQ(input.size(), output.size());
  OverlaySurfaceCandidate output_osc = output[0];

  EXPECT_EQ(input_osc.transform, output_osc.transform);
  EXPECT_EQ(input_osc.format, output_osc.format);
  EXPECT_EQ(input_osc.buffer_size, output_osc.buffer_size);
  EXPECT_EQ(input_osc.display_rect, output_osc.display_rect);
  EXPECT_EQ(input_osc.crop_rect, output_osc.crop_rect);
  EXPECT_EQ(input_osc.plane_z_order, output_osc.plane_z_order);
  EXPECT_EQ(input_osc.overlay_handled, output_osc.overlay_handled);

  EXPECT_FALSE(input < output);
  EXPECT_FALSE(output < input);

  std::map<OverlaySurfaceCandidateList, int> map;
  map[input] = 42;
  const auto& iter = map.find(output);

  EXPECT_NE(map.end(), iter);
  EXPECT_EQ(42, iter->second);
}

TEST_F(DrmUtilTest, TestDisplayModesExtraction) {
  // Initialize a list of display modes.
  constexpr size_t kNumModes = 5;
  drmModeModeInfo modes[kNumModes] = {
      {0,
       640 /* hdisplay */,
       0,
       0,
       0,
       0,
       400 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
      {0,
       640 /* hdisplay */,
       0,
       0,
       0,
       0,
       480 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
      {0,
       800 /* hdisplay */,
       0,
       0,
       0,
       0,
       600 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
      {0,
       1024 /* hdisplay */,
       0,
       0,
       0,
       0,
       768 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
      {0,
       1280 /* hdisplay */,
       0,
       0,
       0,
       0,
       768 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
  };
  drmModeModeInfoPtr modes_ptr = static_cast<drmModeModeInfoPtr>(
      drmMalloc(kNumModes * sizeof(drmModeModeInfo)));
  std::memcpy(modes_ptr, &modes[0], kNumModes * sizeof(drmModeModeInfo));

  // Initialize a connector.
  drmModeConnector connector = {
      0,
      0,
      0,
      0,
      DRM_MODE_CONNECTED,
      0,
      0,
      DRM_MODE_SUBPIXEL_UNKNOWN,
      5 /* count_modes */,
      modes_ptr,
      0,
      nullptr,
      nullptr,
      0,
      nullptr,
  };
  drmModeConnector* connector_ptr =
      static_cast<drmModeConnector*>(drmMalloc(sizeof(drmModeConnector)));
  *connector_ptr = connector;

  // Initialize a CRTC.
  drmModeCrtc crtc = {
      0, 0, 0, 0, 0, 0, 1 /* mode_valid */, modes[0], 0,
  };
  drmModeCrtcPtr crtc_ptr =
      static_cast<drmModeCrtcPtr>(drmMalloc(sizeof(drmModeCrtc)));
  *crtc_ptr = crtc;

  HardwareDisplayControllerInfo info(ScopedDrmConnectorPtr(connector_ptr),
                                     ScopedDrmCrtcPtr(crtc_ptr), 0);

  const display::DisplayMode* current_mode;
  const display::DisplayMode* native_mode;
  auto extracted_modes =
      ExtractDisplayModes(&info, gfx::Size(), &current_mode, &native_mode);

  // With no preferred mode and no active pixel size, the native mode will be
  // selected as the first mode.
  ASSERT_EQ(5u, extracted_modes.size());
  EXPECT_EQ(extracted_modes[0].get(), current_mode);
  EXPECT_EQ(extracted_modes[0].get(), native_mode);
  EXPECT_EQ(gfx::Size(640, 400), native_mode->size());

  // With no preferred mode, but with an active pixel size, the native mode will
  // be the mode that has the same size as the active pixel size.
  const gfx::Size active_pixel_size(1280, 768);
  extracted_modes = ExtractDisplayModes(&info, active_pixel_size, &current_mode,
                                        &native_mode);
  ASSERT_EQ(5u, extracted_modes.size());
  EXPECT_EQ(extracted_modes[0].get(), current_mode);
  EXPECT_EQ(extracted_modes[4].get(), native_mode);
  EXPECT_EQ(active_pixel_size, native_mode->size());

  // The preferred mode is always returned as the native mode, even when a valid
  // active pixel size supplied.
  modes_ptr[2].type |= DRM_MODE_TYPE_PREFERRED;
  extracted_modes = ExtractDisplayModes(&info, active_pixel_size, &current_mode,
                                        &native_mode);
  ASSERT_EQ(5u, extracted_modes.size());
  EXPECT_EQ(extracted_modes[0].get(), current_mode);
  EXPECT_EQ(extracted_modes[2].get(), native_mode);
  EXPECT_EQ(gfx::Size(800, 600), native_mode->size());

  // While KMS specification says there should be at most one preferred mode per
  // connector, we found monitors with more than one preferred mode. With this
  // test we make sure the first one is the one used for native_mode.
  modes_ptr[1].type |= DRM_MODE_TYPE_PREFERRED;
  extracted_modes = ExtractDisplayModes(&info, active_pixel_size, &current_mode,
                                        &native_mode);
  ASSERT_EQ(5u, extracted_modes.size());
  EXPECT_EQ(extracted_modes[1].get(), native_mode);
}

}  // namespace ui
