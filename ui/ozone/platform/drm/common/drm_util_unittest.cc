// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <map>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

namespace ui {

class DrmUtilTest : public testing::Test {};

TEST_F(DrmUtilTest, TestDisplayModesExtraction) {
  // Initialize a list of display modes.
  constexpr size_t kNumModes = 5;
  drmModeModeInfo modes[kNumModes] = {{.hdisplay = 640, .vdisplay = 400},
                                      {.hdisplay = 640, .vdisplay = 480},
                                      {.hdisplay = 800, .vdisplay = 600},
                                      {.hdisplay = 1024, .vdisplay = 768},
                                      {.hdisplay = 1280, .vdisplay = 768}};
  drmModeModeInfoPtr modes_ptr = static_cast<drmModeModeInfoPtr>(
      drmMalloc(kNumModes * sizeof(drmModeModeInfo)));
  std::memcpy(modes_ptr, &modes[0], kNumModes * sizeof(drmModeModeInfo));

  // Initialize a connector.
  drmModeConnector connector = {.connection = DRM_MODE_CONNECTED,
                                .subpixel = DRM_MODE_SUBPIXEL_UNKNOWN,
                                .count_modes = 5,
                                .modes = modes_ptr};
  drmModeConnector* connector_ptr =
      static_cast<drmModeConnector*>(drmMalloc(sizeof(drmModeConnector)));
  *connector_ptr = connector;

  // Initialize a CRTC.
  drmModeCrtc crtc = {.mode_valid = 1, .mode = modes[0]};
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

TEST(PathBlobParser, InvalidBlob) {
  char data[] = "this doesn't matter";
  drmModePropertyBlobRes blob{1, 0, data};
  EXPECT_TRUE(ParsePathBlob(blob).empty());
}

TEST(PathBlobParser, EmptyOrNullString) {
  {
    char empty[] = "";
    drmModePropertyBlobRes blob{1, sizeof(empty), empty};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char null[] = "\0";
    drmModePropertyBlobRes blob{1, sizeof(null), null};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }
}

TEST(PathBlobParser, InvalidPathFormat) {
  // Space(s)
  {
    char s[] = " ";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "           ";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  // Missing colon
  {
    char s[] = "mst6-2-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  // Caps 'mst:'
  {
    char s[] = "MST:6-2-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  // Subset of "mst:"
  {
    char s[] = "ms";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  // No 'mst:'
  {
    char s[] = "6-2-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  // Other colon-delimited prefix
  {
    char s[] = "path:6-2-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  // Invalid port number or format
  {
    char s[] = "mst:";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:6";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst::6-2-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:-6-2-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:6-2-1-";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:c7";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:6-b-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:6--2";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:---";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:6- -2- -1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:6 -2-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  {
    char s[] = "mst:6-'2'-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }

  // Null character
  {
    char s[] = "mst:6-\0-2-1";
    drmModePropertyBlobRes blob{1, sizeof(s), s};
    EXPECT_TRUE(ParsePathBlob(blob).empty());
  }
}

TEST(PathBlobParser, ValidPathFormat) {
  std::vector<uint64_t> expected = {6u, 2u, 1u};

  {
    char valid[] = "mst:6-2-1";
    drmModePropertyBlobRes blob{1, sizeof(valid), valid};
    EXPECT_EQ(expected, ParsePathBlob(blob));
  }

  {
    char valid[] = "mst:6-2-1\0";
    drmModePropertyBlobRes blob{1, sizeof(valid), valid};
    EXPECT_EQ(expected, ParsePathBlob(blob));
  }

  {
    char valid[] = {'m', 's', 't', ':', '6', '-', '2', '-', '1'};
    drmModePropertyBlobRes blob{1, sizeof(valid), valid};
    EXPECT_EQ(expected, ParsePathBlob(blob));
  }
}

}  // namespace ui
