// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_UNITTEST_CC_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_UNITTEST_CC_

#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"

#include "base/files/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_display.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

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

const std::vector<ResolutionAndRefreshRate> kStandardModes = {
    ResolutionAndRefreshRate{gfx::Size(3840, 2160), 60u},
    ResolutionAndRefreshRate{gfx::Size(3840, 2160), 50u},
    ResolutionAndRefreshRate{gfx::Size(3840, 2160), 30u},
    ResolutionAndRefreshRate{gfx::Size(1920, 1080), 60u},
    ResolutionAndRefreshRate{gfx::Size(1920, 1080), 50u},
    ResolutionAndRefreshRate{gfx::Size(1920, 1080), 30u}};

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
    device_manager_ = nullptr;
    next_drm_device_number_ = 0u;
  }

  // Note: the first device added will be marked as the primary device.
  scoped_refptr<MockDrmDevice> AddAndInitializeDrmDeviceWithState(
      MockDrmDevice::MockDrmState& drm_state,
      bool use_atomic = true) {
    std::string card_path = base::StringPrintf(kDefaultTestGraphicsCardPattern,
                                               next_drm_device_number_++);
    base::FilePath file_path(card_path);

    device_manager_->AddDrmDevice(file_path, base::ScopedFD());
    MockDrmDevice* mock_drm = static_cast<MockDrmDevice*>(
        device_manager_->GetDrmDevices().back().get());
    mock_drm->SetPropertyBlob(MockDrmDevice::AllocateInFormatsBlob(
        kInFormatsBlobIdBase, {DRM_FORMAT_XRGB8888}, {}));
    mock_drm->InitializeState(drm_state, use_atomic);
    return scoped_refptr<MockDrmDevice>(mock_drm);
  }

  void AddCrtcAndPlanes(MockDrmDevice::MockDrmState& drm_state,
                        size_t num_of_planes = 1u) {
    const auto& crtc = drm_state.AddCrtc();
    for (size_t i = 0; i < num_of_planes; ++i) {
      drm_state.AddPlane(crtc.id, DRM_PLANE_TYPE_PRIMARY);
      for (size_t j = 0; j < num_of_planes - 1; ++j) {
        drm_state.AddPlane(crtc.id, DRM_PLANE_TYPE_OVERLAY);
      }
      drm_state.AddPlane(crtc.id, DRM_PLANE_TYPE_CURSOR);
    }
  }

  bool ConfigureDisplays(const MovableDisplaySnapshots& display_snapshots) {
    std::vector<display::DisplayConfigurationParams> config_requests;
    for (const auto& snapshot : display_snapshots) {
      config_requests.emplace_back(snapshot->display_id(), snapshot->origin(),
                                   snapshot->native_mode());
    }
    return drm_gpu_display_manager_->ConfigureDisplays(
        config_requests, display::kTestModeset | display::kCommitModeset);
  }

  DrmDisplay* FindDisplay(int64_t display_id) {
    return drm_gpu_display_manager_->FindDisplay(display_id);
  }

  size_t next_drm_device_number_ = 0u;

  std::unique_ptr<DrmDeviceManager> device_manager_;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<DrmGpuDisplayManager> drm_gpu_display_manager_;
};

TEST_F(DrmGpuDisplayManagerTest, CapOutOnMaxDrmDeviceCount) {
  // Add |kMaxDrmCount| + 1 DRM devices, each with one active display.
  for (size_t i = 0; i < kMaxDrmCount + 1; ++i) {
    auto drm_state =
        MockDrmDevice::MockDrmState::CreateStateWithAllProperties();

    // Add 1 CRTC
    AddCrtcAndPlanes(drm_state);

    // Add one encoder
    auto& encoder = drm_state.AddEncoder();
    encoder.possible_crtcs = 0b1;

    // Add 1 Connector
    auto& connector = drm_state.AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{kStandardModes[0]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);

    AddAndInitializeDrmDeviceWithState(drm_state);
  }

  ASSERT_EQ(drm_gpu_display_manager_->GetDisplays().size(), kMaxDrmCount);
}

TEST_F(DrmGpuDisplayManagerTest, CapOutOnMaxConnectorCount) {
  // One DRM device.
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithAllProperties();

  // Add |kMaxDrmConnectors| + 1 connector, each with one active display.
  for (size_t i = 0; i < kMaxDrmConnectors + 1; ++i) {
    // Add 1 CRTC
    AddCrtcAndPlanes(drm_state);

    // Add one encoder
    auto& encoder = drm_state.AddEncoder();
    encoder.possible_crtcs = 1 << i;

    // Add 1 Connector
    auto& connector = drm_state.AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{kStandardModes[0]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
  }
  AddAndInitializeDrmDeviceWithState(drm_state);

  ASSERT_EQ(drm_gpu_display_manager_->GetDisplays().size(), kMaxDrmConnectors);
}

// TODO(crbug.com/1431767): Re-enable this test
#if defined(LEAK_SANITIZER)
#define MAYBE_FindAndConfigureDisplaysOnSameDrmDevice \
  DISABLED_FindAndConfigureDisplaysOnSameDrmDevice
#else
#define MAYBE_FindAndConfigureDisplaysOnSameDrmDevice \
  FindAndConfigureDisplaysOnSameDrmDevice
#endif
TEST_F(DrmGpuDisplayManagerTest,
       MAYBE_FindAndConfigureDisplaysOnSameDrmDevice) {
  // One DRM device.
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithAllProperties();

  // Add 3 connectors, each with one active display.
  for (size_t i = 0; i < 3; ++i) {
    AddCrtcAndPlanes(drm_state);

    auto& encoder = drm_state.AddEncoder();
    encoder.possible_crtcs = 1 << i;

    auto& connector = drm_state.AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        kStandardModes[i % kStandardModes.size()]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
  }
  scoped_refptr<MockDrmDevice> drm =
      AddAndInitializeDrmDeviceWithState(drm_state);

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
  ASSERT_TRUE(ConfigureDisplays(display_snapshots));
}

// This case tests scenarios in which a display ID is searched across multiple
// DRM devices, such as in DisplayLink hubs.
// TODO(crbug.com/1431767): Re-enable this test
#if defined(LEAK_SANITIZER)
#define MAYBE_FindAndConfigureDisplaysAcrossDifferentDrmDevices \
  DISABLED_FindAndConfigureDisplaysAcrossDifferentDrmDevices
#else
#define MAYBE_FindAndConfigureDisplaysAcrossDifferentDrmDevices \
  FindAndConfigureDisplaysAcrossDifferentDrmDevices
#endif
TEST_F(DrmGpuDisplayManagerTest,
       MAYBE_FindAndConfigureDisplaysAcrossDifferentDrmDevices) {
  // Add 3 DRM devices, each with one active display.
  for (size_t i = 0; i < 3; ++i) {
    auto drm_state =
        MockDrmDevice::MockDrmState::CreateStateWithAllProperties();

    AddCrtcAndPlanes(drm_state);

    auto& encoder = drm_state.AddEncoder();
    encoder.possible_crtcs = 0b1;

    auto& connector = drm_state.AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{
        kStandardModes[i % kStandardModes.size()]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);

    AddAndInitializeDrmDeviceWithState(drm_state);
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
  ASSERT_TRUE(ConfigureDisplays(display_snapshots));
}

// TODO(crbug.com/1431767): Re-enable this test
#if defined(LEAK_SANITIZER)
#define MAYBE_OriginsPersistThroughSimilarExtendedModeConfigurations \
  DISABLED_OriginsPersistThroughSimilarExtendedModeConfigurations
#else
#define MAYBE_OriginsPersistThroughSimilarExtendedModeConfigurations \
  OriginsPersistThroughSimilarExtendedModeConfigurations
#endif
TEST_F(DrmGpuDisplayManagerTest,
       MAYBE_OriginsPersistThroughSimilarExtendedModeConfigurations) {
  // One DRM device.
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithAllProperties();

  // Add three connectors, each with one active display.
  for (size_t i = 0; i < 3; ++i) {
    // Add 1 CRTC
    AddCrtcAndPlanes(drm_state);

    // Add one encoder
    auto& encoder = drm_state.AddEncoder();
    encoder.possible_crtcs = 1 << i;

    // Add 1 Connector
    auto& connector = drm_state.AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{kStandardModes[0]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob =
        std::vector<uint8_t>(kHPz32x, kHPz32x + kHPz32xLength);
  }
  AddAndInitializeDrmDeviceWithState(drm_state);

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

  ASSERT_TRUE(ConfigureDisplays(display_snapshots));

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

  ASSERT_TRUE(ConfigureDisplays(display_snapshots));
}

TEST_F(DrmGpuDisplayManagerTest, TestEdidIdConflictResolution) {
  // One DRM device.
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithAllProperties();

  // First, add the internal display.
  {
    AddCrtcAndPlanes(drm_state);

    auto& encoder = drm_state.AddEncoder();
    encoder.possible_crtcs = 0b1;

    auto& connector = drm_state.AddConnector();
    connector.connection = true;
    connector.modes = std::vector<ResolutionAndRefreshRate>{kStandardModes[3]};
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob = std::vector<uint8_t>(
        kInternalDisplay, kInternalDisplay + kInternalDisplayLength);
  }

  // Next, add two external displays that will produce an EDID-based ID
  // collision, since their EDIDs do not include viable serial numbers.
  {
    AddCrtcAndPlanes(drm_state);

    auto& encoder = drm_state.AddEncoder();
    encoder.possible_crtcs = 0b10;

    auto& connector = drm_state.AddConnector();
    connector.connection = true;
    connector.modes = kStandardModes;
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob = std::vector<uint8_t>(
        kNoSerialNumberDisplay,
        kNoSerialNumberDisplay + kNoSerialNumberDisplayLength);
  }

  {
    AddCrtcAndPlanes(drm_state);

    auto& encoder = drm_state.AddEncoder();
    encoder.possible_crtcs = 0b100;

    auto& connector = drm_state.AddConnector();
    connector.connection = true;
    connector.modes = kStandardModes;
    connector.encoders = std::vector<uint32_t>{encoder.id};
    connector.edid_blob = std::vector<uint8_t>(
        kNoSerialNumberDisplay,
        kNoSerialNumberDisplay + kNoSerialNumberDisplayLength);
  }

  AddAndInitializeDrmDeviceWithState(drm_state);

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

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_DISPLAY_MANAGER_UNITTEST_CC_
