// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/common/hardware_display_controller_info.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <memory>

#include "base/files/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/common/tile_property.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device_generator.h"

namespace ui {

using ::testing::IsEmpty;
using ::testing::SizeIs;

// Sample EDID data extracted from real devices.
constexpr unsigned char kNormalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";
constexpr size_t kNormalDisplayLength = std::size(kNormalDisplay);

class HardwareDisplayControllerInfoTest : public testing::Test {
 public:
  HardwareDisplayControllerInfoTest() {
    fake_device_generator_ = std::make_unique<FakeDrmDeviceGenerator>();
    device_ = fake_device_generator_->CreateDevice(
        base::FilePath("/test/dri/card0"), base::ScopedFD(),
        /*is_primary_device=*/true);
    fake_drm_ = static_cast<FakeDrmDevice*>(device_.get());

    fake_drm_->ResetStateWithAllProperties();
  }

 protected:
  std::unique_ptr<DrmDeviceGenerator> fake_device_generator_;
  scoped_refptr<DrmDevice> device_;
  raw_ptr<FakeDrmDevice> fake_drm_;
};

TEST_F(HardwareDisplayControllerInfoTest, BasicInfo) {
  uint32_t crtc_id = 0, connector_id = 0;
  {
    crtc_id = fake_drm_->AddCrtc().id;
    auto& encoder = fake_drm_->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm_->AddConnector();
    connector_id = connector.id;
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  fake_drm_->InitializeState(/*use_atomic=*/true);

  HardwareDisplayControllerInfo info(fake_drm_->GetConnector(connector_id),
                                     fake_drm_->GetCrtc(crtc_id),
                                     /*index=*/1, std::nullopt);

  ASSERT_NE(info.connector(), nullptr);
  EXPECT_EQ(info.connector()->connector_id, connector_id);

  ASSERT_NE(info.crtc(), nullptr);
  EXPECT_EQ(info.crtc()->crtc_id, crtc_id);

  EXPECT_EQ(info.index(), 1);

  EXPECT_EQ(info.edid_parser(), std::nullopt);
  EXPECT_EQ(info.tile_property(), std::nullopt);
}

TEST_F(HardwareDisplayControllerInfoTest, HasEdidParser) {
  uint32_t crtc_id = 0, connector_id = 0;
  {
    crtc_id = fake_drm_->AddCrtc().id;
    auto& encoder = fake_drm_->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm_->AddConnector();
    connector_id = connector.id;
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  fake_drm_->InitializeState(/*use_atomic=*/true);

  display::EdidParser edid_parser(std::vector<uint8_t>(
      kNormalDisplay, kNormalDisplay + kNormalDisplayLength));
  HardwareDisplayControllerInfo info(fake_drm_->GetConnector(connector_id),
                                     fake_drm_->GetCrtc(crtc_id),
                                     /*index=*/1, std::move(edid_parser));

  ASSERT_NE(info.connector(), nullptr);
  EXPECT_EQ(info.connector()->connector_id, connector_id);

  ASSERT_NE(info.crtc(), nullptr);
  EXPECT_EQ(info.crtc()->crtc_id, crtc_id);

  EXPECT_EQ(info.index(), 1);

  EXPECT_TRUE(info.edid_parser().has_value());
  EXPECT_EQ(info.tile_property(), std::nullopt);
}

TEST_F(HardwareDisplayControllerInfoTest, TileInfo) {
  uint32_t crtc_id = 0, connector_id = 0;
  {
    crtc_id = fake_drm_->AddCrtc().id;
    auto& encoder = fake_drm_->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm_->AddConnector();
    connector_id = connector.id;
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  fake_drm_->InitializeState(/*use_atomic=*/true);

  TileProperty tile_property{.group_id = 1,
                             .scale_to_fit_display = true,
                             .tile_size = gfx::Size(3840, 4320),
                             .tile_layout = gfx::Size(2, 1),
                             .location = gfx::Point(0, 0)};

  HardwareDisplayControllerInfo info(fake_drm_->GetConnector(connector_id),
                                     fake_drm_->GetCrtc(crtc_id),
                                     /*index=*/1, std::nullopt, tile_property);

  ASSERT_TRUE(info.tile_property().has_value());
  TileProperty actual_tile_property = *info.tile_property();

  EXPECT_EQ(actual_tile_property.group_id, tile_property.group_id);
  EXPECT_EQ(actual_tile_property.scale_to_fit_display,
            tile_property.scale_to_fit_display);
  EXPECT_EQ(actual_tile_property.tile_size, tile_property.tile_size);
  EXPECT_EQ(actual_tile_property.tile_layout, tile_property.tile_layout);
  EXPECT_EQ(actual_tile_property.location, tile_property.location);
}

TEST_F(HardwareDisplayControllerInfoTest, AcquireNonprimaryTileInfo) {
  uint32_t primary_crtc_id = 0, primary_connector_id = 0;
  {
    primary_crtc_id = fake_drm_->AddCrtc().id;
    auto& encoder = fake_drm_->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm_->AddConnector();
    primary_connector_id = connector.id;
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  uint32_t nonprimary_crtc_id = 0, nonprimary_connector_id = 0;
  {
    nonprimary_crtc_id = fake_drm_->AddCrtc().id;
    auto& encoder = fake_drm_->AddEncoder();
    encoder.possible_crtcs = 0x10;

    auto& connector = fake_drm_->AddConnector();
    nonprimary_connector_id = connector.id;
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  fake_drm_->InitializeState(/*use_atomic=*/true);

  TileProperty primary_tile_property{.group_id = 1,
                                     .scale_to_fit_display = true,
                                     .tile_size = gfx::Size(3840, 4320),
                                     .tile_layout = gfx::Size(2, 1),
                                     .location = gfx::Point(0, 0)};

  TileProperty nonprimary_tile_property = primary_tile_property;
  nonprimary_tile_property.location = gfx::Point(1, 0);

  HardwareDisplayControllerInfo primary_info(
      fake_drm_->GetConnector(primary_connector_id),
      fake_drm_->GetCrtc(primary_crtc_id),
      /*index=*/0, std::nullopt, primary_tile_property);

  EXPECT_THAT(primary_info.nonprimary_tile_infos(), IsEmpty());

  auto nonprimary_info = std::make_unique<HardwareDisplayControllerInfo>(
      fake_drm_->GetConnector(nonprimary_connector_id),
      fake_drm_->GetCrtc(nonprimary_crtc_id),
      /*index=*/0, std::nullopt, nonprimary_tile_property);

  primary_info.AcquireNonprimaryTileInfo(std::move(nonprimary_info));

  ASSERT_THAT(primary_info.nonprimary_tile_infos(), SizeIs(1));
}

TEST_F(HardwareDisplayControllerInfoTest, ReleaseConnector) {
  uint32_t crtc_id = 0, connector_id = 0;
  {
    crtc_id = fake_drm_->AddCrtc().id;
    auto& encoder = fake_drm_->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm_->AddConnector();
    connector_id = connector.id;
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  fake_drm_->InitializeState(/*use_atomic=*/true);

  HardwareDisplayControllerInfo info(fake_drm_->GetConnector(connector_id),
                                     fake_drm_->GetCrtc(crtc_id),
                                     /*index=*/1, std::nullopt);

  ASSERT_NE(info.connector(), nullptr);
  EXPECT_EQ(info.connector()->connector_id, connector_id);

  ScopedDrmConnectorPtr connector = info.ReleaseConnector();
  ASSERT_NE(connector, nullptr);
  ASSERT_EQ(info.connector(), nullptr);
  EXPECT_EQ(connector->connector_id, connector_id);
}

TEST_F(HardwareDisplayControllerInfoTest, GetModesOfSize) {
  uint32_t crtc_id = 0, connector_id = 0;
  {
    crtc_id = fake_drm_->AddCrtc().id;
    auto& encoder = fake_drm_->AddEncoder();
    encoder.possible_crtcs = 0x01;

    auto& connector = fake_drm_->AddConnector();
    connector_id = connector.id;
    connector.connection = true;
    connector.modes =
        std::vector<ResolutionAndRefreshRate>{{gfx::Size(1920, 1080), 60},
                                              {gfx::Size(1920, 1080), 30},
                                              {gfx::Size(1280, 720), 60}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  fake_drm_->InitializeState(/*use_atomic=*/true);

  HardwareDisplayControllerInfo info(fake_drm_->GetConnector(connector_id),
                                     fake_drm_->GetCrtc(crtc_id),
                                     /*index=*/1, std::nullopt);

  EXPECT_THAT(info.GetModesOfSize(gfx::Size(1920, 1080)), SizeIs(2));
  EXPECT_THAT(info.GetModesOfSize(gfx::Size(1280, 720)), SizeIs(1));
  EXPECT_THAT(info.GetModesOfSize(gfx::Size(1366, 768)), IsEmpty());
}
}  // namespace ui
