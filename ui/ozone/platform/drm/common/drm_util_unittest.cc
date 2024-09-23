// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <map>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/common/hardware_display_controller_info.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/common/tile_property.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device_generator.h"

namespace ui {

namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

ScopedDrmPropertyBlob CreateTilePropertyBlob(FakeDrmDevice& drm,
                                             const TileProperty& property) {
  // "group_id:tile_is_single_monitor:num_h_tile:num_v_tile:tile_h_loc
  // :tile_v_loc:tile_h_size:tile_v_size"
  std::string tile_property_str = base::StringPrintf(
      "%d:1:%d:%d:%d:%d:%d:%d\0", property.group_id,
      property.tile_layout.width(), property.tile_layout.height(),
      property.location.x(), property.location.y(), property.tile_size.width(),
      property.tile_size.height());

  return drm.CreatePropertyBlob(tile_property_str.data(),
                                tile_property_str.size());
}

testing::Matcher<HardwareDisplayControllerInfo> InfoEqCrtcConnectorIds(
    uint32_t connector_id,
    uint32_t crtc_id) {
  return AllOf(Property(&HardwareDisplayControllerInfo::connector,
                        Pointee(Field(&drmModeConnector::connector_id,
                                      Eq(connector_id)))),
               Property(&HardwareDisplayControllerInfo::crtc,
                        Pointee(Field(&drmModeCrtc::crtc_id, Eq(crtc_id)))));
}

testing::Matcher<HardwareDisplayControllerInfo> InfoHasTilePropertyWithLocation(
    const gfx::Point location) {
  return Property(&HardwareDisplayControllerInfo::tile_property,
                  Optional(Field(&TileProperty::location, Eq(location))));
}

}  // namespace

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
                                     ScopedDrmCrtcPtr(crtc_ptr), 0,
                                     /*edid_parser=*/std::nullopt);

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

TEST(ParseTileBlobTest, TileAtOrigin) {
  char tile_prop_cstr[] = "1:1:2:1:0:0:2560:2880\0";
  drmModePropertyBlobRes blob = {
      .id = 100, .length = sizeof(tile_prop_cstr), .data = tile_prop_cstr};
  std::optional<TileProperty> tile_prop = ParseTileBlob(blob);
  ASSERT_TRUE(tile_prop.has_value());

  EXPECT_EQ(tile_prop->group_id, 1);
  EXPECT_EQ(tile_prop->tile_size, gfx::Size(2560, 2880));
  EXPECT_EQ(tile_prop->tile_layout, gfx::Size(2, 1));
  EXPECT_EQ(tile_prop->location, gfx::Point(0, 0));
}

TEST(ParseTileBlobTest, TileAtOriginTralingSpace) {
  char tile_prop_cstr[] = "1:1:2:1:0:0:2560:2880 \0";
  drmModePropertyBlobRes blob = {
      .id = 100, .length = sizeof(tile_prop_cstr), .data = tile_prop_cstr};
  std::optional<TileProperty> tile_prop = ParseTileBlob(blob);
  ASSERT_TRUE(tile_prop.has_value());

  EXPECT_EQ(tile_prop->group_id, 1);
  EXPECT_EQ(tile_prop->tile_size, gfx::Size(2560, 2880));
  EXPECT_EQ(tile_prop->tile_layout, gfx::Size(2, 1));
  EXPECT_EQ(tile_prop->location, gfx::Point(0, 0));
}

TEST(ParseTileBlobTest, EmptyBlob) {
  char tile_prop_cstr[] = "";
  drmModePropertyBlobRes blob = {
      .id = 100, .length = sizeof(tile_prop_cstr), .data = tile_prop_cstr};
  std::optional<TileProperty> tile_prop = ParseTileBlob(blob);
  EXPECT_FALSE(tile_prop.has_value());
}

TEST(ParseTileBlobTest, NullBlob) {
  char tile_prop_cstr[] = "\0";
  drmModePropertyBlobRes blob = {
      .id = 100, .length = sizeof(tile_prop_cstr), .data = tile_prop_cstr};
  std::optional<TileProperty> tile_prop = ParseTileBlob(blob);
  EXPECT_FALSE(tile_prop.has_value());
}

TEST(ParseTileBlobTest, SpaceBlob) {
  char tile_prop_cstr[] = "          ";
  drmModePropertyBlobRes blob = {
      .id = 100, .length = sizeof(tile_prop_cstr), .data = tile_prop_cstr};
  std::optional<TileProperty> tile_prop = ParseTileBlob(blob);
  EXPECT_FALSE(tile_prop.has_value());
}

TEST(ParseTileBlobTest, MalformedTiledBlob) {
  // This is not of proper format.
  char tile_prop_cstr[] = "1:2:3\0";
  drmModePropertyBlobRes blob = {
      .id = 100, .length = sizeof(tile_prop_cstr), .data = tile_prop_cstr};
  std::optional<TileProperty> tile_prop = ParseTileBlob(blob);
  EXPECT_FALSE(tile_prop.has_value());
}

TEST(ParseTileBlobTest, TileBlobNotParsable) {
  // This is not of proper format.
  char tile_prop_cstr[] = "1:1:2:1:0:0:orange:2880\0";
  drmModePropertyBlobRes blob = {
      .id = 100, .length = sizeof(tile_prop_cstr), .data = tile_prop_cstr};
  std::optional<TileProperty> tile_prop = ParseTileBlob(blob);
  EXPECT_FALSE(tile_prop.has_value());
}

TEST(CreateDisplaySnapshotTest, TiledDisplay) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  TileProperty primary_tile_property{.group_id = 1,
                                     .scale_to_fit_display = true,
                                     .tile_size = gfx::Size(3840, 4320),
                                     .tile_layout = gfx::Size(2, 1),
                                     .location = gfx::Point(0, 0)};
  ScopedDrmPropertyBlob tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, primary_tile_property);
  uint32_t primary_crtc_id = 0, primary_connector_id = 0;
  {
    auto& crtc = fake_drm->AddCrtc();
    primary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 1;

    auto& connector = fake_drm->AddConnector();
    primary_connector_id = connector.id;

    connector.connection = true;
    connector.modes =
        std::vector<ResolutionAndRefreshRate>{{gfx::Size(1920, 1080), 60},
                                              {gfx::Size(3840, 4320), 60},
                                              {gfx::Size(3840, 4320), 48}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    fake_drm->AddProperty(
        primary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }

  TileProperty nonprimary_tile_property = primary_tile_property;
  nonprimary_tile_property.location = gfx::Point(1, 0);
  ScopedDrmPropertyBlob nonprimary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, nonprimary_tile_property);
  uint32_t nonprimary_crtc_id = 0, nonprimary_connector_id = 0;
  {
    auto& nonprimary_crtc = fake_drm->AddCrtc();
    nonprimary_crtc_id = nonprimary_crtc.id;

    auto& nonprimary_encoder = fake_drm->AddEncoder();
    nonprimary_encoder.possible_crtcs = 0b10;

    auto& nonprimary_connector = fake_drm->AddConnector();
    nonprimary_connector_id = nonprimary_connector.id;
    nonprimary_connector.connection = true;
    nonprimary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    nonprimary_connector.encoders =
        std::vector<uint32_t>{nonprimary_encoder.id};
    fake_drm->AddProperty(
        nonprimary_connector_id,
        {.id = kTileBlobPropId, .value = nonprimary_tile_property_blob->id()});
  }

  fake_drm->InitializeState(/*use_atomic=*/true);

  HardwareDisplayControllerInfo info(
      fake_drm->GetConnector(primary_connector_id),
      fake_drm->GetCrtc(primary_crtc_id),
      /*index=*/0, std::nullopt, primary_tile_property);

  {
    std::unique_ptr<HardwareDisplayControllerInfo> nonprimary_info =
        std::make_unique<HardwareDisplayControllerInfo>(
            fake_drm->GetConnector(nonprimary_connector_id),
            fake_drm->GetCrtc(nonprimary_crtc_id),
            /*index=*/1, std::nullopt, nonprimary_tile_property);
    info.AcquireNonprimaryTileInfo(std::move(nonprimary_info));
  }
  std::unique_ptr<display::DisplaySnapshot> tile_snapshot =
      CreateDisplaySnapshot(*fake_drm, &info, /*device_index=*/0);

  ASSERT_NE(tile_snapshot, nullptr);
  EXPECT_THAT(
      tile_snapshot->modes(),
      UnorderedElementsAre(
          Pointee(AllOf(
              // The mode is transformed to tile-composited mode.
              Property(&display::DisplayMode::size, Eq(gfx::Size(7680, 4320))),
              Property(&display::DisplayMode::refresh_rate, Eq(60)))),
          Pointee(AllOf(
              Property(&display::DisplayMode::size, Eq(gfx::Size(1920, 1080))),
              Property(&display::DisplayMode::refresh_rate, Eq(60))))));
}

TEST(ConsolidateTiledDisplayInfoTest, OnlyNontiled) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  TileProperty tile_property{.group_id = 1,
                             .tile_size = gfx::Size(3840, 4320),
                             .tile_layout = gfx::Size(2, 1),
                             .location = gfx::Point(0, 0)};
  ScopedDrmPropertyBlob tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, tile_property);
  uint32_t crtc_1 = 0, connector_1 = 0;
  {
    crtc_1 = fake_drm->AddCrtc().id;
    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm->AddConnector();
    connector_1 = connector.id;
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  uint32_t crtc_2 = 0, connector_2 = 0;
  {
    crtc_2 = fake_drm->AddCrtc().id;
    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x10;

    auto& connector = fake_drm->AddConnector();
    connector_2 = connector.id;
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  fake_drm->InitializeState(/*use_atomic=*/true);

  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> infos;
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(connector_1), fake_drm->GetCrtc(crtc_1),
      /*index=*/0, std::nullopt));
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(connector_2), fake_drm->GetCrtc(crtc_2),
      /*index=*/1, std::nullopt));

  ConsolidateTiledDisplayInfo(infos);

  ASSERT_THAT(infos, UnorderedElementsAre(
                         Pointee(InfoEqCrtcConnectorIds(connector_1, crtc_1)),
                         Pointee(InfoEqCrtcConnectorIds(connector_2, crtc_2))));
}

TEST(ConsolidateTiledDisplayInfoTest, SingleTiled) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  TileProperty tile_property{.group_id = 1,
                             .tile_size = gfx::Size(3840, 4320),
                             .tile_layout = gfx::Size(2, 1),
                             .location = gfx::Point(0, 0)};
  ScopedDrmPropertyBlob tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, tile_property);
  uint32_t primary_crtc_id = 0, primary_connector_id = 0;
  {
    auto& crtc = fake_drm->AddCrtc();
    primary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 1;

    auto& connector = fake_drm->AddConnector();
    primary_connector_id = connector.id;

    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    fake_drm->AddProperty(
        primary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }

  fake_drm->InitializeState(/*use_atomic=*/true);

  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> infos;
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(primary_connector_id),
      fake_drm->GetCrtc(primary_crtc_id),
      /*index=*/0, std::nullopt, tile_property));

  ConsolidateTiledDisplayInfo(infos);

  ASSERT_THAT(infos, UnorderedElementsAre(Pointee(InfoEqCrtcConnectorIds(
                         primary_connector_id, primary_crtc_id))));
  EXPECT_TRUE(infos[0]->tile_property().has_value());
  EXPECT_THAT(infos[0]->nonprimary_tile_infos(), IsEmpty());
}

TEST(ConsolidateTiledDisplayInfoTest, AllTilesPresent) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  TileProperty primary_tile_property{.group_id = 1,
                                     .tile_size = gfx::Size(3840, 4320),
                                     .tile_layout = gfx::Size(2, 1),
                                     .location = gfx::Point(0, 0)};
  uint32_t primary_crtc_id = 0, primary_connector_id = 0;
  {
    auto& crtc = fake_drm->AddCrtc();
    primary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm->AddConnector();
    primary_connector_id = connector.id;

    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, primary_tile_property);
    fake_drm->AddProperty(
        primary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }
  uint32_t nonprimary_crtc_id = 0, nonprimary_connector_id = 0;
  TileProperty nonprimary_tile_property = primary_tile_property;
  nonprimary_tile_property.location = gfx::Point(1, 0);
  {
    auto& crtc = fake_drm->AddCrtc();
    nonprimary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x10;

    auto& connector = fake_drm->AddConnector();
    nonprimary_connector_id = connector.id;

    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};

    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, nonprimary_tile_property);
    fake_drm->AddProperty(
        nonprimary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }

  fake_drm->InitializeState(/*use_atomic=*/true);

  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> infos;
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(primary_connector_id),
      fake_drm->GetCrtc(primary_crtc_id),
      /*index=*/0, std::nullopt, primary_tile_property));
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(nonprimary_connector_id),
      fake_drm->GetCrtc(nonprimary_crtc_id),
      /*index=*/1, std::nullopt, nonprimary_tile_property));

  ConsolidateTiledDisplayInfo(infos);

  // Since the tile properties of both tiles are identical except for location,
  // expect location to be the tie-breaker for choosing the primary tile.
  ASSERT_THAT(infos, UnorderedElementsAre(Pointee(InfoEqCrtcConnectorIds(
                         primary_connector_id, primary_crtc_id))));
  EXPECT_EQ(infos[0]->tile_property()->location, gfx::Point(0, 0));
  EXPECT_THAT(infos[0]->nonprimary_tile_infos(),
              UnorderedElementsAre(
                  Pointee(InfoHasTilePropertyWithLocation(gfx::Point(1, 0)))));
}

TEST(ConsolidateTiledDisplayInfoTest, AllTilesPresentMultipleGroups) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  TileProperty group1_primary_tile_property{.group_id = 1,
                                            .tile_size = gfx::Size(3840, 4320),
                                            .tile_layout = gfx::Size(2, 1),
                                            .location = gfx::Point(0, 0)};
  uint32_t group1_primary_crtc_id = 0, group1_primary_connector_id = 0;
  {
    auto& crtc = fake_drm->AddCrtc();
    group1_primary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x0001;

    auto& connector = fake_drm->AddConnector();
    group1_primary_connector_id = connector.id;

    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, group1_primary_tile_property);
    fake_drm->AddProperty(
        group1_primary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }

  uint32_t group1_nonprimary_crtc_id = 0, group1_nonprimary_connector_id = 0;
  TileProperty group1_nonprimary_tile_property = group1_primary_tile_property;
  group1_nonprimary_tile_property.location = gfx::Point(1, 0);
  {
    auto& crtc = fake_drm->AddCrtc();
    group1_nonprimary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x0100;

    auto& connector = fake_drm->AddConnector();
    group1_nonprimary_connector_id = connector.id;

    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};

    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, group1_nonprimary_tile_property);
    fake_drm->AddProperty(
        group1_nonprimary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }

  TileProperty group2_primary_tile_property{.group_id = 2,
                                            .tile_size = gfx::Size(3840, 4320),
                                            .tile_layout = gfx::Size(2, 1),
                                            .location = gfx::Point(0, 0)};
  uint32_t group2_primary_crtc_id = 0, group2_primary_connector_id = 0;
  {
    auto& crtc = fake_drm->AddCrtc();
    group2_primary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x0100;

    auto& connector = fake_drm->AddConnector();
    group2_primary_connector_id = connector.id;

    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, group2_primary_tile_property);
    fake_drm->AddProperty(
        group2_primary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }
  uint32_t group2_nonprimary_crtc_id = 0, group2_nonprimary_connector_id = 0;
  TileProperty group2_nonprimary_tile_property = group2_primary_tile_property;
  group2_nonprimary_tile_property.location = gfx::Point(1, 0);
  {
    auto& crtc = fake_drm->AddCrtc();
    group2_nonprimary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x1000;

    auto& connector = fake_drm->AddConnector();
    group2_nonprimary_connector_id = connector.id;
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};

    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, group2_nonprimary_tile_property);
    fake_drm->AddProperty(
        group2_nonprimary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }

  fake_drm->InitializeState(/*use_atomic=*/true);

  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> infos;
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(group1_primary_connector_id),
      fake_drm->GetCrtc(group1_primary_crtc_id),
      /*index=*/0, std::nullopt, group1_primary_tile_property));
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(group1_nonprimary_connector_id),
      fake_drm->GetCrtc(group1_nonprimary_crtc_id),
      /*index=*/1, std::nullopt, group1_nonprimary_tile_property));
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(group2_primary_connector_id),
      fake_drm->GetCrtc(group2_primary_crtc_id),
      /*index=*/2, std::nullopt, group2_primary_tile_property));
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(group2_nonprimary_connector_id),
      fake_drm->GetCrtc(group2_nonprimary_crtc_id),
      /*index=*/3, std::nullopt, group2_nonprimary_tile_property));

  ConsolidateTiledDisplayInfo(infos);

  ASSERT_THAT(infos,
              UnorderedElementsAre(
                  Pointee(InfoEqCrtcConnectorIds(group1_primary_connector_id,
                                                 group1_primary_crtc_id)),
                  Pointee(InfoEqCrtcConnectorIds(group2_primary_connector_id,
                                                 group2_primary_crtc_id))));
  EXPECT_EQ(infos[0]->tile_property()->location, gfx::Point(0, 0));
  EXPECT_EQ(infos[1]->tile_property()->location, gfx::Point(0, 0));
}

TEST(ConsolidateTiledDisplayInfoTest, PreferMoreModes) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  TileProperty primary_tile_property{.group_id = 1,
                                     .tile_size = gfx::Size(3840, 4320),
                                     .tile_layout = gfx::Size(2, 1),
                                     .location = gfx::Point(1, 0)};
  uint32_t primary_crtc_id = 0, primary_connector_id = 0;
  {
    auto& crtc = fake_drm->AddCrtc();
    primary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm->AddConnector();
    primary_connector_id = connector.id;

    connector.connection = true;
    connector.modes =
        std::vector<ResolutionAndRefreshRate>{{gfx::Size(1920, 1080), 30},
                                              {gfx::Size(1920, 1080), 60},
                                              {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, primary_tile_property);
    fake_drm->AddProperty(
        primary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }
  uint32_t nonprimary_crtc_id = 0, nonprimary_connector_id = 0;
  TileProperty nonprimary_tile_property = primary_tile_property;
  nonprimary_tile_property.location = gfx::Point(0, 0);
  {
    auto& crtc = fake_drm->AddCrtc();
    nonprimary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x10;

    auto& connector = fake_drm->AddConnector();
    nonprimary_connector_id = connector.id;

    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};

    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, nonprimary_tile_property);
    fake_drm->AddProperty(
        nonprimary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }

  fake_drm->InitializeState(/*use_atomic=*/true);

  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> infos;
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(primary_connector_id),
      fake_drm->GetCrtc(primary_crtc_id),
      /*index=*/0, std::nullopt, primary_tile_property));
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(nonprimary_connector_id),
      fake_drm->GetCrtc(nonprimary_crtc_id),
      /*index=*/0, std::nullopt, nonprimary_tile_property));

  ConsolidateTiledDisplayInfo(infos);

  ASSERT_THAT(infos, UnorderedElementsAre(Pointee(InfoEqCrtcConnectorIds(
                         primary_connector_id, primary_crtc_id))));
  EXPECT_EQ(infos[0]->tile_property()->location, gfx::Point(1, 0));
  EXPECT_THAT(infos[0]->nonprimary_tile_infos(),
              UnorderedElementsAre(
                  Pointee(InfoHasTilePropertyWithLocation(gfx::Point(0, 0)))));
}

TEST(ConsolidateTiledDisplayInfoTest, PreferScaleToFit) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  fake_drm->ResetStateWithAllProperties();
  TileProperty primary_tile_property{.group_id = 1,
                                     .scale_to_fit_display = true,
                                     .tile_size = gfx::Size(3840, 4320),
                                     .tile_layout = gfx::Size(2, 1),
                                     .location = gfx::Point(1, 0)};
  uint32_t primary_crtc_id = 0, primary_connector_id = 0;
  {
    auto& crtc = fake_drm->AddCrtc();
    primary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm->AddConnector();
    primary_connector_id = connector.id;

    connector.connection = true;
    connector.modes =
        std::vector<ResolutionAndRefreshRate>{{gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, primary_tile_property);
    fake_drm->AddProperty(
        primary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }
  uint32_t nonprimary_crtc_id = 0, nonprimary_connector_id = 0;
  TileProperty nonprimary_tile_property = primary_tile_property;
  nonprimary_tile_property.scale_to_fit_display = false;
  nonprimary_tile_property.location = gfx::Point(0, 0);
  {
    auto& crtc = fake_drm->AddCrtc();
    nonprimary_crtc_id = crtc.id;

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0x10;

    auto& connector = fake_drm->AddConnector();
    nonprimary_connector_id = connector.id;

    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};

    ScopedDrmPropertyBlob tile_property_blob =
        CreateTilePropertyBlob(*fake_drm, nonprimary_tile_property);
    fake_drm->AddProperty(
        nonprimary_connector_id,
        {.id = kTileBlobPropId, .value = tile_property_blob->id()});
  }

  fake_drm->InitializeState(/*use_atomic=*/true);

  std::vector<std::unique_ptr<HardwareDisplayControllerInfo>> infos;
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(primary_connector_id),
      fake_drm->GetCrtc(primary_crtc_id),
      /*index=*/0, std::nullopt, primary_tile_property));
  infos.push_back(std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm->GetConnector(nonprimary_connector_id),
      fake_drm->GetCrtc(nonprimary_crtc_id),
      /*index=*/1, std::nullopt, nonprimary_tile_property));

  ConsolidateTiledDisplayInfo(infos);

  ASSERT_THAT(infos, UnorderedElementsAre(Pointee(InfoEqCrtcConnectorIds(
                         primary_connector_id, primary_crtc_id))));
  EXPECT_EQ(infos[0]->tile_property()->location, gfx::Point(1, 0));
  EXPECT_TRUE(infos[0]->tile_property()->scale_to_fit_display);
}

TEST(CreateDisplaySnapshotTest, TiledDisplayWithoutOtherTilesConnected) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());

  TileProperty tile_property{.group_id = 1,
                             .scale_to_fit_display = true,
                             .tile_size = gfx::Size(3840, 4320),
                             .tile_layout = gfx::Size(2, 1),
                             .location = gfx::Point(0, 0)};
  ScopedDrmPropertyBlob tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, tile_property);

  auto& crtc = fake_drm->AddCrtc();
  auto& encoder = fake_drm->AddEncoder();
  encoder.possible_crtcs = 1;

  auto& connector = fake_drm->AddConnector();
  connector.connection = true;
  connector.modes = std::vector<ResolutionAndRefreshRate>{
      {gfx::Size(1920, 1080), 60}, {gfx::Size(3840, 4320), 60}};
  connector.encoders = std::vector<uint32_t>{encoder.id};
  fake_drm->AddProperty(
      connector.id, {.id = kTileBlobPropId, .value = tile_property_blob->id()});

  fake_drm->InitializeState(/*use_atomic=*/true);

  HardwareDisplayControllerInfo info(fake_drm->GetConnector(connector.id),
                                     fake_drm->GetCrtc(crtc.id),
                                     /*index=*/0, std::nullopt, tile_property);
  std::unique_ptr<display::DisplaySnapshot> tile_snapshot =
      CreateDisplaySnapshot(*fake_drm, &info, /*device_index=*/0);

  ASSERT_NE(tile_snapshot, nullptr);
  EXPECT_THAT(
      tile_snapshot->modes(),
      UnorderedElementsAre(
          // 8k mode does not exist.
          Pointee(AllOf(
              Property(&display::DisplayMode::size, Eq(gfx::Size(1920, 1080))),
              Property(&display::DisplayMode::refresh_rate, Eq(60))))));
}

TEST(GetTotalTileDisplaySizeTest, Empty) {
  EXPECT_EQ(GetTotalTileDisplaySize(TileProperty()), gfx::Size(0, 0));
}

TEST(GetTotalTileDisplaySizeTest, Tile) {
  TileProperty tile_property{.group_id = 1,
                             .scale_to_fit_display = true,
                             .tile_size = gfx::Size(200, 300),
                             .tile_layout = gfx::Size(2, 4),
                             .location = gfx::Point(0, 0)};

  EXPECT_EQ(GetTotalTileDisplaySize(tile_property), gfx::Size(400, 1200));
}

TEST(IsTileModeTest, TileMode) {
  TileProperty property = {.group_id = 1,
                           .scale_to_fit_display = true,
                           .tile_size = gfx::Size(1000, 2000),
                           .tile_layout = gfx::Size(2, 3),
                           .location = gfx::Point(1, 1)};
  EXPECT_TRUE(IsTileMode(gfx::Size(1000, 2000), property));
}

TEST(IsTileModeTest, TileCompositedModeIsNotTile) {
  TileProperty property = {.group_id = 1,
                           .scale_to_fit_display = true,
                           .tile_size = gfx::Size(1000, 2000),
                           .tile_layout = gfx::Size(2, 3),
                           .location = gfx::Point(1, 1)};
  EXPECT_FALSE(IsTileMode(gfx::Size(1000 * 2, 2000 * 3), property));
}

TEST(IsTileModeTest, NotTileMode) {
  TileProperty property = {.group_id = 1,
                           .scale_to_fit_display = true,
                           .tile_size = gfx::Size(1000, 2000),
                           .tile_layout = gfx::Size(2, 3),
                           .location = gfx::Point(1, 1)};
  EXPECT_FALSE(IsTileMode(gfx::Size(1920, 1080), property));
}

TEST(TileCrtcOffset, Origin) {
  TileProperty property = {.tile_size = gfx::Size(1000, 2000),
                           .location = gfx::Point(0, 0)};
  EXPECT_EQ(GetTileCrtcOffset(property), gfx::Point(0, 0));
}

TEST(TileCrtcOffset, NotOrigin) {
  TileProperty property = {.tile_size = gfx::Size(1000, 2000),
                           .location = gfx::Point(1, 2)};
  EXPECT_EQ(GetTileCrtcOffset(property), gfx::Point(1000, 4000));
}
}  // namespace ui
