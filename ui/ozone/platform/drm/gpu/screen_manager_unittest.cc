// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/screen_manager.h"

#include <drm_fourcc.h>
#include <stddef.h>
#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

namespace ui {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr drmModeModeInfo ConstructMode(uint16_t hdisplay, uint16_t vdisplay) {
  return {.hdisplay = hdisplay, .vdisplay = vdisplay};
}

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode = ConstructMode(6, 4);

const uint32_t kPrimaryDisplayId = 1;
const uint32_t kSecondaryDisplayId = 2;

Matcher<const CrtcController&> EqualsCrtcConnectorIds(uint32_t crtc,
                                                      uint32_t connector) {
  return AllOf(Property(&CrtcController::crtc, Eq(crtc)),
               Property(&CrtcController::connector, Eq(connector)));
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

std::unique_ptr<HardwareDisplayControllerInfo> GetDisplayInfo(
    uint32_t connector_id,
    uint32_t crtc_id,
    uint8_t index,
    const std::optional<TileProperty>& tile_property = std::nullopt) {
  // Initialize a list of display modes.
  constexpr size_t kNumModes = 5;
  drmModeModeInfo modes[kNumModes] = {
      {.hdisplay = 640, .vdisplay = 400},
      {.hdisplay = 640, .vdisplay = 480},
      {.hdisplay = 800, .vdisplay = 600},
      {.hdisplay = 1024, .vdisplay = 768},
      // Last mode, which should be the largest, is the native mode.
      {.hdisplay = 1920, .vdisplay = 1080}};

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

}  // namespace

class ScreenManagerTest : public testing::Test {
 public:
  struct PlaneState {
    std::vector<uint32_t> formats;
  };

  struct CrtcState {
    std::vector<PlaneState> planes;
  };

  ScreenManagerTest() = default;

  ScreenManagerTest(const ScreenManagerTest&) = delete;
  ScreenManagerTest& operator=(const ScreenManagerTest&) = delete;

  ~ScreenManagerTest() override = default;

  gfx::Rect GetPrimaryBounds() const {
    return gfx::Rect(0, 0, kDefaultMode.hdisplay, kDefaultMode.vdisplay);
  }

  // Secondary is in extended mode, right-of primary.
  gfx::Rect GetSecondaryBounds() const {
    return gfx::Rect(kDefaultMode.hdisplay, 0, kDefaultMode.hdisplay,
                     kDefaultMode.vdisplay);
  }

  void InitializeDrmState(FakeDrmDevice* drm,
                          const std::vector<CrtcState>& crtc_states,
                          bool is_atomic,
                          bool use_modifiers_list = false,
                          const std::vector<PlaneState>& movable_planes = {}) {
    size_t plane_count = crtc_states[0].planes.size();
    for (const auto& crtc_state : crtc_states) {
      ASSERT_EQ(plane_count, crtc_state.planes.size())
          << "FakeDrmDevice::CreateStateWithAllProperties currently expects "
             "the same number of planes per CRTC";
    }

    std::vector<drm_format_modifier> drm_format_modifiers;
    if (use_modifiers_list) {
      for (const auto modifier : supported_modifiers_) {
        drm_format_modifiers.push_back(
            {.formats = 1, .offset = 0, .pad = 0, .modifier = modifier});
      }
    }
    drm->ResetStateWithAllProperties();

    std::vector<uint32_t> crtc_ids;
    for (const auto& crtc_state : crtc_states) {
      uint32_t crtc_id = drm->AddCrtcAndConnector().first.id;
      crtc_ids.push_back(crtc_id);

      for (size_t i = 0; i < crtc_state.planes.size(); ++i) {
        auto in_formats_blob = drm->CreateInFormatsBlob(
            crtc_state.planes[i].formats, drm_format_modifiers);

        auto& plane = drm->AddPlane(
            crtc_id, i == 0 ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY);
        drm->AddProperty(
            plane.id, {.id = kInFormatsPropId, .value = in_formats_blob->id()});
      }
    }
    for (const auto& movable_plane : movable_planes) {
      auto in_formats_blob =
          drm->CreateInFormatsBlob(movable_plane.formats, drm_format_modifiers);
      auto& plane = drm->AddPlane(crtc_ids, DRM_PLANE_TYPE_OVERLAY);
      drm->AddProperty(
          plane.id, {.id = kInFormatsPropId, .value = in_formats_blob->id()});
    }

    drm->SetModifiersOverhead(modifiers_overhead_);
    drm->InitializeState(is_atomic);
  }

  void AddPlaneToCrtc(uint32_t crtc_id, uint32_t plane_type) {
    auto in_formats_blob = drm_->CreateInFormatsBlob({DRM_FORMAT_XRGB8888}, {});
    auto& plane = drm_->AddPlane(crtc_id, plane_type);
    drm_->AddProperty(plane.id,
                      {.id = kInFormatsPropId, .value = in_formats_blob->id()});
  }

  void InitializeDrmStateWithDefault(FakeDrmDevice* drm,
                                     bool is_atomic,
                                     bool use_modifiers_list = false) {
    // A Sample of CRTC states.
    std::vector<CrtcState> crtc_states = {
        {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
        {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}}};
    InitializeDrmState(drm, crtc_states, is_atomic, use_modifiers_list);
  }

  void SetUp() override {
    auto gbm = std::make_unique<MockGbmDevice>();
    supported_modifiers_ = gbm->GetSupportedModifiers();
    drm_ = new FakeDrmDevice(std::move(gbm));
    drm2_ = new FakeDrmDevice(std::make_unique<MockGbmDevice>());
    device_manager_ = std::make_unique<DrmDeviceManager>(nullptr);
    screen_manager_ = std::make_unique<ScreenManager>();
  }

  void TearDown() override {
    screen_manager_.reset();
    drm_->ResetPlaneManagerForTesting();
    drm2_->ResetPlaneManagerForTesting();
    drm_ = nullptr;
    drm2_ = nullptr;
  }

  scoped_refptr<DrmFramebuffer> CreateBuffer(uint32_t format,
                                             const gfx::Size& size) {
    return CreateBufferWithModifier(format, DRM_FORMAT_MOD_NONE, size);
  }

  scoped_refptr<DrmFramebuffer> CreateBufferWithModifier(
      uint32_t format,
      uint64_t format_modifier,
      const gfx::Size& size) {
    std::vector<uint64_t> modifiers;
    if (format_modifier != DRM_FORMAT_MOD_NONE) {
      modifiers.push_back(format_modifier);
    }
    auto buffer = drm_->gbm_device()->CreateBufferWithModifiers(
        format, size, GBM_BO_USE_SCANOUT, modifiers);
    return DrmFramebuffer::AddFramebuffer(drm_, buffer.get(), size, modifiers);
  }

 protected:
  scoped_refptr<FakeDrmDevice> drm_;
  scoped_refptr<FakeDrmDevice> drm2_;
  std::unique_ptr<DrmDeviceManager> device_manager_;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::vector<uint64_t> supported_modifiers_;
  base::flat_map<uint64_t /*modifier*/, int /*overhead*/> modifiers_overhead_{
      {DRM_FORMAT_MOD_LINEAR, 1},
      {I915_FORMAT_MOD_Yf_TILED_CCS, 100}};
};

TEST_F(ScreenManagerTest, CheckWithNoControllers) {
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_EQ(drm_->get_test_modeset_count(), 0);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 0);
  EXPECT_EQ(drm_->get_commit_count(), 0);
}

TEST_F(ScreenManagerTest, CheckWithValidController) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});
  EXPECT_EQ(drm_->get_test_modeset_count(), 1);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 1);

  HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());

  EXPECT_TRUE(controller);
  EXPECT_TRUE(controller->HasCrtc(drm_, crtc_id));
}

TEST_F(ScreenManagerTest, CheckWithSeamlessModeset) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kCommitModeset,
                              display::ModesetFlag::kSeamlessModeset});

  EXPECT_EQ(drm_->get_commit_modeset_count(), 0);
  EXPECT_EQ(drm_->get_seamless_modeset_count(), 1);
}

TEST_F(ScreenManagerTest, CheckWithInvalidBounds) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckForSecondValidController) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_EQ(drm_->get_test_modeset_count(), 1);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 1);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckMultipleDisplaysWithinModifiersLimit) {
  int max_supported_displays_with_modifier = 2;
  drm_->SetSystemLimitOfModifiers(
      modifiers_overhead_[I915_FORMAT_MOD_Yf_TILED_CCS] *
      max_supported_displays_with_modifier);

  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true,
                                /*use_modifiers_list=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  EXPECT_EQ(drm_->get_test_modeset_count(), 1);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 1);
}

TEST_F(ScreenManagerTest, CheckMultipleDisplaysOutsideModifiersLimit) {
  int max_supported_displays_with_modifier = 2;
  drm_->SetSystemLimitOfModifiers(modifiers_overhead_[DRM_FORMAT_MOD_LINEAR] *
                                  max_supported_displays_with_modifier);

  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true,
                                /*use_modifiers_list=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  // Testing for a failed test-modeset with modifiers + a fallback to Linear
  // Modifier and a modeset commit.
  EXPECT_EQ(drm_->get_test_modeset_count(), 2);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 1);
}

TEST_F(ScreenManagerTest, CheckDisplaysWith0Limit) {
  drm_->SetSystemLimitOfModifiers(0);

  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true,
                                /*use_modifiers_list=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  EXPECT_FALSE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  // Testing for a failed test-modeset with modifiers + failed test-modeset with
  // Linear Modifier and no modeset due to failed tests.
  EXPECT_EQ(drm_->get_test_modeset_count(), 2);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 0);
}

TEST_F(ScreenManagerTest, CheckControllerAfterItIsRemoved) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));

  ScreenManager::CrtcsWithDrmList controllers_to_remove;
  controllers_to_remove.emplace_back(crtc_id, drm_);
  screen_manager_->RemoveDisplayControllers(controllers_to_remove);
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
}

TEST_F(ScreenManagerTest, CheckControllerAfterDisabled) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);

  // Enable
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, crtc_id, connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }

  int test_modeset_count_before_disable = drm_->get_test_modeset_count();
  int commit_modeset_count_before_disable = drm_->get_commit_modeset_count();
  // Disable
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_, crtc_id,
                                     connector_id, gfx::Point(), nullptr);
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_EQ(drm_->get_test_modeset_count(),
            test_modeset_count_before_disable + 1);
  EXPECT_EQ(drm_->get_commit_modeset_count(),
            commit_modeset_count_before_disable + 1);

  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
}

TEST_F(ScreenManagerTest, CheckMultipleControllersAfterBeingRemoved) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  int modeset_count_after_enable = drm_->get_commit_modeset_count();
  ScreenManager::CrtcsWithDrmList controllers_to_remove;
  controllers_to_remove.emplace_back(primary_crtc_id, drm_);
  controllers_to_remove.emplace_back(secondary_crtc_id, drm_);
  screen_manager_->RemoveDisplayControllers(controllers_to_remove);

  // Removed displays are disabled in only 1 modeset commit.
  EXPECT_EQ(drm_->get_commit_modeset_count(), modeset_count_after_enable + 1);

  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckMultipleControllersAfterBeingDisabled) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);
  // Enable
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
        GetSecondaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }

  int test_modeset_count_before_disable = drm_->get_test_modeset_count();
  int commit_modeset_count_before_disable = drm_->get_commit_modeset_count();
  // Disable
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_, primary_crtc_id,
                                     primary_connector_id, gfx::Point(),
                                     nullptr);
  controllers_to_enable.emplace_back(kSecondaryDisplayId, drm_,
                                     secondary_crtc_id, secondary_connector_id,
                                     gfx::Point(), nullptr);
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_EQ(drm_->get_test_modeset_count(),
            test_modeset_count_before_disable + 1);
  EXPECT_EQ(drm_->get_commit_modeset_count(),
            commit_modeset_count_before_disable + 1);

  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckDuplicateConfiguration) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic*/ false);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  uint32_t framebuffer = drm_->current_framebuffer();

  controllers_to_enable.clear();
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  // Should not hold onto buffers.
  EXPECT_NE(framebuffer, drm_->current_framebuffer());

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckChangingMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);

  // Modeset with default mode.
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, crtc_id, connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }
  auto new_mode = kDefaultMode;
  new_mode.vdisplay = new_mode.vdisplay++;
  // Modeset with a changed Mode.
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, crtc_id, connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(new_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }

  gfx::Rect new_bounds(0, 0, new_mode.hdisplay, new_mode.vdisplay);
  EXPECT_TRUE(screen_manager_->GetDisplayController(new_bounds));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
  drmModeModeInfo mode = screen_manager_->GetDisplayController(new_bounds)
                             ->crtc_controllers()[0]
                             ->mode();
  EXPECT_EQ(new_mode.vdisplay, mode.vdisplay);
  EXPECT_EQ(new_mode.hdisplay, mode.hdisplay);
}

TEST_F(ScreenManagerTest, CheckChangingVrrState) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);

  // Modeset with default VRR state.
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, crtc_id, connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode), /*enable_vrr=*/false);
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});

    const HardwareDisplayController* hdc =
        screen_manager_->GetDisplayController(GetPrimaryBounds());
    EXPECT_EQ(0U, hdc->crtc_controllers()[0]->vrr_enabled());
  }

  // Modeset with a changed VRR state.
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, crtc_id, connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode), /*enable_vrr=*/true);
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});

    const HardwareDisplayController* hdc =
        screen_manager_->GetDisplayController(GetPrimaryBounds());
    EXPECT_EQ(1U, hdc->crtc_controllers()[0]->vrr_enabled());
  }
}

TEST_F(ScreenManagerTest, CheckForControllersInMirroredMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckMirrorModeTransitions) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}}};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  controllers_to_enable.clear();
  drmModeModeInfo transition1_primary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(transition1_primary_mode));
  drmModeModeInfo transition1_secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(transition1_secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  controllers_to_enable.clear();
  drmModeModeInfo transition2_primary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(transition2_primary_mode));
  drmModeModeInfo transition2_secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(transition2_secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

// Make sure we're using each display's mode when doing mirror mode otherwise
// the timings may be off.
TEST_F(ScreenManagerTest, CheckMirrorModeModesettingWithDisplaysMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  // Copy the mode and use the copy so we can tell what mode the CRTC was
  // configured with. The clock value is modified so we can tell which mode is
  // being used.
  drmModeModeInfo secondary_mode = kDefaultMode;
  secondary_mode.clock++;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  for (const auto& crtc : controller->crtc_controllers()) {
    if (crtc->crtc() == primary_crtc_id)
      EXPECT_EQ(kDefaultMode.clock, crtc->mode().clock);
    else if (crtc->crtc() == secondary_crtc_id)
      EXPECT_EQ(secondary_mode.clock, crtc->mode().clock);
    else
      NOTREACHED_IN_MIGRATION();
  }
}

TEST_F(ScreenManagerTest, MonitorGoneInMirrorMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  ScreenManager::CrtcsWithDrmList controllers_to_remove;
  controllers_to_remove.emplace_back(secondary_crtc_id, drm_);
  screen_manager_->RemoveDisplayControllers(controllers_to_remove);

  HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  EXPECT_TRUE(controller->HasCrtc(drm_, primary_crtc_id));
  EXPECT_FALSE(controller->HasCrtc(drm_, secondary_crtc_id));
}

TEST_F(ScreenManagerTest, MonitorDisabledInMirrorMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  // Enable in Mirror Mode.
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    drmModeModeInfo secondary_mode = kDefaultMode;
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }

  // Disable display Controller.
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(0, drm_, secondary_crtc_id, 0,
                                     gfx::Point(), nullptr);
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  EXPECT_TRUE(controller->HasCrtc(drm_, primary_crtc_id));
  EXPECT_FALSE(controller->HasCrtc(drm_, secondary_crtc_id));
}

TEST_F(ScreenManagerTest, DoNotEnterMirrorModeUnlessSameBounds) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);

  // Configure displays in extended mode.
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    drmModeModeInfo secondary_mode = kDefaultMode;
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
        GetSecondaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }

  {
    auto new_mode = std::make_unique<drmModeModeInfo>(kDefaultMode);
    new_mode->vdisplay = 10;
    // Shouldn't enter mirror mode unless the display bounds are the same.
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
        GetPrimaryBounds().origin(), std::move(new_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }

  EXPECT_FALSE(
      screen_manager_->GetDisplayController(GetPrimaryBounds())->IsMirrored());
}

TEST_F(ScreenManagerTest, ReuseFramebufferIfDisabledThenReEnabled) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  uint32_t framebuffer = drm_->current_framebuffer();

  controllers_to_enable.clear();
  // Disable display controller.
  controllers_to_enable.emplace_back(0, drm_, crtc_id, 0, gfx::Point(),
                                     nullptr);
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});
  EXPECT_EQ(0u, drm_->current_framebuffer());

  controllers_to_enable.clear();
  drmModeModeInfo reenable_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(reenable_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  // Buffers are released when disabled.
  EXPECT_NE(framebuffer, drm_->current_framebuffer());
}

TEST_F(ScreenManagerTest, CheckMirrorModeAfterBeginReEnabled) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;
  uint32_t secondary_crtc_id = drm_->crtc_property(1).id;
  uint32_t secondary_connector_id = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  screen_manager_->AddDisplayController(drm_, secondary_crtc_id,
                                        secondary_connector_id);
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    drmModeModeInfo secondary_mode = kDefaultMode;
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, secondary_crtc_id, secondary_connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(0, drm_, primary_crtc_id, 0,
                                       gfx::Point(), nullptr);
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }

  HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(controller->IsMirrored());

  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    drmModeModeInfo reenable_mode = kDefaultMode;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, primary_crtc_id, primary_connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(reenable_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }

  EXPECT_TRUE(controller);
  EXPECT_TRUE(controller->IsMirrored());
}

TEST_F(ScreenManagerTest, ConfigureOnDifferentDrmDevices) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}}};
  InitializeDrmState(drm2_.get(), crtc_states, /*is_atomic=*/false);

  uint32_t drm_1_crtc_1 = drm_->crtc_property(0).id;
  uint32_t drm_1_connector_1 = drm_->connector_property(0).id;
  uint32_t drm_2_crtc_1 = drm2_->crtc_property(0).id;
  uint32_t drm_2_connector_1 = drm2_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, drm_1_crtc_1, drm_1_connector_1);
  screen_manager_->AddDisplayController(drm2_, drm_2_crtc_1, drm_2_connector_1);
  screen_manager_->AddDisplayController(drm2_, drm_2_crtc_1, drm_2_connector_1);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, drm_1_crtc_1, drm_1_connector_1,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm2_, drm_2_crtc_1, drm_2_connector_1,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  drmModeModeInfo secondary_mode2 = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId + 1, drm2_, drm_2_crtc_1, drm_2_connector_1,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode2));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_EQ(drm_->get_set_crtc_call_count(), 1);
  EXPECT_EQ(drm2_->get_set_crtc_call_count(), 2);
}

// Tests that two devices that may share the same object IDs are
// treated independently.
TEST_F(ScreenManagerTest,
       CheckProperConfigurationWithDifferentDeviceAndSameCrtc) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  InitializeDrmStateWithDefault(drm2_.get(), /*is_atomic=*/true);
  uint32_t drm2_crtc_id = drm2_->crtc_property(0).id;
  uint32_t drm2_connector_id = drm2_->connector_property(0).id;

  ASSERT_EQ(crtc_id, drm2_crtc_id);
  ASSERT_EQ(connector_id, drm2_connector_id);

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  screen_manager_->AddDisplayController(drm2_, crtc_id, connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm2_, crtc_id, connector_id,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  HardwareDisplayController* controller1 =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  HardwareDisplayController* controller2 =
      screen_manager_->GetDisplayController(GetSecondaryBounds());

  EXPECT_NE(controller1, controller2);
  EXPECT_EQ(drm_, controller1->crtc_controllers()[0]->drm());
  EXPECT_EQ(drm2_, controller2->crtc_controllers()[0]->drm());
}

TEST_F(ScreenManagerTest, CheckControllerToWindowMappingWithSameBounds) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_TRUE(screen_manager_->GetWindow(1)->GetController());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, CheckControllerToWindowMappingWithDifferentBounds) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  gfx::Rect new_bounds = GetPrimaryBounds();
  new_bounds.Inset(gfx::Insets::TLBR(0, 0, 1, 1));
  window->SetBounds(new_bounds);
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_FALSE(screen_manager_->GetWindow(1)->GetController());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest,
       CheckControllerToWindowMappingWithOverlappingWindows) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  const size_t kWindowCount = 2;
  for (size_t i = 1; i < kWindowCount + 1; ++i) {
    std::unique_ptr<DrmWindow> window(
        new DrmWindow(i, device_manager_.get(), screen_manager_.get()));
    window->Initialize();
    window->SetBounds(GetPrimaryBounds());
    screen_manager_->AddWindow(i, std::move(window));
  }

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  bool window1_has_controller = screen_manager_->GetWindow(1)->GetController();
  bool window2_has_controller = screen_manager_->GetWindow(2)->GetController();
  // Only one of the windows can have a controller.
  EXPECT_TRUE(window1_has_controller ^ window2_has_controller);

  for (size_t i = 1; i < kWindowCount + 1; ++i) {
    std::unique_ptr<DrmWindow> window = screen_manager_->RemoveWindow(i);
    window->Shutdown();
  }
}

TEST_F(ScreenManagerTest, ShouldDissociateWindowOnControllerRemoval) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  gfx::AcceleratedWidget window_id = 1;
  std::unique_ptr<DrmWindow> window(
      new DrmWindow(window_id, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(window_id, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_TRUE(screen_manager_->GetWindow(window_id)->GetController());

  ScreenManager::CrtcsWithDrmList controllers_to_remove;
  controllers_to_remove.emplace_back(crtc_id, drm_);
  screen_manager_->RemoveDisplayControllers(controllers_to_remove);

  EXPECT_FALSE(screen_manager_->GetWindow(window_id)->GetController());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, EnableControllerWhenWindowHasNoBuffer) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_TRUE(screen_manager_->GetWindow(1)->GetController());
  // There is a buffer after initial config.
  uint32_t framebuffer = drm_->current_framebuffer();
  EXPECT_NE(0U, framebuffer);

  controllers_to_enable.clear();
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  // There is a new buffer after we configured with the same mode but no
  // pending frames on the window.
  EXPECT_NE(framebuffer, drm_->current_framebuffer());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, EnableControllerWhenWindowHasBuffer) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(buffer));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  EXPECT_EQ(buffer->opaque_framebuffer_id(), drm_->current_framebuffer());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

// See crbug.com/868010
TEST_F(ScreenManagerTest, DISABLED_RejectBufferWithIncompatibleModifiers) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  auto buffer = CreateBufferWithModifier(
      DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED, GetPrimaryBounds().size());
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(buffer));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  // ScreenManager::GetModesetBuffer (called to get a buffer to
  // modeset the new controller) should reject the buffer with
  // I915_FORMAT_MOD_X_TILED modifier we created above and the two
  // framebuffer IDs should be different.
  EXPECT_NE(buffer->framebuffer_id(), drm_->current_framebuffer());
  EXPECT_NE(buffer->opaque_framebuffer_id(), drm_->current_framebuffer());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ConfigureDisplayControllerShouldModesetOnce) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  // When a window that had no controller becomes associated with a new
  // controller, expect the crtc to be modeset once.
  EXPECT_EQ(drm_->get_set_crtc_call_count(), 1);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ShouldNotHardwareMirrorDifferentDrmDevices) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  InitializeDrmStateWithDefault(drm2_.get(), /*is_atomic=*/true);

  uint32_t crtc1 = drm_->crtc_property(0).id;
  uint32_t connector1 = drm_->connector_property(0).id;
  uint32_t crtc2 = drm_->crtc_property(1).id;
  uint32_t connector2 = drm_->connector_property(1).id;

  drmModeModeInfo k1920x1080Screen = ConstructMode(1920, 1080);

  // Two displays on different DRM devices must not join a mirror pair.
  //
  // However, they may have the same bounds in a transitional state.
  //
  // This scenario generates the same sequence of display configuration
  // events as a panther (kernel 3.8.11) chromebox with two identical
  // 1080p displays connected, one of them via a DisplayLink adapter.

  // Both displays connect at startup.
  {
    auto window1 = std::make_unique<DrmWindow>(1, device_manager_.get(),
                                               screen_manager_.get());
    window1->Initialize();
    screen_manager_->AddWindow(1, std::move(window1));
    screen_manager_->GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager_->AddDisplayController(drm_, crtc1, connector1);
    screen_manager_->AddDisplayController(drm2_, crtc2, connector2);

    std::vector<ControllerConfigParams> controllers_to_enable;
    std::unique_ptr<drmModeModeInfo> primary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    std::unique_ptr<drmModeModeInfo> secondary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_, crtc1,
                                       connector1, gfx::Point(0, 0),
                                       std::move(primary_mode));
    controllers_to_enable.emplace_back(kSecondaryDisplayId, drm2_, crtc2,
                                       connector2, gfx::Point(0, 1140),
                                       std::move(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});

    auto window2 = std::make_unique<DrmWindow>(2, device_manager_.get(),
                                               screen_manager_.get());
    window2->Initialize();
    screen_manager_->AddWindow(2, std::move(window2));
    screen_manager_->GetWindow(2)->SetBounds(gfx::Rect(0, 1140, 1920, 1080));
  }

  // Displays are stacked vertically, window per display.
  {
    HardwareDisplayController* controller1 =
        screen_manager_->GetWindow(1)->GetController();
    HardwareDisplayController* controller2 =
        screen_manager_->GetWindow(2)->GetController();
    EXPECT_NE(controller1, controller2);
    EXPECT_TRUE(controller1->HasCrtc(drm_, crtc1));
    EXPECT_TRUE(controller2->HasCrtc(drm2_, crtc2));
  }

  // Disconnect first display. Second display moves to origin.
  {
    ScreenManager::CrtcsWithDrmList controllers_to_remove;
    controllers_to_remove.emplace_back(crtc1, drm_);
    screen_manager_->RemoveDisplayControllers(controllers_to_remove);

    std::vector<ControllerConfigParams> controllers_to_enable;
    std::unique_ptr<drmModeModeInfo> secondary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    controllers_to_enable.emplace_back(kSecondaryDisplayId, drm2_, crtc2,
                                       connector2, gfx::Point(0, 0),
                                       std::move(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});

    screen_manager_->GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager_->GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager_->RemoveWindow(2)->Shutdown();
  }

  // Reconnect first display. Original configuration restored.
  {
    screen_manager_->AddDisplayController(drm_, crtc1, connector1);
    std::vector<ControllerConfigParams> controllers_to_enable;
    std::unique_ptr<drmModeModeInfo> primary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_, crtc1,
                                       connector1, gfx::Point(0, 0),
                                       std::move(primary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
    // At this point, both displays are in the same location.
    {
      HardwareDisplayController* controller =
          screen_manager_->GetWindow(1)->GetController();
      EXPECT_FALSE(controller->IsMirrored());
      // We don't really care which crtc it has, but it should have just
      EXPECT_EQ(1U, controller->crtc_controllers().size());
      EXPECT_TRUE(controller->HasCrtc(drm_, crtc1) ||
                  controller->HasCrtc(drm2_, crtc2));
    }
    controllers_to_enable.clear();
    std::unique_ptr<drmModeModeInfo> secondary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    controllers_to_enable.emplace_back(kSecondaryDisplayId, drm2_, crtc2,
                                       connector2, gfx::Point(0, 1140),
                                       std::move(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
    auto window3 = std::make_unique<DrmWindow>(3, device_manager_.get(),
                                               screen_manager_.get());
    window3->Initialize();
    screen_manager_->AddWindow(3, std::move(window3));
    screen_manager_->GetWindow(3)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager_->GetWindow(1)->SetBounds(gfx::Rect(0, 1140, 1920, 1080));
    screen_manager_->GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager_->GetWindow(3)->SetBounds(gfx::Rect(0, 1140, 1920, 1080));
  }

  // Everything is restored.
  {
    HardwareDisplayController* controller1 =
        screen_manager_->GetWindow(1)->GetController();
    HardwareDisplayController* controller3 =
        screen_manager_->GetWindow(3)->GetController();
    EXPECT_NE(controller1, controller3);
    EXPECT_TRUE(controller1->HasCrtc(drm_, crtc1));
    EXPECT_TRUE(controller3->HasCrtc(drm2_, crtc2));
  }

  // Cleanup.
  screen_manager_->RemoveWindow(1)->Shutdown();
  screen_manager_->RemoveWindow(3)->Shutdown();
}

// crbug.com/888553
TEST_F(ScreenManagerTest, ShouldNotUnbindFramebufferOnJoiningMirror) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);

  uint32_t crtc1 = drm_->crtc_property(0).id;
  uint32_t connector1 = drm_->connector_property(0).id;
  uint32_t crtc2 = drm_->crtc_property(1).id;
  uint32_t connector2 = drm_->connector_property(1).id;

  constexpr drmModeModeInfo k1080p60Screen = {
      .clock = 148500,
      .hdisplay = 1920,
      .hsync_start = 2008,
      .hsync_end = 2052,
      .htotal = 2200,
      .hskew = 0,
      .vdisplay = 1080,
      .vsync_start = 1084,
      .vsync_end = 1089,
      .vtotal = 1125,
      .vscan = 0,
      .vrefresh = 60,
      .flags = 0xa,
      .type = 64,
      .name = "1920x1080",
  };

  // Both displays connect at startup.
  {
    auto window1 = std::make_unique<DrmWindow>(1, device_manager_.get(),
                                               screen_manager_.get());
    window1->Initialize();
    screen_manager_->AddWindow(1, std::move(window1));
    screen_manager_->GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager_->AddDisplayController(drm_, crtc1, connector1);
    screen_manager_->AddDisplayController(drm_, crtc2, connector2);

    std::vector<ControllerConfigParams> controllers_to_enable;
    std::unique_ptr<drmModeModeInfo> primary_mode =
        std::make_unique<drmModeModeInfo>(k1080p60Screen);
    std::unique_ptr<drmModeModeInfo> secondary_mode =
        std::make_unique<drmModeModeInfo>(k1080p60Screen);
    controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_, crtc1,
                                       connector1, gfx::Point(0, 0),
                                       std::move(primary_mode));
    controllers_to_enable.emplace_back(kSecondaryDisplayId, drm_, crtc2,
                                       connector2, gfx::Point(0, 0),
                                       std::move(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset});
  }

  EXPECT_NE(0u, drm_->GetFramebufferForCrtc(crtc1));
  EXPECT_NE(0u, drm_->GetFramebufferForCrtc(crtc2));

  // Cleanup.
  screen_manager_->RemoveWindow(1)->Shutdown();
}

TEST_F(ScreenManagerTest, DrmFramebufferSequenceIdIncrementingAtModeset) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  scoped_refptr<DrmFramebuffer> pre_modeset_buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  CHECK_EQ(pre_modeset_buffer->modeset_sequence_id_at_allocation(), 0);

  // Successful modeset
  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  {
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, crtc_id, connector_id,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset}));
  }

  scoped_refptr<DrmFramebuffer> first_post_modeset_buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  CHECK_EQ(first_post_modeset_buffer->modeset_sequence_id_at_allocation(), 1);
  CHECK_EQ(pre_modeset_buffer->modeset_sequence_id_at_allocation(), 0);

  // Unsuccessful modeset
  {
    drm_->set_set_crtc_expectation(false);
    std::vector<ControllerConfigParams> controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, crtc_id, connector_id,
        GetSecondaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    ASSERT_FALSE(screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, {display::ModesetFlag::kTestModeset,
                                display::ModesetFlag::kCommitModeset}));
  }

  scoped_refptr<DrmFramebuffer> second_post_modeset_buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  CHECK_EQ(second_post_modeset_buffer->modeset_sequence_id_at_allocation(), 1);
  CHECK_EQ(first_post_modeset_buffer->modeset_sequence_id_at_allocation(), 1);
  CHECK_EQ(pre_modeset_buffer->modeset_sequence_id_at_allocation(), 0);
}

TEST_F(ScreenManagerTest, CloningPlanesOnModeset) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(buffer));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(crtc_id)
                                 .modeset_framebuffers,
                             buffer));

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, CloningMultiplePlanesOnModeset) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888}}}}};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> primary =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  scoped_refptr<DrmFramebuffer> overlay =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(primary));
  planes.push_back(DrmOverlayPlane::TestPlane(overlay));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(crtc_id)
                                 .modeset_framebuffers,
                             primary));
  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(crtc_id)
                                 .modeset_framebuffers,
                             overlay));

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ModesetWithClonedPlanesNoOverlays) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(buffer));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));
  ASSERT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(crtc_id)
                                 .modeset_framebuffers,
                             buffer));

  EXPECT_EQ(drm_->get_test_modeset_count(), 1);
  EXPECT_EQ(drm_->last_planes_committed_count(), 1);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ModesetWithClonedPlanesWithOverlaySucceeding) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888}}}}};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> primary =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  scoped_refptr<DrmFramebuffer> overlay =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(primary));
  planes.push_back(DrmOverlayPlane::TestPlane(overlay));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(crtc_id)
                                 .modeset_framebuffers,
                             primary));
  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(crtc_id)
                                 .modeset_framebuffers,
                             overlay));

  EXPECT_EQ(drm_->get_test_modeset_count(), 2);
  EXPECT_EQ(drm_->last_planes_committed_count(), 2);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ModesetWithClonedPlanesWithOverlayFailing) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888}}}}};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> primary =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  scoped_refptr<DrmFramebuffer> overlay =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(primary));
  planes.push_back(DrmOverlayPlane::TestPlane(overlay));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  drm_->set_overlay_modeset_expectation(false);
  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(crtc_id)
                                 .modeset_framebuffers,
                             primary));
  EXPECT_FALSE(base::Contains(drm_->plane_manager()
                                  ->GetCrtcStateForCrtcId(crtc_id)
                                  .modeset_framebuffers,
                              overlay));

  EXPECT_EQ(drm_->get_test_modeset_count(), 2);
  EXPECT_EQ(drm_->last_planes_committed_count(), 1);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ModesetWithNewBuffersOnModifiersChange) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888}}}}};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true,
                     /*use_modifiers_list=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());

  scoped_refptr<DrmFramebuffer> primary =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  scoped_refptr<DrmFramebuffer> overlay =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(primary));
  planes.push_back(DrmOverlayPlane::TestPlane(overlay));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  EXPECT_FALSE(base::Contains(drm_->plane_manager()
                                  ->GetCrtcStateForCrtcId(crtc_id)
                                  .modeset_framebuffers,
                              primary));
  EXPECT_FALSE(base::Contains(drm_->plane_manager()
                                  ->GetCrtcStateForCrtcId(crtc_id)
                                  .modeset_framebuffers,
                              overlay));

  // Testing test modifiers only, no linear or overlays test.
  EXPECT_EQ(drm_->get_test_modeset_count(), 1);
  EXPECT_EQ(drm_->last_planes_committed_count(), 1);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, PinnedPlanesAndHwMirroring) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
  };
  std::vector<PlaneState> movable_planes = {{.formats = {DRM_FORMAT_XRGB8888}}};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true,
                     /*use_modifiers_list=*/true, movable_planes);
  uint32_t crtc_id_1 = drm_->crtc_property(0).id;
  uint32_t connector_id_1 = drm_->connector_property(0).id;
  uint32_t crtc_id_2 = drm_->crtc_property(1).id;
  uint32_t connector_id_2 = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, crtc_id_1, connector_id_1);
  screen_manager_->AddDisplayController(drm_, crtc_id_2, connector_id_2);

  // Set up the initial window:
  {
    std::unique_ptr<DrmWindow> window(
        new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
    window->Initialize();
    window->SetBounds(GetPrimaryBounds());
    screen_manager_->AddWindow(1, std::move(window));
  }

  // Set up the first display only:
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id_1, connector_id_1,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  // The movable plane will be associated with the first display:
  {
    DrmOverlayPlaneList planes;
    planes.push_back(DrmOverlayPlane::TestPlane(
        CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size())));
    planes.push_back(DrmOverlayPlane::TestPlane(
        CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size())));
    screen_manager_->GetWindow(1)->SchedulePageFlip(
        std::move(planes), base::DoNothing(), base::DoNothing());
    drm_->RunCallbacks();
  }

  // We should now be able to set up a HW mirrored display:
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, crtc_id_2, connector_id_2,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  auto window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, PinnedPlanesAndModesetting) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
  };
  std::vector<PlaneState> movable_planes = {{.formats = {DRM_FORMAT_XRGB8888}}};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true,
                     /*use_modifiers_list=*/true, movable_planes);
  uint32_t crtc_id_1 = drm_->crtc_property(0).id;
  uint32_t connector_id_1 = drm_->connector_property(0).id;
  uint32_t crtc_id_2 = drm_->crtc_property(1).id;
  uint32_t connector_id_2 = drm_->connector_property(1).id;

  screen_manager_->AddDisplayController(drm_, crtc_id_1, connector_id_1);
  screen_manager_->AddDisplayController(drm_, crtc_id_2, connector_id_2);

  // Set up the windows:
  {
    std::unique_ptr<DrmWindow> window(
        new DrmWindow(1, device_manager_.get(), screen_manager_.get()));
    std::unique_ptr<DrmWindow> window2(
        new DrmWindow(2, device_manager_.get(), screen_manager_.get()));
    window->Initialize();
    window->SetBounds(GetPrimaryBounds());
    screen_manager_->AddWindow(1, std::move(window));

    window2->Initialize();
    window2->SetBounds(GetSecondaryBounds());
    screen_manager_->AddWindow(2, std::move(window2));
  }

  // Set up the first display only:
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id_1, connector_id_1,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  // The movable plane will be associated with the first display:
  {
    DrmOverlayPlaneList planes;
    planes.push_back(DrmOverlayPlane::TestPlane(
        CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size())));
    planes.push_back(DrmOverlayPlane::TestPlane(
        CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size())));
    screen_manager_->GetWindow(1)->SchedulePageFlip(
        std::move(planes), base::DoNothing(), base::DoNothing());
    drm_->RunCallbacks();
  }

  // We should now be able to set up a second display:
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, crtc_id_2, connector_id_2,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  screen_manager_->RemoveWindow(1)->Shutdown();
  screen_manager_->RemoveWindow(2)->Shutdown();
}

TEST_F(ScreenManagerTest, ReplaceDisplayControllersCrtcs) {
  // Initializes 2 CRTC-Connector pairs.
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));

  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  ASSERT_NE(controller, nullptr);

  ConnectorCrtcMap current_pairings = {{connector_id, crtc_id}};

  uint32_t new_crtc_id = drm_->crtc_property(1).id;
  ConnectorCrtcMap new_pairings = {{connector_id, new_crtc_id}};

  ASSERT_TRUE(screen_manager_->ReplaceDisplayControllersCrtcs(
      drm_, current_pairings, new_pairings));

  controllers_to_enable.back().crtc = new_crtc_id;
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  HardwareDisplayController* new_controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(new_controller->HasCrtc(drm_, new_crtc_id));
  EXPECT_FALSE(new_controller->HasCrtc(drm_, crtc_id));
}

// TODO(b/322831691): Deterministic failure.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ReplaceDisplayControllersCrtcsNonexistent \
  DISABLED_ReplaceDisplayControllersCrtcsNonexistent
#else
#define MAYBE_ReplaceDisplayControllersCrtcsNonexistent \
  ReplaceDisplayControllersCrtcsNonexistent
#endif
TEST_F(ScreenManagerTest, MAYBE_ReplaceDisplayControllersCrtcsNonexistent) {
  // Initializes 2 CRTC-Connector pairs.
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  uint32_t crtc_id = drm_->crtc_property(0).id;
  uint32_t connector_id = drm_->connector_property(0).id;

  // But only configure 1 CRTC-connector pair.
  screen_manager_->AddDisplayController(drm_, crtc_id, connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id, connector_id,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));

  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  ASSERT_NE(controller, nullptr);
  // CRTC + 1 is not a current CRTC.
  ConnectorCrtcMap current_pairings = {{connector_id, crtc_id + 1}};

  uint32_t new_crtc_id = drm_->crtc_property(1).id;
  ConnectorCrtcMap new_pairings = {{connector_id, new_crtc_id}};

  EXPECT_DEATH_IF_SUPPORTED(screen_manager_->ReplaceDisplayControllersCrtcs(
                                drm_, current_pairings, new_pairings),
                            "controller not found for connector");
}

TEST_F(ScreenManagerTest, ReplaceDisplayControllersCrtcsComplex) {
  // 3 CRTCs and 2 connectors.
  // Original state: {crtc_1 - connector_1}, {crtc_2 - connector_2}
  // After replacement: {crtc_2 - connector_1}, {crtc_3 - connector_2}

  drm_->ResetStateWithAllProperties();

  // Create 3 CRTCs
  uint32_t crtc_1 = drm_->AddCrtc().id;
  AddPlaneToCrtc(crtc_1, DRM_PLANE_TYPE_PRIMARY);
  AddPlaneToCrtc(crtc_1, DRM_PLANE_TYPE_OVERLAY);
  uint32_t crtc_2 = drm_->AddCrtc().id;
  AddPlaneToCrtc(crtc_2, DRM_PLANE_TYPE_PRIMARY);
  AddPlaneToCrtc(crtc_2, DRM_PLANE_TYPE_OVERLAY);
  uint32_t crtc_3 = drm_->AddCrtc().id;
  AddPlaneToCrtc(crtc_3, DRM_PLANE_TYPE_PRIMARY);
  AddPlaneToCrtc(crtc_3, DRM_PLANE_TYPE_OVERLAY);

  // Create 2 Connectors that can use all 3 CRTCs.
  uint32_t connector_1, connector_2;
  {
    FakeDrmDevice::EncoderProperties& encoder = drm_->AddEncoder();
    encoder.possible_crtcs = 0b111;
    const uint32_t encoder_id = encoder.id;
    FakeDrmDevice::ConnectorProperties& connector = drm_->AddConnector();
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder_id};
    connector_1 = connector.id;
  }
  {
    FakeDrmDevice::EncoderProperties& encoder = drm_->AddEncoder();
    encoder.possible_crtcs = 0b111;
    const uint32_t encoder_id = encoder.id;
    FakeDrmDevice::ConnectorProperties& connector = drm_->AddConnector();
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder_id};
    connector_2 = connector.id;
  }

  drm_->InitializeState(/*is_atomic=*/true);

  // Configure to {crtc_1 - connector_1}, {crtc_2 - connector_2}.
  screen_manager_->AddDisplayController(drm_, crtc_1, connector_1);
  screen_manager_->AddDisplayController(drm_, crtc_2, connector_2);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_1, connector_1, GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, crtc_2, connector_2,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  HardwareDisplayController* controller_1 =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  ASSERT_NE(controller_1, nullptr);
  EXPECT_THAT(controller_1->crtc_controllers(),
              UnorderedElementsAre(
                  Pointee(EqualsCrtcConnectorIds(crtc_1, connector_1))));

  HardwareDisplayController* controller_2 =
      screen_manager_->GetDisplayController(GetSecondaryBounds());
  ASSERT_NE(controller_2, nullptr);
  EXPECT_THAT(controller_2->crtc_controllers(),
              UnorderedElementsAre(
                  Pointee(EqualsCrtcConnectorIds(crtc_2, connector_2))));

  ConnectorCrtcMap current_pairings = {{connector_1, crtc_1},
                                       {connector_2, crtc_2}};
  ConnectorCrtcMap new_pairings = {{connector_1, crtc_2},
                                   {connector_2, crtc_3}};
  ASSERT_TRUE(screen_manager_->ReplaceDisplayControllersCrtcs(
      drm_, current_pairings, new_pairings));

  // Check that the HDCs now reflect the replaced CRTCs, and that no old
  // CrtcControllers remain.
  EXPECT_THAT(controller_1->crtc_controllers(),
              UnorderedElementsAre(
                  Pointee(EqualsCrtcConnectorIds(crtc_2, connector_1))));
  EXPECT_THAT(controller_2->crtc_controllers(),
              UnorderedElementsAre(
                  Pointee(EqualsCrtcConnectorIds(crtc_3, connector_2))));
}

TEST_F(ScreenManagerTest, TileDisplay) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}}};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true);

  uint32_t crtc_id_1 = drm_->crtc_property(0).id;
  uint32_t connector_id_1 = drm_->connector_property(0).id;
  uint32_t crtc_id_2 = drm_->crtc_property(1).id;
  uint32_t connector_id_2 = drm_->connector_property(1).id;

  TileProperty primary_tile_prop = {.group_id = 1,
                                    .scale_to_fit_display = true,
                                    .tile_size = gfx::Size(3840, 4320),
                                    .tile_layout = gfx::Size(2, 1),
                                    .location = gfx::Point(0, 0)};
  std::unique_ptr<ui::HardwareDisplayControllerInfo> primary_info =
      GetDisplayInfo(connector_id_1, crtc_id_1, /*index=*/1, primary_tile_prop);

  TileProperty nonprimary_tile_prop = primary_tile_prop;
  nonprimary_tile_prop.location = gfx::Point(1, 0);
  primary_info->AcquireNonprimaryTileInfo(GetDisplayInfo(
      connector_id_2, crtc_id_2, /*index=*/2, nonprimary_tile_prop));

  std::unique_ptr<display::FakeDisplaySnapshot> snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(kPrimaryDisplayId)
          .SetBaseConnectorId(primary_info->connector()->connector_id)
          .SetNativeMode(gfx::Size(3840, 4320))
          .SetCurrentMode(gfx::Size(3840, 4320))
          .Build();

  DrmDisplay drm_display(drm_.get(), primary_info.get(), *snapshot);

  screen_manager_->AddDisplayControllersForDisplay(drm_display);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, crtc_id_1, connector_id_1, gfx::Point(0, 0),
      std::make_unique<drmModeModeInfo>(
          drmModeModeInfo{.hdisplay = 3840, .vdisplay = 4320}));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset}));

  HardwareDisplayController* hdc = screen_manager_->GetDisplayController(
      // This is the full tile composited size.
      gfx::Rect(0, 0, 3840 * 2, 4320));
  ASSERT_NE(hdc, nullptr);
  ASSERT_TRUE(hdc->IsTiled());
  EXPECT_THAT(hdc->GetTileProperty(),
              Optional(EqTileProperty(primary_tile_prop)));

  EXPECT_THAT(
      hdc->crtc_controllers(),
      UnorderedElementsAre(
          Pointee(
              AllOf(Property(&CrtcController::crtc, Eq(crtc_id_1)),
                    Property(&CrtcController::connector, Eq(connector_id_1)),
                    Property(&CrtcController::is_tiled, Eq(true)),
                    Property(&CrtcController::tile_property,
                             Optional(EqTileProperty(primary_tile_prop))))),
          Pointee(AllOf(
              Property(&CrtcController::crtc, Eq(crtc_id_2)),
              Property(&CrtcController::connector, Eq(connector_id_2)),
              Property(&CrtcController::is_tiled, Eq(true)),
              Property(&CrtcController::tile_property,
                       Optional(EqTileProperty(nonprimary_tile_prop)))))));
}
}  // namespace ui
