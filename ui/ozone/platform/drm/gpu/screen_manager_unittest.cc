// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drm_fourcc.h>
#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/files/platform_file.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/ozone/common/linux/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_gbm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {
namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode = {0, 6, 0, 0, 0, 0, 4,     0,
                                      0, 0, 0, 0, 0, 0, {'\0'}};

const uint32_t kPrimaryCrtc = 1;
const uint32_t kPrimaryConnector = 2;
const uint32_t kSecondaryCrtc = 3;
const uint32_t kSecondaryConnector = 4;

drmModeModeInfo Mode(uint16_t hdisplay, uint16_t vdisplay) {
  return {0, hdisplay, 0, 0, 0, 0, vdisplay, 0, 0, 0, 0, 0, 0, 0, {'\0'}};
}

}  // namespace

class ScreenManagerTest : public testing::Test {
 public:
  ScreenManagerTest() {}
  ~ScreenManagerTest() override {}

  gfx::Rect GetPrimaryBounds() const {
    return gfx::Rect(0, 0, kDefaultMode.hdisplay, kDefaultMode.vdisplay);
  }

  // Secondary is in extended mode, right-of primary.
  gfx::Rect GetSecondaryBounds() const {
    return gfx::Rect(kDefaultMode.hdisplay, 0, kDefaultMode.hdisplay,
                     kDefaultMode.vdisplay);
  }

  void SetUp() override {
    auto gbm = std::make_unique<ui::MockGbmDevice>();
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
    return DrmFramebuffer::AddFramebuffer(drm_, buffer.get(), modifiers);
  }

 protected:
  scoped_refptr<ui::MockDrmDevice> drm_;
  std::unique_ptr<ui::DrmDeviceManager> device_manager_;
  std::unique_ptr<ui::ScreenManager> screen_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenManagerTest);
};

TEST_F(ScreenManagerTest, CheckWithNoControllers) {
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
}

TEST_F(ScreenManagerTest, CheckWithValidController) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());

  EXPECT_TRUE(controller);
  EXPECT_TRUE(controller->HasCrtc(drm_, kPrimaryCrtc));
}

TEST_F(ScreenManagerTest, CheckWithInvalidBounds) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckForSecondValidController) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetSecondaryBounds().origin(),
      kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckControllerAfterItIsRemoved) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));

  screen_manager_->RemoveDisplayController(drm_, kPrimaryCrtc);
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
}

TEST_F(ScreenManagerTest, CheckDuplicateConfiguration) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  uint32_t framebuffer = drm_->current_framebuffer();

  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  // Should not hold onto buffers.
  EXPECT_NE(framebuffer, drm_->current_framebuffer());

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckChangingMode) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  drmModeModeInfo new_mode = kDefaultMode;
  new_mode.vdisplay = 10;
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      new_mode);

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
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckMirrorModeTransitions) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetSecondaryBounds().origin(),
      kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetSecondaryBounds().origin(),
      kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

// Make sure we're using each display's mode when doing mirror mode otherwise
// the timings may be off.
TEST_F(ScreenManagerTest, CheckMirrorModeModesettingWithDisplaysMode) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  // Copy the mode and use the copy so we can tell what mode the CRTC was
  // configured with. The clock value is modified so we can tell which mode is
  // being used.
  drmModeModeInfo kSecondaryMode = kDefaultMode;
  kSecondaryMode.clock++;

  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      kSecondaryMode);

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  for (const auto& crtc : controller->crtc_controllers()) {
    if (crtc->crtc() == kPrimaryCrtc)
      EXPECT_EQ(kDefaultMode.clock, crtc->mode().clock);
    else if (crtc->crtc() == kSecondaryCrtc)
      EXPECT_EQ(kSecondaryMode.clock, crtc->mode().clock);
    else
      NOTREACHED();
  }
}

TEST_F(ScreenManagerTest, MonitorGoneInMirrorMode) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  screen_manager_->RemoveDisplayController(drm_, kSecondaryCrtc);

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  EXPECT_TRUE(controller->HasCrtc(drm_, kPrimaryCrtc));
  EXPECT_FALSE(controller->HasCrtc(drm_, kSecondaryCrtc));
}

TEST_F(ScreenManagerTest, MonitorDisabledInMirrorMode) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  screen_manager_->DisableDisplayController(drm_, kSecondaryCrtc);

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  EXPECT_TRUE(controller->HasCrtc(drm_, kPrimaryCrtc));
  EXPECT_FALSE(controller->HasCrtc(drm_, kSecondaryCrtc));
}

TEST_F(ScreenManagerTest, DoNotEnterMirrorModeUnlessSameBounds) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);

  // Configure displays in extended mode.
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetSecondaryBounds().origin(),
      kDefaultMode);

  drmModeModeInfo new_mode = kDefaultMode;
  new_mode.vdisplay = 10;
  // Shouldn't enter mirror mode unless the display bounds are the same.
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      new_mode);

  EXPECT_FALSE(
      screen_manager_->GetDisplayController(GetPrimaryBounds())->IsMirrored());
}

TEST_F(ScreenManagerTest, ReuseFramebufferIfDisabledThenReEnabled) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  uint32_t framebuffer = drm_->current_framebuffer();

  screen_manager_->DisableDisplayController(drm_, kPrimaryCrtc);
  EXPECT_EQ(0u, drm_->current_framebuffer());

  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  // Buffers are released when disabled.
  EXPECT_NE(framebuffer, drm_->current_framebuffer());
}

TEST_F(ScreenManagerTest, CheckMirrorModeAfterBeginReEnabled) {
  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  screen_manager_->DisableDisplayController(drm_, kPrimaryCrtc);

  screen_manager_->AddDisplayController(drm_, kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(controller->IsMirrored());

  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  EXPECT_TRUE(controller);
  EXPECT_TRUE(controller->IsMirrored());
}

TEST_F(ScreenManagerTest,
       CheckProperConfigurationWithDifferentDeviceAndSameCrtc) {
  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  scoped_refptr<ui::MockDrmDevice> drm2 =
      new ui::MockDrmDevice(std::move(gbm_device));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->AddDisplayController(drm2, kPrimaryCrtc, kPrimaryConnector);

  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      drm2, kPrimaryCrtc, kPrimaryConnector, GetSecondaryBounds().origin(),
      kDefaultMode);

  ui::HardwareDisplayController* controller1 =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  ui::HardwareDisplayController* controller2 =
      screen_manager_->GetDisplayController(GetSecondaryBounds());

  EXPECT_NE(controller1, controller2);
  EXPECT_EQ(drm_, controller1->crtc_controllers()[0]->drm());
  EXPECT_EQ(drm2, controller2->crtc_controllers()[0]->drm());
}

TEST_F(ScreenManagerTest, CheckControllerToWindowMappingWithSameBounds) {
  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetWindow(1)->GetController());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, CheckControllerToWindowMappingWithDifferentBounds) {
  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  gfx::Rect new_bounds = GetPrimaryBounds();
  new_bounds.Inset(0, 0, 1, 1);
  window->SetBounds(new_bounds);
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  EXPECT_FALSE(screen_manager_->GetWindow(1)->GetController());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest,
       CheckControllerToWindowMappingWithOverlappingWindows) {
  const size_t kWindowCount = 2;
  for (size_t i = 1; i < kWindowCount + 1; ++i) {
    std::unique_ptr<ui::DrmWindow> window(
        new ui::DrmWindow(i, device_manager_.get(), screen_manager_.get()));
    window->Initialize();
    window->SetBounds(GetPrimaryBounds());
    screen_manager_->AddWindow(i, std::move(window));
  }

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

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
  gfx::AcceleratedWidget window_id = 1;
  std::unique_ptr<ui::DrmWindow> window(new ui::DrmWindow(
      window_id, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(window_id, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetWindow(window_id)->GetController());

  screen_manager_->RemoveDisplayController(drm_, kPrimaryCrtc);

  EXPECT_FALSE(screen_manager_->GetWindow(window_id)->GetController());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, EnableControllerWhenWindowHasNoBuffer) {
  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetWindow(1)->GetController());
  // There is a buffer after initial config.
  uint32_t framebuffer = drm_->current_framebuffer();
  EXPECT_NE(0U, framebuffer);

  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  // There is a new buffer after we configured with the same mode but no
  // pending frames on the window.
  EXPECT_NE(framebuffer, drm_->current_framebuffer());

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST_F(ScreenManagerTest, EnableControllerWhenWindowHasBuffer) {
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
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

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
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

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
  std::unique_ptr<ui::DrmWindow> window(
      new ui::DrmWindow(1, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(GetPrimaryBounds());
  screen_manager_->AddWindow(1, std::move(window));

  screen_manager_->AddDisplayController(drm_, kPrimaryCrtc, kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  // When a window that had no controller becomes associated with a new
  // controller, expect the crtc to be modeset once.
  EXPECT_EQ(drm_->get_set_crtc_call_count(), 1);

  window = screen_manager_->RemoveWindow(1);
  window->Shutdown();
}

TEST(ScreenManagerTest2, ShouldNotHardwareMirrorDifferentDrmDevices) {
  auto gbm_device1 = std::make_unique<MockGbmDevice>();
  auto drm_device1 =
      base::MakeRefCounted<MockDrmDevice>(std::move(gbm_device1));
  auto gbm_device2 = std::make_unique<MockGbmDevice>();
  auto drm_device2 =
      base::MakeRefCounted<MockDrmDevice>(std::move(gbm_device2));
  DrmDeviceManager drm_device_manager(nullptr);
  ScreenManager screen_manager;

  constexpr uint32_t kCrtc19 = 19;
  constexpr uint32_t kConnector28 = 28;
  constexpr uint32_t kCrtc20 = 20;
  constexpr uint32_t kConnector22 = 22;

  // Two displays on different DRM devices must not join a mirror pair.
  //
  // However, they may have the same bounds in a transitional state.
  //
  // This scenario generates the same sequence of display configuration events
  // as a panther (kernel 3.8.11) chromebox with two identical 1080p displays
  // connected, one of them via a DisplayLink adapter.

  // Both displays connect at startup.
  {
    auto window1 =
        std::make_unique<DrmWindow>(1, &drm_device_manager, &screen_manager);
    window1->Initialize();
    screen_manager.AddWindow(1, std::move(window1));
    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.AddDisplayController(drm_device1, kCrtc19, kConnector28);
    screen_manager.AddDisplayController(drm_device2, kCrtc20, kConnector22);
    screen_manager.ConfigureDisplayController(
        drm_device1, kCrtc19, kConnector28, gfx::Point(0, 0), Mode(1920, 1080));
    screen_manager.ConfigureDisplayController(drm_device2, kCrtc20,
                                              kConnector22, gfx::Point(0, 1140),
                                              Mode(1920, 1080));
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
    EXPECT_TRUE(controller1->HasCrtc(drm_device1, kCrtc19));
    EXPECT_TRUE(controller2->HasCrtc(drm_device2, kCrtc20));
  }

  // Disconnect first display. Second display moves to origin.
  {
    screen_manager.RemoveDisplayController(drm_device1, kCrtc19);
    screen_manager.ConfigureDisplayController(
        drm_device2, kCrtc20, kConnector22, gfx::Point(0, 0), Mode(1920, 1080));
    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.RemoveWindow(2)->Shutdown();
  }

  // Reconnect first display. Original configuration restored.
  {
    screen_manager.AddDisplayController(drm_device1, kCrtc19, kConnector28);
    screen_manager.ConfigureDisplayController(
        drm_device1, kCrtc19, kConnector28, gfx::Point(0, 0), Mode(1920, 1080));
    // At this point, both displays are in the same location.
    {
      HardwareDisplayController* controller =
          screen_manager.GetWindow(1)->GetController();
      EXPECT_FALSE(controller->IsMirrored());
      // We don't really care which crtc it has, but it should have just one.
      EXPECT_EQ(1U, controller->crtc_controllers().size());
      EXPECT_TRUE(controller->HasCrtc(drm_device1, kCrtc19) ||
                  controller->HasCrtc(drm_device2, kCrtc20));
    }
    screen_manager.ConfigureDisplayController(drm_device2, kCrtc20,
                                              kConnector22, gfx::Point(0, 1140),
                                              Mode(1920, 1080));
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
    EXPECT_TRUE(controller1->HasCrtc(drm_device1, kCrtc19));
    EXPECT_TRUE(controller3->HasCrtc(drm_device2, kCrtc20));
  }

  // Cleanup.
  screen_manager.RemoveWindow(1)->Shutdown();
  screen_manager.RemoveWindow(3)->Shutdown();
}

// crbug.com/888553
TEST(ScreenManagerTest2, ShouldNotUnbindFramebufferOnJoiningMirror) {
  auto gbm_device = std::make_unique<MockGbmDevice>();
  auto drm_device = base::MakeRefCounted<MockDrmDevice>(std::move(gbm_device));
  DrmDeviceManager drm_device_manager(nullptr);
  ScreenManager screen_manager;

  constexpr uint32_t kCrtc39 = 39;
  constexpr uint32_t kConnector43 = 43;
  constexpr uint32_t kCrtc41 = 41;
  constexpr uint32_t kConnector46 = 46;

  constexpr drmModeModeInfo kMode1080p60 = {
      /* clock= */ 148500,
      /* hdisplay= */ 1920,
      /* hsync_start= */ 2008,
      /* hsync_end= */ 2052,
      /* htotal= */ 2200,
      /* hskew= */ 0,
      /* vdisplay= */ 1080,
      /* vsync_start= */ 1084,
      /* vsync_end= */ 1089,
      /* vtotal= */ 1125,
      /* vscan= */ 0,
      /* vrefresh= */ 60,
      /* flags= */ 0xa,
      /* type= */ 64,
      /* name= */ "1920x1080",
  };

  // Both displays connect at startup.
  {
    auto window1 =
        std::make_unique<DrmWindow>(1, &drm_device_manager, &screen_manager);
    window1->Initialize();
    screen_manager.AddWindow(1, std::move(window1));
    screen_manager.GetWindow(1)->SetBounds(gfx::Rect(0, 0, 1920, 1080));
    screen_manager.AddDisplayController(drm_device, kCrtc39, kConnector43);
    screen_manager.AddDisplayController(drm_device, kCrtc41, kConnector46);
    screen_manager.ConfigureDisplayController(drm_device, kCrtc39, kConnector43,
                                              gfx::Point(0, 0), kMode1080p60);
    screen_manager.ConfigureDisplayController(drm_device, kCrtc41, kConnector46,
                                              gfx::Point(0, 0), kMode1080p60);
  }

  EXPECT_NE(0u, drm_device->GetFramebufferForCrtc(kCrtc39));
  EXPECT_NE(0u, drm_device->GetFramebufferForCrtc(kCrtc41));

  // Cleanup.
  screen_manager.RemoveWindow(1)->Shutdown();
}

}  // namespace ui
