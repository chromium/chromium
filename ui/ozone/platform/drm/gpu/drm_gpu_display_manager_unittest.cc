// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_UNITTEST_CC_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_UNITTEST_CC_

#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_features.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
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
using ::testing::Return;

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

const char kDefaultTestGraphicsCardPattern[] = "/test/dri/card%d";

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
    return drm_gpu_display_manager_->ConfigureDisplays(config_requests,
                                                       modeset_flag);
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

  const uint32_t original_primary_crtc =
      FindDisplayByConnectorId(primary_connector_id)->crtc();
  const uint32_t original_secondary_crtc =
      FindDisplayByConnectorId(secondary_connector_id)->crtc();

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
      .WillOnce(Return(true));

  EXPECT_TRUE(ConfigureDisplays(display_snapshots,
                                {display::ModesetFlag::kTestModeset}));

  histogram_tester_.ExpectBucketCount(kTestOnlyModesetOutcomeTwoDisplays,
                                      TestOnlyModesetOutcome::kFallbackSuccess,
                                      /*count=*/1);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  kTestOnlyModesetFallbacksAttemptedTwoDisplaysMetric),
              UnorderedElementsAre(AllOf(Field(&base::Bucket::min, Gt(0)),
                                         Field(&base::Bucket::count, Eq(1)))));

  // Even if there is a successful fallback configuration, ozone abstractions
  // should not change for test modeset request.
  DrmDisplay* primary_display = FindDisplayByConnectorId(primary_connector_id);
  EXPECT_EQ(primary_display->crtc(), original_primary_crtc);
  DrmDisplay* secondary_display =
      FindDisplayByConnectorId(secondary_connector_id);
  EXPECT_EQ(secondary_display->crtc(), original_secondary_crtc);

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
  EXPECT_EQ(primary_display->crtc(), crtc_1);
  EXPECT_EQ(secondary_display->crtc(), crtc_3);
}

TEST_F(DrmGpuDisplayManagerMockedDeviceTest,
       ConfigureDisplaysFallbackTestSuccessButCommitFailiure) {
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
                                      /*count=*/1);
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
  EXPECT_EQ(primary_display->crtc(), crtc_1);
  DrmDisplay* secondary_display =
      FindDisplayByConnectorId(secondary_connector_id);
  EXPECT_EQ(secondary_display->crtc(), crtc_3);
  histogram_tester_.ExpectBucketCount(kTestOnlyModesetOutcomeTwoDisplays,
                                      TestOnlyModesetOutcome::kFailure,
                                      /*count=*/0);
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
                                      /*count=*/1);
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
                                      /*count=*/1);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  kTestOnlyModesetFallbacksAttemptedTwoDisplaysMetric),
              IsEmpty());
}

TEST_F(DrmGpuDisplayManagerMockedDeviceTest,
       ConfigureDisplaysUseSuccesfulStateForCommit) {
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
        /*count=*/1);
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

    EXPECT_CALL(*mock_drm, CommitProperties)
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));
    EXPECT_FALSE(drm_gpu_display_manager_->ConfigureDisplays(
        failing_request, {display::ModesetFlag::kTestModeset}));

    histogram_tester_.ExpectBucketCount(kTestOnlyModesetOutcomeTwoDisplays,
                                        TestOnlyModesetOutcome::kFailure,
                                        /*count=*/1);
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
  EXPECT_EQ(crtc_1, primary_display->crtc());
  DrmDisplay* secondary_display =
      FindDisplayByConnectorId(secondary_connector_id);
  EXPECT_EQ(crtc_3, secondary_display->crtc());
}

// DrmGpuDisplayManagerGetSeamlessRefreshRateTest is a test fixture for tests
// related to testing seamless refresh rates.
class DrmGpuDisplayManagerGetSeamlessRefreshRateTest
    : public DrmGpuDisplayManagerMockedDeviceTest {
 protected:
  void SetUp() override {
    DrmGpuDisplayManagerMockedDeviceTest::SetUp();

    // Create a FakeDrmDevice with state that represents a display with
    // one downclock mode, and some other modes with different resolutions
    // than the first (native) mode.
    fake_drm_device_ = AddDrmDevice();
    fake_drm_device_->ResetStateWithAllProperties();
    fake_drm_device_->AddCrtcWithPrimaryAndCursorPlanes();

    auto& encoder = fake_drm_device_->AddEncoder();
    encoder.possible_crtcs = 0b1;

    // 120 and 60 are seamless refresh rate candidates; 90 and 40 have different
    // sizes and thus are not.
    auto& connector = fake_drm_device_->AddConnector();
    connector.connection = true;
    connector.modes = {
        ResolutionAndRefreshRate{gfx::Size(3840, 2160), 120u},
        ResolutionAndRefreshRate{gfx::Size(3840, 2160), 60u},
        ResolutionAndRefreshRate{gfx::Size(1920, 1080), 90u},
        ResolutionAndRefreshRate{gfx::Size(1920, 1080), 40u},
    };
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob = std::vector<uint8_t>(
        kInternalDisplay, kInternalDisplay + kInternalDisplayLength);
    fake_drm_device_->InitializeState(/* use_atomic */ true);
    mock_drm_device_ = static_cast<MockDrmDevice*>(fake_drm_device_.get());

    // Do an initial configuration of the display.
    auto display_snapshots = drm_gpu_display_manager_->GetDisplays();
    CHECK_EQ(display_snapshots.size(), 1u);
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

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_UNITTEST_CC_
