// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/mac/color_space_util.h"

namespace gfx {

namespace {

BASE_FEATURE(kIOSurfaceUseNamedSRGBForREC709,
             "IOSurfaceUseNamedSRGBForREC709",
             base::FEATURE_ENABLED_BY_DEFAULT);

void AddIntegerValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     int32_t value) {
  base::apple::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(nullptr, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}

int32_t BytesPerElement(gfx::BufferFormat format, int plane) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      DCHECK_EQ(plane, 0);
      return 1;
    case gfx::BufferFormat::R_16:
      DCHECK_EQ(plane, 0);
      return 2;
    case gfx::BufferFormat::RG_88:
      DCHECK_EQ(plane, 0);
      return 2;
    case gfx::BufferFormat::RG_1616:
      DCHECK_EQ(plane, 0);
      return 4;
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_1010102:
      DCHECK_EQ(plane, 0);
      return 4;
    case gfx::BufferFormat::RGBA_F16:
      DCHECK_EQ(plane, 0);
      return 8;
    case gfx::BufferFormat::YUV_420_BIPLANAR: {
      constexpr int32_t bytes_per_element[] = {1, 2};
      DCHECK_LT(static_cast<size_t>(plane), std::size(bytes_per_element));
      return bytes_per_element[plane];
    }
    case gfx::BufferFormat::YUVA_420_TRIPLANAR: {
      constexpr int32_t bytes_per_element[] = {1, 2, 1};
      DCHECK_LT(static_cast<size_t>(plane), std::size(bytes_per_element));
      return bytes_per_element[plane];
    }
    case gfx::BufferFormat::P010: {
      constexpr int32_t bytes_per_element[] = {2, 4};
      DCHECK_LT(static_cast<size_t>(plane), std::size(bytes_per_element));
      return bytes_per_element[plane];
    }
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_1010102:
    case gfx::BufferFormat::YVU_420:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

}  // namespace

uint32_t BufferFormatToIOSurfacePixelFormat(gfx::BufferFormat format,
                                            bool override_rgba_to_bgra) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return 'L008';
    case gfx::BufferFormat::RG_88:
      return '2C08';
    case gfx::BufferFormat::R_16:
      return 'L016';
    case gfx::BufferFormat::RG_1616:
      return '2C16';
    case gfx::BufferFormat::BGRA_1010102:
      return 'l10r';  // little-endian ARGB2101010 full-range ARGB
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return 'BGRA';
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return override_rgba_to_bgra ? 'BGRA' : 'RGBA';
    case gfx::BufferFormat::RGBA_F16:
      return 'RGhA';
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return '420v';
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return 'v0a8';
    case gfx::BufferFormat::P010:
      return 'x420';
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_1010102:
    // Technically RGBA_1010102 should be accepted as 'R10k', but then it won't
    // be supported by CGLTexImageIOSurface2D(), so it's best to reject it here.
    case gfx::BufferFormat::YVU_420:
      return 0;
  }
}

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
  kern_return_t kr =
      mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, -1);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "IOSurfaceMachPortTraits::Release mach_port_mod_refs";
}

// Common method used by IOSurfaceSetColorSpace and IOSurfaceCanSetColorSpace.
bool IOSurfaceSetColorSpace(IOSurfaceRef io_surface,
                            const ColorSpace& color_space) {
  // Allow but ignore invalid color spaces.
  if (!color_space.IsValid())
    return true;

  static const bool prefer_srgb_trfn =
      base::FeatureList::IsEnabled(kIOSurfaceUseNamedSRGBForREC709);

  // Prefer using named spaces.
  CFStringRef color_space_name = nullptr;
  if (color_space == ColorSpace::CreateSRGB() ||
      (prefer_srgb_trfn && color_space == ColorSpace::CreateREC709())) {
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
    if (ColorSpaceToCVImageBufferKeys(color_space, prefer_srgb_trfn, &primaries,
                                      &transfer, &matrix)) {
      IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorPrimaries"),
                        primaries);
      IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceTransferFunction"),
                        transfer);
      IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceYCbCrMatrix"), matrix);
    }
  }

  if (color_space_name) {
    if (io_surface) {
      IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorSpace"),
                        color_space_name);
    }
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

base::apple::ScopedCFTypeRef<IOSurfaceRef> CreateIOSurface(
    const gfx::Size& size,
    gfx::BufferFormat format,
    bool should_clear,
    bool override_rgba_to_bgra) {
  TRACE_EVENT0("ui", "CreateIOSurface");
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> properties(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties.get(), kIOSurfaceWidth, size.width());
  AddIntegerValue(properties.get(), kIOSurfaceHeight, size.height());
  AddIntegerValue(
      properties.get(), kIOSurfacePixelFormat,
      BufferFormatToIOSurfacePixelFormat(format, override_rgba_to_bgra));

  // Don't specify plane information unless there are indeed multiple planes
  // because DisplayLink drivers do not support this.
  // http://crbug.com/527556
  size_t num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);
  if (num_planes > 1) {
    base::apple::ScopedCFTypeRef<CFMutableArrayRef> planes(CFArrayCreateMutable(
        kCFAllocatorDefault, num_planes, &kCFTypeArrayCallBacks));
    size_t total_bytes_alloc = 0;
    for (size_t plane = 0; plane < num_planes; ++plane) {
      const size_t factor =
          gfx::SubsamplingFactorForBufferFormat(format, plane);
      const size_t plane_width = (size.width() + factor - 1) / factor;
      const size_t plane_height = (size.height() + factor - 1) / factor;
      const size_t plane_bytes_per_element = BytesPerElement(format, plane);
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
    const size_t bytes_per_element = BytesPerElement(format, 0);
    const size_t bytes_per_row = IOSurfaceAlignProperty(
        kIOSurfaceBytesPerRow,
        base::bits::AlignUpDeprecatedDoNotUse(size.width(), 2) *
            bytes_per_element);
    const size_t bytes_alloc = IOSurfaceAlignProperty(
        kIOSurfaceAllocSize,
        base::bits::AlignUpDeprecatedDoNotUse(size.height(), 2) *
            bytes_per_row);
    AddIntegerValue(properties.get(), kIOSurfaceBytesPerElement,
                    bytes_per_element);
    AddIntegerValue(properties.get(), kIOSurfaceBytesPerRow, bytes_per_row);
    AddIntegerValue(properties.get(), kIOSurfaceAllocSize, bytes_alloc);
  }

  base::apple::ScopedCFTypeRef<IOSurfaceRef> surface(
      IOSurfaceCreate(properties.get()));
  if (!surface) {
    LOG(ERROR) << "Failed to allocate IOSurface of size " << size.ToString()
               << ".";
    return base::apple::ScopedCFTypeRef<IOSurfaceRef>();
  }

  if (should_clear) {
    // Zero-initialize the IOSurface. Calling IOSurfaceLock/IOSurfaceUnlock
    // appears to be sufficient. https://crbug.com/584760#c17
    IOReturn r = IOSurfaceLock(surface.get(), 0, nullptr);
    DCHECK_EQ(kIOReturnSuccess, r);
    r = IOSurfaceUnlock(surface.get(), 0, nullptr);
    DCHECK_EQ(kIOReturnSuccess, r);
  }

  // Ensure that all IOSurfaces start as sRGB.
  IOSurfaceSetValue(surface.get(), CFSTR("IOSurfaceColorSpace"),
                    kCGColorSpaceSRGB);

  return surface;
}

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

GFX_EXPORT base::apple::ScopedCFTypeRef<IOSurfaceRef>
IOSurfaceMachPortToIOSurface(
    ScopedRefCountedIOSurfaceMachPort io_surface_mach_port) {
  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface;
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
