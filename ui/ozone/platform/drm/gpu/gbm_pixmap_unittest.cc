// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"

#include <drm_fourcc.h>
#include <gbm.h>

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"

namespace ui {

namespace {

constexpr gfx::Size kBufferSize(16, 16);

class DestructionTrackingDrmDevice : public FakeDrmDevice {
 public:
  explicit DestructionTrackingDrmDevice(bool* destroyed)
      : FakeDrmDevice(std::make_unique<MockGbmDevice>()),
        destroyed_(destroyed) {}

 private:
  ~DestructionTrackingDrmDevice() override { *destroyed_ = true; }

  const raw_ptr<bool> destroyed_;
};

// Forwards to `inner` and runs `on_destroyed` when destroyed, so tests can
// check what is still alive at that point.
class DestructionCheckingGbmBuffer : public GbmBuffer {
 public:
  DestructionCheckingGbmBuffer(std::unique_ptr<GbmBuffer> inner,
                               base::OnceClosure on_destroyed)
      : inner_(std::move(inner)), on_destroyed_(std::move(on_destroyed)) {}

  ~DestructionCheckingGbmBuffer() override { std::move(on_destroyed_).Run(); }

  uint32_t GetFormat() const override { return inner_->GetFormat(); }
  uint64_t GetFormatModifier() const override {
    return inner_->GetFormatModifier();
  }
  uint32_t GetFlags() const override { return inner_->GetFlags(); }
  gfx::Size GetSize() const override { return inner_->GetSize(); }
  viz::SharedImageFormat GetSharedImageFormat() const override {
    return inner_->GetSharedImageFormat();
  }
  bool AreFdsValid() const override { return inner_->AreFdsValid(); }
  size_t GetNumPlanes() const override { return inner_->GetNumPlanes(); }
  bool SupportsZeroCopyWebGPUImport() const override {
    return inner_->SupportsZeroCopyWebGPUImport();
  }
  int GetPlaneFd(size_t plane) const override {
    return inner_->GetPlaneFd(plane);
  }
  uint32_t GetPlaneHandle(size_t plane) const override {
    return inner_->GetPlaneHandle(plane);
  }
  uint32_t GetPlaneStride(size_t plane) const override {
    return inner_->GetPlaneStride(plane);
  }
  size_t GetPlaneOffset(size_t plane) const override {
    return inner_->GetPlaneOffset(plane);
  }
  size_t GetPlaneSize(size_t plane) const override {
    return inner_->GetPlaneSize(plane);
  }
  uint32_t GetHandle() const override { return inner_->GetHandle(); }
  gfx::NativePixmapHandle ExportHandle() const override {
    return inner_->ExportHandle();
  }
  sk_sp<SkSurface> GetSurface() override { return inner_->GetSurface(); }

 private:
  const std::unique_ptr<GbmBuffer> inner_;
  base::OnceClosure on_destroyed_;
};

}  // namespace

class GbmPixmapTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// A buffer without scanout usage has no framebuffer, so the pixmap itself must
// keep the device (and its GBM device) alive until the buffer is destroyed.
TEST_F(GbmPixmapTest, DestroysBufferBeforeDrmDeviceWithoutFramebuffer) {
  bool device_destroyed = false;
  bool buffer_destroyed = false;
  auto drm =
      base::MakeRefCounted<DestructionTrackingDrmDevice>(&device_destroyed);
  std::unique_ptr<GbmBuffer> inner = drm->gbm_device()->CreateBuffer(
      DRM_FORMAT_XRGB8888, kBufferSize, GBM_BO_USE_RENDERING);
  ASSERT_TRUE(inner);
  auto buffer = std::make_unique<DestructionCheckingGbmBuffer>(
      std::move(inner), base::BindLambdaForTesting([&]() {
        EXPECT_FALSE(device_destroyed);
        buffer_destroyed = true;
      }));

  auto pixmap = base::MakeRefCounted<GbmPixmap>(
      /*surface_manager=*/nullptr, std::move(drm), std::move(buffer),
      /*framebuffer=*/nullptr);
  EXPECT_FALSE(device_destroyed);

  pixmap.reset();
  EXPECT_TRUE(buffer_destroyed);
  EXPECT_TRUE(device_destroyed);
}

// A scanout buffer is destroyed after its framebuffer and before the device.
TEST_F(GbmPixmapTest, DestroysFramebufferThenBufferThenDrmDevice) {
  bool device_destroyed = false;
  bool buffer_destroyed = false;
  auto drm =
      base::MakeRefCounted<DestructionTrackingDrmDevice>(&device_destroyed);
  FakeDrmDevice* fake_drm = drm.get();
  std::unique_ptr<GbmBuffer> inner = drm->gbm_device()->CreateBuffer(
      DRM_FORMAT_XRGB8888, kBufferSize, GBM_BO_USE_SCANOUT);
  ASSERT_TRUE(inner);
  auto buffer = std::make_unique<DestructionCheckingGbmBuffer>(
      std::move(inner), base::BindLambdaForTesting([&]() {
        ASSERT_FALSE(device_destroyed);
        EXPECT_EQ(fake_drm->get_remove_framebuffer_call_count(), 1);
        buffer_destroyed = true;
      }));
  scoped_refptr<DrmFramebuffer> framebuffer =
      DrmFramebuffer::AddFramebuffer(drm, buffer.get(), kBufferSize);
  ASSERT_TRUE(framebuffer);

  auto pixmap = base::MakeRefCounted<GbmPixmap>(
      /*surface_manager=*/nullptr, std::move(drm), std::move(buffer),
      std::move(framebuffer));
  EXPECT_EQ(fake_drm->get_remove_framebuffer_call_count(), 0);

  pixmap.reset();
  EXPECT_TRUE(buffer_destroyed);
  EXPECT_TRUE(device_destroyed);
}

}  // namespace ui
