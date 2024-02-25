// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/platform_shared_memory_handle.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mojom/accelerated_widget_mojom_traits.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"
#include "ui/gfx/mojom/hdr_metadata.mojom.h"
#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"
#include "ui/gfx/mojom/presentation_feedback.mojom.h"
#include "ui/gfx/mojom/presentation_feedback_mojom_traits.h"
#include "ui/gfx/mojom/traits_test_service.mojom.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/selection_bound.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/koid.h"
#endif

namespace gfx {

namespace {

gfx::AcceleratedWidget CastToAcceleratedWidget(int i) {
#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_APPLE)
  return static_cast<gfx::AcceleratedWidget>(i);
#else
  return reinterpret_cast<gfx::AcceleratedWidget>(i);
#endif
}

// Used by the GpuMemoryBufferHandle test to produce a valid object handle to
// embed in a NativePixmapPlane object, so that the test isn't sending an
// invalid FD/vmo object where the mojom requires a valid one.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
base::ScopedFD CreateValidLookingBufferHandle() {
  return base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
             base::UnsafeSharedMemoryRegion::Create(1024))
      .PassPlatformHandle()
      .fd;
}
#elif BUILDFLAG(IS_FUCHSIA)
zx::vmo CreateValidLookingBufferHandle() {
  return base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
             base::UnsafeSharedMemoryRegion::Create(1024))
      .PassPlatformHandle();
}
#endif

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

  StructTraitsTest(const StructTraitsTest&) = delete;
  StructTraitsTest& operator=(const StructTraitsTest&) = delete;

 protected:
  mojo::Remote<mojom::TraitsTestService> GetTraitsTestRemote() {
    mojo::Remote<mojom::TraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
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

  void EchoRRectF(const RRectF& r, EchoRRectFCallback callback) override {
    std::move(callback).Run(r);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<TraitsTestService> traits_test_receivers_;
};

}  // namespace

TEST_F(StructTraitsTest, SelectionBound) {
  const gfx::SelectionBound::Type type = gfx::SelectionBound::CENTER;
  const gfx::PointF edge_start(1234.5f, 5678.6f);
  const gfx::PointF edge_end(910112.5f, 13141516.6f);
  const bool visible = true;
  gfx::SelectionBound input;
  input.set_type(type);
  input.SetEdge(edge_start, edge_end);
  input.set_visible(visible);
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gfx::SelectionBound output;
  remote->EchoSelectionBound(input, &output);
  EXPECT_EQ(type, output.type());
  EXPECT_EQ(edge_start, output.edge_start());
  EXPECT_EQ(edge_end, output.edge_end());
  EXPECT_EQ(input.edge_start_rounded(), output.edge_start_rounded());
  EXPECT_EQ(input.edge_end_rounded(), output.edge_end_rounded());
  EXPECT_EQ(visible, output.visible());
}

TEST_F(StructTraitsTest, Transform) {
  const float r0c0 = 1.f;
  const float r0c1 = 2.f;
  const float r0c2 = 3.f;
  const float r0c3 = 4.f;
  const float r1c0 = 5.f;
  const float r1c1 = 6.f;
  const float r1c2 = 7.f;
  const float r1c3 = 8.f;
  const float r2c0 = 9.f;
  const float r2c1 = 10.f;
  const float r2c2 = 11.f;
  const float r2c3 = 12.f;
  const float r3c0 = 13.f;
  const float r3c1 = 14.f;
  const float r3c2 = 15.f;
  const float r3c3 = 16.f;
  auto input =
      gfx::Transform::RowMajor(r0c0, r0c1, r0c2, r0c3, r1c0, r1c1, r1c2, r1c3,
                               r2c0, r2c1, r2c2, r2c3, r3c0, r3c1, r3c2, r3c3);
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gfx::Transform output;
  remote->EchoTransform(input, &output);
  EXPECT_EQ(r0c0, output.rc(0, 0));
  EXPECT_EQ(r0c1, output.rc(0, 1));
  EXPECT_EQ(r0c2, output.rc(0, 2));
  EXPECT_EQ(r0c3, output.rc(0, 3));
  EXPECT_EQ(r1c0, output.rc(1, 0));
  EXPECT_EQ(r1c1, output.rc(1, 1));
  EXPECT_EQ(r1c2, output.rc(1, 2));
  EXPECT_EQ(r1c3, output.rc(1, 3));
  EXPECT_EQ(r2c0, output.rc(2, 0));
  EXPECT_EQ(r2c1, output.rc(2, 1));
  EXPECT_EQ(r2c2, output.rc(2, 2));
  EXPECT_EQ(r2c3, output.rc(2, 3));
  EXPECT_EQ(r3c0, output.rc(3, 0));
  EXPECT_EQ(r3c1, output.rc(3, 1));
  EXPECT_EQ(r3c2, output.rc(3, 2));
  EXPECT_EQ(r3c3, output.rc(3, 3));
}

TEST_F(StructTraitsTest, AcceleratedWidget) {
  gfx::AcceleratedWidget input(CastToAcceleratedWidget(1001));
  gfx::AcceleratedWidget output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::AcceleratedWidget>(input,
                                                                     output);
  EXPECT_EQ(input, output);
}

TEST_F(StructTraitsTest, GpuMemoryBufferHandle) {
  const gfx::GpuMemoryBufferId kId(99);
  const uint32_t kOffset = 126;
  const uint32_t kStride = 256;
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

  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gfx::GpuMemoryBufferHandle output;
  remote->EchoGpuMemoryBufferHandle(std::move(handle), &output);
  EXPECT_EQ(gfx::SHARED_MEMORY_BUFFER, output.type);
  EXPECT_EQ(kId, output.id);
  EXPECT_EQ(kOffset, output.offset);
  EXPECT_EQ(kStride, output.stride);

  base::UnsafeSharedMemoryRegion output_memory = std::move(output.region);
  EXPECT_TRUE(output_memory.Map().IsValid());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
  gfx::GpuMemoryBufferHandle handle2;
  const uint64_t kSize = kOffset + kStride;
  handle2.type = gfx::NATIVE_PIXMAP;
  handle2.id = kId;
  handle2.offset = kOffset;
  handle2.stride = kStride;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  const uint64_t kModifier = 2;
  base::ScopedFD buffer_handle = CreateValidLookingBufferHandle();
  handle2.native_pixmap_handle.modifier = kModifier;
#elif BUILDFLAG(IS_FUCHSIA)
  zx::vmo buffer_handle = CreateValidLookingBufferHandle();
  zx::eventpair client_handle, service_handle;
  auto status = zx::eventpair::create(0, &client_handle, &service_handle);
  DCHECK_EQ(status, ZX_OK);
  zx_koid_t handle_koid = base::GetKoid(client_handle).value();
  handle2.native_pixmap_handle.buffer_collection_handle =
      std::move(client_handle);
  handle2.native_pixmap_handle.buffer_index = 4;
  handle2.native_pixmap_handle.ram_coherency = true;
#endif
  handle2.native_pixmap_handle.planes.emplace_back(kOffset, kStride, kSize,
                                                   std::move(buffer_handle));
  remote->EchoGpuMemoryBufferHandle(std::move(handle2), &output);
  EXPECT_EQ(gfx::NATIVE_PIXMAP, output.type);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(kModifier, output.native_pixmap_handle.modifier);
#elif BUILDFLAG(IS_FUCHSIA)
  EXPECT_EQ(handle_koid,
            base::GetKoid(output.native_pixmap_handle.buffer_collection_handle)
                .value());
  EXPECT_EQ(4U, output.native_pixmap_handle.buffer_index);
  EXPECT_EQ(true, output.native_pixmap_handle.ram_coherency);
#endif
  ASSERT_EQ(1u, output.native_pixmap_handle.planes.size());
  EXPECT_EQ(kSize, output.native_pixmap_handle.planes.back().size);
#endif
}

TEST_F(StructTraitsTest, NullGpuMemoryBufferHandle) {
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  GpuMemoryBufferHandle output;
  remote->EchoGpuMemoryBufferHandle(GpuMemoryBufferHandle(), &output);
  EXPECT_TRUE(output.is_null());
}

TEST_F(StructTraitsTest, BufferFormat) {
  using BufferFormatTraits =
      mojo::EnumTraits<gfx::mojom::BufferFormat, gfx::BufferFormat>;
  BufferFormat output;
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
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
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  for (int i = 0; i <= static_cast<int>(BufferUsage::LAST); ++i) {
    BufferUsage input = static_cast<BufferUsage>(i);
    BufferUsageTraits::FromMojom(BufferUsageTraits::ToMojom(input), &output);
    EXPECT_EQ(output, input);
  }
}

TEST_F(StructTraitsTest, PresentationFeedback) {
  base::TimeTicks timestamp = base::TimeTicks() + base::Seconds(12);
  base::TimeDelta interval = base::Milliseconds(23);
  uint32_t flags =
      PresentationFeedback::kVSync | PresentationFeedback::kZeroCopy;
  PresentationFeedback input{timestamp, interval, flags};
#if BUILDFLAG(IS_MAC)
  input.ca_layer_error_code = kCALayerFailedPictureContent;
#endif

  input.available_timestamp = base::TimeTicks() + base::Milliseconds(20);
  input.ready_timestamp = base::TimeTicks() + base::Milliseconds(21);
  input.latch_timestamp = base::TimeTicks() + base::Milliseconds(22);
  PresentationFeedback output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::PresentationFeedback>(input,
                                                                        output);
  EXPECT_EQ(timestamp, output.timestamp);
  EXPECT_EQ(interval, output.interval);
  EXPECT_EQ(flags, output.flags);
  EXPECT_EQ(input.available_timestamp, output.available_timestamp);
  EXPECT_EQ(input.ready_timestamp, output.ready_timestamp);
  EXPECT_EQ(input.latch_timestamp, output.latch_timestamp);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(input.ca_layer_error_code, output.ca_layer_error_code);
#endif
}

TEST_F(StructTraitsTest, RRectF) {
  gfx::RRectF input(40, 50, 60, 70, 1, 2);
  input.SetCornerRadii(RRectF::Corner::kUpperRight, 3, 4);
  input.SetCornerRadii(RRectF::Corner::kLowerRight, 5, 6);
  input.SetCornerRadii(RRectF::Corner::kLowerLeft, 7, 8);
  EXPECT_EQ(input.GetType(), RRectF::Type::kComplex);
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gfx::RRectF output;
  remote->EchoRRectF(input, &output);
  EXPECT_EQ(input, output);
  input = gfx::RRectF(40, 50, 0, 70, 0);
  EXPECT_EQ(input.GetType(), RRectF::Type::kEmpty);
  remote->EchoRRectF(input, &output);
  EXPECT_EQ(input, output);
  input = RRectF(40, 50, 60, 70, 0);
  EXPECT_EQ(input.GetType(), RRectF::Type::kRect);
  remote->EchoRRectF(input, &output);
  EXPECT_EQ(input, output);
  input = RRectF(40, 50, 60, 70, 5);
  EXPECT_EQ(input.GetType(), RRectF::Type::kSingle);
  remote->EchoRRectF(input, &output);
  EXPECT_EQ(input, output);
  input = RRectF(40, 50, 60, 70, 6, 3);
  EXPECT_EQ(input.GetType(), RRectF::Type::kSimple);
  remote->EchoRRectF(input, &output);
  EXPECT_EQ(input, output);
  input = RRectF(40, 50, 60, 70, 30, 35);
  EXPECT_EQ(input.GetType(), RRectF::Type::kOval);
  remote->EchoRRectF(input, &output);
  EXPECT_EQ(input, output);
  input.SetCornerRadii(RRectF::Corner::kUpperLeft, 50, 50);
  input.SetCornerRadii(RRectF::Corner::kUpperRight, 20, 20);
  input.SetCornerRadii(RRectF::Corner::kLowerRight, 0, 0);
  input.SetCornerRadii(RRectF::Corner::kLowerLeft, 0, 0);
  remote->EchoRRectF(input, &output);
  EXPECT_EQ(input, output);
}

TEST_F(StructTraitsTest, HDRMetadata) {
  // Test an empty input/output.
  gfx::HDRMetadata input;
  gfx::HDRMetadata output;
  mojo::test::SerializeAndDeserialize<gfx::mojom::HDRMetadata>(input, output);
  EXPECT_EQ(input, output);

  // Include CTA 861.3.
  input.cta_861_3.emplace(123, 456);
  mojo::test::SerializeAndDeserialize<gfx::mojom::HDRMetadata>(input, output);
  EXPECT_EQ(input, output);

  // Include SMPTE ST 2086.
  input.smpte_st_2086.emplace(SkNamedPrimariesExt::kRec2020, 789, 123);
  mojo::test::SerializeAndDeserialize<gfx::mojom::HDRMetadata>(input, output);
  EXPECT_EQ(input, output);

  // Include SDR white level.
  input.ndwl.emplace(123.f);
  mojo::test::SerializeAndDeserialize<gfx::mojom::HDRMetadata>(input, output);
  EXPECT_EQ(input, output);

  // Include extended range.
  input.extended_range.emplace(10.f, 4.f);
  mojo::test::SerializeAndDeserialize<gfx::mojom::HDRMetadata>(input, output);
  EXPECT_EQ(input, output);
}

}  // namespace gfx
