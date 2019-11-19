// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/common/linux/gbm_buffer.h"
#include "ui/ozone/common/linux/gbm_device.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::SaveArg;

namespace ui {

namespace {

class FakeGbmBuffer : public GbmBuffer {
 public:
  FakeGbmBuffer() = default;
  ~FakeGbmBuffer() override = default;

  uint32_t GetFormat() const override { return 0; }
  uint64_t GetFormatModifier() const override { return 0; }
  uint32_t GetFlags() const override { return 0; }
  gfx::Size GetSize() const override { return gfx::Size(); }
  gfx::BufferFormat GetBufferFormat() const override {
    return gfx::BufferFormat::BGRA_8888;
  }
  bool AreFdsValid() const override { return false; }
  size_t GetNumPlanes() const override { return 0; }
  int GetPlaneFd(size_t plane) const override { return -1; }
  uint32_t GetPlaneHandle(size_t plane) const override { return 0; }
  uint32_t GetPlaneStride(size_t plane) const override { return 0u; }
  size_t GetPlaneOffset(size_t plane) const override { return 0u; }
  size_t GetPlaneSize(size_t plane) const override { return 0; }
  uint32_t GetHandle() const override { return 0; }
  gfx::NativePixmapHandle ExportHandle() const override {
    return gfx::NativePixmapHandle();
  }
  sk_sp<SkSurface> GetSurface() override { return nullptr; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGbmBuffer);
};

class FakeGbmDevice : public GbmDevice {
 public:
  FakeGbmDevice() = default;
  ~FakeGbmDevice() override = default;

  std::unique_ptr<GbmBuffer> CreateBuffer(uint32_t format,
                                          const gfx::Size& size,
                                          uint32_t flags) override {
    return nullptr;
  }

  std::unique_ptr<GbmBuffer> CreateBufferWithModifiers(
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags,
      const std::vector<uint64_t>& modifiers) override {
    return nullptr;
  }
  std::unique_ptr<GbmBuffer> CreateBufferFromHandle(
      uint32_t format,
      const gfx::Size& size,
      gfx::NativePixmapHandle handle) override {
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGbmDevice);
};

}  // namespace

class WaylandSurfaceFactoryTest : public WaylandTest {
 public:
  WaylandSurfaceFactoryTest() = default;
  ~WaylandSurfaceFactoryTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    auto manager_ptr = connection_->buffer_manager_host()->BindInterface();
    buffer_manager_gpu_->Initialize(std::move(manager_ptr), {}, false);

    // Wait until initialization and mojo calls go through.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // The mojo call to destroy shared buffer goes after surfaces are destroyed.
    // Wait until it's done.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvas(
      gfx::AcceleratedWidget widget) {
    auto canvas = surface_factory_->CreateCanvasForWidget(
        widget_, base::ThreadTaskRunnerHandle::Get().get());
    base::RunLoop().RunUntilIdle();

    return canvas;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandSurfaceFactoryTest);
};

TEST_P(WaylandSurfaceFactoryTest, Canvas) {
  auto canvas = CreateCanvas(widget_);
  ASSERT_TRUE(canvas);

  canvas->ResizeCanvas(window_->GetBounds().size());
  canvas->GetSurface();
  canvas->PresentCanvas(gfx::Rect(5, 10, 20, 15));

  // Wait until the mojo calls are done.
  base::RunLoop().RunUntilIdle();

  Expectation damage = EXPECT_CALL(*surface_, DamageBuffer(5, 10, 20, 15));
  wl_resource* buffer_resource = nullptr;
  Expectation attach = EXPECT_CALL(*surface_, Attach(_, 0, 0))
                           .WillOnce(SaveArg<0>(&buffer_resource));
  EXPECT_CALL(*surface_, Commit()).After(damage, attach);

  Sync();

  ASSERT_TRUE(buffer_resource);
  wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(wl_shm_buffer_get_width(buffer), 800);
  EXPECT_EQ(wl_shm_buffer_get_height(buffer), 600);

  // TODO(forney): We could check that the contents match something drawn to the
  // SkSurface above.
}

TEST_P(WaylandSurfaceFactoryTest, CanvasResize) {
  auto canvas = CreateCanvas(widget_);
  ASSERT_TRUE(canvas);

  canvas->ResizeCanvas(window_->GetBounds().size());
  canvas->GetSurface();
  canvas->ResizeCanvas(gfx::Size(100, 50));
  canvas->GetSurface();
  canvas->PresentCanvas(gfx::Rect(0, 0, 100, 50));

  base::RunLoop().RunUntilIdle();

  Expectation damage = EXPECT_CALL(*surface_, DamageBuffer(0, 0, 100, 50));
  wl_resource* buffer_resource = nullptr;
  Expectation attach = EXPECT_CALL(*surface_, Attach(_, 0, 0))
                           .WillOnce(SaveArg<0>(&buffer_resource));
  EXPECT_CALL(*surface_, Commit()).After(damage, attach);

  Sync();

  ASSERT_TRUE(buffer_resource);
  wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(wl_shm_buffer_get_width(buffer), 100);
  EXPECT_EQ(wl_shm_buffer_get_height(buffer), 50);
}

TEST_P(WaylandSurfaceFactoryTest, CreateSurfaceCheckGbm) {
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  // When gbm is not available, only canvas can be created with viz process
  // used.
  EXPECT_FALSE(buffer_manager_gpu_->gbm_device());

  auto* gl_ozone = surface_factory_->GetGLOzone(gl::kGLImplementationEGLGLES2);
  EXPECT_TRUE(gl_ozone);
  auto gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(widget_);
  EXPECT_FALSE(gl_surface);

  // Now, set gbm.
  buffer_manager_gpu_->set_gbm_device(std::make_unique<FakeGbmDevice>());

  gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(widget_);
  EXPECT_TRUE(gl_surface);

  // Reset gbm now. WaylandConnectionProxy can reset it when zwp is not
  // available. And factory must behave the same way as previously.
  buffer_manager_gpu_->set_gbm_device(nullptr);
  gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(widget_);
  EXPECT_FALSE(gl_surface);
}

INSTANTIATE_TEST_SUITE_P(XdgVersionV5Test,
                         WaylandSurfaceFactoryTest,
                         ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandSurfaceFactoryTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
