// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drm_fourcc.h>
#include <stddef.h>
#include <stdint.h>
#include <xf86drm.h>
#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/files/platform_file.h"
#include "testing/gtest/include/gtest/gtest.h"
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
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {
namespace {

constexpr drmModeModeInfo ConstructMode(uint16_t hdisplay, uint16_t vdisplay) {
  return {.hdisplay = hdisplay, .vdisplay = vdisplay};
}

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode = ConstructMode(6, 4);

const uint32_t kPrimaryDisplayId = 1;
const uint32_t kSecondaryDisplayId = 2;

constexpr uint32_t kCrtcIdBase = 100;
constexpr uint32_t kPrimaryCrtc = kCrtcIdBase;
constexpr uint32_t kSecondaryCrtc = kCrtcIdBase + 1;

constexpr uint32_t kConnectorIdBase = 200;
constexpr uint32_t kPrimaryConnector = kConnectorIdBase;
constexpr uint32_t kSecondaryConnector = kConnectorIdBase + 1;
constexpr uint32_t kPlaneIdBase = 300;
constexpr uint32_t kInFormatsBlobPropIdBase = 400;

constexpr uint32_t kFbIdPropId = 3005;
constexpr uint32_t kTypePropId = 3010;
constexpr uint32_t kInFormatsPropId = 3011;

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

  void InitializeDrmState(ui::MockDrmDevice* drm,
                          const std::vector<CrtcState>& crtc_states,
                          bool is_atomic,
                          bool use_modifiers_list = false) {
    std::vector<ui::MockDrmDevice::CrtcProperties> crtc_properties(
        crtc_states.size());
    std::map<uint32_t, std::string> crtc_property_names = {
        {1000, "ACTIVE"},
        {1001, "MODE_ID"},
    };

    std::vector<ui::MockDrmDevice::ConnectorProperties> connector_properties(3);
    std::map<uint32_t, std::string> connector_property_names = {
        {2000, "CRTC_ID"},
    };
    for (size_t i = 0; i < connector_properties.size(); ++i) {
      connector_properties[i].id = kPrimaryConnector + i;
      for (const auto& pair : connector_property_names) {
        connector_properties[i].properties.push_back(
            {.id = pair.first, .value = 0});
      }
    }

    std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties;
    std::map<uint32_t, std::string> plane_property_names = {
        // Add all required properties.
        {3000, "CRTC_ID"},
        {3001, "CRTC_X"},
        {3002, "CRTC_Y"},
        {3003, "CRTC_W"},
        {3004, "CRTC_H"},
        {kFbIdPropId, "FB_ID"},
        {3006, "SRC_X"},
        {3007, "SRC_Y"},
        {3008, "SRC_W"},
        {3009, "SRC_H"},
        // Defines some optional properties we use for convenience.
        {kTypePropId, "type"},
        {kInFormatsPropId, "IN_FORMATS"},
    };

    uint32_t plane_id = kPlaneIdBase;
    uint32_t property_id = kInFormatsBlobPropIdBase;

    for (size_t crtc_idx = 0; crtc_idx < crtc_states.size(); ++crtc_idx) {
      crtc_properties[crtc_idx].id = kPrimaryCrtc + crtc_idx;
      for (const auto& pair : crtc_property_names) {
        crtc_properties[crtc_idx].properties.push_back(
            {.id = pair.first, .value = 0});
      }

      std::vector<ui::MockDrmDevice::PlaneProperties> crtc_plane_properties(
          crtc_states[crtc_idx].planes.size());
      for (size_t plane_idx = 0;
           plane_idx < crtc_states[crtc_idx].planes.size(); ++plane_idx) {
        crtc_plane_properties[plane_idx].id = plane_id++;
        crtc_plane_properties[plane_idx].crtc_mask = 1 << crtc_idx;

        for (const auto& pair : plane_property_names) {
          uint64_t value = 0;
          if (pair.first == kTypePropId) {
            value = plane_idx == 0 ? DRM_PLANE_TYPE_PRIMARY
                                   : DRM_PLANE_TYPE_OVERLAY;
          } else if (pair.first == kInFormatsPropId) {
            value = property_id++;
            std::vector<drm_format_modifier> drm_format_modifiers;
            if (use_modifiers_list) {
              for (const auto modifier : supported_modifiers_) {
                drm_format_modifiers.push_back({.formats = 1,
                                                .offset = 0,
                                                .pad = 0,
                                                .modifier = modifier});
              }
            }

            drm->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
                value, crtc_states[crtc_idx].planes[plane_idx].formats,
                std::move(drm_format_modifiers)));
          }

          crtc_plane_properties[plane_idx].properties.push_back(
              {.id = pair.first, .value = value});
        }
      }

      plane_properties.insert(plane_properties.end(),
                              crtc_plane_properties.begin(),
                              crtc_plane_properties.end());
    }

    std::map<uint32_t, std::string> property_names;
    property_names.insert(crtc_property_names.begin(),
                          crtc_property_names.end());
    property_names.insert(connector_property_names.begin(),
                          connector_property_names.end());
    property_names.insert(plane_property_names.begin(),
                          plane_property_names.end());

    drm->SetModifiersOverhead(modifiers_overhead_);
    drm->InitializeState(crtc_properties, connector_properties,
                         plane_properties, property_names, is_atomic);
  }

  void InitializeDrmStateWithDefault(ui::MockDrmDevice* drm,
                                     bool is_atomic,
                                     bool use_modifiers_list = false) {
    // A Sample of CRTC states.
    std::vector<CrtcState> crtc_states = {
        {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
        {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}}};
    InitializeDrmState(drm, crtc_states, is_atomic, use_modifiers_list);
  }

  void SetUp() override {
    auto gbm = std::make_unique<ui::MockGbmDevice>();
    supported_modifiers_ = gbm->GetSupportedModifiers();
    drm_ = new ui::MockDrmDevice(std::move(gbm));
    device_manager_ = std::make_unique<ui::DrmDeviceManager>(nullptr);
    screen_manager_ = std::make_unique<ui::ScreenManager>();
  }

  void TearDown() override {
    screen_manager_.reset();
    drm_ = nullptr;
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
    if (format_modifier != DRM_FORMAT_MOD_NONE)
      modifiers.push_back(format_modifier);
    auto buffer = drm_->gbm_device()->CreateBufferWithModifiers(
        format, size, GBM_BO_USE_SCANOUT, modifiers);
    return DrmFramebuffer::AddFramebuffer(drm_, buffer.get(), size, modifiers);
  }

 protected:
  scoped_refptr<ui::MockDrmDevice> drm_;
  std::unique_ptr<ui::DrmDeviceManager> device_manager_;
  std::unique_ptr<ui::ScreenManager> screen_manager_;
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

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  EXPECT_EQ(drm_->get_test_modeset_count(), 1);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 1);

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());

  EXPECT_TRUE(controller);
  EXPECT_TRUE(controller->HasCrtc(drm_, kPrimaryCrtc));
}

TEST_F(ScreenManagerTest, CheckWithSeamlessModeset) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable,
      display::kCommitModeset | display::kSeamlessModeset);

  EXPECT_EQ(drm_->get_commit_modeset_count(), 0);
  EXPECT_EQ(drm_->get_seamless_modeset_count(), 1);
}

TEST_F(ScreenManagerTest, CheckWithInvalidBounds) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckForSecondValidController) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

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

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset));

  EXPECT_EQ(drm_->get_test_modeset_count(), 1);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 1);
}

TEST_F(ScreenManagerTest, CheckMultipleDisplaysOutsideModifiersLimit) {
  int max_supported_displays_with_modifier = 2;
  drm_->SetSystemLimitOfModifiers(modifiers_overhead_[DRM_FORMAT_MOD_LINEAR] *
                                  max_supported_displays_with_modifier);

  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true,
                                /*use_modifiers_list=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset));

  // Testing for a failed test-modeset with modifiers + a fallback to Linear
  // Modifier and a modeset commit.
  EXPECT_EQ(drm_->get_test_modeset_count(), 2);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 1);
}

TEST_F(ScreenManagerTest, CheckDisplaysWith0Limit) {
  drm_->SetSystemLimitOfModifiers(0);

  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true,
                                /*use_modifiers_list=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  EXPECT_FALSE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset));

  // Testing for a failed test-modeset with modifiers + failed test-modeset with
  // Linear Modifier and no modeset due to failed tests.
  EXPECT_EQ(drm_->get_test_modeset_count(), 2);
  EXPECT_EQ(drm_->get_commit_modeset_count(), 0);
}

TEST_F(ScreenManagerTest, CheckControllerAfterItIsRemoved) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));

  ScreenManager::CrtcsWithDrmList controllers_to_remove;
  controllers_to_remove.emplace_back(kPrimaryCrtc, drm_);
  screen_manager_->RemoveDisplayControllers(controllers_to_remove);
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
}

TEST_F(ScreenManagerTest, CheckControllerAfterDisabled) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);

  // Enable
  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }

  int test_modeset_count_before_disable = drm_->get_test_modeset_count();
  int commit_modeset_count_before_disable = drm_->get_commit_modeset_count();
  // Disable
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_, kPrimaryCrtc,
                                     kPrimaryConnector, gfx::Point(), nullptr);
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_EQ(drm_->get_test_modeset_count(),
            test_modeset_count_before_disable + 1);
  EXPECT_EQ(drm_->get_commit_modeset_count(),
            commit_modeset_count_before_disable + 1);

  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
}

TEST_F(ScreenManagerTest, CheckMultipleControllersAfterBeingRemoved) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  int modeset_count_after_enable = drm_->get_commit_modeset_count();
  ScreenManager::CrtcsWithDrmList controllers_to_remove;
  controllers_to_remove.emplace_back(kPrimaryCrtc, drm_);
  controllers_to_remove.emplace_back(kSecondaryCrtc, drm_);
  screen_manager_->RemoveDisplayControllers(controllers_to_remove);

  // Removed displays are disabled in only 1 modeset commit.
  EXPECT_EQ(drm_->get_commit_modeset_count(), modeset_count_after_enable + 1);

  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckMultipleControllersAfterBeingDisabled) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);
  // Enable
  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
        GetSecondaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }

  int test_modeset_count_before_disable = drm_->get_test_modeset_count();
  int commit_modeset_count_before_disable = drm_->get_commit_modeset_count();
  // Disable
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_, kPrimaryCrtc,
                                     kPrimaryConnector, gfx::Point(), nullptr);
  controllers_to_enable.emplace_back(kSecondaryDisplayId, drm_, kSecondaryCrtc,
                                     kSecondaryConnector, gfx::Point(),
                                     nullptr);
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_EQ(drm_->get_test_modeset_count(),
            test_modeset_count_before_disable + 1);
  EXPECT_EQ(drm_->get_commit_modeset_count(),
            commit_modeset_count_before_disable + 1);

  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckDuplicateConfiguration) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic*/ false);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  uint32_t framebuffer = drm_->current_framebuffer();

  controllers_to_enable.clear();
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  // Should not hold onto buffers.
  EXPECT_NE(framebuffer, drm_->current_framebuffer());

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckChangingMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);

  // Modeset with default mode.
  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }
  auto new_mode = kDefaultMode;
  new_mode.vdisplay = new_mode.vdisplay++;
  // Modeset with a changed Mode.
  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(new_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
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

TEST_F(ScreenManagerTest, CheckForControllersInMirroredMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

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

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  controllers_to_enable.clear();
  drmModeModeInfo transition1_primary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(transition1_primary_mode));
  drmModeModeInfo transition1_secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(transition1_secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  controllers_to_enable.clear();
  drmModeModeInfo transition2_primary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(transition2_primary_mode));
  drmModeModeInfo transition2_secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(transition2_secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

// Make sure we're using each display's mode when doing mirror mode otherwise
// the timings may be off.
TEST_F(ScreenManagerTest, CheckMirrorModeModesettingWithDisplaysMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  // Copy the mode and use the copy so we can tell what mode the CRTC was
  // configured with. The clock value is modified so we can tell which mode is
  // being used.
  drmModeModeInfo secondary_mode = kDefaultMode;
  secondary_mode.clock++;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  for (const auto& crtc : controller->crtc_controllers()) {
    if (crtc->crtc() == kPrimaryCrtc)
      EXPECT_EQ(kDefaultMode.clock, crtc->mode().clock);
    else if (crtc->crtc() == kSecondaryCrtc)
      EXPECT_EQ(secondary_mode.clock, crtc->mode().clock);
    else
      NOTREACHED();
  }
}

TEST_F(ScreenManagerTest, MonitorGoneInMirrorMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  ScreenManager::CrtcsWithDrmList controllers_to_remove;
  controllers_to_remove.emplace_back(kSecondaryCrtc, drm_);
  screen_manager_->RemoveDisplayControllers(controllers_to_remove);

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  EXPECT_TRUE(controller->HasCrtc(drm_, kPrimaryCrtc));
  EXPECT_FALSE(controller->HasCrtc(drm_, kSecondaryCrtc));
}

TEST_F(ScreenManagerTest, MonitorDisabledInMirrorMode) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  // Enable in Mirror Mode.
  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    drmModeModeInfo secondary_mode = kDefaultMode;
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }

  // Disable display Controller.
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(0, drm_, kSecondaryCrtc, 0, gfx::Point(),
                                     nullptr);
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  EXPECT_TRUE(controller->HasCrtc(drm_, kPrimaryCrtc));
  EXPECT_FALSE(controller->HasCrtc(drm_, kSecondaryCrtc));
}

TEST_F(ScreenManagerTest, DoNotEnterMirrorModeUnlessSameBounds) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  // Configure displays in extended mode.
  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    drmModeModeInfo secondary_mode = kDefaultMode;
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
        GetSecondaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }

  {
    auto new_mode = std::make_unique<drmModeModeInfo>(kDefaultMode);
    new_mode->vdisplay = 10;
    // Shouldn't enter mirror mode unless the display bounds are the same.
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
        GetPrimaryBounds().origin(), std::move(new_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }

  EXPECT_FALSE(
      screen_manager_->GetDisplayController(GetPrimaryBounds())->IsMirrored());
}

TEST_F(ScreenManagerTest, ReuseFramebufferIfDisabledThenReEnabled) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  uint32_t framebuffer = drm_->current_framebuffer();

  controllers_to_enable.clear();
  // Disable display controller.
  controllers_to_enable.emplace_back(0, drm_, kPrimaryCrtc, 0, gfx::Point(),
                                     nullptr);
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  EXPECT_EQ(0u, drm_->current_framebuffer());

  controllers_to_enable.clear();
  drmModeModeInfo reenable_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(reenable_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  // Buffers are released when disabled.
  EXPECT_NE(framebuffer, drm_->current_framebuffer());
}

TEST_F(ScreenManagerTest, CheckMirrorModeAfterBeginReEnabled) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);
  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    drmModeModeInfo secondary_mode = kDefaultMode;
    controllers_to_enable.emplace_back(
        kSecondaryDisplayId, drm_, kSecondaryCrtc, kSecondaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(secondary_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }
  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(0, drm_, kPrimaryCrtc, 0, gfx::Point(),
                                       nullptr);
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(controller->IsMirrored());

  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    drmModeModeInfo reenable_mode = kDefaultMode;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(reenable_mode));
    screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }

  EXPECT_TRUE(controller);
  EXPECT_TRUE(controller->IsMirrored());
}

TEST_F(ScreenManagerTest, ConfigureOnDifferentDrmDevices) {
  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  scoped_refptr<ui::MockDrmDevice> drm2 =
      new ui::MockDrmDevice(std::move(gbm_device));

  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}}};
  InitializeDrmState(drm2.get(), crtc_states, /*is_atomic=*/false);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm2, kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->AddDisplayController(drm2, kSecondaryCrtc + 1,
                                        kSecondaryConnector + 1);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm2, kSecondaryCrtc, kSecondaryConnector,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  drmModeModeInfo secondary_mode2 = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId + 1, drm2, kSecondaryCrtc + 1,
      kSecondaryConnector + 1, GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode2));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_EQ(drm_->get_set_crtc_call_count(), 1);
  EXPECT_EQ(drm2->get_set_crtc_call_count(), 2);
}

TEST_F(ScreenManagerTest,
       CheckProperConfigurationWithDifferentDeviceAndSameCrtc) {
  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  scoped_refptr<ui::MockDrmDevice> drm2 =
      new ui::MockDrmDevice(std::move(gbm_device));

  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);
  InitializeDrmStateWithDefault(drm2.get(), /*is_atomic=*/true);

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm2, kPrimaryCrtc, kPrimaryConnector);

  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  drmModeModeInfo secondary_mode = kDefaultMode;
  controllers_to_enable.emplace_back(
      kSecondaryDisplayId, drm2, kPrimaryCrtc, kPrimaryConnector,
      GetSecondaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(secondary_mode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  ui::HardwareDisplayController* controller1 =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  ui::HardwareDisplayController* controller2 =
      screen_manager_->GetDisplayController(GetSecondaryBounds());

  EXPECT_NE(controller1, controller2);
  EXPECT_EQ(drm_, controller1->crtc_controllers()[0]->drm());
  EXPECT_EQ(drm2, controller2->crtc_controllers()[0]->drm());
}

TEST_F(ScreenManagerTest, CheckControllerToWindowMappingWithSameBounds) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_TRUE(screen_manager_->GetWindow(1)->GetController());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, CheckControllerToWindowMappingWithDifferentBounds) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  gfx::Rect new_bounds = GetPrimaryBounds();
  new_bounds.Inset(gfx::Insets::TLBR(0, 0, 1, 1));
  window->SetBounds(new_bounds);
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_FALSE(screen_manager_->GetWindow(1)->GetController());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest,
       CheckControllerToWindowMappingWithOverlappingWindows) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  const size_t kWindowCount = 2;
  for (size_t i = 1; i < kWindowCount + 1; ++i) {
    std::unique_ptr<ui::DrmWindow> window(
        new ui::DrmWindow(i, device_manager_.get(), screen_manager_.get()));
    window->Initialize();
    window->SetBounds(GetPrimaryBounds());
    screen_manager_->AddWindow(i, std::move(window));
  }

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  bool window1_has_controller = screen_manager_->GetWindow(1)->GetController();
  bool window2_has_controller = screen_manager_->GetWindow(2)->GetController();
  // Only one of the windows can have a controller.
  EXPECT_TRUE(window1_has_controller ^ window2_has_controller);

  for (size_t i = 1; i < kWindowCount + 1; ++i) {
    std::unique_ptr<ui::DrmWindow> window = screen_manager_->RemoveWindow(i);
    window->Shutdown();
  }
}

TEST_F(ScreenManagerTest, ShouldDissociateWindowOnControllerRemoval) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  gfx::AcceleratedWidget window_id = 1;
  std::unique_ptr<ui::DrmWindow> window(new ui::DrmWindow(
      window_id, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(window_id, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_TRUE(screen_manager_->GetWindow(window_id)->GetController());

  ScreenManager::CrtcsWithDrmList controllers_to_remove;
  controllers_to_remove.emplace_back(kPrimaryCrtc, drm_);
  screen_manager_->RemoveDisplayControllers(controllers_to_remove);

  EXPECT_FALSE(screen_manager_->GetWindow(window_id)->GetController());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, EnableControllerWhenWindowHasNoBuffer) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_TRUE(screen_manager_->GetWindow(1)->GetController());
  // There is a buffer after initial config.
  uint32_t framebuffer = drm_->current_framebuffer();
  EXPECT_NE(0U, framebuffer);

  controllers_to_enable.clear();
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  // There is a new buffer after we configured with the same mode but no
  // pending frames on the window.
  EXPECT_NE(framebuffer, drm_->current_framebuffer());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, EnableControllerWhenWindowHasBuffer) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/false);

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  ui::DrmOverlayPlaneList planes;
  planes.push_back(ui::DrmOverlayPlane(buffer, nullptr));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  EXPECT_EQ(buffer->opaque_framebuffer_id(), drm_->current_framebuffer());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

// See crbug.com/868010
TEST_F(ScreenManagerTest, DISABLED_RejectBufferWithIncompatibleModifiers) {
  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  auto buffer = CreateBufferWithModifier(
      DRM_FORMAT_XRGB8888, I915_FORMAT_MOD_X_TILED, GetPrimaryBounds().size());
  ui::DrmOverlayPlaneList planes;
  planes.push_back(ui::DrmOverlayPlane(buffer, nullptr));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

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

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  // When a window that had no controller becomes associated with a new
  // controller, expect the crtc to be modeset once.
  EXPECT_EQ(drm_->get_set_crtc_call_count(), 1);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ShouldNotHardwareMirrorDifferentDrmDevices) {
  auto gbm_device1 = std::make_unique<MockGbmDevice>();
  auto drm_device1 =
      base::MakeRefCounted<MockDrmDevice>(std::move(gbm_device1));
  InitializeDrmStateWithDefault(drm_device1.get(), /*is_atomic=*/true);

  auto gbm_device2 = std::make_unique<MockGbmDevice>();
  auto drm_device2 =
      base::MakeRefCounted<MockDrmDevice>(std::move(gbm_device2));
  InitializeDrmStateWithDefault(drm_device2.get(), /*is_atomic=*/true);

  DrmDeviceManager drm_device_manager(nullptr);
  ScreenManager screen_manager;

  constexpr uint32_t kCrtc1 = kPrimaryCrtc;
  constexpr uint32_t kConnector1 = kPrimaryConnector;
  constexpr uint32_t kCrtc2 = kSecondaryCrtc;
  constexpr uint32_t kConnector2 = kSecondaryConnector;

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
    auto window1 =
        std::make_unique<DrmWindow>(1, &drm_device_manager, &screen_manager);
    window1->Initialize();
    screen_manager.AddWindow(1, std::move(window1));
    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.AddDisplayController(drm_device1, kCrtc1, kConnector1);
    screen_manager.AddDisplayController(drm_device2, kCrtc2, kConnector2);

    ScreenManager::ControllerConfigsList controllers_to_enable;
    std::unique_ptr<drmModeModeInfo> primary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    std::unique_ptr<drmModeModeInfo> secondary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_device1, kCrtc1,
                                       kConnector1, gfx::Point(0, 0),
                                       std::move(primary_mode));
    controllers_to_enable.emplace_back(kSecondaryDisplayId, drm_device2, kCrtc2,
                                       kConnector2, gfx::Point(0, 1140),
                                       std::move(secondary_mode));
    screen_manager.ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);

    auto window2 =
        std::make_unique<DrmWindow>(2, &drm_device_manager, &screen_manager);
    window2->Initialize();
    screen_manager.AddWindow(2, std::move(window2));
    screen_manager.GetWindow(2)->SetBounds(gfx::Rect(0, 1140, 1920, 1080));
  }

  // Displays are stacked vertically, window per display.
  {
    HardwareDisplayController* controller1 =
        screen_manager.GetWindow(1)->GetController();
    HardwareDisplayController* controller2 =
        screen_manager.GetWindow(2)->GetController();
    EXPECT_NE(controller1, controller2);
    EXPECT_TRUE(controller1->HasCrtc(drm_device1, kCrtc1));
    EXPECT_TRUE(controller2->HasCrtc(drm_device2, kCrtc2));
  }

  // Disconnect first display. Second display moves to origin.
  {
    ScreenManager::CrtcsWithDrmList controllers_to_remove;
    controllers_to_remove.emplace_back(kCrtc1, drm_device1);
    screen_manager.RemoveDisplayControllers(controllers_to_remove);

    ScreenManager::ControllerConfigsList controllers_to_enable;
    std::unique_ptr<drmModeModeInfo> secondary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    controllers_to_enable.emplace_back(kSecondaryDisplayId, drm_device2, kCrtc2,
                                       kConnector2, gfx::Point(0, 0),
                                       std::move(secondary_mode));
    screen_manager.ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);

    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.RemoveWindow(2)->Shutdown();
  }

  // Reconnect first display. Original configuration restored.
  {
    screen_manager.AddDisplayController(drm_device1, kCrtc1, kConnector1);
    ScreenManager::ControllerConfigsList controllers_to_enable;
    std::unique_ptr<drmModeModeInfo> primary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_device1, kCrtc1,
                                       kConnector1, gfx::Point(0, 0),
                                       std::move(primary_mode));
    screen_manager.ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
    // At this point, both displays are in the same location.
    {
      HardwareDisplayController* controller =
          screen_manager.GetWindow(1)->GetController();
      EXPECT_FALSE(controller->IsMirrored());
      // We don't really care which crtc it has, but it should have just
      EXPECT_EQ(1U, controller->crtc_controllers().size());
      EXPECT_TRUE(controller->HasCrtc(drm_device1, kCrtc1) ||
                  controller->HasCrtc(drm_device2, kCrtc2));
    }
    controllers_to_enable.clear();
    std::unique_ptr<drmModeModeInfo> secondary_mode =
        std::make_unique<drmModeModeInfo>(k1920x1080Screen);
    controllers_to_enable.emplace_back(kSecondaryDisplayId, drm_device2, kCrtc2,
                                       kConnector2, gfx::Point(0, 1140),
                                       std::move(secondary_mode));
    screen_manager.ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
    auto window3 =
        std::make_unique<DrmWindow>(3, &drm_device_manager, &screen_manager);
    window3->Initialize();
    screen_manager.AddWindow(3, std::move(window3));
    screen_manager.GetWindow(3)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 1140, 1920, 1080));
    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.GetWindow(3)->SetBounds(gfx::Rect(0, 1140, 1920, 1080));
  }

  // Everything is restored.
  {
    HardwareDisplayController* controller1 =
        screen_manager.GetWindow(1)->GetController();
    HardwareDisplayController* controller3 =
        screen_manager.GetWindow(3)->GetController();
    EXPECT_NE(controller1, controller3);
    EXPECT_TRUE(controller1->HasCrtc(drm_device1, kCrtc1));
    EXPECT_TRUE(controller3->HasCrtc(drm_device2, kCrtc2));
  }

  // Cleanup.
  screen_manager.RemoveWindow(1)->Shutdown();
  screen_manager.RemoveWindow(3)->Shutdown();
}

// crbug.com/888553
TEST_F(ScreenManagerTest, ShouldNotUnbindFramebufferOnJoiningMirror) {
  auto gbm_device = std::make_unique<MockGbmDevice>();
  auto drm_device = base::MakeRefCounted<MockDrmDevice>(std::move(gbm_device));
  InitializeDrmStateWithDefault(drm_device.get(), /*is_atomic=*/false);

  DrmDeviceManager drm_device_manager(nullptr);
  ScreenManager screen_manager;

  constexpr uint32_t kCrtc1 = kPrimaryCrtc;
  constexpr uint32_t kConnector1 = kPrimaryConnector;
  constexpr uint32_t kCrtc2 = kSecondaryCrtc;
  constexpr uint32_t kConnector2 = kSecondaryConnector;

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
    auto window1 =
        std::make_unique<DrmWindow>(1, &drm_device_manager, &screen_manager);
    window1->Initialize();
    screen_manager.AddWindow(1, std::move(window1));
    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.AddDisplayController(drm_device, kCrtc1, kConnector1);
    screen_manager.AddDisplayController(drm_device, kCrtc2, kConnector2);

    ScreenManager::ControllerConfigsList controllers_to_enable;
    std::unique_ptr<drmModeModeInfo> primary_mode =
        std::make_unique<drmModeModeInfo>(k1080p60Screen);
    std::unique_ptr<drmModeModeInfo> secondary_mode =
        std::make_unique<drmModeModeInfo>(k1080p60Screen);
    controllers_to_enable.emplace_back(kPrimaryDisplayId, drm_device, kCrtc1,
                                       kConnector1, gfx::Point(0, 0),
                                       std::move(primary_mode));
    controllers_to_enable.emplace_back(kSecondaryDisplayId, drm_device, kCrtc2,
                                       kConnector2, gfx::Point(0, 0),
                                       std::move(secondary_mode));
    screen_manager.ConfigureDisplayControllers(
        controllers_to_enable, display::kTestModeset | display::kCommitModeset);
  }

  EXPECT_NE(0u, drm_device->GetFramebufferForCrtc(kCrtc1));
  EXPECT_NE(0u, drm_device->GetFramebufferForCrtc(kCrtc2));

  // Cleanup.
  screen_manager.RemoveWindow(1)->Shutdown();
}

TEST_F(ScreenManagerTest, DrmFramebufferSequenceIdIncrementingAtModeset) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  scoped_refptr<DrmFramebuffer> pre_modeset_buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  CHECK_EQ(pre_modeset_buffer->modeset_sequence_id_at_allocation(), 0);

  // Successful modeset
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  {
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetPrimaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable,
        display::kTestModeset | display::kCommitModeset));
  }

  scoped_refptr<DrmFramebuffer> first_post_modeset_buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  CHECK_EQ(first_post_modeset_buffer->modeset_sequence_id_at_allocation(), 1);
  CHECK_EQ(pre_modeset_buffer->modeset_sequence_id_at_allocation(), 0);

  // Unsuccessful modeset
  {
    drm_->set_set_crtc_expectation(false);
    ScreenManager::ControllerConfigsList controllers_to_enable;
    controllers_to_enable.emplace_back(
        kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
        GetSecondaryBounds().origin(),
        std::make_unique<drmModeModeInfo>(kDefaultMode));
    ASSERT_FALSE(screen_manager_->ConfigureDisplayControllers(
        controllers_to_enable,
        display::kTestModeset | display::kCommitModeset));
  }

  scoped_refptr<DrmFramebuffer> second_post_modeset_buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  CHECK_EQ(second_post_modeset_buffer->modeset_sequence_id_at_allocation(), 1);
  CHECK_EQ(first_post_modeset_buffer->modeset_sequence_id_at_allocation(), 1);
  CHECK_EQ(pre_modeset_buffer->modeset_sequence_id_at_allocation(), 0);
}

TEST_F(ScreenManagerTest, CloningPlanesOnModeset) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  ui::DrmOverlayPlaneList planes;
  planes.push_back(ui::DrmOverlayPlane(buffer, nullptr));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset));

  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                 .modeset_framebuffers,
                             buffer));

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, CloningMultiplePlanesOnModeset) {
  std::vector<CrtcState> crtc_states = {{
      .planes =
          {
              {.formats = {DRM_FORMAT_XRGB8888}},
              {.formats = {DRM_FORMAT_XRGB8888}}
          }
  }};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true);

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> primary =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  scoped_refptr<DrmFramebuffer> overlay =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  ui::DrmOverlayPlaneList planes;
  planes.push_back(ui::DrmOverlayPlane(primary, nullptr));
  planes.push_back(ui::DrmOverlayPlane(overlay, nullptr));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset));

  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                 .modeset_framebuffers,
                             primary));
  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                 .modeset_framebuffers,
                             overlay));

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ModesetWithClonedPlanesNoOverlays) {
  InitializeDrmStateWithDefault(drm_.get(), /*is_atomic=*/true);

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> buffer =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  ui::DrmOverlayPlaneList planes;
  planes.push_back(ui::DrmOverlayPlane(buffer, nullptr));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset));
  ASSERT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                 .modeset_framebuffers,
                             buffer));

  EXPECT_EQ(drm_->get_test_modeset_count(), 1);
  EXPECT_EQ(drm_->last_planes_committed_count(), 1);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ModesetWithClonedPlanesWithOverlaySucceeding) {
  std::vector<CrtcState> crtc_states = {{
      .planes =
          {
              {.formats = {DRM_FORMAT_XRGB8888}},
              {.formats = {DRM_FORMAT_XRGB8888}}
          }
  }};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true);

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> primary =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  scoped_refptr<DrmFramebuffer> overlay =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  ui::DrmOverlayPlaneList planes;
  planes.push_back(ui::DrmOverlayPlane(primary, nullptr));
  planes.push_back(ui::DrmOverlayPlane(overlay, nullptr));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset));

  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                 .modeset_framebuffers,
                             primary));
  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                 .modeset_framebuffers,
                             overlay));

  EXPECT_EQ(drm_->get_test_modeset_count(), 2);
  EXPECT_EQ(drm_->last_planes_committed_count(), 2);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ModesetWithClonedPlanesWithOverlayFailing) {
  std::vector<CrtcState> crtc_states = {{
      .planes =
          {
              {.formats = {DRM_FORMAT_XRGB8888}},
              {.formats = {DRM_FORMAT_XRGB8888}}
          }
  }};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true);

  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  scoped_refptr<DrmFramebuffer> primary =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  scoped_refptr<DrmFramebuffer> overlay =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  ui::DrmOverlayPlaneList planes;
  planes.push_back(ui::DrmOverlayPlane(primary, nullptr));
  planes.push_back(ui::DrmOverlayPlane(overlay, nullptr));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  drm_->set_overlay_modeset_expecation(false);
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  EXPECT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset));

  EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                 ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                 .modeset_framebuffers,
                             primary));
  EXPECT_FALSE(base::Contains(drm_->plane_manager()
                                  ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                  .modeset_framebuffers,
                              overlay));

  EXPECT_EQ(drm_->get_test_modeset_count(), 2);
  EXPECT_EQ(drm_->last_planes_committed_count(), 1);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, ModesetWithNewBuffersOnModifiersChange) {
  std::vector<CrtcState> crtc_states = {{
      .planes =
          {
              {.formats = {DRM_FORMAT_XRGB8888}},
              {.formats = {DRM_FORMAT_XRGB8888}}
          }
  }};
  InitializeDrmState(drm_.get(), crtc_states, /*is_atomic=*/true,
                     /*use_modifiers_list=*/true);
  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());

  scoped_refptr<DrmFramebuffer> primary =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  scoped_refptr<DrmFramebuffer> overlay =
      CreateBuffer(DRM_FORMAT_XRGB8888, GetPrimaryBounds().size());
  ui::DrmOverlayPlaneList planes;
  planes.push_back(ui::DrmOverlayPlane(primary, nullptr));
  planes.push_back(ui::DrmOverlayPlane(overlay, nullptr));
  window->SchedulePageFlip(std::move(planes), base::DoNothing(),
                           base::DoNothing());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  ScreenManager::ControllerConfigsList controllers_to_enable;
  controllers_to_enable.emplace_back(
      kPrimaryDisplayId, drm_, kPrimaryCrtc, kPrimaryConnector,
      GetPrimaryBounds().origin(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  ASSERT_TRUE(screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset));

  EXPECT_FALSE(base::Contains(drm_->plane_manager()
                                  ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                  .modeset_framebuffers,
                              primary));
  EXPECT_FALSE(base::Contains(drm_->plane_manager()
                                  ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                  .modeset_framebuffers,
                              overlay));

  // Testing test modifiers only, no linear or overlays test.
  EXPECT_EQ(drm_->get_test_modeset_count(), 1);
  EXPECT_EQ(drm_->last_planes_committed_count(), 1);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

}  // namespace ui
