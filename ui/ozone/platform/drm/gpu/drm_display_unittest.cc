// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <utility>

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/types/display_color_management.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SizeIs;

namespace ui {

namespace {

constexpr gfx::Size kNativeDisplaySize(1920, 1080);

std::unique_ptr<HardwareDisplayControllerInfo> GetDisplayInfo(
    uint8_t index = 0) {
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
  connector->connector_id = 123;
  connector->connection = DRM_MODE_CONNECTED;
  connector->count_props = 0;
  connector->count_modes = kNumModes;
  connector->modes = DrmAllocator<drmModeModeInfo>(kNumModes);
  std::memcpy(connector->modes, &modes[0], kNumModes * sizeof(drmModeModeInfo));

  // Initialize a CRTC.
  ScopedDrmCrtcPtr crtc(DrmAllocator<drmModeCrtc>());
  crtc->crtc_id = 456;
  crtc->mode_valid = 1;
  crtc->mode = connector->modes[kNumModes - 1];

  return std::make_unique<HardwareDisplayControllerInfo>(
      std::move(connector), std::move(crtc), index);
}

// Verifies that the argument goes from 0 to the maximum uint16_t times |scale|
// following a power function with |exponent|.
MATCHER_P2(MatchesPowerFunction, scale, exponent, "") {
  EXPECT_FALSE(arg.IsDefaultIdentity());

  const uint16_t kMaxValue = std::numeric_limits<uint16_t>::max() * scale;
  const uint16_t kEpsilon = std::numeric_limits<uint16_t>::max() * 0.02f;

  const size_t kSize = 10;
  for (size_t i = 0; i < kSize; ++i) {
    float x = i / (kSize - 1.f);
    uint16_t r, g, b;
    arg.Evaluate(x, r, g, b);

    const uint16_t expected_value = kMaxValue * pow(x, exponent);
    EXPECT_NEAR(r, expected_value, kEpsilon);
    EXPECT_NEAR(g, expected_value, kEpsilon);
    EXPECT_NEAR(b, expected_value, kEpsilon);
  }
  return true;
}

MATCHER(IsDefaultIdentity, "") {
  EXPECT_TRUE(arg.IsDefaultIdentity());
  return true;
}

class MockHardwareDisplayPlaneManager : public HardwareDisplayPlaneManager {
 public:
  explicit MockHardwareDisplayPlaneManager(DrmDevice* drm)
      : HardwareDisplayPlaneManager(drm) {}
  ~MockHardwareDisplayPlaneManager() override = default;

  MOCK_METHOD(bool,
              SetGammaCorrection,
              (uint32_t crtc_id,
               const display::GammaCurve& degamma,
               const display::GammaCurve& gamma),
              (override));

  bool Commit(CommitRequest commit_request, uint32_t flags) override {
    return false;
  }
  bool Commit(HardwareDisplayPlaneList* plane_list,
              scoped_refptr<PageFlipRequest> page_flip_request,
              gfx::GpuFenceHandle* release_fence) override {
    return false;
  }
  bool DisableOverlayPlanes(HardwareDisplayPlaneList* plane_list) override {
    return false;
  }
  bool SetColorCorrectionOnAllCrtcPlanes(
      uint32_t crtc_id,
      ScopedDrmColorCtmPtr ctm_blob_data) override {
    return false;
  }
  bool ValidatePrimarySize(const DrmOverlayPlane& primary,
                           const drmModeModeInfo& mode) override {
    return false;
  }
  void RequestPlanesReadyCallback(
      DrmOverlayPlaneList planes,
      base::OnceCallback<void(DrmOverlayPlaneList planes)> callback) override {
    return;
  }
  bool InitializePlanes() override { return false; }
  bool SetPlaneData(HardwareDisplayPlaneList* plane_list,
                    HardwareDisplayPlane* hw_plane,
                    const DrmOverlayPlane& overlay,
                    uint32_t crtc_id,
                    const gfx::Rect& src_rect) override {
    return false;
  }
  std::unique_ptr<HardwareDisplayPlane> CreatePlane(
      uint32_t plane_id) override {
    return nullptr;
  }
  bool IsCompatible(HardwareDisplayPlane* plane,
                    const DrmOverlayPlane& overlay,
                    uint32_t crtc_index) const override {
    return false;
  }
  bool CommitColorMatrix(const CrtcProperties& crtc_props) override {
    return false;
  }
  bool CommitGammaCorrection(const CrtcProperties& crtc_props) override {
    return false;
  }
};

}  // namespace

class DrmDisplayTest : public testing::Test {
 protected:
  DrmDisplayTest()
      : mock_drm_device_(base::MakeRefCounted<MockDrmDevice>(
            std::make_unique<MockGbmDevice>())) {
    auto info = GetDisplayInfo();
    auto snapshot = display::FakeDisplaySnapshot::Builder()
                        .SetId(123456)
                        .SetBaseConnectorId(info->connector()->connector_id)
                        .SetNativeMode(kNativeDisplaySize)
                        .SetCurrentMode(kNativeDisplaySize)
                        .SetColorSpace(gfx::ColorSpace::CreateSRGB())
                        .Build();
    drm_display_ =
        std::make_unique<DrmDisplay>(mock_drm_device_, info.get(), *snapshot);
  }

  MockHardwareDisplayPlaneManager* AddMockHardwareDisplayPlaneManager() {
    auto mock_hardware_display_plane_manager =
        std::make_unique<MockHardwareDisplayPlaneManager>(
            mock_drm_device_.get());
    MockHardwareDisplayPlaneManager* pointer =
        mock_hardware_display_plane_manager.get();
    mock_drm_device_->plane_manager_ =
        std::move(mock_hardware_display_plane_manager);
    return pointer;
  }

  base::test::TaskEnvironment env_;
  scoped_refptr<DrmDevice> mock_drm_device_;
  std::unique_ptr<DrmDisplay> drm_display_;
};

TEST_F(DrmDisplayTest, SetColorSpace) {
  drm_display_->set_is_hdr_capable_for_testing(true);
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  ON_CALL(*plane_manager, SetGammaCorrection(_, IsDefaultIdentity(), _))
      .WillByDefault(::testing::Return(true));

  const auto kHDRColorSpace = gfx::ColorSpace::CreateHDR10();
  EXPECT_CALL(*plane_manager,
              SetGammaCorrection(_, IsDefaultIdentity(), IsDefaultIdentity()));
  drm_display_->SetColorSpace(kHDRColorSpace);

  const auto kSDRColorSpace = gfx::ColorSpace::CreateREC709();
  constexpr float kSDRLevel = 0.85;
  constexpr float kExponent = 1.2;
  EXPECT_CALL(*plane_manager,
              SetGammaCorrection(_, IsDefaultIdentity(),
                                 MatchesPowerFunction(kSDRLevel, kExponent)));
  drm_display_->SetColorSpace(kSDRColorSpace);
}

TEST_F(DrmDisplayTest, SetEmptyGammaCorrectionNonHDRDisplay) {
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  ON_CALL(*plane_manager, SetGammaCorrection(_, _, _))
      .WillByDefault(::testing::Return(true));

  EXPECT_CALL(*plane_manager,
              SetGammaCorrection(_, IsDefaultIdentity(), IsDefaultIdentity()));
  drm_display_->SetGammaCorrection({}, {});
}

TEST_F(DrmDisplayTest, SetEmptyGammaCorrectionHDRDisplay) {
  drm_display_->set_is_hdr_capable_for_testing(true);
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  ON_CALL(*plane_manager, SetGammaCorrection(_, _, _))
      .WillByDefault(Return(true));

  constexpr float kSDRLevel = 0.85;
  constexpr float kExponent = 1.2;
  EXPECT_CALL(*plane_manager,
              SetGammaCorrection(_, IsDefaultIdentity(),
                                 MatchesPowerFunction(kSDRLevel, kExponent)));
  drm_display_->SetGammaCorrection({}, {});
}

}  // namespace ui
