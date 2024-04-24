// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <map>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device_generator.h"

namespace ui {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

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

TEST(GetPossibleCrtcsBitmaskFromEncoders, MultipleCrtcsMultipleEncoders) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  uint32_t first_encoder_id, second_encoder_id;
  {
    // Add 2 CRTCs
    fake_drm->AddCrtc();
    fake_drm->AddCrtc();

    // Add 2 encoders
    FakeDrmDevice::EncoderProperties& first_encoder = fake_drm->AddEncoder();
    first_encoder.possible_crtcs = 0b10;
    first_encoder_id = first_encoder.id;

    FakeDrmDevice::EncoderProperties& second_encoder = fake_drm->AddEncoder();
    second_encoder.possible_crtcs = 0b01;
    second_encoder_id = second_encoder.id;

    // Add 1 connector
    FakeDrmDevice::ConnectorProperties& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.encoders =
        std::vector<uint32_t>{first_encoder_id, second_encoder_id};
  }

  fake_drm->InitializeState(/*use_atomic*/ true);

  EXPECT_EQ(GetPossibleCrtcsBitmaskFromEncoders(
                *fake_drm, {first_encoder_id, second_encoder_id}),
            0b11u);
}

TEST(GetPossibleCrtcsBitmaskFromEncoders, EmptyEncodersList) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());
  fake_drm->ResetStateWithAllProperties();
  {
    fake_drm->AddCrtc();
    FakeDrmDevice::EncoderProperties& first_encoder = fake_drm->AddEncoder();
    first_encoder.possible_crtcs = 0b01;
    uint32_t encoder_id = first_encoder.id;
    FakeDrmDevice::ConnectorProperties& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder_id};
  }

  fake_drm->InitializeState(/*use_atomic*/ true);

  EXPECT_EQ(GetPossibleCrtcsBitmaskFromEncoders(*fake_drm, /*encoder_ids=*/{}),
            0u);
}

TEST(GetPossibleCrtcsBitmaskFromEncoders, InvalidEncoderID) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());
  fake_drm->ResetStateWithAllProperties();
  {
    fake_drm->AddCrtc();
    FakeDrmDevice::EncoderProperties& first_encoder = fake_drm->AddEncoder();
    first_encoder.possible_crtcs = 0b01;
    uint32_t encoder_id = first_encoder.id;
    FakeDrmDevice::ConnectorProperties& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder_id};
  }

  fake_drm->InitializeState(/*use_atomic*/ true);

  EXPECT_THAT(GetPossibleCrtcsBitmaskFromEncoders(*fake_drm,
                                                  /*invalid encoder_ids=*/{1}),
              0u);
}

TEST(GetPossibleCrtcIdsFromBitmask, ValidBitmask) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  uint32_t crtc_1_id, crtc_2_id;
  {
    // Add 2 CRTCs
    crtc_1_id = fake_drm->AddCrtc().id;
    crtc_2_id = fake_drm->AddCrtc().id;

    // Add 2 encoders
    FakeDrmDevice::EncoderProperties& first_encoder = fake_drm->AddEncoder();
    first_encoder.possible_crtcs = 0b10;
    uint32_t first_encoder_id = first_encoder.id;

    FakeDrmDevice::EncoderProperties& second_encoder = fake_drm->AddEncoder();
    second_encoder.possible_crtcs = 0b01;
    uint32_t second_encoder_id = second_encoder.id;

    // Add 1 connector
    FakeDrmDevice::ConnectorProperties& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.encoders =
        std::vector<uint32_t>{first_encoder_id, second_encoder_id};
  }

  fake_drm->InitializeState(/*use_atomic*/ true);

  EXPECT_THAT(GetPossibleCrtcIdsFromBitmask(*fake_drm, 0b11),
              UnorderedElementsAre(crtc_1_id, crtc_2_id));
}

TEST(GetPossibleCrtcIdsFromBitmask, ZeroBitmask) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  {
    fake_drm->AddCrtc();

    FakeDrmDevice::EncoderProperties& first_encoder = fake_drm->AddEncoder();
    first_encoder.possible_crtcs = 0b01;
    uint32_t first_encoder_id = first_encoder.id;

    // Add 1 connector
    FakeDrmDevice::ConnectorProperties& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{first_encoder_id};
  }

  fake_drm->InitializeState(/*use_atomic*/ true);

  EXPECT_THAT(
      GetPossibleCrtcIdsFromBitmask(*fake_drm, /*possible_crtcs_bitmask=*/0),
      IsEmpty());
}

TEST(GetPossibleCrtcIdsFromBitmask, BitmaskTooLong) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  uint32_t crtc_id;
  {
    crtc_id = fake_drm->AddCrtc().id;

    FakeDrmDevice::EncoderProperties& first_encoder = fake_drm->AddEncoder();
    first_encoder.possible_crtcs = 0b01;
    uint32_t first_encoder_id = first_encoder.id;

    // Add 1 connector
    FakeDrmDevice::ConnectorProperties& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{first_encoder_id};
  }

  fake_drm->InitializeState(/*use_atomic*/ true);

  // Recall that there was only 1 CRTC, but the bitmask has 4.
  EXPECT_THAT(GetPossibleCrtcIdsFromBitmask(*fake_drm,
                                            /*possible_crtcs_bitmask=*/0b1111),
              UnorderedElementsAre(crtc_id));
}

}  // namespace ui
