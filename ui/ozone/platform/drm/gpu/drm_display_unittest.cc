// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <utility>

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SizeIs;

// Verifies that the argument goes from 0 to the maximum uint16_t times |scale|
// following a power function with |exponent|.
MATCHER_P2(MatchesPowerFunction, scale, exponent, "") {
  EXPECT_FALSE(arg.empty());

  const uint16_t max_value = std::numeric_limits<uint16_t>::max() * scale;

  float i = 1.0;
  for (const auto rgb_value : arg) {
    const uint16_t expected_value = max_value * pow(i / arg.size(), exponent);
    i++;
    EXPECT_NEAR(rgb_value.r, expected_value, 1.0);
    EXPECT_NEAR(rgb_value.g, expected_value, 1.0);
    EXPECT_NEAR(rgb_value.b, expected_value, 1.0);
  }

  return true;
}

namespace ui {

namespace {

class MockHardwareDisplayPlaneManager : public HardwareDisplayPlaneManager {
 public:
  explicit MockHardwareDisplayPlaneManager(DrmDevice* drm)
      : HardwareDisplayPlaneManager(drm) {}
  ~MockHardwareDisplayPlaneManager() override = default;

  MOCK_METHOD(bool,
              SetGammaCorrection,
              (uint32_t crtc_id,
               const std::vector<display::GammaRampRGBEntry>& degamma_lut,
               const std::vector<display::GammaRampRGBEntry>& gamma_lut),
              (override));
  MOCK_METHOD(bool,
              SetVrrEnabled,
              (uint32_t crtc_id, bool vrr_enabled),
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
            std::make_unique<MockGbmDevice>())),
        drm_display_(mock_drm_device_) {}

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
  DrmDisplay drm_display_;
};

TEST_F(DrmDisplayTest, SetColorSpace) {
  drm_display_.set_is_hdr_capable_for_testing(true);
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  ON_CALL(*plane_manager, SetGammaCorrection(_, SizeIs(0), _))
      .WillByDefault(::testing::Return(true));

  const auto kHDRColorSpace = gfx::ColorSpace::CreateHDR10();
  EXPECT_CALL(*plane_manager, SetGammaCorrection(_, SizeIs(0), SizeIs(0)));
  drm_display_.SetColorSpace(kHDRColorSpace);

  const auto kSDRColorSpace = gfx::ColorSpace::CreateREC709();
  constexpr float kSDRLevel = 0.85;
  constexpr float kExponent = 1.2;
  EXPECT_CALL(*plane_manager,
              SetGammaCorrection(_, SizeIs(0),
                                 MatchesPowerFunction(kSDRLevel, kExponent)));
  drm_display_.SetColorSpace(kSDRColorSpace);
}

TEST_F(DrmDisplayTest, SetEmptyGammaCorrectionNonHDRDisplay) {
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  ON_CALL(*plane_manager, SetGammaCorrection(_, _, _))
      .WillByDefault(::testing::Return(true));

  EXPECT_CALL(*plane_manager, SetGammaCorrection(_, SizeIs(0), SizeIs(0)));
  drm_display_.SetGammaCorrection(std::vector<display::GammaRampRGBEntry>(),
                                  std::vector<display::GammaRampRGBEntry>());
}

TEST_F(DrmDisplayTest, SetEmptyGammaCorrectionHDRDisplay) {
  drm_display_.set_is_hdr_capable_for_testing(true);
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  ON_CALL(*plane_manager, SetGammaCorrection(_, _, _))
      .WillByDefault(Return(true));

  constexpr float kSDRLevel = 0.85;
  constexpr float kExponent = 1.2;
  EXPECT_CALL(*plane_manager,
              SetGammaCorrection(_, SizeIs(0),
                                 MatchesPowerFunction(kSDRLevel, kExponent)));
  drm_display_.SetGammaCorrection(std::vector<display::GammaRampRGBEntry>(),
                                  std::vector<display::GammaRampRGBEntry>());
}

TEST_F(DrmDisplayTest, SetVrrEnabled) {
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  EXPECT_CALL(*plane_manager, SetVrrEnabled(_, _)).WillOnce(Return(false));
  EXPECT_FALSE(drm_display_.SetVrrEnabled(true));

  EXPECT_CALL(*plane_manager, SetVrrEnabled(_, _)).WillOnce(Return(true));
  EXPECT_TRUE(drm_display_.SetVrrEnabled(true));
}

}  // namespace ui
