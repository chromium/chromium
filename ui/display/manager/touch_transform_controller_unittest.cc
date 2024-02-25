// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/touch_transform_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/default_touch_transform_setter.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/touch_device_manager_test_api.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/display/screen_base.h"
#include "ui/events/devices/device_data_manager.h"

namespace display::test {

namespace {

constexpr int kDisplayId1 = 1;
constexpr int kDisplayId2 = 2;
constexpr int kTouchId1 = 5;
constexpr int kTouchId2 = 6;

ui::TouchDeviceTransform CreateTouchDeviceTransform(
    int64_t display_id,
    int32_t device_id,
    const gfx::Transform& transform,
    double radius_scale = 1.0) {
  ui::TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = display_id;
  touch_device_transform.device_id = device_id;
  touch_device_transform.transform = transform;
  touch_device_transform.radius_scale = radius_scale;
  return touch_device_transform;
}

ui::TouchscreenDevice CreateTouchscreenDevice(unsigned int id,
                                              const gfx::Size& size) {
  return ui::TouchscreenDevice(id, ui::InputDeviceType::INPUT_DEVICE_USB,
                               std::string(), size, 0);
}

std::string GetTouchPointString(
    const TouchCalibrationData::CalibrationPointPairQuad& pts) {
  std::string str = "Failed for point pairs: ";
  for (std::size_t row = 0; row < pts.size(); row++) {
    str += "{(" + base::NumberToString(pts[row].first.x()) + "," +
           base::NumberToString(pts[row].first.y()) + "), (" +
           base::NumberToString(pts[row].second.x()) + "," +
           base::NumberToString(pts[row].second.y()) + ")} ";
  }
  return str;
}

// Checks if the touch input has been calibrated properly. The input is said to
// be calibrated if any touch input is transformed to the correct corresponding
// display point within an error delta of |max_error_delta.width()| along the X
// axis and |max_error_delta.height()| along the Y axis;
void CheckPointsOfInterests(const int touch_id,
                            const gfx::Size& touch_size,
                            const gfx::Size& display_size,
                            const gfx::Size& max_error_delta,
                            const std::string& error_msg) {
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  float x, y;

  // Origin of the touch device should correspond to origin of the display.
  x = y = 0.0;
  device_manager->ApplyTouchTransformer(touch_id, &x, &y);
  EXPECT_NEAR(0, x, max_error_delta.width()) << error_msg;
  EXPECT_NEAR(0, y, max_error_delta.height()) << error_msg;

  // Center of the touch device should correspond to the center of the display
  // device.
  x = touch_size.width() / 2;
  y = touch_size.height() / 2;
  device_manager->ApplyTouchTransformer(touch_id, &x, &y);
  EXPECT_NEAR(display_size.width() / 2, x, max_error_delta.width())
      << error_msg;
  EXPECT_NEAR(display_size.height() / 2, y, max_error_delta.height())
      << error_msg;

  // Bottom right corner of the touch device should correspond to rightmost
  // corner of display device.
  x = touch_size.width();
  y = touch_size.height();
  device_manager->ApplyTouchTransformer(touch_id, &x, &y);
  EXPECT_NEAR(display_size.width(), x, max_error_delta.width()) << error_msg;
  EXPECT_NEAR(display_size.height(), y, max_error_delta.height()) << error_msg;
}

}  //  namespace

class TouchTransformControllerTest : public testing::Test {
 public:
  TouchTransformControllerTest() {}

  TouchTransformControllerTest(const TouchTransformControllerTest&) = delete;
  TouchTransformControllerTest& operator=(const TouchTransformControllerTest&) =
      delete;

  ~TouchTransformControllerTest() override {}

  gfx::Transform GetTouchTransform(
      const ManagedDisplayInfo& display,
      const ManagedDisplayInfo& touch_display,
      const ui::TouchscreenDevice& touchscreen) const {
    return touch_transform_controller_->GetTouchTransform(
        display, touch_display, touchscreen);
  }

  double GetTouchResolutionScale(
      const ManagedDisplayInfo& touch_display,
      const ui::TouchscreenDevice& touch_device) const {
    return touch_transform_controller_->GetTouchResolutionScale(touch_display,
                                                                touch_device);
  }

  TouchDeviceManager* touch_device_manager() { return touch_device_manager_; }

  // testing::Test:
  void SetUp() override {
    ui::DeviceDataManager::CreateInstance();
    std::unique_ptr<ScreenBase> screen = std::make_unique<ScreenBase>();
    Screen::SetScreenInstance(screen.get());
    display_manager_ = std::make_unique<DisplayManager>(std::move(screen));
    touch_device_manager_ = display_manager_->touch_device_manager();
    touch_transform_controller_ = std::make_unique<TouchTransformController>(
        display_manager_.get(),
        std::make_unique<DefaultTouchTransformSetter>());
  }

  void TearDown() override {
    Screen::SetScreenInstance(nullptr);
    ui::DeviceDataManager::DeleteInstance();
  }

  ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                       const ui::TouchscreenDevice& device,
                                       const gfx::Rect& bounds) {
    ManagedDisplayInfo info = display::CreateDisplayInfo(id, bounds);

    // Create a default mode.
    ManagedDisplayInfo::ManagedDisplayModeList default_modes(
        1, ManagedDisplayMode(bounds.size(), 60, false, true));
    info.SetManagedDisplayModes(default_modes);

    // Associate the display and touch device.
    test::TouchDeviceManagerTestApi tdm_test_api(touch_device_manager_);
    tdm_test_api.Associate(&info, device);

    return info;
  }

 private:
  std::unique_ptr<DisplayManager> display_manager_;
  std::unique_ptr<TouchTransformController> touch_transform_controller_;
  raw_ptr<TouchDeviceManager> touch_device_manager_;
};

TEST_F(TouchTransformControllerTest, MirrorModeLetterboxing) {
  gfx::Size fb_size(1920, 1200);
  // TODO(kylechar): Check the TouchscreenDevice size makes sense for Ozone.
  ui::TouchscreenDevice internal_touchscreen =
      CreateTouchscreenDevice(10, fb_size);
  ui::TouchscreenDevice external_touchscreen =
      CreateTouchscreenDevice(11, fb_size);

  // The internal display has native resolution of 2560x1700, and in
  // mirror mode it is configured as 1920x1200. This is in letterboxing
  // mode.
  ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(1, internal_touchscreen, gfx::Rect(0, 0, 1920, 1200));
  internal_display_info.set_is_aspect_preserving_scaling(true);

  ManagedDisplayInfo::ManagedDisplayModeList internal_modes;

  internal_modes.push_back(
      ManagedDisplayMode(gfx::Size(2560, 1700), 60, false, true));
  internal_modes.push_back(
      ManagedDisplayMode(gfx::Size(1920, 1200), 60, false, false));
  internal_display_info.SetManagedDisplayModes(internal_modes);

  ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(2, external_touchscreen, gfx::Rect(0, 0, 1920, 1200));

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();

  std::vector<ui::TouchDeviceTransform> transforms;
  transforms.push_back(CreateTouchDeviceTransform(
      internal_display_info.id(), internal_touchscreen.id,
      GetTouchTransform(internal_display_info, internal_display_info,
                        internal_touchscreen)));
  transforms.push_back(CreateTouchDeviceTransform(
      internal_display_info.id(), external_touchscreen.id,
      GetTouchTransform(external_display_info, external_display_info,
                        external_touchscreen)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(10));
  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(11));

  // External touch display has the default TouchTransformer.
  float x = 100.0;
  float y = 100.0;
  device_manager->ApplyTouchTransformer(11, &x, &y);
  EXPECT_EQ(100, x);
  EXPECT_EQ(100, y);

  // In letterboxing, there is (1-2560*(1200/1920)/1700)/2 = 2.95% of the
  // height on both the top & bottom region of the screen is blank.
  // When touch events coming at Y range [0, 1200), the mapping should be
  // [0, ~35] ---> < 0
  // [~35, ~1165] ---> [0, 1200)
  // [~1165, 1200] ---> >= 1200
  x = 100.0;
  y = 35.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(100, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 100.0;
  y = 1165.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(100, x, 0.5);
  EXPECT_NEAR(1200, y, 0.5);
}

TEST_F(TouchTransformControllerTest, MirrorModePillarboxing) {
  gfx::Size fb_size(1024, 768);
  // TODO(kylechar): Check the TouchscreenDevice size makes sense for Ozone.
  ui::TouchscreenDevice internal_touchscreen =
      CreateTouchscreenDevice(10, fb_size);
  ui::TouchscreenDevice external_touchscreen =
      CreateTouchscreenDevice(11, fb_size);

  // The internal display has native resolution of 1366x768, and in
  // mirror mode it is configured as 1024x768. This is in pillarboxing
  // mode.
  ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(1, internal_touchscreen, gfx::Rect(0, 0, 1024, 768));
  internal_display_info.set_is_aspect_preserving_scaling(true);
  ManagedDisplayInfo::ManagedDisplayModeList internal_modes;
  internal_modes.push_back(
      ManagedDisplayMode(gfx::Size(1366, 768), 60, false, true));
  internal_modes.push_back(
      ManagedDisplayMode(gfx::Size(1024, 768), 60, false, false));
  internal_display_info.SetManagedDisplayModes(internal_modes);

  ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(2, external_touchscreen, gfx::Rect(0, 0, 1024, 768));

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchDeviceTransform> transforms;

  transforms.push_back(CreateTouchDeviceTransform(
      internal_display_info.id(), internal_touchscreen.id,
      GetTouchTransform(internal_display_info, internal_display_info,
                        internal_touchscreen)));

  transforms.push_back(CreateTouchDeviceTransform(
      internal_display_info.id(), external_touchscreen.id,
      GetTouchTransform(external_display_info, external_display_info,
                        external_touchscreen)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(10));
  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(11));

  // External touch display has the default TouchTransformer.
  float x = 100.0;
  float y = 100.0;
  device_manager->ApplyTouchTransformer(11, &x, &y);
  EXPECT_EQ(100, x);
  EXPECT_EQ(100, y);

  // In pillarboxing, there is (1-768*(1024/768)/1366)/2 = 12.5% of the
  // width on both the left & right region of the screen is blank.
  // When touch events coming at X range [0, 1024), the mapping should be
  // [0, ~128] ---> < 0
  // [~128, ~896] ---> [0, 1024)
  // [~896, 1024] ---> >= 1024
  x = 128.0;
  y = 100.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(100, y, 0.5);

  x = 896.0;
  y = 100.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(1024, x, 0.5);
  EXPECT_NEAR(100, y, 0.5);
}

TEST_F(TouchTransformControllerTest, SoftwareMirrorMode) {
  gfx::Size fb_size(1920, 1990);
  // TODO(kylechar): Check the TouchscreenDevice size makes sense for Ozone.
  ui::TouchscreenDevice display1_touchscreen =
      CreateTouchscreenDevice(10, fb_size);
  ui::TouchscreenDevice display2_touchscreen =
      CreateTouchscreenDevice(11, fb_size);

  // External display 1 has size 1280x850. External display 2 has size
  // 1920x1080. When using software mirroring to mirror display 1 onto
  // display 2, the displays are in extended mode and we map touches from both
  // displays to display 1.
  // The total frame buffer is 1920x1990,
  // where 1990 = 850 + 60 (hidden gap) + 1080 and the second monitor is
  // translated to point (0, 950) in the framebuffer.
  ManagedDisplayInfo display1_info =
      CreateDisplayInfo(1, display1_touchscreen, gfx::Rect(0, 0, 1280, 850));
  ManagedDisplayInfo::ManagedDisplayModeList display1_modes;
  display1_modes.push_back(
      ManagedDisplayMode(gfx::Size(1280, 850), 60, false, true));
  display1_info.SetManagedDisplayModes(display1_modes);

  ManagedDisplayInfo display2_info =
      CreateDisplayInfo(2, display2_touchscreen, gfx::Rect(0, 950, 1920, 1080));
  ManagedDisplayInfo::ManagedDisplayModeList display2_modes;
  display2_modes.push_back(
      ManagedDisplayMode(gfx::Size(1920, 1080), 60, false, true));
  display2_info.SetManagedDisplayModes(display2_modes);

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchDeviceTransform> transforms;

  transforms.push_back(CreateTouchDeviceTransform(
      display1_info.id(), display1_touchscreen.id,
      GetTouchTransform(display1_info, display1_info, display1_touchscreen)));

  transforms.push_back(CreateTouchDeviceTransform(
      display1_info.id(), display2_touchscreen.id,
      GetTouchTransform(display1_info, display2_info, display2_touchscreen)));

  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(10));
  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(11));

  // Mapping for touch events from display 1's touchscreen:
  // [0, 1920) x [0, 1990) -> [0, 1280) x [0, 850)
  float x = 0.0;
  float y = 0.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 1920.0;
  y = 1990.0;
  device_manager->ApplyTouchTransformer(10, &x, &y);
  EXPECT_NEAR(1280, x, 0.5);
  EXPECT_NEAR(850, y, 0.5);

  // In pillarboxing, there is (1-1280*(1080/850)/1920)/2 = 7.65% of the
  // width on both the left & right region of the screen is blank.
  // Events come in the range [0, 1920) x [0, 1990).
  //
  // X mapping:
  // [0, ~147] ---> < 0
  // [~147, ~1773] ---> [0, 1280)
  // [~1773, 1920] ---> >= 1280
  // Y mapping:
  // [0, 1990) -> [0, 1080)
  x = 147.0;
  y = 0.0;
  device_manager->ApplyTouchTransformer(11, &x, &y);
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 1773.0;
  y = 1990.0;
  device_manager->ApplyTouchTransformer(11, &x, &y);
  EXPECT_NEAR(1280, x, 0.5);
  EXPECT_NEAR(850, y, 0.5);
}

TEST_F(TouchTransformControllerTest, ExtendedMode) {
  gfx::Size fb_size(2560, 2428);

  // TODO(kylechar): Check the TouchscreenDevice size makes sense for Ozone.
  ui::TouchscreenDevice touchscreen1 = CreateTouchscreenDevice(5, fb_size);
  ui::TouchscreenDevice touchscreen2 = CreateTouchscreenDevice(6, fb_size);

  // The internal display has size 1366 x 768. The external display has
  // size 2560x1600. The total frame buffer is 2560x2428,
  // where 2428 = 768 + 60 (hidden gap) + 1600
  // and the second monitor is translated to Point (0, 828) in the
  // framebuffer.
  ManagedDisplayInfo display1 =
      CreateDisplayInfo(1, touchscreen1, gfx::Rect(0, 0, 1366, 768));
  ManagedDisplayInfo display2 =
      CreateDisplayInfo(2, touchscreen2, gfx::Rect(0, 828, 2560, 1600));

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchDeviceTransform> transforms;
  transforms.push_back(CreateTouchDeviceTransform(
      display1.id(), touchscreen1.id,
      GetTouchTransform(display1, display1, touchscreen1)));

  transforms.push_back(CreateTouchDeviceTransform(
      display2.id(), touchscreen2.id,
      GetTouchTransform(display2, display2, touchscreen2)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(1, device_manager->GetTargetDisplayForTouchDevice(5));
  EXPECT_EQ(2, device_manager->GetTargetDisplayForTouchDevice(6));

  // Mapping for touch events from internal touch display:
  // [0, 2560) x [0, 2428) -> [0, 1366) x [0, 768)
  float x = 0.0;
  float y = 0.0;
  device_manager->ApplyTouchTransformer(5, &x, &y);
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 2559.0;
  y = 2427.0;
  device_manager->ApplyTouchTransformer(5, &x, &y);
  EXPECT_NEAR(1365, x, 0.5);
  EXPECT_NEAR(768, y, 0.5);

  // Mapping for touch events from external touch display:
  // [0, 2560) x [0, 2428) -> [0, 2560) x [0, 1600)
  x = 0.0;
  y = 0.0;
  device_manager->ApplyTouchTransformer(6, &x, &y);
  // On ozone we expect screen coordinates so add display origin.
  EXPECT_NEAR(0 + 0, x, 0.5);
  EXPECT_NEAR(0 + 828, y, 0.5);

  x = 2559.0;
  y = 2427.0;
  device_manager->ApplyTouchTransformer(6, &x, &y);
  // On ozone we expect screen coordinates so add display origin.
  EXPECT_NEAR(2559 + 0, x, 0.5);
  EXPECT_NEAR(1599 + 828, y, 0.5);
}

TEST_F(TouchTransformControllerTest, TouchRadiusScale) {
  ui::TouchscreenDevice touch_device =
      CreateTouchscreenDevice(5, gfx::Size(100001, 100001));
  ManagedDisplayInfo display =
      CreateDisplayInfo(1, touch_device, gfx::Rect(0, 0, 2560, 1600));

  // Default touchscreen position range is 100001x100001;
  EXPECT_EQ(sqrt((2560.0 * 1600.0) / (100001.0 * 100001.0)),
            GetTouchResolutionScale(display, touch_device));
}

TEST_F(TouchTransformControllerTest, OzoneTranslation) {
  // The internal display has size 1920 x 1200. The external display has
  // size 1920x1200. The total frame buffer is 1920x2450,
  // where 2458 = 1200 + 50 (hidden gap) + 1200
  // and the second monitor is translated to Point (0, 1250) in the
  // framebuffer.
  const gfx::Size kDisplaySize(1920, 1200);
  const int kHiddenGap = 50;

  ui::TouchscreenDevice touchscreen1 =
      CreateTouchscreenDevice(kTouchId1, kDisplaySize);
  ui::TouchscreenDevice touchscreen2 =
      CreateTouchscreenDevice(kTouchId2, kDisplaySize);

  ManagedDisplayInfo display1 = CreateDisplayInfo(
      kDisplayId1, touchscreen1,
      gfx::Rect(0, 0, kDisplaySize.width(), kDisplaySize.height()));
  ManagedDisplayInfo display2 =
      CreateDisplayInfo(kDisplayId2, touchscreen2,
                        gfx::Rect(0, kDisplaySize.height() + kHiddenGap,
                                  kDisplaySize.width(), kDisplaySize.height()));

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchDeviceTransform> transforms;

  // Mirror displays. Touch screen 2 is associated to display 1.
  transforms.push_back(CreateTouchDeviceTransform(
      display1.id(), touchscreen1.id,
      GetTouchTransform(display1, display1, touchscreen1)));

  transforms.push_back(CreateTouchDeviceTransform(
      display1.id(), touchscreen2.id,
      GetTouchTransform(display1, display2, touchscreen2)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(kDisplayId1,
            device_manager->GetTargetDisplayForTouchDevice(kTouchId1));
  EXPECT_EQ(kDisplayId1,
            device_manager->GetTargetDisplayForTouchDevice(kTouchId2));

  float x, y;

  x = y = 0.0;
  device_manager->ApplyTouchTransformer(kTouchId1, &x, &y);
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = y = 0.0;
  device_manager->ApplyTouchTransformer(kTouchId2, &x, &y);
  EXPECT_NEAR(0, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 1920.0;
  y = 1200.0;
  device_manager->ApplyTouchTransformer(kTouchId1, &x, &y);
  EXPECT_NEAR(1920, x, 0.5);
  EXPECT_NEAR(1200, y, 0.5);

  x = 1920.0;
  y = 1200.0;
  device_manager->ApplyTouchTransformer(kTouchId2, &x, &y);
  EXPECT_NEAR(1920, x, 0.5);
  EXPECT_NEAR(1200, y, 0.5);

  // Remove mirroring of displays.
  transforms.push_back(CreateTouchDeviceTransform(
      display2.id(), touchscreen2.id,
      GetTouchTransform(display2, display2, touchscreen2)));
  device_manager->ConfigureTouchDevices(transforms);

  x = 1920.0;
  y = 1200.0;
  device_manager->ApplyTouchTransformer(kTouchId1, &x, &y);
  EXPECT_NEAR(1920, x, 0.5);
  EXPECT_NEAR(1200, y, 0.5);

  x = 1920.0;
  y = 1200.0;
  device_manager->ApplyTouchTransformer(kTouchId2, &x, &y);
  EXPECT_NEAR(1920, x, 0.5);
  EXPECT_NEAR(1200 + kDisplaySize.height() + kHiddenGap, y, 0.5);
}

TEST_F(TouchTransformControllerTest, AccurateUserTouchCalibration) {
  const gfx::Size kDisplaySize(1920, 1200);
  const gfx::Size kTouchSize(1920, 1200);

  ui::TouchscreenDevice touchscreen =
      CreateTouchscreenDevice(kTouchId1, kTouchSize);

  ManagedDisplayInfo display = CreateDisplayInfo(
      kDisplayId1, touchscreen,
      gfx::Rect(0, 0, kDisplaySize.width(), kDisplaySize.height()));

  // Assuming the user provided accurate inputs during calibration. ie the user
  // actually tapped (100,100) when asked to tap (100,100) with no human error.
  TouchCalibrationData::CalibrationPointPairQuad user_input = {{
      std::make_pair(gfx::Point(100, 100), gfx::Point(100, 100)),
      std::make_pair(gfx::Point(1820, 100), gfx::Point(1820, 100)),
      std::make_pair(gfx::Point(100, 1100), gfx::Point(100, 1100)),
      std::make_pair(gfx::Point(1820, 1100), gfx::Point(1820, 1100)),
  }};
  TouchCalibrationData touch_data(user_input, kDisplaySize);

  const std::string msg = GetTouchPointString(user_input);

  touch_device_manager()->AddTouchCalibrationData(touchscreen, display.id(),
                                                  touch_data);

  EXPECT_FALSE(touch_device_manager()
                   ->GetCalibrationData(touchscreen, display.id())
                   .IsEmpty());

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchDeviceTransform> transforms;
  transforms.push_back(CreateTouchDeviceTransform(
      display.id(), touchscreen.id,
      GetTouchTransform(display, display, touchscreen)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(kDisplayId1,
            device_manager->GetTargetDisplayForTouchDevice(kTouchId1));

  CheckPointsOfInterests(kTouchId1, kTouchSize, kDisplaySize, gfx::Size(1, 1),
                         msg);
}

TEST_F(TouchTransformControllerTest, ErrorProneUserTouchCalibration) {
  const gfx::Size kDisplaySize(1920, 1200);
  const gfx::Size kTouchSize(1920, 1200);
  // User touch inputs have a max error of 5%.
  const float kError = 0.05;
  // The maximum user error rate is |kError|%. Since the calibration is
  // performed with a best fit algorithm, the error rate observed should be less
  // than |kError|.
  const gfx::Size kMaxErrorDelta = gfx::ScaleToCeiledSize(kTouchSize, kError);

  ui::TouchscreenDevice touchscreen =
      CreateTouchscreenDevice(kTouchId1, kTouchSize);

  ManagedDisplayInfo display = CreateDisplayInfo(
      kDisplayId1, touchscreen,
      gfx::Rect(0, 0, kDisplaySize.width(), kDisplaySize.height()));

  // Assuming the user provided inaccurate inputs during calibration. ie the
  // user did not tap (100,100) when asked to tap (100,100) due to human error.
  TouchCalibrationData::CalibrationPointPairQuad user_input = {
      {std::make_pair(gfx::Point(100, 100), gfx::Point(130, 60)),
       std::make_pair(gfx::Point(1820, 100), gfx::Point(1878, 130)),
       std::make_pair(gfx::Point(100, 1100), gfx::Point(158, 1060)),
       std::make_pair(gfx::Point(1820, 1100), gfx::Point(1790, 1140))}};
  TouchCalibrationData touch_data(user_input, kDisplaySize);

  const std::string msg = GetTouchPointString(user_input);

  touch_device_manager()->AddTouchCalibrationData(touchscreen, display.id(),
                                                  touch_data);

  EXPECT_FALSE(touch_device_manager()
                   ->GetCalibrationData(touchscreen, display.id())
                   .IsEmpty());

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchDeviceTransform> transforms;
  transforms.push_back(CreateTouchDeviceTransform(
      display.id(), touchscreen.id,
      GetTouchTransform(display, display, touchscreen)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(kDisplayId1,
            device_manager->GetTargetDisplayForTouchDevice(kTouchId1));

  CheckPointsOfInterests(kTouchId1, kTouchSize, kDisplaySize, kMaxErrorDelta,
                         msg);
}

TEST_F(TouchTransformControllerTest, ResolutionChangeUserTouchCalibration) {
  const gfx::Size kDisplaySize(2560, 1600);
  const gfx::Size kTouchSize(1920, 1200);
  // User touch inputs have a max error of 5%.
  const float kError = 0.05;
  // The maximum user error rate is |kError|%. Since the calibration is
  // performed with a best fit algorithm, the error rate observed should be less
  // tha |kError|.
  gfx::Size kMaxErrorDelta = gfx::ScaleToCeiledSize(kDisplaySize, kError);

  ui::TouchscreenDevice touchscreen =
      CreateTouchscreenDevice(kTouchId1, kTouchSize);

  ManagedDisplayInfo display = CreateDisplayInfo(
      kDisplayId1, touchscreen,
      gfx::Rect(0, 0, kDisplaySize.width(), kDisplaySize.height()));

  // The calibration was performed at a resolution different from the curent
  // resolution of the display.
  const gfx::Size CALIBRATION_SIZE(1920, 1200);
  TouchCalibrationData::CalibrationPointPairQuad user_input = {
      {std::make_pair(gfx::Point(100, 100), gfx::Point(50, 70)),
       std::make_pair(gfx::Point(1820, 100), gfx::Point(1780, 70)),
       std::make_pair(gfx::Point(100, 1100), gfx::Point(70, 1060)),
       std::make_pair(gfx::Point(1820, 1100), gfx::Point(1770, 1140))}};

  TouchCalibrationData touch_data(user_input, CALIBRATION_SIZE);

  const std::string msg = GetTouchPointString(user_input);

  touch_device_manager()->AddTouchCalibrationData(touchscreen, display.id(),
                                                  touch_data);

  EXPECT_FALSE(touch_device_manager()
                   ->GetCalibrationData(touchscreen, display.id())
                   .IsEmpty());

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchDeviceTransform> transforms;
  transforms.push_back(CreateTouchDeviceTransform(
      display.id(), touchscreen.id,
      GetTouchTransform(display, display, touchscreen)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(kDisplayId1,
            device_manager->GetTargetDisplayForTouchDevice(kTouchId1));

  CheckPointsOfInterests(kTouchId1, kTouchSize, kDisplaySize, kMaxErrorDelta,
                         msg);
}

TEST_F(TouchTransformControllerTest, DifferentBoundsUserTouchCalibration) {
  // The display bounds is different from the touch device bounds in this test.
  const gfx::Size kDisplaySize(1024, 600);
  const gfx::Size kTouchSize(4096, 4096);
  const float kAcceptableError = 0.04;
  gfx::Size kMaxErrorDelta =
      gfx::ScaleToCeiledSize(kDisplaySize, kAcceptableError);

  ui::TouchscreenDevice touchscreen =
      CreateTouchscreenDevice(kTouchId1, kTouchSize);

  ManagedDisplayInfo display = CreateDisplayInfo(
      kDisplayId1, touchscreen,
      gfx::Rect(0, 0, kDisplaySize.width(), kDisplaySize.height()));

  // Real world data.
  TouchCalibrationData::CalibrationPointPairQuad user_input = {
      {std::make_pair(gfx::Point(136, 136), gfx::Point(538, 931)),
       std::make_pair(gfx::Point(873, 136), gfx::Point(3475, 922)),
       std::make_pair(gfx::Point(136, 411), gfx::Point(611, 2800)),
       std::make_pair(gfx::Point(873, 411), gfx::Point(3535, 2949))}};
  TouchCalibrationData touch_data(user_input, kDisplaySize);

  const std::string msg = GetTouchPointString(user_input);

  touch_device_manager()->AddTouchCalibrationData(touchscreen, display.id(),
                                                  touch_data);

  EXPECT_FALSE(touch_device_manager()
                   ->GetCalibrationData(touchscreen, display.id())
                   .IsEmpty());

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();

  std::vector<ui::TouchDeviceTransform> transforms;
  transforms.push_back(CreateTouchDeviceTransform(
      display.id(), touchscreen.id,
      GetTouchTransform(display, display, touchscreen)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(kDisplayId1,
            device_manager->GetTargetDisplayForTouchDevice(kTouchId1));

  CheckPointsOfInterests(kTouchId1, kTouchSize, kDisplaySize, kMaxErrorDelta,
                         msg);
}

TEST_F(TouchTransformControllerTest, LetterboxingUserTouchCalibration) {
  // The internal display has native resolution of 2560x1700, and in
  // mirror mode it is configured as 1920x1200. This is in letterboxing
  // mode.
  const gfx::Size kNativeDisplaySize(2560, 1700);
  const gfx::Size kDisplaySize(1920, 1200);
  const gfx::Size kTouchSize(1920, 1200);

  ui::TouchscreenDevice internal_touchscreen =
      CreateTouchscreenDevice(kTouchId1, kTouchSize);

  ManagedDisplayInfo internal_display_info = CreateDisplayInfo(
      kDisplayId1, internal_touchscreen,
      gfx::Rect(0, 0, kDisplaySize.width(), kDisplaySize.height()));
  internal_display_info.set_is_aspect_preserving_scaling(true);

  ManagedDisplayInfo::ManagedDisplayModeList internal_modes;

  internal_modes.push_back(ManagedDisplayMode(
      gfx::Size(kNativeDisplaySize.width(), kNativeDisplaySize.height()), 60,
      false, true));
  internal_modes.push_back(
      ManagedDisplayMode(gfx::Size(kDisplaySize.width(), kDisplaySize.height()),
                         60, false, false));
  internal_display_info.SetManagedDisplayModes(internal_modes);

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();

  // Assuming the user provided inaccurate inputs during calibration. ie the
  // user did not tap (100,100) when asked to tap (100,100) due to human error.
  // Since the display is of size 2560x1700 and the touch device is of size
  // 1920x1200, the corresponding points have to be scaled.
  TouchCalibrationData::CalibrationPointPairQuad user_input = {{
      std::make_pair(gfx::Point(100, 100), gfx::Point(75, 71)),
      std::make_pair(gfx::Point(2460, 100), gfx::Point(1845, 71)),
      std::make_pair(gfx::Point(100, 1600), gfx::Point(75, 1130)),
      std::make_pair(gfx::Point(2460, 1600), gfx::Point(1845, 1130)),
  }};
  // The calibration was performed at the native display resolution.
  TouchCalibrationData touch_data(user_input, kNativeDisplaySize);
  touch_device_manager()->AddTouchCalibrationData(
      internal_touchscreen, internal_display_info.id(), touch_data);

  EXPECT_FALSE(
      touch_device_manager()
          ->GetCalibrationData(internal_touchscreen, internal_display_info.id())
          .IsEmpty());

  std::vector<ui::TouchDeviceTransform> transforms;
  transforms.push_back(CreateTouchDeviceTransform(
      internal_display_info.id(), internal_touchscreen.id,
      GetTouchTransform(internal_display_info, internal_display_info,
                        internal_touchscreen)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(kDisplayId1,
            device_manager->GetTargetDisplayForTouchDevice(kTouchId1));

  float x, y;
  // In letterboxing, there is (1-2560*(1200/1920)/1700)/2 = 2.95% of the
  // height on both the top & bottom region of the screen is blank.
  // When touch events coming at Y range [0, 1200), the mapping should be
  // [0, ~35] ---> < 0
  // [~35, ~1165] ---> [0, 1200)
  // [~1165, 1200] ---> >= 1200
  x = 100.0;
  y = 35.0;
  device_manager->ApplyTouchTransformer(kTouchId1, &x, &y);
  EXPECT_NEAR(100, x, 0.5);
  EXPECT_NEAR(0, y, 0.5);

  x = 100.0;
  y = 1165.0;
  device_manager->ApplyTouchTransformer(kTouchId1, &x, &y);
  EXPECT_NEAR(100, x, 0.5);
  EXPECT_NEAR(1200, y, 0.5);
}

TEST_F(TouchTransformControllerTest, PillarBoxingUserTouchCalibration) {
  // The internal display has native resolution of 2560x1700, and in
  // mirror mode it is configured as 1920x1200. This is in letterboxing
  // mode.
  const gfx::Size kNativeDisplaySize(2560, 1600);
  const gfx::Size kDisplaySize(1920, 1400);

  ui::TouchscreenDevice internal_touchscreen =
      CreateTouchscreenDevice(kTouchId1, kDisplaySize);

  ManagedDisplayInfo internal_display_info = CreateDisplayInfo(
      kDisplayId1, internal_touchscreen,
      gfx::Rect(0, 0, kDisplaySize.width(), kDisplaySize.height()));
  internal_display_info.set_is_aspect_preserving_scaling(true);

  ManagedDisplayInfo::ManagedDisplayModeList internal_modes;

  internal_modes.push_back(ManagedDisplayMode(
      gfx::Size(kNativeDisplaySize.width(), kNativeDisplaySize.height()), 60,
      false, true));
  internal_modes.push_back(
      ManagedDisplayMode(gfx::Size(kDisplaySize.width(), kDisplaySize.height()),
                         60, false, false));
  internal_display_info.SetManagedDisplayModes(internal_modes);

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();

  // Assuming the user provided accurate inputs during calibration. ie the user
  // actually tapped (100,100) when asked to tap (100,100) with no human error.
  // Since the display is of size 2560x1600 and the touch device is of size
  // 1920x1400, the corresponding points have to be scaled.
  TouchCalibrationData::CalibrationPointPairQuad user_input = {{
      std::make_pair(gfx::Point(100, 100), gfx::Point(75, 88)),
      std::make_pair(gfx::Point(2460, 100), gfx::Point(1845, 88)),
      std::make_pair(gfx::Point(100, 1500), gfx::Point(75, 1313)),
      std::make_pair(gfx::Point(2460, 1500), gfx::Point(1845, 1313)),
  }};
  // The calibration was performed at the native display resolution.
  TouchCalibrationData touch_data(user_input, kNativeDisplaySize);

  touch_device_manager()->AddTouchCalibrationData(
      internal_touchscreen, internal_display_info.id(), touch_data);

  EXPECT_FALSE(
      touch_device_manager()
          ->GetCalibrationData(internal_touchscreen, internal_display_info.id())
          .IsEmpty());

  std::vector<ui::TouchDeviceTransform> transforms;
  transforms.push_back(CreateTouchDeviceTransform(
      internal_display_info.id(), internal_touchscreen.id,
      GetTouchTransform(internal_display_info, internal_display_info,
                        internal_touchscreen)));
  device_manager->ConfigureTouchDevices(transforms);

  EXPECT_EQ(kDisplayId1,
            device_manager->GetTargetDisplayForTouchDevice(kTouchId1));

  float x, y;
  // In pillarboxing, there is (1-1600*(1920/1400)/2560)/2 = 7.14% of the
  // width on both the left & region region of the screen is blank.
  // When touch events coming at X range [0, 1920), the mapping should be
  // [0, ~137] ---> < 0
  // [~137, ~1782] ---> [0, 1920)
  // [~1782, 1920] ---> >= 1920
  x = 136.0;
  y = 0.0;
  device_manager->ApplyTouchTransformer(kTouchId1, &x, &y);
  EXPECT_LT(-1.0f, x);
  EXPECT_LT(x, 0.0f);
  EXPECT_NEAR(0.0f, y, 0.01f);

  x = 137.0;
  y = 0.0;
  device_manager->ApplyTouchTransformer(kTouchId1, &x, &y);
  EXPECT_LT(0.0f, x);
  EXPECT_LT(x, 1.0f);
  EXPECT_NEAR(0.0f, y, 0.01f);

  x = 1782.0;
  y = 0.0;
  device_manager->ApplyTouchTransformer(kTouchId1, &x, &y);
  EXPECT_LT(1919.0f, x);
  EXPECT_LT(x, 1920.0f);
  EXPECT_NEAR(0.0f, y, 0.01f);

  x = 1783.0;
  y = 0.0;
  device_manager->ApplyTouchTransformer(kTouchId1, &x, &y);
  EXPECT_LT(1920.0f, x);
  EXPECT_LT(x, 1921.0f);
  EXPECT_NEAR(0.0f, y, 0.01f);
}

}  // namespace display::test
