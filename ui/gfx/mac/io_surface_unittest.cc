// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/io_surface.h"

#include <CoreVideo/CoreVideo.h>

#include "components/viz/common/resources/shared_image_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

namespace {

// Uncompressed biplanar layouts. Video-range and full-range of the same
// layout share WebGPU / overlay flags; range lives on gfx::ColorSpace.
constexpr struct {
  uint32_t video_range;
  uint32_t full_range;
} kUncompressedBiplanarRangePairs[] = {
    {kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
     kCVPixelFormatType_420YpCbCr8BiPlanarFullRange},
    {kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange,
     kCVPixelFormatType_422YpCbCr8BiPlanarFullRange},
    {kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange,
     kCVPixelFormatType_444YpCbCr8BiPlanarFullRange},
    {kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
     kCVPixelFormatType_420YpCbCr10BiPlanarFullRange},
    {kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange,
     kCVPixelFormatType_422YpCbCr10BiPlanarFullRange},
    {kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange,
     kCVPixelFormatType_444YpCbCr10BiPlanarFullRange},
};

TEST(IOSurface, SharedImageFormatToIOSurfacePixelFormat) {
  bool override_rgba_to_bgra = true;
  EXPECT_EQ(SharedImageFormatToIOSurfacePixelFormat(
                viz::SinglePlaneFormat::kR_8, override_rgba_to_bgra),
            static_cast<uint32_t>(kCVPixelFormatType_OneComponent8));
  EXPECT_EQ(
      SharedImageFormatToIOSurfacePixelFormat(viz::MultiPlaneFormat::kNV12,
                                              override_rgba_to_bgra),
      static_cast<uint32_t>(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange));
  EXPECT_EQ(
      SharedImageFormatToIOSurfacePixelFormat(viz::MultiPlaneFormat::kP010,
                                              override_rgba_to_bgra),
      static_cast<uint32_t>(kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange));
  EXPECT_EQ(SharedImageFormatToIOSurfacePixelFormat(
                viz::MultiPlaneFormat::kI420, override_rgba_to_bgra),
            static_cast<uint32_t>(kCVPixelFormatType_420YpCbCr8Planar));
  EXPECT_EQ(SharedImageFormatToIOSurfacePixelFormat(
                viz::MultiPlaneFormat::kYV12, override_rgba_to_bgra),
            std::nullopt);
}

TEST(IOSurface, IOSurfacePixelFormatToSharedImageFormat) {
  EXPECT_EQ(
      IOSurfacePixelFormatToSharedImageFormat(kCVPixelFormatType_OneComponent8),
      viz::SinglePlaneFormat::kR_8);
  EXPECT_EQ(IOSurfacePixelFormatToSharedImageFormat(
                kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
            viz::MultiPlaneFormat::kNV12);
  EXPECT_EQ(IOSurfacePixelFormatToSharedImageFormat(
                kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange),
            viz::MultiPlaneFormat::kP010);
  EXPECT_EQ(IOSurfacePixelFormatToSharedImageFormat(
                kCVPixelFormatType_420YpCbCr8Planar),
            viz::MultiPlaneFormat::kI420);
  // Full-range formats are recognized when they arrive from a decoder, even
  // though SharedImageFormatToIOSurfacePixelFormat never selects them.
  EXPECT_EQ(IOSurfacePixelFormatToSharedImageFormat(
                kCVPixelFormatType_420YpCbCr10BiPlanarFullRange),
            viz::MultiPlaneFormat::kP010);
  EXPECT_EQ(IOSurfacePixelFormatToSharedImageFormat(
                kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarFullRange),
            viz::MultiPlaneFormat::kP010);
  EXPECT_EQ(IOSurfacePixelFormatToSharedImageFormat('FAKE'), std::nullopt);
}

TEST(IOSurface, WebGPUCompatibility) {
  EXPECT_TRUE(
      IOSurfacePixelFormatIsWebGPUCompatible(kCVPixelFormatType_32BGRA));
  EXPECT_TRUE(
      IOSurfacePixelFormatIsWebGPUCompatible(kCVPixelFormatType_32RGBA));
  EXPECT_TRUE(IOSurfacePixelFormatIsWebGPUCompatible(
      kCVPixelFormatType_TwoComponent16Half));
  EXPECT_FALSE(IOSurfacePixelFormatIsWebGPUCompatible(
      kCVPixelFormatType_420YpCbCr8Planar));

  for (const auto& pair : kUncompressedBiplanarRangePairs) {
    EXPECT_TRUE(IOSurfacePixelFormatIsWebGPUCompatible(pair.video_range));
    EXPECT_TRUE(IOSurfacePixelFormatIsWebGPUCompatible(pair.full_range));
  }
}

TEST(IOSurface, MaxBitsPerComponent) {
  EXPECT_EQ(
      IOSurfacePixelFormatMaxBitsPerComponent(kCVPixelFormatType_OneComponent8),
      8u);
  EXPECT_EQ(IOSurfacePixelFormatMaxBitsPerComponent(
                kCVPixelFormatType_ARGB2101010LEPacked),
            10u);
  EXPECT_EQ(
      IOSurfacePixelFormatMaxBitsPerComponent(kCVPixelFormatType_64RGBAHalf),
      16u);
  EXPECT_EQ(IOSurfacePixelFormatMaxBitsPerComponent(
                kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
            8u);
  EXPECT_EQ(IOSurfacePixelFormatMaxBitsPerComponent(
                kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange),
            10u);
  EXPECT_EQ(IOSurfacePixelFormatMaxBitsPerComponent('FAKE'), 0u);
}

TEST(IOSurface, CanDisplayAsAVSampleBuffer) {
  for (const auto& pair : kUncompressedBiplanarRangePairs) {
    EXPECT_TRUE(
        IOSurfacePixelFormatCanDisplayAsAVSampleBuffer(pair.video_range));
    EXPECT_TRUE(
        IOSurfacePixelFormatCanDisplayAsAVSampleBuffer(pair.full_range));
  }

  EXPECT_FALSE(IOSurfacePixelFormatCanDisplayAsAVSampleBuffer(
      kCVPixelFormatType_32BGRA));
  EXPECT_FALSE(IOSurfacePixelFormatCanDisplayAsAVSampleBuffer(
      kCVPixelFormatType_32RGBA));
  EXPECT_FALSE(IOSurfacePixelFormatCanDisplayAsAVSampleBuffer(
      kCVPixelFormatType_420YpCbCr8Planar));
  EXPECT_FALSE(IOSurfacePixelFormatCanDisplayAsAVSampleBuffer(
      kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar));
  EXPECT_FALSE(IOSurfacePixelFormatCanDisplayAsAVSampleBuffer(
      kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarVideoRange));
  EXPECT_FALSE(IOSurfacePixelFormatCanDisplayAsAVSampleBuffer(
      kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarFullRange));
  EXPECT_FALSE(IOSurfacePixelFormatCanDisplayAsAVSampleBuffer('FAKE'));
}

TEST(IOSurface, RangeID) {
  EXPECT_EQ(IOSurfacePixelFormatRangeID(kCVPixelFormatType_OneComponent8),
            ColorSpace::RangeID::FULL);
  EXPECT_EQ(IOSurfacePixelFormatRangeID(kCVPixelFormatType_ARGB2101010LEPacked),
            ColorSpace::RangeID::FULL);
  EXPECT_EQ(IOSurfacePixelFormatRangeID(
                kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
            ColorSpace::RangeID::LIMITED);
  EXPECT_EQ(IOSurfacePixelFormatRangeID(
                kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange),
            ColorSpace::RangeID::LIMITED);
  EXPECT_EQ(IOSurfacePixelFormatRangeID(
                kCVPixelFormatType_420YpCbCr8BiPlanarFullRange),
            ColorSpace::RangeID::FULL);
  EXPECT_EQ(IOSurfacePixelFormatRangeID(
                kCVPixelFormatType_420YpCbCr10BiPlanarFullRange),
            ColorSpace::RangeID::FULL);
  EXPECT_EQ(IOSurfacePixelFormatRangeID('FAKE'), ColorSpace::RangeID::INVALID);
}

TEST(IOSurface, OddSizeMultiPlanar) {
  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface =
      CreateIOSurface(gfx::Size(101, 99), viz::MultiPlaneFormat::kNV12);
  DCHECK(io_surface);
  // Plane sizes are rounded up.
  // https://crbug.com/1226056
  EXPECT_EQ(IOSurfaceGetWidthOfPlane(io_surface.get(), 1), 51u);
  EXPECT_EQ(IOSurfaceGetHeightOfPlane(io_surface.get(), 1), 50u);
}

TEST(IOSurface, MachPortRetainDeadName) {
  const mach_port_t task = mach_task_self();

  // Create a port and give it send rights so that it will transition to a dead
  // name when the receive right is removed.
  mach_port_t port = MACH_PORT_NULL;
  ASSERT_EQ(KERN_SUCCESS,
            mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port));
  ASSERT_EQ(KERN_SUCCESS,
            mach_port_insert_right(task, port, port, MACH_MSG_TYPE_MAKE_SEND));

  // Remove the receive right.
  ASSERT_EQ(KERN_SUCCESS,
            mach_port_mod_refs(task, port, MACH_PORT_RIGHT_RECEIVE, -1));
  mach_port_type_t port_type = MACH_PORT_TYPE_NONE;
  ASSERT_EQ(KERN_SUCCESS, mach_port_type(task, port, &port_type));
  ASSERT_TRUE(port_type & MACH_PORT_TYPE_DEAD_NAME)
      << "port should have transitioned to a dead name";

  // Attempting to retain a dead name fails, `Retain(port)` should return NULL.
  mach_port_t result = internal::IOSurfaceMachPortTraits::Retain(port);
  EXPECT_EQ(result, static_cast<mach_port_t>(MACH_PORT_NULL))
      << "IOSurface should not retain a dead port";
}

}  // namespace

}  // namespace gfx
