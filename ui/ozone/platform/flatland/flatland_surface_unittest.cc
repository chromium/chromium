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

#include "base/fuchsia/koid.h"
#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/fidl_matchers.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_native_pixmap.h"
#include "ui/ozone/public/overlay_plane.h"

using ::fuchsia::math::SizeU;
using ::fuchsia::math::Vec;
using ::fuchsia::ui::composition::ImageFlip;
using ::fuchsia::ui::composition::Orientation;
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
            Pointee(AllOf(
                Field("translation", &FakeTransform::translation,
                      ::base::test::FidlEq(FakeTransform::kDefaultTranslation)),
                Field("scale", &FakeTransform::scale,
                      ::base::test::FidlEq(scale)),
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
      Property("size", &fuchsia::ui::composition::ImageProperties::size,
               ::base::test::FidlEq(size)));
}

Matcher<FakeTransformPtr> IsImageTransform(
    const fuchsia::math::SizeU& size,
    fuchsia::ui::composition::BlendMode blend_mode,
    const fuchsia::math::Vec& translation = FakeTransform::kDefaultTranslation,
    Orientation orientation = FakeTransform::kDefaultOrientation,
    const fuchsia::math::RectF& sample_region = FakeImage::kDefaultSampleRegion,
    const fuchsia::math::SizeU& destination_size =
        FakeImage::kDefaultDestinationSize,
    float image_opacity = FakeImage::kDefaultOpacity,
    ImageFlip image_flip = FakeImage::kDefaultFlip) {
  return Pointee(AllOf(
      Field("translation", &FakeTransform::translation,
            ::base::test::FidlEq(translation)),
      Field("orientation", &FakeTransform::orientation, orientation),
      Field("scale", &FakeTransform::scale,
            ::base::test::FidlEq(FakeTransform::kDefaultScale)),
      Field("opacity", &FakeTransform::opacity, FakeTransform::kDefaultOpacity),
      Field("children", &FakeTransform::children, IsEmpty()),
      Field("content", &FakeTransform::content,
            Pointee(VariantWith<FakeImage>(
                AllOf(Field("image_properties", &FakeImage::image_properties,
                            IsImageProperties(size)),
                      Field("sample_region", &FakeImage::sample_region,
                            ::base::test::FidlEq(sample_region)),
                      Field("destination_size", &FakeImage::destination_size,
                            ::base::test::FidlEq(destination_size)),
                      Field("blend_mode", &FakeImage::blend_mode, blend_mode),
                      Field("opacity", &FakeImage::opacity, image_opacity),
                      Field("flip", &FakeImage::flip, image_flip)))))));
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

template <class T>
class FlatlandSurfaceTestBase : public T {
 protected:
  FlatlandSurfaceTestBase()
      : fake_flatland_publisher_(test_context_.additional_services(),
                                 fake_flatland_.GetFlatlandRequestHandler()),
        fake_allocator_publisher_(test_context_.additional_services(),
                                  fake_flatland_.GetAllocatorRequestHandler()) {
  }
  ~FlatlandSurfaceTestBase() override = default;

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

class FlatlandSurfaceTest : public FlatlandSurfaceTestBase<testing::Test> {};

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

// The parameters for an overlay transform test are:
// - The input overlay transform.
// - The expected Flatland Transform orientation.
// - The expected Flatland image flip.
// - The expected Flatland Transform translation.
// - The expected destination image size.
using OverlayPlaneTestParams =
    std::tuple<gfx::OverlayTransform, Orientation, ImageFlip, Vec, SizeU>;

class FlatlandSurfaceOverlayPlaneTransformTest
    : public FlatlandSurfaceTestBase<
          testing::TestWithParam<OverlayPlaneTestParams>> {
 protected:
  const int kTestLogicalSize = 100;
  const float kTestDevicePixelRatio = 1.5;

  // Expected image size should be equal to |kTestLogicalSize| *
  // |kTestDevicePixelRatioSize|.
  const uint32_t kExpectedImageSize = 150;

  // Overlay properties.
  const int32_t kOverlayX = 10;
  const int32_t kOverlayY = 20;
  const uint32_t kOverlayWidth = 120;
  const uint32_t kOverlayHeight = 115;
};

INSTANTIATE_TEST_SUITE_P(
    ParameterizedFlatlandSurfaceOverlayPlaneTransformTest,
    FlatlandSurfaceOverlayPlaneTransformTest,
    // Expected translation and expected destination image size are based on the
    // overlay position and size as defined in
    // |FlatlandSurfaceOverlayPlaneTransformTest|.
    testing::Values(std::make_tuple(gfx::OVERLAY_TRANSFORM_NONE,
                                    Orientation::CCW_0_DEGREES,
                                    ImageFlip::NONE,
                                    Vec{10, 20},
                                    SizeU{120U, 115U}),
                    std::make_tuple(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
                                    Orientation::CCW_270_DEGREES,
                                    ImageFlip::NONE,
                                    Vec{130, 20},
                                    SizeU{115U, 120U}),
                    std::make_tuple(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180,
                                    Orientation::CCW_180_DEGREES,
                                    ImageFlip::NONE,
                                    Vec{130, 135},
                                    SizeU{120U, 115U}),
                    std::make_tuple(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
                                    Orientation::CCW_90_DEGREES,
                                    ImageFlip::NONE,
                                    Vec{10, 135},
                                    SizeU{115U, 120U}),
                    std::make_tuple(gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL,
                                    Orientation::CCW_0_DEGREES,
                                    ImageFlip::LEFT_RIGHT,
                                    Vec{10, 20},
                                    SizeU{120U, 115U}),
                    std::make_tuple(gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL,
                                    Orientation::CCW_0_DEGREES,
                                    ImageFlip::UP_DOWN,
                                    Vec{10, 20},
                                    SizeU{120U, 115U})));

TEST_P(FlatlandSurfaceOverlayPlaneTransformTest, PresentOverlayPlane) {
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(
      [&presents_called](auto) { ++presents_called; });

  const auto [input_transform, expected_orientation, expected_image_flip,
              expected_translation, expected_image_destination_size] =
      GetParam();

  FlatlandSurface* surface = CreateFlatlandSurface();
  auto platform_handle = surface->CreateView();
  fuchsia::ui::views::ViewportCreationToken viewport_creation_token;
  viewport_creation_token.value = zx::channel(platform_handle.TakeHandle());

  SetLayoutInfo(kTestLogicalSize, kTestLogicalSize, kTestDevicePixelRatio);
  const float expected_scale = 1 / kTestDevicePixelRatio;

  auto primary_plane_pixmap =
      CreateFlatlandSysmemNativePixmap(kExpectedImageSize);

  const float kOverlayOpacity = .7f;
  const gfx::RectF kOverlayBounds(kOverlayX, kOverlayY, kOverlayWidth,
                                  kOverlayHeight);
  const float kCropX = 0.01f;
  const float kCropY = 0.02f;
  const float kCropWidth = 0.97f;
  const float kCropHeight = 0.93f;
  gfx::OverlayPlaneData overlay_data(
      /*z_order=*/1, input_transform, kOverlayBounds,
      gfx::RectF(kCropX, kCropY, kCropWidth, kCropHeight),
      /*enable_blend=*/true,
      /*damage_rect=*/gfx::Rect(), kOverlayOpacity,
      gfx::OverlayPriorityHint::kNone,
      /*rounded_corners=*/gfx::RRectF(), gfx::ColorSpace(),
      /*hdr_metadata=*/std::nullopt);
  ui::OverlayPlane overlay_plane(
      CreateFlatlandSysmemNativePixmap(kExpectedImageSize), nullptr,
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
          {IsImageTransform({kExpectedImageSize, kExpectedImageSize},
                            fuchsia::ui::composition::BlendMode::SRC_OVER),
           IsImageTransform(
               {kExpectedImageSize, kExpectedImageSize},
               fuchsia::ui::composition::BlendMode::SRC_OVER,
               expected_translation, expected_orientation,
               {kCropX * kExpectedImageSize, kCropY * kExpectedImageSize,
                kCropWidth * kExpectedImageSize,
                kCropHeight * kExpectedImageSize},
               expected_image_destination_size, kOverlayOpacity,
               expected_image_flip)}));
}

}  // namespace ui
