// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_surface.h"

#include <fuchsia/ui/composition/cpp/fidl.h>
#include <lib/ui/scenic/cpp/testing/fake_flatland.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/callback_helpers.h"
#include "base/fuchsia/koid.h"
#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_native_pixmap.h"
#include "ui/ozone/public/overlay_plane.h"

using ::scenic::FakeGraph;
using ::scenic::FakeImage;
using ::scenic::FakeTransform;
using ::scenic::FakeTransformPtr;
using ::scenic::FakeView;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SaveArg;
using ::testing::VariantWith;

namespace ui {

namespace {

Matcher<FakeGraph> IsSurfaceGraph(
    const fuchsia::ui::composition::ParentViewportWatcherPtr&
        parent_viewport_watcher,
    const fuchsia::ui::views::ViewportCreationToken& viewport_creation_token,
    const fuchsia::math::VecF& scale,
    std::vector<Matcher<FakeTransformPtr>> child_transform_matchers) {
  auto view_token_koid = base::GetRelatedKoid(viewport_creation_token.value);
  auto watcher_koid = base::GetRelatedKoid(parent_viewport_watcher.channel());

  return AllOf(
      Field("root_transform", &FakeGraph::root_transform,
            Pointee(AllOf(Field("translation", &FakeTransform::translation,
                                FakeTransform::kDefaultTranslation),
                          Field("scale", &FakeTransform::scale, scale),
                          Field("opacity", &FakeTransform::opacity,
                                FakeTransform::kDefaultOpacity),
                          Field("children", &FakeTransform::children,
                                ElementsAreArray(child_transform_matchers))))),
      Field("view", &FakeGraph::view,
            Optional(AllOf(
                Field("view_token", &FakeView::view_token, view_token_koid),
                Field("parent_viewport_watcher",
                      &FakeView::parent_viewport_watcher, watcher_koid)))));
}

Matcher<fuchsia::ui::composition::ImageProperties> IsImageProperties(
    const fuchsia::math::SizeU& size) {
  return AllOf(
      Property("has_size", &fuchsia::ui::composition::ImageProperties::has_size,
               true),
      Property("size", &fuchsia::ui::composition::ImageProperties::size, size));
}

Matcher<FakeTransformPtr> IsImageTransform(
    const fuchsia::math::SizeU& size,
    fuchsia::ui::composition::BlendMode blend_mode,
    const fuchsia::math::Vec& translation = FakeTransform::kDefaultTranslation,
    const fuchsia::math::SizeU& destination_size =
        FakeImage::kDefaultDestinationSize,
    float image_opacity = FakeImage::kDefaultOpacity) {
  return Pointee(AllOf(
      Field("translation", &FakeTransform::translation, translation),
      Field("scale", &FakeTransform::scale, FakeTransform::kDefaultScale),
      Field("opacity", &FakeTransform::opacity, FakeTransform::kDefaultOpacity),
      Field("children", &FakeTransform::children, IsEmpty()),
      Field("content", &FakeTransform::content,
            Pointee(VariantWith<FakeImage>(AllOf(
                Field("image_properties", &FakeImage::image_properties,
                      IsImageProperties(size)),
                Field("destination_size", &FakeImage::destination_size,
                      destination_size),
                Field("blend_mode", &FakeImage::blend_mode, blend_mode),
                Field("opacity", &FakeImage::opacity, image_opacity)))))));
}

scoped_refptr<FlatlandSysmemNativePixmap> CreateFlatlandSysmemNativePixmap(
    uint32_t image_size) {
  gfx::NativePixmapHandle handle;
  zx::eventpair service_handle;
  zx::eventpair::create(0, &service_handle, &handle.buffer_collection_handle);
  auto collection = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
  collection->InitializeForTesting(std::move(service_handle),
                                   gfx::BufferUsage::SCANOUT);
  return base::MakeRefCounted<FlatlandSysmemNativePixmap>(
      collection, std::move(handle), gfx::Size(image_size, image_size));
}

}  // namespace

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

  void SetLayoutInfo(uint32_t width, uint32_t height, float dpr) {
    fuchsia::ui::composition::LayoutInfo layout_info;
    layout_info.set_logical_size({width, height});
    layout_info.set_device_pixel_ratio({dpr, dpr});
    flatland_surface_->OnGetLayout(std::move(layout_info));
  }

  size_t NumberOfPendingClosures() {
    return flatland_surface_->pending_present_closures_.size();
  }

  const fuchsia::ui::composition::ParentViewportWatcherPtr&
  parent_viewport_watcher() {
    return flatland_surface_->parent_viewport_watcher_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  scenic::FakeFlatland fake_flatland_;

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
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(
      [&presents_called](auto) { ++presents_called; });

  FlatlandSurface* surface = CreateFlatlandSurface();
  auto platform_handle = surface->CreateView();
  fuchsia::ui::views::ViewportCreationToken viewport_creation_token;
  viewport_creation_token.value = zx::channel(platform_handle.TakeHandle());

  const int kTestLogicalSize = 100;
  const float kTestDevicePixelRatio = 1.5;
  SetLayoutInfo(kTestLogicalSize, kTestLogicalSize, kTestDevicePixelRatio);
  const float expected_scale = 1 / kTestDevicePixelRatio;
  const uint32_t expected_image_size = kTestLogicalSize * kTestDevicePixelRatio;

  auto primary_plane_pixmap =
      CreateFlatlandSysmemNativePixmap(expected_image_size);
  surface->Present(
      primary_plane_pixmap, std::vector<ui::OverlayPlane>(),
      std::vector<gfx::GpuFenceHandle>(), std::vector<gfx::GpuFenceHandle>(),
      base::BindOnce([](gfx::SwapCompletionResult result) {}),
      base::BindOnce([](const gfx::PresentationFeedback& feedback) {}));

  // There should be a present call in FakeFlatland after flushing the tasks.
  EXPECT_EQ(0u, presents_called);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, presents_called);

  EXPECT_THAT(
      fake_flatland_.graph(),
      IsSurfaceGraph(
          parent_viewport_watcher(), viewport_creation_token,
          {expected_scale, expected_scale},
          {IsImageTransform({expected_image_size, expected_image_size},
                            fuchsia::ui::composition::BlendMode::SRC_OVER)}));
}

TEST_F(FlatlandSurfaceTest, PresentBeforeLayoutInfo) {
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(
      [&presents_called](auto) { ++presents_called; });

  FlatlandSurface* surface = CreateFlatlandSurface();

  auto platform_handle = surface->CreateView();
  fuchsia::ui::views::ViewportCreationToken viewport_creation_token;
  viewport_creation_token.value = zx::channel(platform_handle.TakeHandle());

  const int kTestLogicalSize = 80;
  const float kTestDevicePixelRatio = 2;
  const float expected_scale = 1 / kTestDevicePixelRatio;
  const uint32_t expected_image_size = kTestLogicalSize * kTestDevicePixelRatio;

  auto primary_plane_pixmap =
      CreateFlatlandSysmemNativePixmap(expected_image_size);
  surface->Present(
      primary_plane_pixmap, std::vector<ui::OverlayPlane>(),
      std::vector<gfx::GpuFenceHandle>(), std::vector<gfx::GpuFenceHandle>(),
      base::BindOnce([](gfx::SwapCompletionResult result) {}),
      base::BindOnce([](const gfx::PresentationFeedback& feedback) {}));

  // There should be a one pending present.
  EXPECT_EQ(1u, NumberOfPendingClosures());

  SetLayoutInfo(kTestLogicalSize, kTestLogicalSize, kTestDevicePixelRatio);
  EXPECT_EQ(0u, NumberOfPendingClosures());

  EXPECT_EQ(0u, presents_called);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, presents_called);
  EXPECT_THAT(
      fake_flatland_.graph(),
      IsSurfaceGraph(
          parent_viewport_watcher(), viewport_creation_token,
          {expected_scale, expected_scale},
          {IsImageTransform({expected_image_size, expected_image_size},
                            fuchsia::ui::composition::BlendMode::SRC_OVER)}));
}

TEST_F(FlatlandSurfaceTest, PresentOverlayPlane) {
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(
      [&presents_called](auto) { ++presents_called; });

  FlatlandSurface* surface = CreateFlatlandSurface();
  auto platform_handle = surface->CreateView();
  fuchsia::ui::views::ViewportCreationToken viewport_creation_token;
  viewport_creation_token.value = zx::channel(platform_handle.TakeHandle());

  const int kTestLogicalSize = 100;
  const float kTestDevicePixelRatio = 1.5;
  SetLayoutInfo(kTestLogicalSize, kTestLogicalSize, kTestDevicePixelRatio);
  const float expected_scale = 1 / kTestDevicePixelRatio;
  const uint32_t expected_image_size = kTestLogicalSize * kTestDevicePixelRatio;

  auto primary_plane_pixmap =
      CreateFlatlandSysmemNativePixmap(expected_image_size);

  const float kOverlayOpacity = .7f;
  const int32_t kOverlayX = 10;
  const int32_t kOverlayY = 20;
  const uint32_t kOverlayWidth = expected_image_size - 30;
  const uint32_t kOverlayHeight = expected_image_size - 40;
  const gfx::RectF kOverlayBounds(kOverlayX, kOverlayY, kOverlayWidth,
                                  kOverlayHeight);
  gfx::OverlayPlaneData overlay_data(
      /*z_order=*/1, gfx::OVERLAY_TRANSFORM_NONE, kOverlayBounds,
      /*crop_rect=*/gfx::RectF(),
      /*enable_blend=*/true,
      /*damage_rect=*/gfx::Rect(), kOverlayOpacity,
      gfx::OverlayPriorityHint::kNone,
      /*rounded_corners=*/gfx::RRectF(), gfx::ColorSpace(),
      /*hdr_metadata=*/absl::nullopt);
  ui::OverlayPlane overlay_plane(
      CreateFlatlandSysmemNativePixmap(expected_image_size), nullptr,
      overlay_data);
  std::vector<ui::OverlayPlane> overlays;
  overlays.push_back(std::move(overlay_plane));

  surface->Present(
      primary_plane_pixmap, std::move(overlays),
      std::vector<gfx::GpuFenceHandle>(), std::vector<gfx::GpuFenceHandle>(),
      base::BindOnce([](gfx::SwapCompletionResult result) {}),
      base::BindOnce([](const gfx::PresentationFeedback& feedback) {}));

  EXPECT_EQ(0u, presents_called);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, presents_called);
  EXPECT_THAT(
      fake_flatland_.graph(),
      IsSurfaceGraph(
          parent_viewport_watcher(), viewport_creation_token,
          {expected_scale, expected_scale},
          {IsImageTransform({expected_image_size, expected_image_size},
                            fuchsia::ui::composition::BlendMode::SRC_OVER),
           IsImageTransform({expected_image_size, expected_image_size},
                            fuchsia::ui::composition::BlendMode::SRC_OVER,
                            {kOverlayX, kOverlayY},
                            {kOverlayWidth, kOverlayHeight},
                            kOverlayOpacity)}));
}

}  // namespace ui
