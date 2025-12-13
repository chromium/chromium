// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/io_surface.h"

#include <Availability.h>
#include <CoreGraphics/CoreGraphics.h>
#include <stddef.h>
#include <stdint.h>

#include "base/apple/mach_logging.h"
#include "base/bits.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/mac/color_space_util.h"

namespace gfx {

namespace {

void AddIntegerValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     int32_t value) {
  base::apple::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(nullptr, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}

// Return the expected four character code pixel format for an IOSurface with
// the specified viz::SharedImageFormat.
uint32_t SharedImageFormatToIOSurfacePixelFormat(viz::SharedImageFormat format,
                                                 bool override_rgba_to_bgra) {
  if (format == viz::SinglePlaneFormat::kR_8) {
    return 'L008';
  } else if (format == viz::SinglePlaneFormat::kR_F16) {
    return 'L00h';
  } else if (format == viz::SinglePlaneFormat::kRG_88) {
    return '2C08';
  } else if (format == viz::SinglePlaneFormat::kR_16) {
    return 'L016';
  } else if (format == viz::SinglePlaneFormat::kRG_1616) {
    return '2C16';
  } else if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
    return 'l10r';  // little-endian ARGB2101010 full-range ARGB
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888 ||
             format == viz::SinglePlaneFormat::kBGRX_8888) {
    return 'BGRA';
  } else if (format == viz::SinglePlaneFormat::kRGBA_8888 ||
             format == viz::SinglePlaneFormat::kRGBX_8888) {
    return override_rgba_to_bgra ? 'BGRA' : 'RGBA';
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return 'RGhA';
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    return '420v';
  } else if (format == viz::MultiPlaneFormat::kNV12A) {
    return 'v0a8';
  } else if (format == viz::MultiPlaneFormat::kP010) {
    return 'x420';
  } else {
    // Technically RGBA_1010102 should be accepted as 'R10k', but then it won't
    // be supported by CGLTexImageIOSurface2D(), so it's best to reject it here.
    NOTREACHED();
  }
}

}  // namespace

namespace internal {

// static
mach_port_t IOSurfaceMachPortTraits::Retain(mach_port_t port) {
  kern_return_t kr =
      mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "IOSurfaceMachPortTraits::Retain mach_port_mod_refs";
  return port;
}

// static
void IOSurfaceMachPortTraits::Release(mach_port_t port) {
  kern_return_t kr = mach_port_deallocate(mach_task_self(), port);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "IOSurfaceMachPortTraits::Release mach_port_deallocate";
}

// Common method used by IOSurfaceSetColorSpace and IOSurfaceCanSetColorSpace.
bool IOSurfaceSetColorSpace(IOSurfaceRef io_surface,
                            const ColorSpace& color_space) {
  // Allow but ignore invalid color spaces.
  if (!color_space.IsValid())
    return true;

  // Prefer using named spaces.
  CFStringRef color_space_name = nullptr;
  if (color_space == ColorSpace::CreateSRGB() ||
      color_space == ColorSpace::CreateREC709()) {
    color_space_name = kCGColorSpaceSRGB;
  } else if (color_space == ColorSpace::CreateDisplayP3D65()) {
    color_space_name = kCGColorSpaceDisplayP3;
  } else if (color_space == ColorSpace::CreateExtendedSRGB()) {
    color_space_name = kCGColorSpaceExtendedSRGB;
  } else if (color_space == ColorSpace::CreateSRGBLinear()) {
    color_space_name = kCGColorSpaceExtendedLinearSRGB;
  }

  // The symbols kCGColorSpaceITUR_2020_PQ_EOTF and kCGColorSpaceITUR_2020_HLG
  // have been deprecated. Claim that we were able to set the color space,
  // because the path that will render these color spaces will use the
  // HDRCopier, which will manually convert them to a non-deprecated format.
  // https://crbug.com/1108627: Bug wherein these symbols are deprecated and
  // also not available in some SDK versions.
  // https://crbug.com/1101041: Introduces the HDR copier.
  // https://crbug.com/1061723: Discussion of issues related to HLG.
  if (color_space == ColorSpace(ColorSpace::PrimaryID::BT2020,
                                ColorSpace::TransferID::PQ,
                                ColorSpace::MatrixID::BT2020_NCL,
                                ColorSpace::RangeID::LIMITED)) {
    color_space_name = kCGColorSpaceITUR_2100_PQ;
  } else if (color_space == ColorSpace(ColorSpace::PrimaryID::BT2020,
                                       ColorSpace::TransferID::HLG,
                                       ColorSpace::MatrixID::BT2020_NCL,
                                       ColorSpace::RangeID::LIMITED)) {
    color_space_name = kCGColorSpaceITUR_2100_HLG;
  }

  // https://crbug.com/1488397: Set parameters that will be rendering YUV
  // content.
  // TODO(b/304442486): Add gamma support here.
  {
    CFStringRef primaries = nullptr;
    CFStringRef transfer = nullptr;
    CFStringRef matrix = nullptr;
    if (ColorSpaceToCVImageBufferKeys(color_space, /*prefer_srgb_trfn=*/true,
                                      &primaries, &transfer, &matrix)) {
      IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorPrimaries"),
                        primaries);
      IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceTransferFunction"),
                        transfer);
      IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceYCbCrMatrix"), matrix);
    }
  }

  if (color_space_name) {
    IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorSpace"),
                      color_space_name);
    return true;
  }

  gfx::ColorSpace as_rgb = color_space.GetAsRGB();
  gfx::ColorSpace as_full_range_rgb = color_space.GetAsFullRangeRGB();

  // IOSurfaces do not support full-range YUV video. Fortunately, the hardware
  // decoders never produce full-range video.
  // https://crbug.com/882627
  if (color_space != as_rgb && as_rgb == as_full_range_rgb)
    return false;

  // Generate an ICCProfile from the parametric color space.
  ICCProfile icc_profile = ICCProfile::FromColorSpace(as_full_range_rgb);
  if (!icc_profile.IsValid())
    return false;

  // Package it as a CFDataRef and send it to the IOSurface.
  std::vector<char> icc_profile_data = icc_profile.GetData();
  base::apple::ScopedCFTypeRef<CFDataRef> cf_data_icc_profile(CFDataCreate(
      nullptr, reinterpret_cast<const UInt8*>(icc_profile_data.data()),
      icc_profile_data.size()));

  IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorSpace"),
                    cf_data_icc_profile.get());
  return true;
}

}  // namespace internal

ScopedIOSurface CreateIOSurface(const gfx::Size& size,
                                viz::SharedImageFormat format,
                                bool should_clear,
                                bool override_rgba_to_bgra) {
  TRACE_EVENT0("ui", "CreateIOSurface");
  base::TimeTicks start_time = base::TimeTicks::Now();

  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> properties(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties.get(), kIOSurfaceWidth, size.width());
  AddIntegerValue(properties.get(), kIOSurfaceHeight, size.height());
  AddIntegerValue(
      properties.get(), kIOSurfacePixelFormat,
      SharedImageFormatToIOSurfacePixelFormat(format, override_rgba_to_bgra));

  // Don't specify plane information unless there are indeed multiple planes
  // because DisplayLink drivers do not support this.
  // http://crbug.com/527556
  size_t num_planes = format.NumberOfPlanes();
  if (num_planes > 1) {
    base::apple::ScopedCFTypeRef<CFMutableArrayRef> planes(CFArrayCreateMutable(
        kCFAllocatorDefault, num_planes, &kCFTypeArrayCallBacks));
    size_t total_bytes_alloc = 0;
    for (size_t plane = 0; plane < num_planes; ++plane) {
      const gfx::Size plane_size = format.GetPlaneSize(plane, size);
      const size_t plane_width = plane_size.width();
      const size_t plane_height = plane_size.height();
      size_t plane_bytes_per_element = format.NumChannelsInPlane(plane);
      if (format.channel_format() !=
          viz::SharedImageFormat::ChannelFormat::k8) {
        plane_bytes_per_element *= 2;
      }
      const size_t plane_bytes_per_row =
          IOSurfaceAlignProperty(kIOSurfacePlaneBytesPerRow,
                                 base::bits::AlignUp(plane_width, size_t{2}) *
                                     plane_bytes_per_element);
      const size_t plane_bytes_alloc = IOSurfaceAlignProperty(
          kIOSurfacePlaneSize,
          base::bits::AlignUp(plane_height, size_t{2}) * plane_bytes_per_row);
      const size_t plane_offset =
          IOSurfaceAlignProperty(kIOSurfacePlaneOffset, total_bytes_alloc);

      base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> plane_info(
          CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                    &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks));
      AddIntegerValue(plane_info.get(), kIOSurfacePlaneWidth, plane_width);
      AddIntegerValue(plane_info.get(), kIOSurfacePlaneHeight, plane_height);
      AddIntegerValue(plane_info.get(), kIOSurfacePlaneBytesPerElement,
                      plane_bytes_per_element);
      AddIntegerValue(plane_info.get(), kIOSurfacePlaneBytesPerRow,
                      plane_bytes_per_row);
      AddIntegerValue(plane_info.get(), kIOSurfacePlaneSize, plane_bytes_alloc);
      AddIntegerValue(plane_info.get(), kIOSurfacePlaneOffset, plane_offset);
      CFArrayAppendValue(planes.get(), plane_info.get());
      total_bytes_alloc = plane_offset + plane_bytes_alloc;
    }
    CFDictionaryAddValue(properties.get(), kIOSurfacePlaneInfo, planes.get());

    total_bytes_alloc =
        IOSurfaceAlignProperty(kIOSurfaceAllocSize, total_bytes_alloc);
    AddIntegerValue(properties.get(), kIOSurfaceAllocSize, total_bytes_alloc);
  } else {
    const size_t bytes_per_element = format.BytesPerPixel();
    const size_t bytes_per_row = IOSurfaceAlignProperty(
        kIOSurfaceBytesPerRow,
        base::bits::AlignUp(static_cast<size_t>(size.width()), size_t{2}) *
            bytes_per_element);
    const size_t bytes_alloc = IOSurfaceAlignProperty(
        kIOSurfaceAllocSize,
        base::bits::AlignUp(static_cast<size_t>(size.height()), size_t{2}) *
            bytes_per_row);
    AddIntegerValue(properties.get(), kIOSurfaceBytesPerElement,
                    bytes_per_element);
    AddIntegerValue(properties.get(), kIOSurfaceBytesPerRow, bytes_per_row);
    AddIntegerValue(properties.get(), kIOSurfaceAllocSize, bytes_alloc);
  }

  ScopedIOSurface io_surface(IOSurfaceCreate(properties.get()));
  if (!io_surface) {
    LOG(ERROR) << "Failed to allocate IOSurface of size " << size.ToString()
               << ".";
    return ScopedIOSurface();
  }

  if (should_clear) {
    // Zero-initialize the IOSurface. Calling IOSurfaceLock/IOSurfaceUnlock
    // appears to be sufficient. https://crbug.com/584760#c17
    kern_return_t r = IOSurfaceLock(io_surface.get(), 0, nullptr);
    DCHECK_EQ(KERN_SUCCESS, r);
    r = IOSurfaceUnlock(io_surface.get(), 0, nullptr);
    DCHECK_EQ(KERN_SUCCESS, r);
  }

  // Ensure that all IOSurfaces start as sRGB.
  IOSurfaceSetValue(io_surface.get(), CFSTR("IOSurfaceColorSpace"),
                    kCGColorSpaceSRGB);

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "GPU.IOSurface.CreationTimeUs", base::TimeTicks::Now() - start_time,
      base::Microseconds(1), base::Milliseconds(50), /*bucket_count=*/100);

  return io_surface;
}

#if BUILDFLAG(IS_IOS)
void ExportIOSurfaceSharedMemoryRegion(
    IOSurfaceRef io_surface,
    base::UnsafeSharedMemoryRegion& shared_memory_region,
    std::array<uint32_t, kMaxIOSurfacePlanes>& plane_strides,
    std::array<uint32_t, kMaxIOSurfacePlanes>& plane_offsets) {
  CHECK(io_surface);

  const void* io_surface_base_addr = IOSurfaceGetBaseAddress(io_surface);
  const size_t io_surface_alloc_size = IOSurfaceGetAllocSize(io_surface);

  memory_object_size_t alloc_size = io_surface_alloc_size;
  base::apple::ScopedMachSendRight named_right;
  kern_return_t kr = mach_make_memory_entry_64(
      mach_task_self(), &alloc_size,
      reinterpret_cast<memory_object_offset_t>(io_surface_base_addr),
      VM_PROT_READ | VM_PROT_WRITE,
      base::apple::ScopedMachSendRight::Receiver(named_right).get(),
      MACH_PORT_NULL);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_make_memory_entry_64";
  CHECK_GE(alloc_size, io_surface_alloc_size);

  using base::subtle::PlatformSharedMemoryRegion;
  auto platform_shared_memory_region = PlatformSharedMemoryRegion::Take(
      std::move(named_right), PlatformSharedMemoryRegion::Mode::kUnsafe,
      alloc_size, base::UnguessableToken::Create());
  CHECK(platform_shared_memory_region.IsValid());

  shared_memory_region = base::UnsafeSharedMemoryRegion::Deserialize(
      std::move(platform_shared_memory_region));
  CHECK(shared_memory_region.IsValid());

  // IOSurfaceGetPlaneCount returns 0 for single-plane surfaces.
  const size_t plane_count = std::max(1ul, IOSurfaceGetPlaneCount(io_surface));
  plane_strides = {};
  plane_offsets = {};
  for (size_t plane = 0; plane < plane_count; plane++) {
    plane_strides[plane] = base::checked_cast<uint32_t>(
        IOSurfaceGetBytesPerRowOfPlane(io_surface, plane));

    const void* io_surface_plane_addr =
        IOSurfaceGetBaseAddressOfPlane(io_surface, plane);
    plane_offsets[plane] = base::checked_cast<uint32_t>(
        reinterpret_cast<intptr_t>(io_surface_plane_addr) -
        reinterpret_cast<intptr_t>(io_surface_base_addr));
  }
}
#endif

bool IOSurfaceCanSetColorSpace(const ColorSpace& color_space) {
  return internal::IOSurfaceSetColorSpace(nullptr, color_space);
}

void IOSurfaceSetColorSpace(IOSurfaceRef io_surface,
                            const ColorSpace& color_space) {
  if (!internal::IOSurfaceSetColorSpace(io_surface, color_space)) {
    DLOG(ERROR) << "Failed to set color space for IOSurface: "
                << color_space.ToString();
  }
}

ScopedIOSurface IOSurfaceMachPortToIOSurface(
    ScopedRefCountedIOSurfaceMachPort io_surface_mach_port) {
  ScopedIOSurface io_surface;
  if (!io_surface_mach_port) {
    DLOG(ERROR) << "Invalid mach port.";
    return io_surface;
  }
  io_surface.reset(IOSurfaceLookupFromMachPort(io_surface_mach_port.get()));
  if (!io_surface) {
    DLOG(ERROR) << "Unable to lookup IOSurface.";
    return io_surface;
  }
  return io_surface;
}

}  // namespace gfx
