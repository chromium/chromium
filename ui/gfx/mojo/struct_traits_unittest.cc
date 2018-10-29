// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/mojo/accelerated_widget_struct_traits.h"
#include "ui/gfx/mojo/buffer_types_struct_traits.h"
#include "ui/gfx/mojo/presentation_feedback.mojom.h"
#include "ui/gfx/mojo/presentation_feedback_struct_traits.h"
#include "ui/gfx/mojo/traits_test_service.mojom.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/selection_bound.h"
#include "ui/gfx/transform.h"

namespace gfx {

namespace {

gfx::AcceleratedWidget CastToAcceleratedWidget(int i) {
#if defined(USE_OZONE) || defined(USE_X11) || defined(OS_MACOSX)
  return static_cast<gfx::AcceleratedWidget>(i);
#else
  return reinterpret_cast<gfx::AcceleratedWidget>(i);
#endif
}

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    mojom::TraitsTestServicePtr proxy;
    traits_test_bindings_.AddBinding(this, mojo::MakeRequest(&proxy));
    return proxy;
  }

 private:
  // TraitsTestService:
  void EchoSelectionBound(const SelectionBound& s,
                          EchoSelectionBoundCallback callback) override {
    std::move(callback).Run(s);
  }

  void EchoTransform(const Transform& t,
                     EchoTransformCallback callback) override {
    std::move(callback).Run(t);
  }

  void EchoGpuMemoryBufferHandle(
      GpuMemoryBufferHandle handle,
      EchoGpuMemoryBufferHandleCallback callback) override {
    std::move(callback).Run(std::move(handle));
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, SelectionBound) {
  const gfx::SelectionBound::Type type = gfx::SelectionBound::CENTER;
  const gfx::PointF edge_top(1234.5f, 5678.6f);
  const gfx::PointF edge_bottom(910112.5f, 13141516.6f);
  const bool visible = true;
  gfx::SelectionBound input;
  input.set_type(type);
  input.SetEdge(edge_top, edge_bottom);
  input.set_visible(visible);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::SelectionBound output;
  proxy->EchoSelectionBound(input, &output);
  EXPECT_EQ(type, output.type());
  EXPECT_EQ(edge_top, output.edge_top());
  EXPECT_EQ(edge_bottom, output.edge_bottom());
  EXPECT_EQ(input.edge_top_rounded(), output.edge_top_rounded());
  EXPECT_EQ(input.edge_bottom_rounded(), output.edge_bottom_rounded());
  EXPECT_EQ(visible, output.visible());
}

TEST_F(StructTraitsTest, Transform) {
  const float col1row1 = 1.f;
  const float col2row1 = 2.f;
  const float col3row1 = 3.f;
  const float col4row1 = 4.f;
  const float col1row2 = 5.f;
  const float col2row2 = 6.f;
  const float col3row2 = 7.f;
  const float col4row2 = 8.f;
  const float col1row3 = 9.f;
  const float col2row3 = 10.f;
  const float col3row3 = 11.f;
  const float col4row3 = 12.f;
  const float col1row4 = 13.f;
  const float col2row4 = 14.f;
  const float col3row4 = 15.f;
  const float col4row4 = 16.f;
  gfx::Transform input(col1row1, col2row1, col3row1, col4row1, col1row2,
                       col2row2, col3row2, col4row2, col1row3, col2row3,
                       col3row3, col4row3, col1row4, col2row4, col3row4,
                       col4row4);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::Transform output;
  proxy->EchoTransform(input, &output);
  EXPECT_EQ(col1row1, output.matrix().get(0, 0));
  EXPECT_EQ(col2row1, output.matrix().get(0, 1));
  EXPECT_EQ(col3row1, output.matrix().get(0, 2));
  EXPECT_EQ(col4row1, output.matrix().get(0, 3));
  EXPECT_EQ(col1row2, output.matrix().get(1, 0));
  EXPECT_EQ(col2row2, output.matrix().get(1, 1));
  EXPECT_EQ(col3row2, output.matrix().get(1, 2));
  EXPECT_EQ(col4row2, output.matrix().get(1, 3));
  EXPECT_EQ(col1row3, output.matrix().get(2, 0));
  EXPECT_EQ(col2row3, output.matrix().get(2, 1));
  EXPECT_EQ(col3row3, output.matrix().get(2, 2));
  EXPECT_EQ(col4row3, output.matrix().get(2, 3));
  EXPECT_EQ(col1row4, output.matrix().get(3, 0));
  EXPECT_EQ(col2row4, output.matrix().get(3, 1));
  EXPECT_EQ(col3row4, output.matrix().get(3, 2));
  EXPECT_EQ(col4row4, output.matrix().get(3, 3));
}

// AcceleratedWidgets can only be sent between processes on some platforms.
#if defined(OS_WIN) || defined(USE_OZONE) || defined(USE_X11) || \
    defined(OS_MACOSX)
#define MAYBE_AcceleratedWidget AcceleratedWidget
#else
#define MAYBE_AcceleratedWidget DISABLED_AcceleratedWidget
#endif

TEST_F(StructTraitsTest, MAYBE_AcceleratedWidget) {
  gfx::AcceleratedWidget input(CastToAcceleratedWidget(1001));
  gfx::AcceleratedWidget output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::AcceleratedWidget>(&input,
                                                                     &output);
  EXPECT_EQ(input, output);
}

TEST_F(StructTraitsTest, GpuMemoryBufferHandle) {
  const gfx::GpuMemoryBufferId kId(99);
  const uint32_t kOffset = 126;
  const int32_t kStride = 256;
  base::UnsafeSharedMemoryRegion shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(1024);
  ASSERT_TRUE(shared_memory_region.IsValid());
  ASSERT_TRUE(shared_memory_region.Map().IsValid());

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.id = kId;
  handle.region = shared_memory_region.Duplicate();
  handle.offset = kOffset;
  handle.stride = kStride;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::GpuMemoryBufferHandle output;
  proxy->EchoGpuMemoryBufferHandle(std::move(handle), &output);
  EXPECT_EQ(gfx::SHARED_MEMORY_BUFFER, output.type);
  EXPECT_EQ(kId, output.id);
  EXPECT_EQ(kOffset, output.offset);
  EXPECT_EQ(kStride, output.stride);

  base::UnsafeSharedMemoryRegion output_memory = std::move(output.region);
  EXPECT_TRUE(output_memory.Map().IsValid());

#if defined(OS_LINUX)
  gfx::GpuMemoryBufferHandle handle2;
  const uint64_t kSize = kOffset + kStride;
  const uint64_t kModifier = 2;
  handle2.type = gfx::NATIVE_PIXMAP;
  handle2.id = kId;
  handle2.offset = kOffset;
  handle2.stride = kStride;
  handle2.native_pixmap_handle.planes.emplace_back(kOffset, kStride, kSize,
                                                   kModifier);
  proxy->EchoGpuMemoryBufferHandle(std::move(handle2), &output);
  EXPECT_EQ(gfx::NATIVE_PIXMAP, output.type);
  EXPECT_EQ(kId, output.id);
  ASSERT_EQ(1u, output.native_pixmap_handle.planes.size());
  EXPECT_EQ(kSize, output.native_pixmap_handle.planes.back().size);
  EXPECT_EQ(kModifier, output.native_pixmap_handle.planes.back().modifier);
#endif
}

TEST_F(StructTraitsTest, NullGpuMemoryBufferHandle) {
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  GpuMemoryBufferHandle output;
  proxy->EchoGpuMemoryBufferHandle(GpuMemoryBufferHandle(), &output);
  EXPECT_TRUE(output.is_null());
}

TEST_F(StructTraitsTest, BufferFormat) {
  using BufferFormatTraits =
      mojo::EnumTraits<gfx::mojom::BufferFormat, gfx::BufferFormat>;
  BufferFormat output;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  for (int i = 0; i <= static_cast<int>(BufferFormat::LAST); ++i) {
    BufferFormat input = static_cast<BufferFormat>(i);
    BufferFormatTraits::FromMojom(BufferFormatTraits::ToMojom(input), &output);
    EXPECT_EQ(output, input);
  }
}

TEST_F(StructTraitsTest, BufferUsage) {
  using BufferUsageTraits =
      mojo::EnumTraits<gfx::mojom::BufferUsage, gfx::BufferUsage>;
  BufferUsage output;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  for (int i = 0; i <= static_cast<int>(BufferUsage::LAST); ++i) {
    BufferUsage input = static_cast<BufferUsage>(i);
    BufferUsageTraits::FromMojom(BufferUsageTraits::ToMojom(input), &output);
    EXPECT_EQ(output, input);
  }
}

TEST_F(StructTraitsTest, PresentationFeedback) {
  base::TimeTicks timestamp =
      base::TimeTicks() + base::TimeDelta::FromSeconds(12);
  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(23);
  uint32_t flags =
      PresentationFeedback::kVSync | PresentationFeedback::kZeroCopy;
  PresentationFeedback input{timestamp, interval, flags};
  PresentationFeedback output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::PresentationFeedback>(
      &input, &output);
  EXPECT_EQ(timestamp, output.timestamp);
  EXPECT_EQ(interval, output.interval);
  EXPECT_EQ(flags, output.flags);
}

}  // namespace gfx
