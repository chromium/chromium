// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display_manager.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gl {

class GLDisplayManagerEGLTest : public testing::Test {
 public:
  GLDisplayManagerEGLTest() = default;
  ~GLDisplayManagerEGLTest() override = default;

 protected:
  class ScopedGLDisplayManagerEGL {
   public:
    ScopedGLDisplayManagerEGL() = default;

    ScopedGLDisplayManagerEGL(const ScopedGLDisplayManagerEGL&) = delete;
    ScopedGLDisplayManagerEGL& operator=(const ScopedGLDisplayManagerEGL&) =
        delete;

    GLDisplayManagerEGL* operator->() { return &manager_; }

   private:
    GLDisplayManagerEGL manager_;
  };
};

TEST_F(GLDisplayManagerEGLTest, SingleGPU) {
  constexpr uint64_t kSingleGpu = 18;

  // Set up.
  ScopedGLDisplayManagerEGL manager;
  manager->SetGpuPreference(GpuPreference::kDefault, kSingleGpu);

  // Query the default display.
  GLDisplayEGL* display = manager->GetDisplay(GpuPreference::kDefault);
  EXPECT_NE(nullptr, display);
  EXPECT_EQ(kSingleGpu, display->system_device_id());

  // Query again should return the same display.
  GLDisplayEGL* display_2 = manager->GetDisplay(GpuPreference::kDefault);
  EXPECT_EQ(display, display_2);

  // Query the low power display. It should be the same as the default.
  GLDisplayEGL* display_low_power =
      manager->GetDisplay(GpuPreference::kLowPower);
  EXPECT_EQ(display, display_low_power);

  // Query the high performance display. It should be the same as the default.
  GLDisplayEGL* display_high_performance =
      manager->GetDisplay(GpuPreference::kHighPerformance);
  EXPECT_EQ(display, display_high_performance);
}

TEST_F(GLDisplayManagerEGLTest, DualGPUs) {
  constexpr uint64_t kIntegratedGpu = 18;
  constexpr uint64_t kDiscreteGpu = 76;
  constexpr uint64_t kDefaultGpu = kIntegratedGpu;

  // Set up.
  ScopedGLDisplayManagerEGL manager;
  manager->OverrideEGLDualGPURenderingSupportForTests(true);
  manager->SetGpuPreference(GpuPreference::kDefault, kDefaultGpu);
  manager->SetGpuPreference(GpuPreference::kLowPower, kIntegratedGpu);
  manager->SetGpuPreference(GpuPreference::kHighPerformance, kDiscreteGpu);

  // Query the low power display.
  GLDisplayEGL* display_low_power =
      manager->GetDisplay(GpuPreference::kLowPower);
  EXPECT_NE(nullptr, display_low_power);
  EXPECT_EQ(kIntegratedGpu, display_low_power->system_device_id());

  // Query again should return the same display.
  GLDisplayEGL* display_low_power_2 =
      manager->GetDisplay(GpuPreference::kLowPower);
  EXPECT_EQ(display_low_power, display_low_power_2);

  // Query the high performance display.
  GLDisplayEGL* display_high_performance =
      manager->GetDisplay(GpuPreference::kHighPerformance);
  EXPECT_NE(nullptr, display_high_performance);
  EXPECT_EQ(kDiscreteGpu, display_high_performance->system_device_id());

  // Query the default display.
  // Due to the set up, it should be the same as the low power display.
  GLDisplayEGL* display_default = manager->GetDisplay(GpuPreference::kDefault);
  EXPECT_EQ(display_low_power, display_default);
}

TEST_F(GLDisplayManagerEGLTest, RemoveDefaultGPU) {
  constexpr uint64_t kIntegratedGpu = 18;
  constexpr uint64_t kDiscreteGpu = 76;
  constexpr uint64_t kDefaultGpu = kIntegratedGpu;

  // Set up.
  ScopedGLDisplayManagerEGL manager;
  manager->OverrideEGLDualGPURenderingSupportForTests(true);
  manager->SetGpuPreference(GpuPreference::kDefault, kDefaultGpu);
  manager->SetGpuPreference(GpuPreference::kLowPower, kIntegratedGpu);
  manager->SetGpuPreference(GpuPreference::kHighPerformance, kDiscreteGpu);
  // Remove the low power GPU, which should change the default to the high
  // performance GPU.
  manager->RemoveGpuPreference(GpuPreference::kLowPower);

  // Query the low power display. Expect it to return the high performance GPU
  GLDisplayEGL* display_low_power =
      manager->GetDisplay(GpuPreference::kLowPower);
  EXPECT_NE(nullptr, display_low_power);
  EXPECT_EQ(kDiscreteGpu, display_low_power->system_device_id());

  // Query the high performance display.
  GLDisplayEGL* display_high_performance =
      manager->GetDisplay(GpuPreference::kHighPerformance);
  EXPECT_NE(nullptr, display_high_performance);
  EXPECT_EQ(kDiscreteGpu, display_high_performance->system_device_id());

  // Query the default display.
  // Due to the set up, it should be the same as the high performance display.
  GLDisplayEGL* display_default = manager->GetDisplay(GpuPreference::kDefault);
  EXPECT_EQ(display_high_performance, display_default);
}

TEST_F(GLDisplayManagerEGLTest, RemoveNonDefaultGPU) {
  constexpr uint64_t kIntegratedGpu = 18;
  constexpr uint64_t kDiscreteGpu = 76;
  constexpr uint64_t kDefaultGpu = kIntegratedGpu;

  // Set up.
  ScopedGLDisplayManagerEGL manager;
  manager->OverrideEGLDualGPURenderingSupportForTests(true);
  manager->SetGpuPreference(GpuPreference::kDefault, kDefaultGpu);
  manager->SetGpuPreference(GpuPreference::kLowPower, kIntegratedGpu);
  manager->SetGpuPreference(GpuPreference::kHighPerformance, kDiscreteGpu);
  // Remove the high performance GPU, which shouldn't affect the default or low
  // power preference.
  manager->RemoveGpuPreference(GpuPreference::kHighPerformance);

  // Query the low power display.
  GLDisplayEGL* display_low_power =
      manager->GetDisplay(GpuPreference::kLowPower);
  EXPECT_NE(nullptr, display_low_power);
  EXPECT_EQ(kIntegratedGpu, display_low_power->system_device_id());

  // Query the high performance display. Expect it to return the low power GPU.
  GLDisplayEGL* display_high_performance =
      manager->GetDisplay(GpuPreference::kHighPerformance);
  EXPECT_NE(nullptr, display_high_performance);
  EXPECT_EQ(kIntegratedGpu, display_high_performance->system_device_id());

  // Query the default display.
  // Due to the set up, it should be the same as the low power display.
  GLDisplayEGL* display_default = manager->GetDisplay(GpuPreference::kDefault);
  EXPECT_EQ(display_low_power, display_default);
}

TEST_F(GLDisplayManagerEGLTest, NoSetUp) {
  // Verify that without setting up GPU preferences, it's still OK to call
  // GetDisplay().
  // This is to make sure tests that do not care about multi-gpus still work.
  ScopedGLDisplayManagerEGL manager;

  // Query the default display.
  GLDisplayEGL* display = manager->GetDisplay(GpuPreference::kDefault);
  EXPECT_NE(nullptr, display);
  EXPECT_EQ(0u, display->system_device_id());
}

}  // namespace gl
