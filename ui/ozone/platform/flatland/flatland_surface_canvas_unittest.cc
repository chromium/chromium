// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_surface_canvas.h"

#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/testing/fake_flatland.h>

#include "base/fuchsia/koid.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"

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
using ::testing::VariantWith;

namespace ui {

namespace {

const size_t kImageSize = 100;

Matcher<fuchsia::ui::composition::ImageProperties> IsImageProperties(
    const fuchsia::math::SizeU& size) {
  return AllOf(
      Property("has_size", &fuchsia::ui::composition::ImageProperties::has_size,
               true),
      Property("size", &fuchsia::ui::composition::ImageProperties::size, size));
}

Matcher<FakeGraph> IsSurfaceGraph(
    const fuchsia::ui::views::ViewportCreationToken& viewport_creation_token,
    const fuchsia::math::SizeU& size,
    fuchsia::ui::composition::BlendMode blend_mode,
    const fuchsia::math::Vec& translation = FakeTransform::kDefaultTranslation,
    const fuchsia::math::SizeU& destination_size =
        FakeImage::kDefaultDestinationSize,
    float image_opacity = FakeImage::kDefaultOpacity) {
  auto view_token_koid = base::GetRelatedKoid(viewport_creation_token.value);

  return AllOf(
      Field(
          "root_transform", &FakeGraph::root_transform,
          Pointee(AllOf(
              Field("translation", &FakeTransform::translation,
                    FakeTransform::kDefaultTranslation),
              Field("scale", &FakeTransform::scale,
                    FakeTransform::kDefaultScale),
              Field("opacity", &FakeTransform::opacity,
                    FakeTransform::kDefaultOpacity),
              Field("children", &FakeTransform::children, IsEmpty()),
              Field("content", &FakeTransform::content,
                    Pointee(VariantWith<FakeImage>(AllOf(
                        Field("image_properties", &FakeImage::image_properties,
                              IsImageProperties(size)),
                        Field("destination_size", &FakeImage::destination_size,
                              destination_size),
                        Field("blend_mode", &FakeImage::blend_mode, blend_mode),
                        Field("opacity", &FakeImage::opacity, image_opacity)))))

                  ))),
      Field("view", &FakeGraph::view,
            Optional(
                Field("view_token", &FakeView::view_token, view_token_koid))));
}

fuchsia::sysmem::AllocatorSyncPtr ConnectSysmemAllocator() {
  fuchsia::sysmem::AllocatorSyncPtr allocator;
  base::ComponentContextForProcess()->svc()->Connect(allocator.NewRequest());
  return allocator;
}

size_t RoundUp(size_t value, size_t alignment) {
  return ((value + alignment - 1) / alignment) * alignment;
}

}  // namespace

class FlatlandSurfaceCanvasTest : public ::testing::Test {
 public:
  FlatlandSurfaceCanvasTest()
      : sysmem_allocator_(ConnectSysmemAllocator()),
        fake_flatland_publisher_(test_context_.additional_services(),
                                 fake_flatland_.GetFlatlandRequestHandler()) {}
  ~FlatlandSurfaceCanvasTest() override {}

 protected:
  struct FrameBuffer {
    base::ReadOnlySharedMemoryMapping mapping;
  };

  void InitializeCanvas() {
    canvas_ = std::make_unique<FlatlandSurfaceCanvas>(sysmem_allocator_.get(),
                                                      &fake_flatland_);
    auto platform_handle = canvas_->CreateView();
    viewport_creation_token_ = {.value =
                                    zx::channel(platform_handle.TakeHandle())};
    task_environment_.RunUntilIdle();

    canvas_->ResizeCanvas(gfx::Size(kImageSize, kImageSize), 1.0);

    task_environment_.RunUntilIdle();
    ASSERT_EQ(fake_flatland_.graph_bindings().buffer_collections.size(), 1U);

    fuchsia::sysmem::BufferCollectionSyncPtr buffer_collection;
    sysmem_allocator_->BindSharedCollection(
        std::move(fake_flatland_.graph_bindings()
                      .buffer_collections.begin()
                      ->second.sysmem_token),
        buffer_collection.NewRequest());

    fuchsia::sysmem::BufferCollectionConstraints constraints;
    constraints.usage.cpu = fuchsia::sysmem::cpuUsageRead;

    constraints.has_buffer_memory_constraints = true;
    constraints.buffer_memory_constraints.cpu_domain_supported = true;

    buffer_collection->SetConstraints(
        /*has_constraints=*/true, std::move(constraints));

    int32_t wait_status;
    fuchsia::sysmem::BufferCollectionInfo_2 buffer_info;
    zx_status_t status =
        buffer_collection->WaitForBuffersAllocated(&wait_status, &buffer_info);
    ASSERT_EQ(status, ZX_OK);

    buffer_collection->Close();

    EXPECT_GE(buffer_info.settings.image_format_constraints.min_coded_width,
              kImageSize);
    EXPECT_GE(buffer_info.settings.image_format_constraints.min_coded_height,
              kImageSize);

    for (size_t i = 0; i < buffer_info.buffer_count; ++i) {
      auto memory_region = base::ReadOnlySharedMemoryRegion::Deserialize(
          base::subtle::PlatformSharedMemoryRegion::Take(
              std::move(buffer_info.buffers[i].vmo),
              base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly,
              buffer_info.settings.buffer_settings.size_bytes,
              base::UnguessableToken::Create()));
      auto mapping = memory_region.Map();
      ASSERT_TRUE(mapping.IsValid());
      frames_.push_back(FrameBuffer{.mapping = std::move(mapping)});
    }

    const fuchsia::sysmem::ImageFormatConstraints& format =
        buffer_info.settings.image_format_constraints;
    image_stride_ =
        RoundUp(std::max(static_cast<size_t>(format.min_bytes_per_row),
                         kImageSize * 4U),
                format.bytes_per_row_divisor);
  }

  void PresentFrame(SkRect dirty_rect = SkRect()) {
    if (dirty_rect.isEmpty()) {
      dirty_rect = SkRect::MakeWH(kImageSize, kImageSize);
    }

    canvas_->PresentCanvas(gfx::Rect(dirty_rect.left(), dirty_rect.top(),
                                     dirty_rect.width(), dirty_rect.height()));
    task_environment_.RunUntilIdle();

    fuchsia::ui::composition::OnNextFrameBeginValues begin_values;
    begin_values.set_additional_present_credits(1);
    fake_flatland_.FireOnNextFrameBeginEvent(std::move(begin_values));
    task_environment_.RunUntilIdle();

    ASSERT_TRUE(std::holds_alternative<FakeImage>(
        *fake_flatland_.graph().root_transform->content));
    ASSERT_EQ(
        std::get<FakeImage>(*fake_flatland_.graph().root_transform->content)
            .collection_id,
        fake_flatland_.graph_bindings().buffer_collections.begin()->first);
  }

  SkColor GetPixelValueAt(size_t x, size_t y) {
    int frame_index =
        std::get<FakeImage>(*fake_flatland_.graph().root_transform->content)
            .vmo_index;
    const char* image_data =
        reinterpret_cast<const char*>(frames_[frame_index].mapping.memory());
    size_t pixel_offset = y * image_stride_ + x * sizeof(SkColor);
    auto result = *reinterpret_cast<const SkColor*>(image_data + pixel_offset);
    return result;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  fuchsia::sysmem::AllocatorSyncPtr sysmem_allocator_;
  scenic::FakeFlatland fake_flatland_;

  base::TestComponentContextForProcess test_context_;

  // Injects binding for responding to Flatland protocol connection requests.
  const base::ScopedServicePublisher<fuchsia::ui::composition::Flatland>
      fake_flatland_publisher_;

  std::unique_ptr<FlatlandSurfaceCanvas> canvas_ = nullptr;
  fuchsia::ui::views::ViewportCreationToken viewport_creation_token_;

  std::vector<FrameBuffer> frames_;
  size_t image_stride_ = 0;
};

TEST_F(FlatlandSurfaceCanvasTest, InitializeAndPresent) {
  ASSERT_NO_FATAL_FAILURE(InitializeCanvas());

  auto* canvas = canvas_->GetCanvas();
  EXPECT_TRUE(canvas);

  canvas_->PresentCanvas(gfx::Rect(kImageSize, kImageSize));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(fake_flatland_.graph(),
              IsSurfaceGraph(viewport_creation_token_, {kImageSize, kImageSize},
                             fuchsia::ui::composition::BlendMode::SRC_OVER));
}

TEST_F(FlatlandSurfaceCanvasTest, ImageContent) {
  ASSERT_NO_FATAL_FAILURE(InitializeCanvas());

  // First, present a blue frame.
  auto* canvas = canvas_->GetCanvas();
  ASSERT_TRUE(canvas);
  canvas->drawColor(SK_ColorBLUE);

  ASSERT_NO_FATAL_FAILURE(PresentFrame());

  EXPECT_EQ(GetPixelValueAt(0, 0), SK_ColorBLUE);
  EXPECT_EQ(GetPixelValueAt(kImageSize / 2, kImageSize / 2), SK_ColorBLUE);

  // Fill the bottom right corner to green.
  canvas = canvas_->GetCanvas();
  SkRect green_rect = SkRect::MakeXYWH(kImageSize / 2, kImageSize / 2,
                                       kImageSize / 2, kImageSize / 2);

  ASSERT_TRUE(canvas);
  canvas->clipRect(green_rect);
  canvas->drawColor(SK_ColorGREEN);

  ASSERT_NO_FATAL_FAILURE(PresentFrame(green_rect));

  EXPECT_EQ(GetPixelValueAt(0, 0), SK_ColorBLUE);
  EXPECT_EQ(GetPixelValueAt(kImageSize / 2, kImageSize / 2), SK_ColorGREEN);
}

}  // namespace ui
