// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_UNITTEST_CC_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_UNITTEST_CC_

#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"

#include <xf86drm.h>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_display.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Exactly;
using ::testing::Field;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

// HP z32x monitor.
constexpr unsigned char kHPz32x[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x75\x32\x01\x01\x01\x01"
    "\x1B\x1B\x01\x04\xB5\x46\x27\x78\x3A\x8D\x15\xAC\x51\x32\xB8\x26"
    "\x0B\x50\x54\x21\x08\x00\xD1\xC0\xA9\xC0\x81\xC0\xD1\x00\xB3\x00"
    "\x95\x00\xA9\x40\x81\x80\x4D\xD0\x00\xA0\xF0\x70\x3E\x80\x30\x20"
    "\x35\x00\xB9\x88\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x18\x3C\x1E"
    "\x87\x3C\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x5A\x33\x32\x78\x0A\x20\x20\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x43\x37\x32\x37\x30\x4D\x57\x30\x0A\x20\x20\x01\x46"
    "\x02\x03\x18\xF1\x4B\x10\x1F\x04\x13\x03\x12\x02\x11\x01\x05\x14"
    "\x23\x09\x07\x07\x83\x01\x00\x00\xA3\x66\x00\xA0\xF0\x70\x1F\x80"
    "\x30\x20\x35\x00\xB9\x88\x21\x00\x00\x1A\x56\x5E\x00\xA0\xA0\xA0"
    "\x29\x50\x30\x20\x35\x00\xB9\x88\x21\x00\x00\x1A\xEF\x51\x00\xA0"
    "\xF0\x70\x19\x80\x30\x20\x35\x00\xB9\x88\x21\x00\x00\x1A\xE2\x68"
    "\x00\xA0\xA0\x40\x2E\x60\x20\x30\x63\x00\xB9\x88\x21\x00\x00\x1C"
    "\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20\x36\x00\xB9\x88\x21\x00"
    "\x00\x1A\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x3E";
constexpr size_t kHPz32xLength = std::size(kHPz32x);

// An EDID that can be found on an internal panel.
constexpr unsigned char kInternalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\xa3\x42\x31\x00\x00\x00\x00"
    "\x00\x15\x01\x03\x80\x1a\x10\x78\x0a\xd3\xe5\x95\x5c\x60\x90\x27"
    "\x19\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x9e\x1b\x00\xa0\x50\x20\x12\x30\x10\x30"
    "\x13\x00\x05\xa3\x10\x00\x00\x19\x00\x00\x00\x0f\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x23\x87\x02\x64\x00\x00\x00\x00\xfe\x00\x53"
    "\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x00\x00\x00\xfe"
    "\x00\x31\x32\x31\x41\x54\x31\x31\x2d\x38\x30\x31\x0a\x20\x00\x45";
constexpr size_t kInternalDisplayLength = std::size(kInternalDisplay);

// Serial number is unavailable. Omitted from bytes 12-15 of block zero and SN
// descriptor (tag: 0xff). Displays like this will produce colliding EDID-based
// display IDs.
constexpr unsigned char kNoSerialNumberDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x00\x00\x00\x00"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\x10"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x71";
constexpr size_t kNoSerialNumberDisplayLength =
    std::size(kNoSerialNumberDisplay);

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

constexpr char kDefaultTestGraphicsCardPattern[] = "/test/dri/card%d";

constexpr char kTestOnlyModesetOutcomeOneDisplay[] =
    "ConfigureDisplays.Modeset.Test.OneDisplay.Outcome";
constexpr char kTestOnlyModesetOutcomeTwoDisplays[] =
    "ConfigureDisplays.Modeset.Test.TwoDisplays.Outcome";
constexpr char kTestOnlyModesetFallbacksAttemptedTwoDisplaysMetric[] =
    "ConfigureDisplays.Modeset.Test.DynamicCRTCs.TwoDisplays."
    "PermutationsAttempted";

const std::vector<ResolutionAndRefreshRate> kStandardModes = {
    ResolutionAndRefreshRate{gfx::Size(3840, 2160), 60u},
    ResolutionAndRefreshRate{gfx::Size(3840, 2160), 50u},
    ResolutionAndRefreshRate{gfx::Size(3840, 2160), 30u},
    ResolutionAndRefreshRate{gfx::Size(1920, 1080), 60u},
    ResolutionAndRefreshRate{gfx::Size(1920, 1080), 50u},
    ResolutionAndRefreshRate{gfx::Size(1920, 1080), 30u}};

enum class TestOnlyModesetOutcome {
  kSuccess = 0,
  kFallbackSuccess = 1,
  kFailure = 2,
  kMaxValue = kFailure,
};

// arg: drmModeAtomicReq*, pairings: CrtcConnectorPairs
MATCHER_P(AtomicRequestHasCrtcConnectorPairs, pairings, "") {
  if (arg == nullptr) {
    return false;
  }

  std::vector<drmModeAtomicReqItem> arg_items;
  for (uint32_t i = 0; i < arg->cursor; ++i) {
    arg_items.push_back(arg->items[i]);
  }

  for (const auto& [crtc, connector] : pairings) {
    if (std::find_if(arg_items.begin(), arg_items.end(),
                     [crtc, connector](const drmModeAtomicReqItem& item) {
                       return item.object_id == connector && item.value == crtc;
                     }) == arg_items.end()) {
      return false;
    }
  }

  return true;
}

class MockDrmDeviceGenerator : public DrmDeviceGenerator {
  scoped_refptr<DrmDevice> CreateDevice(const base::FilePath& path,
                                        base::ScopedFD fd,
                                        bool is_primary_device) override {
    auto gbm_device = std::make_unique<MockGbmDevice>();
    DCHECK(!path.empty());
    return base::MakeRefCounted<MockDrmDevice>(
        std::move(path), std::move(gbm_device), is_primary_device);
  }
};

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

testing::Matcher<display::DisplayMode> EqResAndRefresh(
    const ResolutionAndRefreshRate& mode) {
  return AllOf(Property(&display::DisplayMode::size, Eq(mode.first)),
               Property(&display::DisplayMode::refresh_rate, Eq(mode.second)));
}

// Verifies that two vectors contain equal requests, excluding certain
// properties that are permitted to change during a configuration. Assumes that
// the vectors maintain the same ordering w.r.t. the requests' `id` properties.
void ExpectEqualRequestsWithExceptions(
    const std::vector<display::DisplayConfigurationParams>& a,
    const std::vector<display::DisplayConfigurationParams>& b) {
  EXPECT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    EXPECT_EQ(a[i].id, b[i].id);
    EXPECT_EQ(a[i].origin, b[i].origin);
    EXPECT_EQ(a[i].enable_vrr, b[i].enable_vrr);
    EXPECT_EQ(a[i].mode->size(), b[i].mode->size());
    EXPECT_EQ(a[i].mode->is_interlaced(), b[i].mode->is_interlaced());
    // mode->refresh_rate() excepted because it can update after configuration.
    // mode->vsync_rate_min() excepted because it can update after
    // configuration.
  }
}

}  // namespace

class DrmGpuDisplayManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<FakeDrmDeviceGenerator> fake_device_generator =
        std::make_unique<FakeDrmDeviceGenerator>();
    device_manager_ =
        std::make_unique<DrmDeviceManager>(std::move(fake_device_generator));
    screen_manager_ = std::make_unique<ScreenManager>();
    drm_gpu_display_manager_ = std::make_unique<DrmGpuDisplayManager>(
        screen_manager_.get(), device_manager_.get());
  }

  void TearDown() override {
    drm_gpu_display_manager_ = nullptr;
    screen_manager_ = nullptr;
    for (auto& device : device_manager_->GetDrmDevices()) {
      FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(device.get());
      fake_drm->ResetPlaneManagerForTesting();
    }
    device_manager_ = nullptr;
    next_drm_device_number_ = 0u;
  }

  // Note: the first device added will be marked as the primary device.
  scoped_refptr<FakeDrmDevice> AddDrmDevice() {
    std::string card_path = base::StringPrintf(kDefaultTestGraphicsCardPattern,
                                               next_drm_device_number_++);
    base::FilePath file_path(card_path);

    device_manager_->AddDrmDevice(file_path, base::ScopedFD());
    FakeDrmDevice* fake_drm = static_cast<FakeDrmDevice*>(
        device_manager_->GetDrmDevices().back().get());
    return scoped_refptr<FakeDrmDevice>(fake_drm);
  }

  bool ConfigureDisplays(const MovableDisplaySnapshots& display_snapshots,
                         display::ModesetFlags modeset_flag) {
    std::vector<display::DisplayConfigurationParams> config_requests;
    for (const auto& snapshot : display_snapshots) {
      config_requests.emplace_back(snapshot->display_id(), snapshot->origin(),
                                   snapshot->native_mode());
    }
    std::vector<display::DisplayConfigurationParams> out_requests;
    return drm_gpu_display_manager_->ConfigureDisplays(
        config_requests, modeset_flag, out_requests);
  }

  DrmDisplay* FindDisplay(int64_t display_id) {
    return drm_gpu_display_manager_->FindDisplay(display_id);
  }

  DrmDisplay* FindDisplayByConnectorId(uint32_t connector) {
    return drm_gpu_display_manager_->FindDisplayByConnectorId(connector);
  }

  size_t next_drm_device_number_ = 0u;

  std::unique_ptr<DrmDeviceManager> device_manager_;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<DrmGpuDisplayManager> drm_gpu_display_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DrmGpuDisplayManagerTest, CapOutOnMaxDrmDeviceCount) {
  // Add |kMaxDrmCount| + 1 DRM devices, each with one active display.
  for (size_t i = 0; i < kMaxDrmCount + 1; ++i) {
    auto fake_drm = AddDrmDevice();
    fake_drm->ResetStateWithAllProperties();

    // Add 1 CRTC
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    // Add one encoder
    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0b1;

    // Add 1 Connector
    auto& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{kStandardModes[0]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);

    fake_drm->InitializeState(/* use_atomic */ true);
  }

  ASSERT_EQ(drm_gpu_display_manager_->GetDisplays().size(), kMaxDrmCount);
}

TEST_F(DrmGpuDisplayManagerTest, CapOutOnMaxConnectorCount) {
  // One DRM device.
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  // Add |kMaxDrmConnectors| + 1 connector, each with one active display.
  for (size_t i = 0; i < kMaxDrmConnectors + 1; ++i) {
    // Add 1 CRTC
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    // Add one encoder
    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 1 << i;

    // Add 1 Connector
    auto& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{kStandardModes[0]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
  }
  fake_drm->InitializeState(/* use_atomic */ true);

  ASSERT_EQ(drm_gpu_display_manager_->GetDisplays().size(), kMaxDrmConnectors);
}

TEST_F(DrmGpuDisplayManagerTest, FindAndConfigureDisplaysOnSameDrmDevice) {
  // One DRM device.
  auto drm = AddDrmDevice();
  drm->ResetStateWithAllProperties();

  // Add 3 connectors, each with one active display.
  for (size_t i = 0; i < 3; ++i) {
    drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& encoder = drm->AddEncoder();
    encoder.possible_crtcs = 1 << i;

    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        kStandardModes[i % kStandardModes.size()]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
  }
  drm->InitializeState(/* use_atomic */ true);

  MovableDisplaySnapshots display_snapshots =
      drm_gpu_display_manager_->GetDisplays();
  ASSERT_EQ(display_snapshots.size(), 3u);

  // Make sure the displays are successfully found on the only existing DRM
  // device.
  DrmDisplay* display1 = FindDisplay(display_snapshots[0]->display_id());
  ASSERT_TRUE(display1);
  ASSERT_EQ(display1->drm().get(), drm.get());

  DrmDisplay* display2 = FindDisplay(display_snapshots[1]->display_id());
  ASSERT_TRUE(display2);
  ASSERT_EQ(display2->drm().get(), drm.get());

  DrmDisplay* display3 = FindDisplay(display_snapshots[2]->display_id());
  ASSERT_TRUE(display3);
  ASSERT_EQ(display3->drm().get(), drm.get());

  // Make sure configuration on the returned snapshots is possible.
  ASSERT_TRUE(ConfigureDisplays(display_snapshots,
                                {display::ModesetFlag::kTestModeset,
                                 display::ModesetFlag::kCommitModeset}));
}

// This case tests scenarios in which a display ID is searched across multiple
// DRM devices, such as in DisplayLink hubs.
TEST_F(DrmGpuDisplayManagerTest,
       FindAndConfigureDisplaysAcrossDifferentDrmDevices) {
  // Add 3 DRM devices, each with one active display.
  for (size_t i = 0; i < 3; ++i) {
    auto fake_drm = AddDrmDevice();
    fake_drm->ResetStateWithAllProperties();

    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0b1;

    auto& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        kStandardModes[i % kStandardModes.size()]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);

    fake_drm->InitializeState(/* use_atomic */ true);
  }

  MovableDisplaySnapshots display_snapshots =
      drm_gpu_display_manager_->GetDisplays();
  ASSERT_EQ(display_snapshots.size(), 3u);

  // Make sure the displays are successfully found across all existing DRM
  // devices, and that the returned devices are indeed different.
  DrmDisplay* display1 = FindDisplay(display_snapshots[0]->display_id());
  ASSERT_TRUE(display1);

  DrmDisplay* display2 = FindDisplay(display_snapshots[1]->display_id());
  ASSERT_TRUE(display2);

  DrmDisplay* display3 = FindDisplay(display_snapshots[2]->display_id());
  ASSERT_TRUE(display3);

  ASSERT_NE(display1->drm().get(), display2->drm().get());
  ASSERT_NE(display1->drm().get(), display3->drm().get());
  ASSERT_NE(display2->drm().get(), display3->drm().get());

  // Make sure configuration on the returned snapshots is possible.
  ASSERT_TRUE(ConfigureDisplays(display_snapshots,
                                {display::ModesetFlag::kTestModeset,
                                 display::ModesetFlag::kCommitModeset}));
}

TEST_F(DrmGpuDisplayManagerTest,
       OriginsPersistThroughSimilarExtendedModeConfigurations) {
  // One DRM device.
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  // Add three connectors, each with one active display.
  for (size_t i = 0; i < 3; ++i) {
    // Add 1 CRTC
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    // Add one encoder
    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 1 << i;

    // Add 1 Connector
    auto& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{kStandardModes[0]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
  }
  fake_drm->InitializeState(/* use_atomic */ true);

  auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_EQ(display_snapshots.size(), 3u);

  // Set the first set of Display's origins to imitate the process that happens
  // in the DisplayConfigurator during a full display configuration. Moving on,
  // these origins should persist while moving between extended and SW mirror
  // mode, since they're both technically the same (i.e. extended mode).
  DrmDisplay* display1 = FindDisplay(display_snapshots[0]->display_id());
  ASSERT_TRUE(display1);
  const gfx::Point display1_origin = gfx::Point(0, 0);
  display_snapshots[0]->set_origin(display1_origin);
  display1->SetOrigin(display_snapshots[0]->origin());

  DrmDisplay* display2 = FindDisplay(display_snapshots[1]->display_id());
  ASSERT_TRUE(display2);
  const gfx::Point display2_origin =
      gfx::Point(0, display_snapshots[0]->native_mode()->size().height());
  display_snapshots[1]->set_origin(display2_origin);
  display2->SetOrigin(display_snapshots[1]->origin());

  DrmDisplay* display3 = FindDisplay(display_snapshots[2]->display_id());
  ASSERT_TRUE(display3);
  const gfx::Point display3_origin =
      gfx::Point(0, display_snapshots[0]->native_mode()->size().height() +
                        display_snapshots[1]->native_mode()->size().height());
  display_snapshots[2]->set_origin(display3_origin);
  display3->SetOrigin(display_snapshots[2]->origin());

  ASSERT_TRUE(ConfigureDisplays(display_snapshots,
                                {display::ModesetFlag::kTestModeset,
                                 display::ModesetFlag::kCommitModeset}));

  // "Switch" to mirror mode - so nothing changes as far as display resources
  // go.
  display_snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_TRUE(!display_snapshots.empty());

  display1 = FindDisplay(display_snapshots[0]->display_id());
  ASSERT_TRUE(display1);
  ASSERT_EQ(display1->origin(), display_snapshots[0]->origin());
  ASSERT_EQ(display1->origin(), display1_origin);

  display2 = FindDisplay(display_snapshots[1]->display_id());
  ASSERT_TRUE(display2);
  ASSERT_EQ(display2->origin(), display_snapshots[1]->origin());
  ASSERT_EQ(display2->origin(), display2_origin);

  display3 = FindDisplay(display_snapshots[2]->display_id());
  ASSERT_TRUE(display3);
  ASSERT_EQ(display3->origin(), display_snapshots[2]->origin());
  ASSERT_EQ(display3->origin(), display3_origin);

  ASSERT_TRUE(ConfigureDisplays(display_snapshots,
                                {display::ModesetFlag::kTestModeset,
                                 display::ModesetFlag::kCommitModeset}));
}

TEST_F(DrmGpuDisplayManagerTest, TestEdidIdConflictResolution) {
  // One DRM device.
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  // First, add the internal display.
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0b1;

    auto& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{kStandardModes[3]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob = std::vector<uint8_t>(
        kInternalDisplay, kInternalDisplay + kInternalDisplayLength);
  }

  // Next, add two external displays that will produce an EDID-based ID
  // collision, since their EDIDs do not include viable serial numbers.
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0b10;

    auto& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.modes = kStandardModes;
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob = std::vector<uint8_t>(
        kNoSerialNumberDisplay,
        kNoSerialNumberDisplay + kNoSerialNumberDisplayLength);
  }

  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& encoder = fake_drm->AddEncoder();
    encoder.possible_crtcs = 0b100;

    auto& connector = fake_drm->AddConnector();
    connector.connection = true;
    connector.modes = kStandardModes;
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob = std::vector<uint8_t>(
        kNoSerialNumberDisplay,
        kNoSerialNumberDisplay + kNoSerialNumberDisplayLength);
  }

  fake_drm->InitializeState(/* use_atomic */ true);

  auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_EQ(display_snapshots.size(), 3u);

  // First, ensure all display IDs are unique.
  ASSERT_NE(display_snapshots[0]->edid_display_id(),
            display_snapshots[1]->edid_display_id());
  ASSERT_NE(display_snapshots[0]->edid_display_id(),
            display_snapshots[2]->edid_display_id());
  ASSERT_NE(display_snapshots[1]->edid_display_id(),
            display_snapshots[2]->edid_display_id());

  // The EDID-based display IDs occupy the first 32 bits of the id field.
  // Extract unresolved EDID IDs from the snapshots and show that they are
  // equal, and thus have been successfully resolved.
  const int64_t unresolved_display_id1 =
      display_snapshots[1]->edid_display_id() & 0xffffffff;
  const int64_t unresolved_display_id2 =
      display_snapshots[2]->edid_display_id() & 0xffffffff;
  ASSERT_EQ(unresolved_display_id1, unresolved_display_id2);
}

class DrmGpuDisplayManagerMockedDeviceTest : public DrmGpuDisplayManagerTest {
 protected:
  void SetUp() override {
    std::unique_ptr<MockDrmDeviceGenerator> fake_device_generator =
        std::make_unique<MockDrmDeviceGenerator>();
    device_manager_ =
        std::make_unique<DrmDeviceManager>(std::move(fake_device_generator));
    screen_manager_ = std::make_unique<ScreenManager>();
    drm_gpu_display_manager_ = std::make_unique<DrmGpuDisplayManager>(
        screen_manager_.get(), device_manager_.get());
  }
};

TEST_F(DrmGpuDisplayManagerMockedDeviceTest,
       ConfigureDisplaysAlternateCrtcFallbackSuccess) {
  auto drm = AddDrmDevice();
  drm->ResetStateWithAllProperties();

  // Create a pool of 3 CRTCs
  const uint32_t crtc_1 = drm->AddCrtcWithPrimaryAndCursorPlanes().id;
  drm->AddCrtcWithPrimaryAndCursorPlanes();
  const uint32_t crtc_3 = drm->AddCrtcWithPrimaryAndCursorPlanes().id;

  uint32_t big_display_connector_id, small_display_connector_id;

  // First, add a "big" display with high bandwidth mode.
  {
    auto& encoder = drm->AddEncoder();
    // Can use all 3 CRTCs
    encoder.possible_crtcs = 0b111;

    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        ResolutionAndRefreshRate{gfx::Size(7680, 4320), 144u}};
    connector.encoders = std::vector<uint32_t>{encoder.id};

    big_display_connector_id = connector.id;
  }
  // Add a "small" external display.
  {
    auto& encoder = drm->AddEncoder();
    encoder.possible_crtcs = 0b111;

    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = kStandardModes;
    connector.encoders = std::vector<uint32_t>{encoder.id};

    small_display_connector_id = connector.id;
  }

  drm->InitializeState(/* use_atomic */ true);
  MockDrmDevice* mock_drm = static_cast<MockDrmDevice*>(drm.get());

  auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_EQ(display_snapshots.size(), 2u);

  DrmDisplay* big_display = FindDisplayByConnectorId(big_display_connector_id);
  const DrmDisplay::CrtcConnectorPair*
      original_big_display_crtc_connector_pair =
          big_display->GetCrtcConnectorPairForConnectorId(
              big_display_connector_id);
  ASSERT_NE(original_big_display_crtc_connector_pair, nullptr);

  DrmDisplay* small_display =
      FindDisplayByConnectorId(small_display_connector_id);
  const DrmDisplay::CrtcConnectorPair*
      original_small_display_crtc_connector_pair =
          small_display->GetCrtcConnectorPairForConnectorId(
              small_display_connector_id);
  ASSERT_NE(original_small_display_crtc_connector_pair, nullptr);

  // Modesets should fail by default, unless it is with CRTC-connector pairings
  // specified with |desired_pairings|.
  EXPECT_CALL(*mock_drm, CommitProperties)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));

  CrtcConnectorPairs desired_pairings = {{crtc_1, big_display_connector_id},
                                         {crtc_3, small_display_connector_id}};
  EXPECT_CALL(
      *mock_drm,
      CommitProperties(AtomicRequestHasCrtcConnectorPairs(desired_pairings), _,
                       _, _))
      .WillOnce(Return(true));

  EXPECT_TRUE(ConfigureDisplays(display_snapshots,
                                {display::ModesetFlag::kTestModeset}));

  histogram_tester_.ExpectBucketCount(kTestOnlyModesetOutcomeTwoDisplays,
                                      TestOnlyModesetOutcome::kFallbackSuccess,
                                      /*expected_count=*/1);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  kTestOnlyModesetFallbacksAttemptedTwoDisplaysMetric),
              UnorderedElementsAre(AllOf(Field(&base::Bucket::min, Gt(0)),
                                         Field(&base::Bucket::count, Eq(1)))));

  // Even if there is a successful fallback configuration, ozone abstractions
  // should not change for test modeset request.
  big_display = FindDisplayByConnectorId(big_display_connector_id);
  EXPECT_EQ(
      big_display->GetCrtcConnectorPairForConnectorId(big_display_connector_id)
          ->crtc_id,
      original_big_display_crtc_connector_pair->crtc_id);
  small_display = FindDisplayByConnectorId(small_display_connector_id);
  EXPECT_EQ(small_display
                ->GetCrtcConnectorPairForConnectorId(small_display_connector_id)
                ->crtc_id,
            original_small_display_crtc_connector_pair->crtc_id);

  // Check that commit modeset after fallback changes the CRTC assignment for
  // displays.
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_drm));
  EXPECT_CALL(
      *mock_drm,
      CommitProperties(AtomicRequestHasCrtcConnectorPairs(desired_pairings), _,
                       _, _))
      .WillOnce(Return(true));

  EXPECT_TRUE(ConfigureDisplays(display_snapshots,
                                {display::ModesetFlag::kCommitModeset}));
  EXPECT_TRUE(big_display->ContainsCrtc(crtc_1));
  EXPECT_TRUE(small_display->ContainsCrtc(crtc_3));

  // DrmDevice seems to leak on successful configure in tests. Manually
  // checking for mock calls and allowing leak for now.
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_drm));
  testing::Mock::AllowLeak(mock_drm);
}

TEST_F(DrmGpuDisplayManagerMockedDeviceTest,
       ConfigureDisplaysFallbackTestSuccessButCommitFailure) {
  auto drm = AddDrmDevice();
  drm->ResetStateWithAllProperties();

  // Create a pool of 3 CRTCs
  const uint32_t crtc_1 = drm->AddCrtcWithPrimaryAndCursorPlanes().id;
  drm->AddCrtcWithPrimaryAndCursorPlanes();
  const uint32_t crtc_3 = drm->AddCrtcWithPrimaryAndCursorPlanes().id;

  uint32_t primary_connector_id, secondary_connector_id;

  // First, add a display with high bandwidth mode.
  {
    auto& encoder = drm->AddEncoder();
    // Can use all 3 CRTCs
    encoder.possible_crtcs = 0b111;

    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        ResolutionAndRefreshRate{gfx::Size(7680, 4320), 144u}};
    connector.encoders = std::vector<uint32_t>{encoder.id};

    primary_connector_id = connector.id;
  }
  // Add a normal external display.
  {
    auto& encoder = drm->AddEncoder();
    encoder.possible_crtcs = 0b111;

    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = kStandardModes;
    connector.encoders = std::vector<uint32_t>{encoder.id};

    secondary_connector_id = connector.id;
  }

  drm->InitializeState(/* use_atomic */ true);
  MockDrmDevice* mock_drm = static_cast<MockDrmDevice*>(drm.get());

  auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_EQ(display_snapshots.size(), 2u);

  // Modesets should fail by default, unless it is with CRTC-connector pairings
  // specified with |desired_pairings|.
  EXPECT_CALL(*mock_drm, CommitProperties)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));

  CrtcConnectorPairs desired_pairings = {{crtc_1, primary_connector_id},
                                         {crtc_3, secondary_connector_id}};
  EXPECT_CALL(
      *mock_drm,
      CommitProperties(AtomicRequestHasCrtcConnectorPairs(desired_pairings), _,
                       _, _))
      .WillRepeatedly(Return(true));

  // test should succeed.
  EXPECT_TRUE(ConfigureDisplays(display_snapshots,
                                {display::ModesetFlag::kTestModeset}));

  histogram_tester_.ExpectBucketCount(kTestOnlyModesetOutcomeTwoDisplays,
                                      TestOnlyModesetOutcome::kFallbackSuccess,
                                      /*expected_count=*/1);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  kTestOnlyModesetFallbacksAttemptedTwoDisplaysMetric),
              UnorderedElementsAre(AllOf(Field(&base::Bucket::min, Gt(0)),
                                         Field(&base::Bucket::count, Eq(1)))));

  EXPECT_CALL(
      *mock_drm,
      CommitProperties(AtomicRequestHasCrtcConnectorPairs(desired_pairings),
                       // For non-test.
                       DRM_MODE_ATOMIC_ALLOW_MODESET, _, _))
      // Return false - even though testing with the exact same pairings
      // succeeded.
      .WillRepeatedly(Return(false));
  // Commit with exact config should fail.
  EXPECT_FALSE(ConfigureDisplays(display_snapshots,
                                 {display::ModesetFlag::kCommitModeset}));

  DrmDisplay* primary_display = FindDisplayByConnectorId(primary_connector_id);
  EXPECT_TRUE(primary_display->ContainsCrtc(crtc_1));
  DrmDisplay* secondary_display =
      FindDisplayByConnectorId(secondary_connector_id);
  EXPECT_TRUE(secondary_display->ContainsCrtc(crtc_3));
  histogram_tester_.ExpectBucketCount(kTestOnlyModesetOutcomeTwoDisplays,
                                      TestOnlyModesetOutcome::kFailure,
                                      /*expected_count=*/0);
}

TEST_F(DrmGpuDisplayManagerMockedDeviceTest,
       ConfigureDisplaysAlternateCrtcFallbackAllFailed) {
  auto drm = AddDrmDevice();
  drm->ResetStateWithAllProperties();

  // Create a pool of 3 CRTCs
  drm->AddCrtcWithPrimaryAndCursorPlanes();
  drm->AddCrtcWithPrimaryAndCursorPlanes();
  drm->AddCrtcWithPrimaryAndCursorPlanes();

  // First, add a display with high bandwidth mode.
  {
    auto& encoder = drm->AddEncoder();
    // Can use all 3 CRTCs
    encoder.possible_crtcs = 0b111;

    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        ResolutionAndRefreshRate{gfx::Size(7680, 4320), 144u}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }
  // Add a normal external display.
  {
    auto& encoder = drm->AddEncoder();
    encoder.possible_crtcs = 0b111;

    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = kStandardModes;
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  drm->InitializeState(/* use_atomic */ true);
  MockDrmDevice* mock_drm = static_cast<MockDrmDevice*>(drm.get());

  auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_EQ(display_snapshots.size(), 2u);
  EXPECT_CALL(*mock_drm, CommitProperties)
      // Expect at least 4 calls to ensure fallback is being triggered (2 from
      // the initial linear/preferred modifier test modeset, and another 2 for
      // the first fallback).
      .Times(AtLeast(4))
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(ConfigureDisplays(display_snapshots,
                                 {display::ModesetFlag::kTestModeset}));
  histogram_tester_.ExpectBucketCount(kTestOnlyModesetOutcomeTwoDisplays,
                                      TestOnlyModesetOutcome::kFailure,
                                      /*expected_count=*/1);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  kTestOnlyModesetFallbacksAttemptedTwoDisplaysMetric),
              UnorderedElementsAre(AllOf(Field(&base::Bucket::min, Gt(0)),
                                         Field(&base::Bucket::count, Eq(1)))));
}

TEST_F(DrmGpuDisplayManagerMockedDeviceTest,
       NoConfigureDisplaysAlternateCrtcFallbackWithHardwareMirroring) {
  // Enable hardware mirroring
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      display::features::kEnableHardwareMirrorMode);
  ASSERT_TRUE(display::features::IsHardwareMirrorModeEnabled());
  // Re-initialize DrmGpuDisplayManager with HW mirroring enabled.
  drm_gpu_display_manager_ = std::make_unique<DrmGpuDisplayManager>(
      screen_manager_.get(), device_manager_.get());

  auto drm = AddDrmDevice();
  drm->ResetStateWithAllProperties();

  // Create a pool of 2 CRTCs
  drm->AddCrtcWithPrimaryAndCursorPlanes();
  drm->AddCrtcWithPrimaryAndCursorPlanes();

  {
    auto& encoder = drm->AddEncoder();
    encoder.possible_crtcs = 0b11;
    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        ResolutionAndRefreshRate{gfx::Size(7680, 4320), 144u}};
    connector.encoders = std::vector<uint32_t>{encoder.id};
  }

  drm->InitializeState(/* use_atomic */ true);
  MockDrmDevice* mock_drm = static_cast<MockDrmDevice*>(drm.get());

  auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_EQ(display_snapshots.size(), 1u);

  // Test-modeset should only be called twice - without any fallbacks attempted.
  EXPECT_CALL(*mock_drm, CommitProperties)
      // Expect 2 calls as ScreenManager tests modeset with preferred modifiers
      // and then linear modifiers on failure.
      .Times(Exactly(2))
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(ConfigureDisplays(display_snapshots,
                                 {display::ModesetFlag::kTestModeset}));
  histogram_tester_.ExpectBucketCount(kTestOnlyModesetOutcomeOneDisplay,
                                      TestOnlyModesetOutcome::kFailure,
                                      /*expected_count=*/1);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  kTestOnlyModesetFallbacksAttemptedTwoDisplaysMetric),
              IsEmpty());
}

TEST_F(DrmGpuDisplayManagerMockedDeviceTest,
       ConfigureDisplaysUseSuccessfulStateForCommit) {
  auto drm = AddDrmDevice();
  drm->ResetStateWithAllProperties();

  // Create a pool of 3 CRTCs
  const uint32_t crtc_1 = drm->AddCrtcWithPrimaryAndCursorPlanes().id;
  drm->AddCrtcWithPrimaryAndCursorPlanes();
  const uint32_t crtc_3 = drm->AddCrtcWithPrimaryAndCursorPlanes().id;

  uint32_t primary_connector_id, secondary_connector_id;

  // First, add a display with high bandwidth mode.
  {
    auto& encoder = drm->AddEncoder();
    // Can use all 3 CRTCs
    encoder.possible_crtcs = 0b111;

    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        ResolutionAndRefreshRate{gfx::Size(7680, 4320), 144u}};
    connector.encoders = std::vector<uint32_t>{encoder.id};

    primary_connector_id = connector.id;
  }
  // Add a normal external display.
  {
    auto& encoder = drm->AddEncoder();
    encoder.possible_crtcs = 0b111;

    auto& connector = drm->AddConnector();
    connector.connection = true;
    connector.modes = kStandardModes;
    connector.encoders = std::vector<uint32_t>{encoder.id};

    secondary_connector_id = connector.id;
  }

  drm->InitializeState(/* use_atomic */ true);
  MockDrmDevice* mock_drm = static_cast<MockDrmDevice*>(drm.get());

  auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_EQ(display_snapshots.size(), 2u);

  CrtcConnectorPairs desired_pairings = {{crtc_1, primary_connector_id},
                                         {crtc_3, secondary_connector_id}};
  // First test modeset should fallback and succeed on |desired_pairings|.
  {
    // Modesets should fail by default, unless it is with CRTC-connector
    // pairings specified with |desired_pairings|.
    EXPECT_CALL(*mock_drm, CommitProperties)
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));

    EXPECT_CALL(
        *mock_drm,
        CommitProperties(AtomicRequestHasCrtcConnectorPairs(desired_pairings),
                         _, _, _))
        .WillOnce(Return(true));

    EXPECT_TRUE(ConfigureDisplays(display_snapshots,
                                  {display::ModesetFlag::kTestModeset}));

    histogram_tester_.ExpectBucketCount(
        kTestOnlyModesetOutcomeTwoDisplays,
        TestOnlyModesetOutcome::kFallbackSuccess,
        /*expected_count=*/1);
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kTestOnlyModesetFallbacksAttemptedTwoDisplaysMetric),
        UnorderedElementsAre(AllOf(Field(&base::Bucket::min, Gt(0)),
                                   Field(&base::Bucket::count, Eq(1)))));
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_drm));
  }
  // Second test modeset with different config should fail, and leave the
  // DrmGpuDisplayManager with a failed CRTC-connector pairings.
  {
    std::vector<display::DisplayConfigurationParams> failing_request;
    for (const auto& snapshot : display_snapshots) {
      failing_request.emplace_back(snapshot->display_id(), snapshot->origin(),
                                   snapshot->modes().back().get());
    }

    std::vector<display::DisplayConfigurationParams> out_requests;
    EXPECT_CALL(*mock_drm, CommitProperties)
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));
    EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
        failing_request, {display::ModesetFlag::kTestModeset}, out_requests));

    histogram_tester_.ExpectBucketCount(kTestOnlyModesetOutcomeTwoDisplays,
                                        TestOnlyModesetOutcome::kFailure,
                                        /*expected_count=*/1);
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            kTestOnlyModesetFallbacksAttemptedTwoDisplaysMetric),
        UnorderedElementsAre(AllOf(Field(&base::Bucket::min, Gt(0)),
                                   Field(&base::Bucket::count, Eq(1))),
                             AllOf(Field(&base::Bucket::min, Gt(0)),
                                   Field(&base::Bucket::count, Eq(1)))));
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_drm));
  }
  // A commit call made with previously successful config (first config) should
  // restore the abstractions back to the state of its success.
  {
    // Only commit modeset should be called once.
    EXPECT_CALL(*mock_drm, CommitProperties).Times(0);
    EXPECT_CALL(
        *mock_drm,
        CommitProperties(AtomicRequestHasCrtcConnectorPairs(desired_pairings),
                         _, _, _))
        .WillOnce(Return(true));

    EXPECT_TRUE(ConfigureDisplays(display_snapshots,
                                  {display::ModesetFlag::kCommitModeset}));
  }

  DrmDisplay* primary_display = FindDisplayByConnectorId(primary_connector_id);
  EXPECT_TRUE(primary_display->ContainsCrtc(crtc_1));
  DrmDisplay* secondary_display =
      FindDisplayByConnectorId(secondary_connector_id);
  EXPECT_TRUE(secondary_display->ContainsCrtc(crtc_3));
}

// DrmGpuDisplayManagerGetSeamlessRefreshRateTest is a test fixture for tests
// related to testing seamless refresh rates.
class DrmGpuDisplayManagerGetSeamlessRefreshRateTest
    : public DrmGpuDisplayManagerMockedDeviceTest {
 protected:
  void SetUp() override {
    DrmGpuDisplayManagerMockedDeviceTest::SetUp();

    fake_drm_device_ = AddDrmDevice();
    fake_drm_device_->ResetStateWithAllProperties();

    // Add internal display with a possible downclock mode and without VRR
    // capability.
    {
      fake_drm_device_->AddCrtcWithPrimaryAndCursorPlanes();

      auto& encoder = fake_drm_device_->AddEncoder();
      encoder.possible_crtcs = 0b1;

      auto& connector = fake_drm_device_->AddConnector();
      connector.connection = true;
      connector.modes = {
          // native mode.
          ResolutionAndRefreshRate{gfx::Size(3840, 2160), 120u},
          // downclock mode.
          ResolutionAndRefreshRate{gfx::Size(3840, 2160), 60u},
      };
      connector.encoders = std::vector<uint32_t>{encoder.id};
      connector.edid_blob = std::vector<uint8_t>(
          kInternalDisplay, kInternalDisplay + kInternalDisplayLength);
    }

    // Add external display with a possible downclock mode and with VRR
    // capability.
    {
      fake_drm_device_->AddCrtcWithPrimaryAndCursorPlanes();

      auto& encoder = fake_drm_device_->AddEncoder();
      encoder.possible_crtcs = 0b10;

      auto& connector = fake_drm_device_->AddConnector();
      connector.connection = true;
      connector.modes = {
          // native mode.
          ResolutionAndRefreshRate{gfx::Size(3840, 2160), 120u},
          // downclock mode.
          ResolutionAndRefreshRate{gfx::Size(3840, 2160), 60u},
      };
      connector.encoders = std::vector<uint32_t>{encoder.id};
      // Use HPz32x because it sets vsync_rate_min=24Hz, which is required for
      // VRR capability.
      connector.edid_blob =
          std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
      fake_drm_device_->AddProperty(connector.id,
                                    {.id = kVrrCapablePropId, .value = 1});
    }

    fake_drm_device_->InitializeState(/* use_atomic */ true);
    mock_drm_device_ = static_cast<MockDrmDevice*>(fake_drm_device_.get());

    // Do an initial configuration of the display.
    auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
    CHECK_EQ(display_snapshots.size(), 2u);
    CHECK(ConfigureDisplays(display_snapshots,
                            {display::ModesetFlag::kCommitModeset}));
  }

  scoped_refptr<FakeDrmDevice> fake_drm_device_;
  raw_ptr<MockDrmDevice> mock_drm_device_;
};

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       GetSeamlessRefreshRates) {
  // Get the display id for the display to probe.
  auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_TRUE(!display_snapshots.empty());
  int64_t display_id = display_snapshots[0]->display_id();

  // Expect two test commits to correspond to the two modes with
  // the same visible size as the currently configured mode.
  const uint32_t seamless_test_flags = DRM_MODE_ATOMIC_TEST_ONLY;
  EXPECT_CALL(*mock_drm_device_, CommitProperties(_, seamless_test_flags, _, _))
      .Times(2)
      .WillRepeatedly(Return(true));

  std::optional<std::vector<float>> refresh_rates =
      drm_gpu_display_manager_->GetSeamlessRefreshRates(display_id);
  ASSERT_TRUE(refresh_rates);
  EXPECT_EQ(refresh_rates->size(), 2u);
  EXPECT_EQ(std::count(refresh_rates->begin(), refresh_rates->end(), 120u), 1);
  EXPECT_EQ(std::count(refresh_rates->begin(), refresh_rates->end(), 60u), 1);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       GetSeamlessRefreshRatesWithInvalidId) {
  // Return std::nullopt for invalid display id.
  const int64_t wrong_display_id = 42;
  EXPECT_CALL(*mock_drm_device_, CommitProperties(_, _, _, _)).Times(0);
  std::optional<std::vector<float>> refresh_rates =
      drm_gpu_display_manager_->GetSeamlessRefreshRates(wrong_display_id);

  ASSERT_FALSE(refresh_rates);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureDisplaysModeMatching_UnsetMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[0];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset};
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(snapshot->display_id(),
                                          snapshot->origin(), nullptr)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureDisplaysModeMatching_ExistingMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[0];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset};
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), snapshot->native_mode())};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureDisplaysModeMatching_NonExistingMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[0];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset};
  const display::DisplayMode nonmatching_mode(snapshot->native_mode()->size(),
                                              false, 600);
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), &nonmatching_mode)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
      config_requests, flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureDisplaysSeamlessModeMatching_UnsetMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[0];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(snapshot->display_id(),
                                          snapshot->origin(), nullptr)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureDisplaysSeamlessModeMatching_ExistingSeamlessMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[0];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), snapshot->native_mode())};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(
    DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
    ConfigureDisplaysSeamlessModeMatching_ExistingModeFailingSeamlessVerification) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[0];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const display::DisplayMode* failing_mode = snapshot->modes().back().get();
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(snapshot->display_id(),
                                          snapshot->origin(), failing_mode)};

  const uint32_t seamless_test_flags = DRM_MODE_ATOMIC_TEST_ONLY;
  std::vector<display::DisplayConfigurationParams> out_requests;
  // Override mock behavior to fail seamless verification.
  EXPECT_CALL(*mock_drm_device_, CommitProperties(_, seamless_test_flags, _, _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
      config_requests, flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureDisplaysSeamlessModeMatching_NonExistingMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[0];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const display::DisplayMode nonmatching_mode(snapshot->native_mode()->size(),
                                              false, 600);
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), &nonmatching_mode)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
      config_requests, flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureVrrDisplaysModeMatching_UnsetMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset};
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(snapshot->display_id(),
                                          snapshot->origin(), nullptr)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureVrrDisplaysModeMatching_ExistingMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset};
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), snapshot->native_mode())};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureVrrDisplaysModeMatching_NonExistingFasterMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset};
  const display::DisplayMode nonmatching_mode = display::DisplayMode(
      snapshot->native_mode()->size(), false, 600, std::nullopt);
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), &nonmatching_mode)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
      config_requests, flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureVrrDisplaysModeMatching_NonExistingSlowerMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset};
  const display::DisplayMode nonmatching_mode = display::DisplayMode(
      snapshot->native_mode()->size(), false, 51, std::nullopt);
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), &nonmatching_mode)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_NE(config_requests, out_requests);
  ExpectEqualRequestsWithExceptions(config_requests, out_requests);
  EXPECT_EQ(51.00354f, out_requests[0].mode->refresh_rate());
  EXPECT_EQ(24.0f, out_requests[0].mode->vsync_rate_min());
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureVrrDisplaysModeMatching_NonExistingSlowerModeBelowVSyncMin) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset};
  const display::DisplayMode nonmatching_mode = display::DisplayMode(
      snapshot->native_mode()->size(), false, 20, std::nullopt);
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), &nonmatching_mode)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
      config_requests, flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureVrrDisplaysSeamlessModeMatching_UnsetMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(snapshot->display_id(),
                                          snapshot->origin(), nullptr)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureVrrDisplaysSeamlessModeMatching_ExistingSeamlessMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), snapshot->native_mode())};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(
    DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
    ConfigureVrrDisplaysSeamlessModeMatching_ExistingModeFailingSeamlessVerification) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const display::DisplayMode* failing_mode = snapshot->modes().back().get();
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(snapshot->display_id(),
                                          snapshot->origin(), failing_mode)};

  const uint32_t seamless_test_flags = DRM_MODE_ATOMIC_TEST_ONLY;
  std::vector<display::DisplayConfigurationParams> out_requests;
  // Override mock behavior to fail seamless verification.
  EXPECT_CALL(*mock_drm_device_, CommitProperties(_, seamless_test_flags, _, _))
      .Times(3)
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
      config_requests, flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureVrrDisplaysSeamlessModeMatching_NonExistingFasterMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const display::DisplayMode nonmatching_mode = display::DisplayMode(
      snapshot->native_mode()->size(), false, 600, std::nullopt);
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), &nonmatching_mode)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
      config_requests, flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

TEST_F(DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
       ConfigureVrrDisplaysSeamlessModeMatching_NonExistingSlowerMode) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const display::DisplayMode nonmatching_mode = display::DisplayMode(
      snapshot->native_mode()->size(), false, 50, std::nullopt);
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), &nonmatching_mode)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                          flags, out_requests));
  EXPECT_NE(config_requests, out_requests);
  ExpectEqualRequestsWithExceptions(config_requests, out_requests);
  EXPECT_EQ(50.0f, out_requests[0].mode->refresh_rate());
  EXPECT_EQ(24.0f, out_requests[0].mode->vsync_rate_min());
}

TEST_F(
    DrmGpuDisplayManagerGetSeamlessRefreshRateTest,
    ConfigureVrrDisplaysSeamlessModeMatching_NonExistingSlowerModeBelowVSyncMin) {
  const auto snapshots = drm_gpu_display_manager_->GetDisplays();
  ASSERT_FALSE(snapshots.empty());
  const auto& snapshot = snapshots[1];
  const display::ModesetFlags flags = {display::ModesetFlag::kTestModeset,
                                       display::ModesetFlag::kSeamlessModeset};
  const display::DisplayMode nonmatching_mode = display::DisplayMode(
      snapshot->native_mode()->size(), false, 20, std::nullopt);
  const std::vector<display::DisplayConfigurationParams> config_requests = {
      display::DisplayConfigurationParams(
          snapshot->display_id(), snapshot->origin(), &nonmatching_mode)};

  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
      config_requests, flags, out_requests));
  EXPECT_EQ(config_requests, out_requests);
}

using TiledDisplayGetDisplaysTest = DrmGpuDisplayManagerTest;

TEST_F(TiledDisplayGetDisplaysTest, SingleTile) {
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  // Add 1 CRTC
  fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

  // Add one encoder
  auto& encoder = fake_drm->AddEncoder();
  encoder.possible_crtcs = 0b1;

  // Add 1 Connector with a tile blob.
  auto& connector = fake_drm->AddConnector();
  connector.connection = true;
  // One tile, one non-tile modes.
  connector.modes = std::vector<ResolutionAndRefreshRate>{
      {gfx::Size(3840, 4320), 60}, {gfx::Size(1920, 1080), 60}};
  connector.encoders = std::vector<uint32_t>{encoder.id};

  const TileProperty tile_property = {.group_id = 1,
                                      .scale_to_fit_display = true,
                                      .tile_size = gfx::Size(3840, 4320),
                                      .tile_layout = gfx::Size(2, 1),
                                      .location = gfx::Point(0, 0)};
  ScopedDrmPropertyBlob tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, tile_property);
  fake_drm->AddProperty(
      connector.id, {.id = kTileBlobPropId, .value = tile_property_blob->id()});

  fake_drm->InitializeState(/* use_atomic */ true);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(display::features::kTiledDisplaySupport);
  ASSERT_TRUE(display::features::IsTiledDisplaySupportEnabled());

  MovableDisplaySnapshots displays = drm_gpu_display_manager_->GetDisplays();

  // Expect tiled modes to be pruned when not all tiles in a group are present.
  ASSERT_THAT(displays, SizeIs(1));
  EXPECT_THAT(displays[0]->modes(),
              UnorderedElementsAre(
                  Pointee(EqResAndRefresh({gfx::Size(1920, 1080), 60}))));
}

TEST_F(TiledDisplayGetDisplaysTest, AllTilesPresent) {
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  const TileProperty expected_primary_tile_prop = {
      .group_id = 1,
      .scale_to_fit_display = true,
      .tile_size = gfx::Size(3840, 4320),
      .tile_layout = gfx::Size(2, 1),
      .location = gfx::Point(0, 0)};

  // Primary tile at (0,0)
  ScopedDrmPropertyBlob primary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_primary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& primary_encoder = fake_drm->AddEncoder();
    primary_encoder.possible_crtcs = 0b1;

    auto& primary_connector = fake_drm->AddConnector();
    primary_connector.connection = true;
    primary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(3840, 4320), 60}, {gfx::Size(1920, 1080), 60}};
    primary_connector.encoders = std::vector<uint32_t>{primary_encoder.id};
    fake_drm->AddProperty(
        primary_connector.id,
        {.id = kTileBlobPropId, .value = primary_tile_property_blob->id()});
  }

  // Non-primary tile at (0,1) - Identical to the primary tile except for tile
  // location.
  TileProperty expected_nonprimary_tile_prop = expected_primary_tile_prop;
  expected_nonprimary_tile_prop.location = gfx::Point(1, 0);
  ScopedDrmPropertyBlob nonprimary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_nonprimary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& nonprimary_encoder = fake_drm->AddEncoder();
    nonprimary_encoder.possible_crtcs = 0b10;

    auto& nonprimary_connector = fake_drm->AddConnector();
    nonprimary_connector.connection = true;
    nonprimary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(3840, 4320), 60}, {gfx::Size(1920, 1080), 60}};
    nonprimary_connector.encoders =
        std::vector<uint32_t>{nonprimary_encoder.id};
    fake_drm->AddProperty(
        nonprimary_connector.id,
        {.id = kTileBlobPropId, .value = nonprimary_tile_property_blob->id()});
  }

  fake_drm->InitializeState(/* use_atomic */ true);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(display::features::kTiledDisplaySupport);
  ASSERT_TRUE(display::features::IsTiledDisplaySupportEnabled());

  MovableDisplaySnapshots displays = drm_gpu_display_manager_->GetDisplays();

  // Expect the tiled mode to have composited resolution.
  ASSERT_THAT(displays, SizeIs(1));
  EXPECT_THAT(displays[0]->modes(),
              UnorderedElementsAre(
                  Pointee(EqResAndRefresh({gfx::Size(7680, 4320), 60})),
                  Pointee(EqResAndRefresh({gfx::Size(1920, 1080), 60}))));
}

TEST_F(TiledDisplayGetDisplaysTest, PrimaryCanStretchToFit) {
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  // Primary tile at (1,0)
  const TileProperty expected_primary_tile_prop = {
      .group_id = 1,
      .scale_to_fit_display = true,
      .tile_size = gfx::Size(3840, 4320),
      .tile_layout = gfx::Size(2, 1),
      .location = gfx::Point(1, 0)};
  ScopedDrmPropertyBlob primary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_primary_tile_prop);
  {  // Add 1 CRTC
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    // Add an encoder
    auto& primary_encoder = fake_drm->AddEncoder();
    primary_encoder.possible_crtcs = 0b1;

    auto& primary_connector = fake_drm->AddConnector();
    primary_connector.connection = true;
    primary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(3840, 4320), 60}, {gfx::Size(1920, 1080), 60}};
    primary_connector.encoders = std::vector<uint32_t>{primary_encoder.id};
    // |kTiledDisplay| contains tile display DisplayID extension block with
    // stretch to fit behavior.
    primary_connector.edid_blob = std::vector<uint8_t>(
        kTiledDisplay, kTiledDisplay + kTiledDisplayLength);
    fake_drm->AddProperty(
        primary_connector.id,
        {.id = kTileBlobPropId, .value = primary_tile_property_blob->id()});
  }

  // Non-primary tile at (0,0) - does not advertise stretch to fit behavior in
  // EDID.
  TileProperty expected_nonprimary_tile_prop = expected_primary_tile_prop;
  expected_nonprimary_tile_prop.scale_to_fit_display = false;
  expected_nonprimary_tile_prop.location = gfx::Point(0, 0);
  ScopedDrmPropertyBlob nonprimary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_nonprimary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& nonprimary_encoder = fake_drm->AddEncoder();
    nonprimary_encoder.possible_crtcs = 0b10;

    auto& nonprimary_connector = fake_drm->AddConnector();
    nonprimary_connector.connection = true;
    nonprimary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(3840, 4320), 60}, {gfx::Size(1920, 1080), 60}};
    nonprimary_connector.encoders =
        std::vector<uint32_t>{nonprimary_encoder.id};
    // Only TileProperty::scale_to_fit is parsed by the EdidParser, meaning that
    // any EDID ID that doesn't have that bit would suffice for scale_to_fit to
    // be false.
    nonprimary_connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
    fake_drm->AddProperty(
        nonprimary_connector.id,
        {.id = kTileBlobPropId, .value = nonprimary_tile_property_blob->id()});
  }

  fake_drm->InitializeState(/* use_atomic */ true);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(display::features::kTiledDisplaySupport);
  ASSERT_TRUE(display::features::IsTiledDisplaySupportEnabled());

  MovableDisplaySnapshots displays = drm_gpu_display_manager_->GetDisplays();

  ASSERT_THAT(displays, SizeIs(1));

  // Expect the tiled mode to have composited resolution.
  EXPECT_THAT(displays[0]->modes(),
              UnorderedElementsAre(
                  Pointee(EqResAndRefresh({gfx::Size(7680, 4320), 60})),
                  Pointee(EqResAndRefresh({gfx::Size(1920, 1080), 60}))));
}

TEST_F(TiledDisplayGetDisplaysTest, MoreModesInOneTile) {
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  // Primary tile at (0,1) - has 3 modes.
  const TileProperty expected_primary_tile_prop = {
      .group_id = 1,
      .scale_to_fit_display = false,
      .tile_size = gfx::Size(3840, 4320),
      .tile_layout = gfx::Size(2, 1),
      .location = gfx::Point(1, 0)};
  ScopedDrmPropertyBlob primary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_primary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& primary_encoder = fake_drm->AddEncoder();
    primary_encoder.possible_crtcs = 0b1;

    auto& primary_connector = fake_drm->AddConnector();
    primary_connector.connection = true;
    primary_connector.modes =
        std::vector<ResolutionAndRefreshRate>{{gfx::Size(3840, 4320), 60},
                                              {gfx::Size(1920, 1080), 60},
                                              {gfx::Size(2560, 1440), 60}};
    primary_connector.encoders = std::vector<uint32_t>{primary_encoder.id};
    fake_drm->AddProperty(
        primary_connector.id,
        {.id = kTileBlobPropId, .value = primary_tile_property_blob->id()});
  }

  // Non-primary tile at (0,0) - has 1 mode.
  TileProperty expected_nonprimary_tile_prop = expected_primary_tile_prop;
  expected_nonprimary_tile_prop.location = gfx::Point(0, 0);
  ScopedDrmPropertyBlob nonprimary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_nonprimary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& nonprimary_encoder = fake_drm->AddEncoder();
    nonprimary_encoder.possible_crtcs = 0b10;

    auto& nonprimary_connector = fake_drm->AddConnector();
    nonprimary_connector.connection = true;

    nonprimary_connector.modes =
        std::vector<ResolutionAndRefreshRate>{{gfx::Size(3840, 4320), 60}};
    nonprimary_connector.encoders =
        std::vector<uint32_t>{nonprimary_encoder.id};
    nonprimary_connector.properties.push_back(
        {.id = kTileBlobPropId, .value = kTileBlobId + 1});
    fake_drm->AddProperty(
        nonprimary_connector.id,
        {.id = kTileBlobPropId, .value = nonprimary_tile_property_blob->id()});
  }

  fake_drm->InitializeState(/* use_atomic */ true);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(display::features::kTiledDisplaySupport);
  ASSERT_TRUE(display::features::IsTiledDisplaySupportEnabled());

  MovableDisplaySnapshots displays = drm_gpu_display_manager_->GetDisplays();

  ASSERT_THAT(displays, SizeIs(1));

  // Expect the tiled mode to have composited resolution, and to have modes from
  // the primary tile.
  EXPECT_THAT(displays[0]->modes(),
              UnorderedElementsAre(
                  Pointee(EqResAndRefresh({gfx::Size(7680, 4320), 60})),
                  // The non-tile mode in the primary tile (but not in
                  // non-primary) should live.
                  Pointee(EqResAndRefresh({gfx::Size(1920, 1080), 60})),
                  Pointee(EqResAndRefresh({gfx::Size(2560, 1440), 60}))));
}

TEST_F(TiledDisplayGetDisplaysTest, PruneTileModesNotInAllTiles) {
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  // Primary tile at (0,1) - has 2 modes - one of which is 3840x4320@48.
  const TileProperty expected_primary_tile_prop = {
      .group_id = 1,
      .scale_to_fit_display = false,
      .tile_size = gfx::Size(3840, 4320),
      .tile_layout = gfx::Size(2, 1),
      .location = gfx::Point(1, 0)};
  ScopedDrmPropertyBlob primary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_primary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& primary_encoder = fake_drm->AddEncoder();
    primary_encoder.possible_crtcs = 0b1;

    auto& primary_connector = fake_drm->AddConnector();
    primary_connector.connection = true;
    primary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(3840, 4320), 60}, {gfx::Size(3840, 4320), 48}};
    primary_connector.encoders = std::vector<uint32_t>{primary_encoder.id};
    primary_connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
    fake_drm->AddProperty(
        primary_connector.id,
        {.id = kTileBlobPropId, .value = primary_tile_property_blob->id()});
  }

  // Non-primary tile at (0,0) - has 1 mode.
  TileProperty expected_nonprimary_tile_prop = expected_primary_tile_prop;
  expected_nonprimary_tile_prop.location = gfx::Point(0, 0);
  ScopedDrmPropertyBlob nonprimary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_nonprimary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& nonprimary_encoder = fake_drm->AddEncoder();
    nonprimary_encoder.possible_crtcs = 0b10;

    auto& nonprimary_connector = fake_drm->AddConnector();
    nonprimary_connector.connection = true;
    nonprimary_connector.modes =
        std::vector<ResolutionAndRefreshRate>{{gfx::Size(3840, 4320), 60}};
    nonprimary_connector.encoders =
        std::vector<uint32_t>{nonprimary_encoder.id};
    nonprimary_connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
    fake_drm->AddProperty(
        nonprimary_connector.id,
        {.id = kTileBlobPropId, .value = nonprimary_tile_property_blob->id()});
  }

  fake_drm->InitializeState(/* use_atomic */ true);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(display::features::kTiledDisplaySupport);
  ASSERT_TRUE(display::features::IsTiledDisplaySupportEnabled());

  MovableDisplaySnapshots displays = drm_gpu_display_manager_->GetDisplays();

  ASSERT_THAT(displays, SizeIs(1));

  // 3840x4320@48 is pruned since it is not in all the tiles.
  EXPECT_THAT(displays[0]->modes(),
              UnorderedElementsAre(
                  Pointee(EqResAndRefresh({gfx::Size(7680, 4320), 60}))));
}

TEST_F(TiledDisplayGetDisplaysTest, NonTileModeNotPruned) {
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  const TileProperty expected_primary_tile_prop = {
      .group_id = 1,
      .scale_to_fit_display = false,
      .tile_size = gfx::Size(3840, 4320),
      .tile_layout = gfx::Size(2, 1),
      .location = gfx::Point(1, 0)};
  ScopedDrmPropertyBlob primary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_primary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& primary_encoder = fake_drm->AddEncoder();
    primary_encoder.possible_crtcs = 0b1;

    auto& primary_connector = fake_drm->AddConnector();
    primary_connector.connection = true;

    // 7680x4320@30 has the same size as the tile composited mode (the total
    // size of the tiled display), but is not a tile mode (3840x4320).
    primary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(7680, 4320), 30}, {gfx::Size(3840, 4320), 60}};
    primary_connector.encoders = std::vector<uint32_t>{primary_encoder.id};
    primary_connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
    fake_drm->AddProperty(
        primary_connector.id,
        {.id = kTileBlobPropId, .value = primary_tile_property_blob->id()});
  }

  fake_drm->InitializeState(/* use_atomic */ true);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(display::features::kTiledDisplaySupport);
  ASSERT_TRUE(display::features::IsTiledDisplaySupportEnabled());

  MovableDisplaySnapshots displays = drm_gpu_display_manager_->GetDisplays();

  ASSERT_THAT(displays, SizeIs(1));

  // 7680x4320@30 should not be pruned as it is not a tiled-composited mode
  // (3840x4320), so does not get pruned when it is not present in all tiles.
  EXPECT_THAT(displays[0]->modes(),
              UnorderedElementsAre(
                  Pointee(EqResAndRefresh({gfx::Size(7680, 4320), 30}))));
}

TEST_F(TiledDisplayGetDisplaysTest, ConfigureTileDisplayTileCompositeMode) {
  auto fake_drm = AddDrmDevice();
  fake_drm->ResetStateWithAllProperties();

  // Primary tile at (0,0)
  const TileProperty expected_primary_tile_prop = {
      .group_id = 1,
      .scale_to_fit_display = true,
      .tile_size = gfx::Size(3840, 4320),
      .tile_layout = gfx::Size(2, 1),
      .location = gfx::Point(0, 0)};
  ScopedDrmPropertyBlob primary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_primary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

    auto& primary_encoder = fake_drm->AddEncoder();
    primary_encoder.possible_crtcs = 0b1;

    auto& primary_connector = fake_drm->AddConnector();
    primary_connector.connection = true;
    primary_connector.modes = std::vector<ResolutionAndRefreshRate>{
        {gfx::Size(3840, 4320), 60}, {gfx::Size(1920, 1080), 60}};
    primary_connector.encoders = std::vector<uint32_t>{primary_encoder.id};
    primary_connector.edid_blob = std::vector<uint8_t>(
        kTiledDisplay, kTiledDisplay + kTiledDisplayLength);
    fake_drm->AddProperty(
        primary_connector.id,
        {.id = kTileBlobPropId, .value = primary_tile_property_blob->id()});
  }

  // Non-primary tile at (0,1) - Identical to the primary tile except for tile
  // location.
  TileProperty expected_nonprimary_tile_prop = expected_primary_tile_prop;
  expected_nonprimary_tile_prop.location = gfx::Point(1, 0);
  ScopedDrmPropertyBlob nonprimary_tile_property_blob =
      CreateTilePropertyBlob(*fake_drm, expected_nonprimary_tile_prop);
  {
    fake_drm->AddCrtcWithPrimaryAndCursorPlanes();

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
    fake_drm->AddProperty(
        nonprimary_connector.id,
        {.id = kTileBlobPropId, .value = nonprimary_tile_property_blob->id()});
  }

  fake_drm->InitializeState(/* use_atomic */ true);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(display::features::kTiledDisplaySupport);
  ASSERT_TRUE(display::features::IsTiledDisplaySupportEnabled());

  MovableDisplaySnapshots displays = drm_gpu_display_manager_->GetDisplays();
  ASSERT_THAT(displays, testing::SizeIs(1));
  const display::DisplayMode* native_tile_mode = displays[0]->native_mode();
  EXPECT_EQ(native_tile_mode->size(), gfx::Size(7680, 4320));

  std::vector<display::DisplayConfigurationParams> config_requests;
  config_requests.emplace_back(displays[0]->display_id(), displays[0]->origin(),
                               native_tile_mode);
  std::vector<display::DisplayConfigurationParams> out_requests;
  EXPECT_TRUE(drm_gpu_display_manager_->ConfigureDisplays(
      config_requests,
      {display::ModesetFlag::kTestModeset,
       display::ModesetFlag::kCommitModeset},
      out_requests));
  ExpectEqualRequestsWithExceptions(config_requests, out_requests);
}

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_UNITTEST_CC_
