// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <utility>

#include "base/files/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/tile_property.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace ui {
namespace {

constexpr gfx::Size kNativeDisplaySize(1920, 1080);

constexpr unsigned char kTiledDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x10\xac\x47\x41\x4c\x34\x37\x41"
    "\x0b\x21\x01\x04\xb5\x46\x27\x78\x3a\x76\x45\xae\x51\x33\xba\x26"
    "\x0d\x50\x54\xa5\x4b\x00\x81\x00\xb3\x00\xd1\x00\xa9\x40\x81\x80"
    "\xd1\xc0\x01\x01\x01\x01\x4d\xd0\x00\xa0\xf0\x70\x3e\x80\x30\x20"
    "\x35\x00\xba\x89\x21\x00\x00\x1a\x00\x00\x00\xff\x00\x4a\x48\x4e"
    "\x34\x4a\x33\x33\x47\x41\x37\x34\x4c\x0a\x00\x00\x00\xfc\x00\x44"
    "\x45\x4c\x4c\x20\x55\x50\x33\x32\x31\x38\x4b\x0a\x00\x00\x00\xfd"
    "\x00\x18\x4b\x1e\xb4\x6c\x01\x0a\x20\x20\x20\x20\x20\x20\x02\x79"
    "\x02\x03\x1d\xf1\x50\x10\x1f\x20\x05\x14\x04\x13\x12\x11\x03\x02"
    "\x16\x15\x07\x06\x01\x23\x09\x1f\x07\x83\x01\x00\x00\xa3\x66\x00"
    "\xa0\xf0\x70\x1f\x80\x30\x20\x35\x00\xba\x89\x21\x00\x00\x1a\x56"
    "\x5e\x00\xa0\xa0\xa0\x29\x50\x30\x20\x35\x00\xba\x89\x21\x00\x00"
    "\x1a\x7c\x39\x00\xa0\x80\x38\x1f\x40\x30\x20\x3a\x00\xba\x89\x21"
    "\x00\x00\x1a\xa8\x16\x00\xa0\x80\x38\x13\x40\x30\x20\x3a\x00\xba"
    "\x89\x21\x00\x00\x1a\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x47"
    "\x70\x12\x79\x00\x00\x12\x00\x16\x82\x10\x10\x00\xff\x0e\xdf\x10"
    "\x00\x00\x00\x00\x00\x44\x45\x4c\x47\x41\x4c\x34\x37\x41\x03\x01"
    "\x50\x70\x92\x01\x84\xff\x1d\xc7\x00\x1d\x80\x09\x00\xdf\x10\x2f"
    "\x00\x02\x00\x04\x00\xc1\x42\x01\x84\xff\x1d\xc7\x00\x2f\x80\x1f"
    "\x00\xdf\x10\x30\x00\x02\x00\x04\x00\xa8\x4e\x01\x04\xff\x0e\xc7"
    "\x00\x2f\x80\x1f\x00\xdf\x10\x61\x00\x02\x00\x09\x00\x97\x9d\x01"
    "\x04\xff\x0e\xc7\x00\x2f\x80\x1f\x00\xdf\x10\x2f\x00\x02\x00\x09"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x78\x90";
constexpr size_t kTiledDisplayLength = std::size(kTiledDisplay);

std::unique_ptr<HardwareDisplayControllerInfo> GetDisplayInfo(
    uint32_t connector_id = 123,
    uint32_t crtc_id = 456,
    uint8_t index = 0,
    const std::optional<TileProperty>& tile_property = std::nullopt) {
  // Initialize a list of display modes.
  constexpr size_t kNumModes = 5;
  drmModeModeInfo modes[kNumModes] = {
      {.hdisplay = 640, .vdisplay = 400},
      {.hdisplay = 640, .vdisplay = 480},
      {.hdisplay = 800, .vdisplay = 600},
      {.hdisplay = 1024, .vdisplay = 768},
      // Last mode, which should be the largest, is the native mode.
      {.hdisplay = kNativeDisplaySize.width(),
       .vdisplay = kNativeDisplaySize.height()}};

  // Initialize a connector.
  ScopedDrmConnectorPtr connector(DrmAllocator<drmModeConnector>());
  connector->connector_id = connector_id;
  connector->connection = DRM_MODE_CONNECTED;
  connector->count_props = 0;
  connector->count_modes = kNumModes;
  connector->modes = DrmAllocator<drmModeModeInfo>(kNumModes);
  std::memcpy(connector->modes, &modes[0], kNumModes * sizeof(drmModeModeInfo));

  // Initialize a CRTC.
  ScopedDrmCrtcPtr crtc(DrmAllocator<drmModeCrtc>());
  crtc->crtc_id = crtc_id;
  crtc->mode_valid = 1;
  crtc->mode = connector->modes[kNumModes - 1];

  return std::make_unique<HardwareDisplayControllerInfo>(
      std::move(connector), std::move(crtc), index,
      /*edid_parser=*/std::nullopt, tile_property);
}

// TODO(b/364634013): Create a test util file for ozone/drm and de-deuplicate
// EqTileProperty().
testing::Matcher<TileProperty> EqTileProperty(const TileProperty& expected) {
  return AllOf(Field(&TileProperty::group_id, Eq(expected.group_id)),
               Field(&TileProperty::scale_to_fit_display,
                     Eq(expected.scale_to_fit_display)),
               Field(&TileProperty::tile_size, Eq(expected.tile_size)),
               Field(&TileProperty::tile_layout, Eq(expected.tile_layout)),
               Field(&TileProperty::location, Eq(expected.location)));
}
}  // namespace

TEST(DrmDisplayTest, TiledDisplay) {
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator =
      std::make_unique<FakeDrmDeviceGenerator>();
  scoped_refptr<DrmDevice> device = fake_device_generator->CreateDevice(
      base::FilePath("/test/dri/card0"), base::ScopedFD(),
      /*is_primary_device=*/true);
  FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());
  fake_drm->ResetStateWithAllProperties();

  // Primary tile at (0,0)
  uint32_t primary_crtc_id, primary_connector_id;
  {
    primary_crtc_id = fake_drm->AddCrtcWithPrimaryAndCursorPlanes().id;

    auto& primary_encoder = fake_drm->AddEncoder();
    primary_encoder.possible_crtcs = 0b1;

    auto& primary_connector = fake_drm->AddConnector();
    primary_connector.connection = true;
    primary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(3840, 4320), 60}, {gfx::Size(1920, 1080), 60}};
    primary_connector.encoders = std::vector<uint32_t>{primary_encoder.id};
    primary_connector.edid_blob = std::vector<uint8_t>(
        kTiledDisplay, kTiledDisplay + kTiledDisplayLength);
    primary_connector.properties.push_back(
        {.id = kTileBlobPropId, .value = kTileBlobId});

    primary_connector_id = primary_connector.id;
  }

  // Non-primary tile at (0,1) - Identical to the primary tile except for tile
  // location.
  uint32_t nonprimary_crtc_id, nonprimary_connector_id;
  {
    nonprimary_crtc_id = fake_drm->AddCrtcWithPrimaryAndCursorPlanes().id;

    auto& nonprimary_encoder = fake_drm->AddEncoder();
    nonprimary_encoder.possible_crtcs = 0b10;

    auto& nonprimary_connector = fake_drm->AddConnector();
    nonprimary_connector.connection = true;
    nonprimary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(3840, 4320), 60}, {gfx::Size(1920, 1080), 60}};
    nonprimary_connector.encoders =
        std::vector<uint32_t>{nonprimary_encoder.id};
    nonprimary_connector.edid_blob = std::vector<uint8_t>(
        kTiledDisplay, kTiledDisplay + kTiledDisplayLength);
    nonprimary_connector.properties.push_back(
        {.id = kTileBlobPropId, .value = kTileBlobId + 1});
    nonprimary_connector_id = nonprimary_connector.id;
  }

  fake_drm->InitializeState(/*use_atomic=*/true);

  TileProperty primary_tile_prop = {.group_id = 1,
                                    .scale_to_fit_display = true,
                                    .tile_size = gfx::Size(3840, 4320),
                                    .tile_layout = gfx::Size(2, 1),
                                    .location = gfx::Point(0, 0)};
  std::unique_ptr<ui::HardwareDisplayControllerInfo> primary_info =
      GetDisplayInfo(primary_connector_id, primary_crtc_id, /*index=*/1,
                     primary_tile_prop);

  TileProperty nonprimary_tile_prop = primary_tile_prop;
  nonprimary_tile_prop.location = gfx::Point(1, 0);
  primary_info->AcquireNonprimaryTileInfo(
      GetDisplayInfo(nonprimary_connector_id, nonprimary_crtc_id, /*index=*/2,
                     nonprimary_tile_prop));

  std::unique_ptr<display::FakeDisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123456)
          .SetBaseConnectorId(primary_info->connector()->connector_id)
          .SetNativeMode(kNativeDisplaySize)
          .SetCurrentMode(kNativeDisplaySize)
          .SetColorSpace(gfx::ColorSpace::CreateSRGB())
          .Build();

  DrmDisplay drm_display(fake_drm, primary_info.get(), *snapshot);

  EXPECT_THAT(drm_display.crtc_connector_pairs(),
              UnorderedElementsAre(
                  AllOf(Field(&DrmDisplay::CrtcConnectorPair::connector,
                              Pointee(Field(&drmModeConnector::connector_id,
                                            Eq(primary_connector_id)))),
                        Field(&DrmDisplay::CrtcConnectorPair::crtc_id,
                              Eq(primary_crtc_id)),
                        Field(&DrmDisplay::CrtcConnectorPair::tile_location,
                              Eq(gfx::Point(0, 0)))),
                  AllOf(Field(&DrmDisplay::CrtcConnectorPair::connector,
                              Pointee(Field(&drmModeConnector::connector_id,
                                            Eq(nonprimary_connector_id)))),
                        Field(&DrmDisplay::CrtcConnectorPair::crtc_id,
                              Eq(nonprimary_crtc_id)),
                        Field(&DrmDisplay::CrtcConnectorPair::tile_location,
                              Eq(gfx::Point(1, 0))))));
  EXPECT_EQ(drm_display.GetPrimaryConnectorId(), primary_connector_id);
  EXPECT_EQ(drm_display.GetPrimaryCrtcId(), primary_crtc_id);

  std::optional<TileProperty> actual_tile_property =
      drm_display.GetTileProperty();
  ASSERT_TRUE(actual_tile_property.has_value());
  EXPECT_THAT(*actual_tile_property, EqTileProperty(primary_tile_prop));
}
}  // namespace ui
