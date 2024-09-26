// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_change_observer.h"

#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/math_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/util/display_manager_util.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range_f.h"

namespace display {

namespace {

float ComputeDpi(float diagonal_inch, const gfx::Size& resolution) {
  // We assume that displays have square pixel.
  float diagonal_pixel = std::sqrt(std::pow(resolution.width(), 2) +
                                   std::pow(resolution.height(), 2));
  return diagonal_pixel / diagonal_inch;
}

float ComputeDeviceScaleFactor(float dpi, const gfx::Size& resolution) {
  return DisplayChangeObserver::FindDeviceScaleFactor(dpi, resolution);
}

std::unique_ptr<DisplayMode> MakeDisplayMode(
    int width,
    int height,
    bool is_interlaced,
    float refresh_rate,
    const std::optional<float>& vsync_rate_min = std::nullopt) {
  return std::make_unique<DisplayMode>(gfx::Size{width, height}, is_interlaced,
                                       refresh_rate, vsync_rate_min);
}

}  // namespace

class DisplayChangeObserverTestBase : public testing::Test {
 public:
  DisplayChangeObserverTestBase() = default;

  DisplayChangeObserverTestBase(const DisplayChangeObserverTestBase&) = delete;
  DisplayChangeObserverTestBase& operator=(
      const DisplayChangeObserverTestBase&) = delete;

  ~DisplayChangeObserverTestBase() override = default;

  // Pass through method to be called by individual test cases.
  ManagedDisplayInfo CreateManagedDisplayInfo(DisplayChangeObserver* observer,
                                              const DisplaySnapshot* snapshot,
                                              const DisplayMode* mode_info) {
    return observer->CreateManagedDisplayInfoInternal(snapshot, mode_info);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DisplayChangeObserverTest : public DisplayChangeObserverTestBase,
                                  public testing::WithParamInterface<bool> {
 public:
  DisplayChangeObserverTest() = default;

  DisplayChangeObserverTest(const DisplayChangeObserverTest&) = delete;
  DisplayChangeObserverTest& operator=(const DisplayChangeObserverTest&) =
      delete;

  ~DisplayChangeObserverTest() override = default;

  // DisplayChangeObserverTestBase:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(features::kListAllDisplayModes);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kListAllDisplayModes);
    }

    DisplayChangeObserverTestBase::SetUp();
  }
};

class DisplayChangeObserverPanelRadiiTest
    : public DisplayChangeObserverTestBase {
 public:
  DisplayChangeObserverPanelRadiiTest() = default;

  DisplayChangeObserverPanelRadiiTest(
      const DisplayChangeObserverPanelRadiiTest&) = delete;
  DisplayChangeObserverPanelRadiiTest& operator=(
      const DisplayChangeObserverPanelRadiiTest&) = delete;

  ~DisplayChangeObserverPanelRadiiTest() override = default;

  // testing::Test:
  void SetUp() override {
    display_manager_ = std::make_unique<DisplayManager>(/*screen=*/nullptr);
    default_display_mode_ = MakeDisplayMode(1920, 1080, true, 60);
    scoped_feature_list_.InitAndEnableFeature(features::kRoundedDisplay);

    ui::DeviceDataManager::CreateInstance();
    DisplayChangeObserverTestBase::SetUp();
  }

  void InitializeDisplayChangeObserver() {
    display_change_observer_ =
        std::make_unique<DisplayChangeObserver>(display_manager_.get());
  }

 protected:
  base::test::ScopedCommandLine command_line_;
  std::unique_ptr<DisplayManager> display_manager_;
  std::unique_ptr<DisplayChangeObserver> display_change_observer_;
  std::unique_ptr<DisplayMode> default_display_mode_;
};

TEST_F(DisplayChangeObserverPanelRadiiTest, RadiiSpecifiedForInternalDisplay) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kDisplayProperties,
      "[{\"connector-type\": 14, \"rounded-corners\": {\"bottom-left\": 15, "
      "\"bottom-right\": 15, \"top-left\": 16, \"top-right\": 16}}]");

  InitializeDisplayChangeObserver();

  // Radii specified for the connection protocol.
  std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
          .SetType(
              display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .Build();

  const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
      display_change_observer_.get(), display_snapshot.get(),
      default_display_mode_.get());

  EXPECT_EQ(display_info.panel_corners_radii(),
            gfx::RoundedCornersF(16, 16, 15, 15));
}

TEST_F(DisplayChangeObserverPanelRadiiTest, IgnoreRadiiIfNotInternalDisplay) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kDisplayProperties,
      "[{\"connector-type\": 15, \"rounded-corners\": {\"bottom-left\": 15, "
      "\"bottom-right\": 15, \"top-left\": 16, \"top-right\": 16}}]");

  InitializeDisplayChangeObserver();

  // The snapshot is of a display that is not a internal display.
  std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
          .SetType(display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_HDMI)
          .Build();

  const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
      display_change_observer_.get(), display_snapshot.get(),
      default_display_mode_.get());

  EXPECT_TRUE(display_info.panel_corners_radii().IsEmpty());
}

TEST_P(DisplayChangeObserverTest, GetExternalManagedDisplayModeList) {
  std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(MakeDisplayMode(1920, 1200, false, 60))
          // Same size as native mode but with higher refresh rate.
          .AddMode(MakeDisplayMode(1920, 1200, false, 75))
          // All non-interlaced (as would be seen with different refresh rates).
          .AddMode(MakeDisplayMode(1920, 1080, false, 80))
          .AddMode(MakeDisplayMode(1920, 1080, false, 70))
          .AddMode(MakeDisplayMode(1920, 1080, false, 60))
          // Interlaced vs non-interlaced.
          .AddMode(MakeDisplayMode(1280, 720, true, 60))
          .AddMode(MakeDisplayMode(1280, 720, false, 60))
          // Interlaced only.
          .AddMode(MakeDisplayMode(1024, 768, true, 70))
          .AddMode(MakeDisplayMode(1024, 768, true, 60))
          // Mixed.
          .AddMode(MakeDisplayMode(1024, 600, true, 60))
          .AddMode(MakeDisplayMode(1024, 600, false, 70))
          .AddMode(MakeDisplayMode(1024, 600, false, 60))
          // Just one interlaced mode.
          .AddMode(MakeDisplayMode(640, 480, true, 60))
          .Build();

  ManagedDisplayInfo::ManagedDisplayModeList display_modes =
      DisplayChangeObserver::GetExternalManagedDisplayModeList(
          *display_snapshot);

  const bool listing_all_modes = GetParam();
  if (listing_all_modes) {
    ASSERT_EQ(13u, display_modes.size());
    EXPECT_EQ(gfx::Size(640, 480), display_modes[0].size());
    EXPECT_TRUE(display_modes[0].is_interlaced());
    EXPECT_EQ(display_modes[0].refresh_rate(), 60);

    EXPECT_EQ(gfx::Size(1024, 600), display_modes[1].size());
    EXPECT_FALSE(display_modes[1].is_interlaced());
    EXPECT_EQ(display_modes[1].refresh_rate(), 60);
    EXPECT_EQ(gfx::Size(1024, 600), display_modes[2].size());
    EXPECT_TRUE(display_modes[2].is_interlaced());
    EXPECT_EQ(display_modes[2].refresh_rate(), 60);
    EXPECT_EQ(gfx::Size(1024, 600), display_modes[3].size());
    EXPECT_FALSE(display_modes[3].is_interlaced());
    EXPECT_EQ(display_modes[3].refresh_rate(), 70);

    EXPECT_EQ(gfx::Size(1024, 768), display_modes[4].size());
    EXPECT_TRUE(display_modes[4].is_interlaced());
    EXPECT_EQ(display_modes[4].refresh_rate(), 60);
    EXPECT_EQ(gfx::Size(1024, 768), display_modes[5].size());
    EXPECT_TRUE(display_modes[5].is_interlaced());
    EXPECT_EQ(display_modes[5].refresh_rate(), 70);

    EXPECT_EQ(gfx::Size(1280, 720), display_modes[6].size());
    EXPECT_FALSE(display_modes[6].is_interlaced());
    EXPECT_EQ(display_modes[6].refresh_rate(), 60);
    EXPECT_EQ(gfx::Size(1280, 720), display_modes[7].size());
    EXPECT_TRUE(display_modes[7].is_interlaced());
    EXPECT_EQ(display_modes[7].refresh_rate(), 60);

    EXPECT_EQ(gfx::Size(1920, 1080), display_modes[8].size());
    EXPECT_FALSE(display_modes[8].is_interlaced());
    EXPECT_EQ(display_modes[8].refresh_rate(), 60);
    EXPECT_EQ(gfx::Size(1920, 1080), display_modes[9].size());
    EXPECT_FALSE(display_modes[9].is_interlaced());
    EXPECT_EQ(display_modes[9].refresh_rate(), 70);
    EXPECT_EQ(gfx::Size(1920, 1080), display_modes[10].size());
    EXPECT_FALSE(display_modes[10].is_interlaced());
    EXPECT_EQ(display_modes[10].refresh_rate(), 80);

    EXPECT_EQ(gfx::Size(1920, 1200), display_modes[11].size());
    EXPECT_FALSE(display_modes[11].is_interlaced());
    EXPECT_EQ(display_modes[11].refresh_rate(), 60);

    EXPECT_EQ(gfx::Size(1920, 1200), display_modes[12].size());
    EXPECT_FALSE(display_modes[12].is_interlaced());
    EXPECT_EQ(display_modes[12].refresh_rate(), 75);
  } else {
    ASSERT_EQ(6u, display_modes.size());
    EXPECT_EQ(gfx::Size(640, 480), display_modes[0].size());
    EXPECT_TRUE(display_modes[0].is_interlaced());
    EXPECT_EQ(display_modes[0].refresh_rate(), 60);

    EXPECT_EQ(gfx::Size(1024, 600), display_modes[1].size());
    EXPECT_FALSE(display_modes[1].is_interlaced());
    EXPECT_EQ(display_modes[1].refresh_rate(), 70);

    EXPECT_EQ(gfx::Size(1024, 768), display_modes[2].size());
    EXPECT_TRUE(display_modes[2].is_interlaced());
    EXPECT_EQ(display_modes[2].refresh_rate(), 70);

    EXPECT_EQ(gfx::Size(1280, 720), display_modes[3].size());
    EXPECT_FALSE(display_modes[3].is_interlaced());
    EXPECT_EQ(display_modes[3].refresh_rate(), 60);

    EXPECT_EQ(gfx::Size(1920, 1080), display_modes[4].size());
    EXPECT_FALSE(display_modes[4].is_interlaced());
    EXPECT_EQ(display_modes[4].refresh_rate(), 80);

    EXPECT_EQ(gfx::Size(1920, 1200), display_modes[5].size());
    EXPECT_FALSE(display_modes[5].is_interlaced());
    EXPECT_EQ(display_modes[5].refresh_rate(), 60);
  }
}

TEST_P(DisplayChangeObserverTest, GetEmptyExternalManagedDisplayModeList) {
  DisplaySnapshot::ColorInfo color_info;
  FakeDisplaySnapshot display_snapshot(
      /*display_id=*/123, /*port_display_id=*/123, /*edid_display_id=*/456,
      /*connector_index=*/0x0001, gfx::Point(), gfx::Size(),
      DISPLAY_CONNECTION_TYPE_UNKNOWN,
      /*base_connector_id=*/1u, /*path_topology=*/{}, false, false,
      PrivacyScreenState::kNotSupported, false, std::string(), base::FilePath(),
      {}, nullptr, nullptr, 0, gfx::Size(), color_info,
      VariableRefreshRateState::kVrrNotCapable, DrmFormatsAndModifiers());

  ManagedDisplayInfo::ManagedDisplayModeList display_modes =
      DisplayChangeObserver::GetExternalManagedDisplayModeList(
          display_snapshot);
  EXPECT_EQ(0u, display_modes.size());
}

bool IsDpiOutOfRange(float dpi) {
  // http://go/cros-ppi-spectrum
  constexpr gfx::RangeF good_ranges[] = {
      {125.f, 165.f},
      {180.f, 210.f},
      {220.f, 265.f},
      {270.f, 350.f},
  };
  for (auto& range : good_ranges) {
    if (range.start() <= dpi && range.end() > dpi)
      return true;
  }
  return false;
}

TEST_P(DisplayChangeObserverTest, FindDeviceScaleFactor) {
  // Validation check
  EXPECT_EQ(1.25f,
            DisplayChangeObserver::FindDeviceScaleFactor(150, gfx::Size()));
  EXPECT_EQ(1.6f,
            DisplayChangeObserver::FindDeviceScaleFactor(180, gfx::Size()));
  EXPECT_EQ(kDsf_1_777,
            DisplayChangeObserver::FindDeviceScaleFactor(220, gfx::Size()));
  EXPECT_EQ(2.f,
            DisplayChangeObserver::FindDeviceScaleFactor(230, gfx::Size()));
  EXPECT_EQ(2.4f,
            DisplayChangeObserver::FindDeviceScaleFactor(270, gfx::Size()));
  EXPECT_EQ(kDsf_2_252, DisplayChangeObserver::FindDeviceScaleFactor(
                            0, gfx::Size(3000, 2000)));
  EXPECT_EQ(kDsf_2_666,
            DisplayChangeObserver::FindDeviceScaleFactor(310, gfx::Size()));

  std::set<std::tuple<float, int, int>> dup_check;

  for (auto& entry : display_configs) {
    std::tuple<float, int, int> key{entry.diagonal_size,
                                    entry.resolution.width(),
                                    entry.resolution.height()};
    DCHECK(!dup_check.count(key));
    dup_check.emplace(key);

    SCOPED_TRACE(base::StringPrintf(
        "%dx%d, diag=%1.3f inch, expected=%1.10f", entry.resolution.width(),
        entry.resolution.height(), entry.diagonal_size, entry.expected_dsf));

    float dpi = ComputeDpi(entry.diagonal_size, entry.resolution);
    // Check ScaleFactor.
    float scale_factor = ComputeDeviceScaleFactor(dpi, entry.resolution);
    EXPECT_EQ(entry.expected_dsf, scale_factor);
    bool bad_range = !IsDpiOutOfRange(dpi);
    EXPECT_EQ(bad_range, entry.bad_range);

    // Check DP size.
    gfx::ScaleToCeiledSize(entry.resolution, 1.f / scale_factor);

    const gfx::Size dp_size =
        gfx::ScaleToCeiledSize(entry.resolution, 1.f / scale_factor);

    // Check Screenshot size.
    EXPECT_EQ(entry.expected_dp_size, dp_size);
    gfx::Transform transform;
    transform.Scale(scale_factor, scale_factor);
    const gfx::Size screenshot_size =
        cc::MathUtil::MapEnclosingClippedRect(transform, gfx::Rect(dp_size))
            .size();
    switch (entry.screenshot_size_error) {
      case kEpsilon: {
        EXPECT_NE(entry.resolution, screenshot_size);
        constexpr float kEpsilon = 0.001f;
        EXPECT_EQ(entry.resolution,
                  cc::MathUtil::MapEnclosingClippedRectIgnoringError(
                      transform, gfx::Rect(dp_size), kEpsilon)
                      .size());
        break;
      }
      case kExact:
        EXPECT_EQ(entry.resolution, screenshot_size);
        break;
      case kSkip:
        break;
    }
  }

  float max_scale_factor = kDsf_2_666;
  // Erroneous values should still work.
  EXPECT_EQ(1.0f,
            DisplayChangeObserver::FindDeviceScaleFactor(-100.0f, gfx::Size()));
  EXPECT_EQ(1.0f,
            DisplayChangeObserver::FindDeviceScaleFactor(0.0f, gfx::Size()));
  EXPECT_EQ(max_scale_factor, DisplayChangeObserver::FindDeviceScaleFactor(
                                  10000.0f, gfx::Size()));
}

TEST_P(DisplayChangeObserverTest,
       FindExternalDisplayNativeModeWhenOverwritten) {
  std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
          .AddMode(MakeDisplayMode(1920, 1080, false, 60))
          .Build();

  ManagedDisplayInfo::ManagedDisplayModeList display_modes =
      DisplayChangeObserver::GetExternalManagedDisplayModeList(
          *display_snapshot);

  const bool listing_all_modes = GetParam();

  if (listing_all_modes) {
    ASSERT_EQ(2u, display_modes.size());
    EXPECT_EQ(gfx::Size(1920, 1080), display_modes[0].size());
    EXPECT_FALSE(display_modes[0].is_interlaced());
    EXPECT_FALSE(display_modes[0].native());
    EXPECT_EQ(display_modes[0].refresh_rate(), 60);

    EXPECT_EQ(gfx::Size(1920, 1080), display_modes[1].size());
    EXPECT_TRUE(display_modes[1].is_interlaced());
    EXPECT_TRUE(display_modes[1].native());
    EXPECT_EQ(display_modes[1].refresh_rate(), 60);
  } else {
    // Only the native mode will be listed.
    ASSERT_EQ(1u, display_modes.size());
    EXPECT_EQ(gfx::Size(1920, 1080), display_modes[0].size());
    EXPECT_TRUE(display_modes[0].is_interlaced());
    EXPECT_TRUE(display_modes[0].native());
    EXPECT_EQ(display_modes[0].refresh_rate(), 60);
  }
}

TEST_P(DisplayChangeObserverTest, InvalidDisplayColorSpaces) {
  const std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetName("AmazingFakeDisplay")
          .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
          .SetColorSpace(gfx::ColorSpace())
          .Build();

  ui::DeviceDataManager::CreateInstance();
  DisplayManager manager(nullptr);
  const auto display_mode = MakeDisplayMode(1920, 1080, true, 60);
  DisplayChangeObserver observer(&manager);
  const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
      &observer, display_snapshot.get(), display_mode.get());

  EXPECT_EQ(display_info.bits_per_channel(), 8u);
  const auto display_color_spaces = display_info.display_color_spaces();
  EXPECT_FALSE(display_color_spaces.SupportsHDR());

  EXPECT_EQ(
      DisplaySnapshot::PrimaryFormat(),
      display_color_spaces.GetOutputBufferFormat(gfx::ContentColorUsage::kSRGB,
                                                 /*needs_alpha=*/true));

  const auto color_space = display_color_spaces.GetRasterColorSpace();
  // DisplayColorSpaces will fix an invalid ColorSpace to return sRGB.
  EXPECT_TRUE(color_space.IsValid());
  EXPECT_EQ(color_space, gfx::ColorSpace::CreateSRGB());
}

TEST_P(DisplayChangeObserverTest, SDRDisplayColorSpaces) {
  const std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetName("AmazingFakeDisplay")
          .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
          .SetColorSpace(gfx::ColorSpace::CreateSRGB())
          .Build();

  ui::DeviceDataManager::CreateInstance();
  DisplayManager manager(nullptr);
  const auto display_mode = MakeDisplayMode(1920, 1080, true, 60);
  DisplayChangeObserver observer(&manager);
  const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
      &observer, display_snapshot.get(), display_mode.get());

  EXPECT_EQ(display_info.bits_per_channel(), 8u);

  const auto display_color_spaces = display_info.display_color_spaces();
  EXPECT_FALSE(display_color_spaces.SupportsHDR());

  EXPECT_EQ(
      DisplaySnapshot::PrimaryFormat(),
      display_color_spaces.GetOutputBufferFormat(gfx::ContentColorUsage::kSRGB,
                                                 /*needs_alpha=*/true));

  const auto color_space = display_color_spaces.GetRasterColorSpace();
  EXPECT_TRUE(color_space.IsValid());
  EXPECT_EQ(color_space.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT709);
  EXPECT_EQ(color_space.GetTransferID(), gfx::ColorSpace::TransferID::SRGB);
}

TEST_P(DisplayChangeObserverTest, WCGDisplayColorSpaces) {
  const std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetName("AmazingFakeDisplay")
          .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
          .SetColorSpace(gfx::ColorSpace::CreateDisplayP3D65())
          .Build();

  ui::DeviceDataManager::CreateInstance();
  DisplayManager manager(nullptr);
  const auto display_mode = MakeDisplayMode(1920, 1080, true, 60);
  DisplayChangeObserver observer(&manager);
  const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
      &observer, display_snapshot.get(), display_mode.get());

  EXPECT_EQ(display_info.bits_per_channel(), 8u);

  const auto display_color_spaces = display_info.display_color_spaces();
  EXPECT_FALSE(display_color_spaces.SupportsHDR());

  EXPECT_EQ(
      DisplaySnapshot::PrimaryFormat(),
      display_color_spaces.GetOutputBufferFormat(gfx::ContentColorUsage::kSRGB,
                                                 /*needs_alpha=*/true));

  const auto color_space = display_color_spaces.GetRasterColorSpace();
  EXPECT_TRUE(color_space.IsValid());
  EXPECT_EQ(color_space.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT709);
  EXPECT_EQ(color_space.GetTransferID(), gfx::ColorSpace::TransferID::SRGB);
}

TEST_P(DisplayChangeObserverTest, HDRDisplayColorSpaces) {
  // TODO(crbug.com/40652358): Remove this flag and provision when HDR is fully
  // supported on ChromeOS.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kEnableExternalDisplayHDR10Mode);

  const auto display_color_space = gfx::ColorSpace::CreateHDR10();
  const std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetName("AmazingFakeDisplay")
          .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
          .SetColorSpace(display_color_space)
          .SetBitsPerChannel(10u)
          .SetHDRStaticMetadata(
              {609.0, 500.0, 0.01,
               gfx::HDRStaticMetadata::EotfMask({
                   gfx::HDRStaticMetadata::Eotf::kGammaSdrRange,
                   gfx::HDRStaticMetadata::Eotf::kPq,
               })})
          .Build();

  ui::DeviceDataManager::CreateInstance();
  DisplayManager manager(nullptr);
  const auto display_mode = MakeDisplayMode(1920, 1080, true, 60);
  DisplayChangeObserver observer(&manager);
  const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
      &observer, display_snapshot.get(), display_mode.get());

  EXPECT_EQ(display_info.bits_per_channel(), 10u);

  const auto display_color_spaces = display_info.display_color_spaces();
  EXPECT_TRUE(display_color_spaces.SupportsHDR());

  // Ensure that all spaces be HDR10, and have headroom of 3x (609/203).
  EXPECT_EQ(
      gfx::BufferFormat::RGBA_1010102,
      display_color_spaces.GetOutputBufferFormat(gfx::ContentColorUsage::kSRGB,
                                                 /*needs_alpha=*/true));
  EXPECT_EQ(
      gfx::ColorSpace::CreateHDR10(),
      display_color_spaces.GetOutputColorSpace(gfx::ContentColorUsage::kSRGB,
                                               /*needs_alpha=*/true));
  EXPECT_EQ(
      gfx::BufferFormat::RGBA_1010102,
      display_color_spaces.GetOutputBufferFormat(gfx::ContentColorUsage::kHDR,
                                                 /*needs_alpha=*/true));
  EXPECT_EQ(
      gfx::ColorSpace::CreateHDR10(),
      display_color_spaces.GetOutputColorSpace(gfx::ContentColorUsage::kHDR,
                                               /*needs_alpha=*/true));
  EXPECT_EQ(kDefaultHdrMaxLuminanceRelative,
            display_color_spaces.GetHDRMaxLuminanceRelative());
}

TEST_P(DisplayChangeObserverTest, VSyncRateMin) {
  ui::DeviceDataManager::CreateInstance();
  DisplayManager manager(nullptr);
  DisplayChangeObserver observer(&manager);

  // Verify that vsync_rate_min is absent from DisplayInfo when it is not
  // present from the DisplayMode.
  {
    const std::unique_ptr<DisplaySnapshot> display_snapshot =
        FakeDisplaySnapshot::Builder()
            .SetId(123)
            .SetName("AmazingFakeDisplay")
            .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
            .Build();
    const std::unique_ptr<DisplayMode> display_mode =
        MakeDisplayMode(1920, 1080, true, 60);
    const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
        &observer, display_snapshot.get(), display_mode.get());

    EXPECT_EQ(display_info.vsync_rate_min(), std::nullopt);
  }

  // Verify that the value of vsync_rate_min is correctly taken from the display
  // mode.
  {
    const std::unique_ptr<DisplaySnapshot> display_snapshot =
        FakeDisplaySnapshot::Builder()
            .SetId(123)
            .SetName("AmazingFakeDisplay")
            .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
            .Build();
    const std::unique_ptr<DisplayMode> display_mode =
        MakeDisplayMode(1920, 1080, true, 60, 48.000488f);
    const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
        &observer, display_snapshot.get(), display_mode.get());

    EXPECT_EQ(display_info.vsync_rate_min(), 48.000488f);
  }
}

TEST_P(DisplayChangeObserverTest, DisplayModeNativeCalculation) {
  ui::DeviceDataManager::CreateInstance();
  DisplayManager manager(nullptr);
  DisplayChangeObserver observer(&manager);

  // For external display, verify that native attribute is determined by
  // comparing current mode with the DisplaySnapshot's native mode. Native is
  // true when they are the same.
  {
    const std::unique_ptr<DisplaySnapshot> display_snapshot =
        FakeDisplaySnapshot::Builder()
            .SetId(123)
            .SetType(DISPLAY_CONNECTION_TYPE_DISPLAYPORT)
            .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
            .SetCurrentMode(MakeDisplayMode(1920, 1080, true, 60))
            .Build();

    const DisplayMode* display_mode = display_snapshot->current_mode();
    const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
        &observer, display_snapshot.get(), display_mode);

    EXPECT_TRUE(display_info.native());
  }

  // For external display, verify that native attribute is determined by
  // comparing current mode with the DisplaySnapshot's native mode. Native is
  // false when they are different.
  {
    const std::unique_ptr<DisplaySnapshot> display_snapshot =
        FakeDisplaySnapshot::Builder()
            .SetId(123)
            .SetType(DISPLAY_CONNECTION_TYPE_DISPLAYPORT)
            .SetNativeMode(MakeDisplayMode(3840, 2160, true, 60))
            .SetCurrentMode(MakeDisplayMode(1920, 1080, true, 60))
            .Build();

    const DisplayMode* display_mode = display_snapshot->current_mode();
    const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
        &observer, display_snapshot.get(), display_mode);

    EXPECT_FALSE(display_info.native());
  }

  // For internal display, verify that native attribute is always true.
  {
    const std::unique_ptr<DisplaySnapshot> display_snapshot =
        FakeDisplaySnapshot::Builder()
            .SetId(123)
            .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
            .SetCurrentMode(MakeDisplayMode(1920, 1080, true, 60))
            .Build();

    const DisplayMode* display_mode = display_snapshot->current_mode();
    const ManagedDisplayInfo display_info = CreateManagedDisplayInfo(
        &observer, display_snapshot.get(), display_mode);

    EXPECT_TRUE(display_info.native());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         DisplayChangeObserverTest,
                         ::testing::Values(false, true));

using DisplayResolutionTest = testing::Test;

auto CreateDisplay = [](const ManagedDisplayInfo& managed_display_info) {
  Display display(/*id=*/1);
  const float effective_scale =
      managed_display_info.GetEffectiveDeviceScaleFactor();
  display.SetScaleAndBounds(effective_scale,
                            managed_display_info.bounds_in_native());
  EXPECT_EQ(effective_scale, display.device_scale_factor());
  return display;
};

TEST_F(DisplayResolutionTest, CheckEffectiveResolutionUMAIndex) {
  std::map<int, gfx::Size> logical_resolutions;
  for (const auto& display_config : display_configs) {
    gfx::Size size = display_config.resolution;
    if (size.width() < size.height())
      size = gfx::Size(size.height(), size.width());

    const float dsf = display_config.expected_dsf;

    std::array<float, kNumOfZoomFactors> zoom_levels;
    bool found = false;
    if (dsf == 1.f) {
      for (const ZoomListBucket& zoom_list_bucket : kZoomListBuckets) {
        if (size.width() >= zoom_list_bucket.first) {
          zoom_levels = zoom_list_bucket.second;
          found = true;
        }
      }
    } else {
      for (const ZoomListBucketDsf& zoom_list_bucket : kZoomListBucketsForDsf) {
        if (cc::MathUtil::IsWithinEpsilon(dsf, zoom_list_bucket.first)) {
          zoom_levels = zoom_list_bucket.second;
          found = true;
        }
      }
    }
    EXPECT_TRUE(found);
    for (float zoom_level : zoom_levels) {
      ManagedDisplayInfo info;
      info.set_device_scale_factor(dsf);
      info.set_zoom_factor(zoom_level);
      info.SetBounds(gfx::Rect(size));

      Display display = CreateDisplay(info);

      gfx::Size logical_resolution = display.size();
      gfx::Size portrait_logical_resolution = logical_resolution;
      portrait_logical_resolution.Transpose();

      const int landscape_key =
          logical_resolution.width() * logical_resolution.height();
      const int portrait_key = landscape_key - 1;

      auto it = logical_resolutions.find(landscape_key);
      if (it != logical_resolutions.end()) {
        EXPECT_EQ(it->second, logical_resolution);
      } else {
        logical_resolutions[landscape_key] = logical_resolution;
      }

      it = logical_resolutions.find(portrait_key);
      if (it != logical_resolutions.end()) {
        EXPECT_EQ(it->second, portrait_logical_resolution);
      } else {
        logical_resolutions[portrait_key] = portrait_logical_resolution;
      }
    }
  }

#if 0
  // Enable this code to re-generate the "EffectiveResolution" in enums.xml.
  for (auto pair : logical_resolutions) {
    std::cout << "  <int value=\"" << pair.first << "\" label=\""
               << pair.second.width() << " x " << pair.second.height()
              << "\"/>" << std::endl;
  }
#endif

  // With the current set of display configs and zoom levels, there are only 322
  // possible effective resolutions for internal displays in chromebooks. Update
  // this value when adding a new display config, and re-generate the
  // EffectiveResolution value in enum.xml.
  EXPECT_EQ(logical_resolutions.size(), 322ul);
}

// Make sure that when display zoom is applied, the effective device scale
// factor (device_scale_factor * zoomfactor) and the rational number (pixel
// width / logical with) is close enough (<kDeviceScaleFactorErrorTolerance).
TEST_F(DisplayResolutionTest, DisplayZoom) {
  // For internal displays
  for (auto& config : display_configs) {
    const float dpi = ComputeDpi(config.diagonal_size, config.resolution);
    const auto snapshot = FakeDisplaySnapshot::Builder()
                              .SetId(10)
                              .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                              .SetNativeMode(config.resolution)
                              .SetCurrentMode(config.resolution)
                              .SetDPI(dpi)
                              .Build();
    const auto* native_mode = snapshot->native_mode();
    auto managed_display_info = DisplayChangeObserver::CreateManagedDisplayInfo(
        snapshot.get(), native_mode,
        /*native=*/true, config.expected_dsf, dpi, std::string());
    const std::vector<float> zooms =
        GetDisplayZoomFactors(managed_display_info.display_modes()[0]);
    // For default scale factor, they should be the same.
    EXPECT_NEAR(config.expected_dsf,
                managed_display_info.GetEffectiveDeviceScaleFactor(),
                0.0000001);

    for (auto zoom : zooms) {
      managed_display_info.set_zoom_factor(zoom);
      const Display display = CreateDisplay(managed_display_info);

      // Emulate how lacros computes the scale factor.
      const float scale_factor = config.resolution.width() /
                                 static_cast<float>(display.size().width());
      EXPECT_NEAR(scale_factor, display.device_scale_factor(),
                  kDeviceScaleFactorErrorTolerance);
    }
  }

  // Typical external display sizes.
  constexpr gfx::Size kExternalDisplaySizes[] = {
      {4096, 2160}, {3840, 2160}, {3440, 1440}, {2560, 1600}, {2560, 1440},
      {1920, 1200}, {1920, 1080}, {1600, 900},  {1440, 900}};
  for (auto& size : kExternalDisplaySizes) {
    const auto snapshot = FakeDisplaySnapshot::Builder()
                              .SetId(10)
                              .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                              .SetNativeMode(size)
                              .SetCurrentMode(size)
                              .Build();
    const auto* native_mode = snapshot->native_mode();
    auto managed_display_info = DisplayChangeObserver::CreateManagedDisplayInfo(
        snapshot.get(), native_mode,
        /*native=*/true, /*device_scale_factor=*/1.0f, /*dpi=*/160,
        std::string());
    const std::vector<float> zooms =
        GetDisplayZoomFactors(managed_display_info.display_modes()[0]);

    for (auto zoom : zooms) {
      managed_display_info.set_zoom_factor(zoom);
      const Display display = CreateDisplay(managed_display_info);

      // Emulate how lacros computes the scale factor.
      const float scale_factor =
          size.width() / static_cast<float>(display.size().width());
      EXPECT_NEAR(scale_factor, display.device_scale_factor(),
                  kDeviceScaleFactorErrorTolerance);
    }
  }
}

}  // namespace display
