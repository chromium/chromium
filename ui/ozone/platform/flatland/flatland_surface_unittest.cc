// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_surface.h"

#include <fuchsia/ui/composition/cpp/fidl.h>
#include <vector>

#include "base/callback_helpers.h"
#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_native_pixmap.h"
#include "ui/ozone/platform/flatland/tests/fake_flatland.h"
#include "ui/ozone/public/overlay_plane.h"

using ::testing::_;
using ::testing::SaveArg;

namespace ui {

class MockFlatlandSurfaceFactory : public FlatlandSurfaceFactory {
 public:
  MockFlatlandSurfaceFactory() = default;
  ~MockFlatlandSurfaceFactory() override {}

  MOCK_METHOD2(AddSurface,
               void(gfx::AcceleratedWidget widget, FlatlandSurface* surface));
  MOCK_METHOD1(RemoveSurface, void(gfx::AcceleratedWidget widget));
};

class FlatlandSurfaceTest : public ::testing::Test {
 protected:
  FlatlandSurfaceTest()
      : fake_flatland_publisher_(test_context_.additional_services(),
                                 fake_flatland_.GetFlatlandRequestHandler()),
        fake_allocator_publisher_(test_context_.additional_services(),
                                  fake_flatland_.GetAllocatorRequestHandler()) {
  }
  ~FlatlandSurfaceTest() override = default;

  FlatlandSurface* CreateFlatlandSurface() {
    EXPECT_CALL(mock_factory_, AddSurface(_, _));
    EXPECT_CALL(mock_factory_, RemoveSurface(_));
    flatland_surface_ = std::make_unique<FlatlandSurface>(
        &mock_factory_, gfx::kNullAcceleratedWidget);
    return flatland_surface_.get();
  }

  void SetLayoutInfo() {
    fuchsia::ui::composition::LayoutInfo layout_info;
    layout_info.set_logical_size({100, 100});
    flatland_surface_->OnGetLayout(std::move(layout_info));
  }

  size_t NumberOfPendingClosures() {
    return flatland_surface_->pending_present_closures_.size();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  FakeFlatland fake_flatland_;

 private:
  base::TestComponentContextForProcess test_context_;
  // Injects binding for responding to Flatland protocol connection requests.
  const base::ScopedServicePublisher<fuchsia::ui::composition::Flatland>
      fake_flatland_publisher_;
  // Injects binding for responding to Allocator protocol connection requests.
  const base::ScopedServicePublisher<fuchsia::ui::composition::Allocator>
      fake_allocator_publisher_;
  MockFlatlandSurfaceFactory mock_factory_;
  std::unique_ptr<FlatlandSurface> flatland_surface_;
};

TEST_F(FlatlandSurfaceTest, Initialization) {
  MockFlatlandSurfaceFactory mock_factory;
  gfx::AcceleratedWidget widget;
  EXPECT_CALL(mock_factory, AddSurface(_, _)).WillOnce(SaveArg<0>(&widget));

  FlatlandSurface surface(&mock_factory, gfx::kNullAcceleratedWidget);

  ASSERT_EQ(widget, gfx::kNullAcceleratedWidget);
  EXPECT_CALL(mock_factory, RemoveSurface(widget));

  // Check that there are no crashes after flushing tasks.
  task_environment_.RunUntilIdle();
}

TEST_F(FlatlandSurfaceTest, PresentPrimaryPlane) {
  fake_flatland_.SetPresentHandler(base::DoNothing());

  FlatlandSurface* surface = CreateFlatlandSurface();
  SetLayoutInfo();

  auto buffer_collection_id = gfx::SysmemBufferCollectionId::Create();
  gfx::NativePixmapHandle handle;
  handle.buffer_collection_id = buffer_collection_id;
  handle.buffer_index = 0;
  auto collection = base::MakeRefCounted<FlatlandSysmemBufferCollection>(
      buffer_collection_id);
  collection->InitializeForTesting(gfx::BufferUsage::SCANOUT);
  auto primary_plane_pixmap = base::MakeRefCounted<FlatlandSysmemNativePixmap>(
      collection, std::move(handle), gfx::Size(1, 1));
  surface->Present(
      primary_plane_pixmap, std::vector<ui::OverlayPlane>(),
      std::vector<gfx::GpuFenceHandle>(), std::vector<gfx::GpuFenceHandle>(),
      base::BindOnce([](gfx::SwapCompletionResult result) {}),
      base::BindOnce([](const gfx::PresentationFeedback& feedback) {}));

  // There should be a present call in FakeFlatland.
  task_environment_.RunUntilIdle();

  // TODO(crbug.com/1307545): Extend with checks on Fake Flatland and Allocator
  // once they move into fuchsia SDK.
}

TEST_F(FlatlandSurfaceTest, PresentBeforeLayoutInfo) {
  fake_flatland_.SetPresentHandler(base::DoNothing());

  FlatlandSurface* surface = CreateFlatlandSurface();

  auto buffer_collection_id = gfx::SysmemBufferCollectionId::Create();
  gfx::NativePixmapHandle handle;
  handle.buffer_collection_id = buffer_collection_id;
  handle.buffer_index = 0;
  auto collection = base::MakeRefCounted<FlatlandSysmemBufferCollection>(
      buffer_collection_id);
  collection->InitializeForTesting(gfx::BufferUsage::SCANOUT);
  auto primary_plane_pixmap = base::MakeRefCounted<FlatlandSysmemNativePixmap>(
      collection, std::move(handle), gfx::Size(1, 1));
  surface->Present(
      primary_plane_pixmap, std::vector<ui::OverlayPlane>(),
      std::vector<gfx::GpuFenceHandle>(), std::vector<gfx::GpuFenceHandle>(),
      base::BindOnce([](gfx::SwapCompletionResult result) {}),
      base::BindOnce([](const gfx::PresentationFeedback& feedback) {}));

  // There should be a one pending present.
  EXPECT_EQ(1u, NumberOfPendingClosures());

  SetLayoutInfo();
  EXPECT_EQ(0u, NumberOfPendingClosures());
}

}  // namespace ui
