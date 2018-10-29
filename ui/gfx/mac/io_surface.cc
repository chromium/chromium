// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/io_surface.h"

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/icc_profile.h"

namespace gfx {

namespace {

void AddIntegerValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     int32_t value) {
  base::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(NULL, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}

int32_t BytesPerElement(gfx::BufferFormat format, int plane) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      DCHECK_EQ(plane, 0);
      return 1;
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_1010102:
      DCHECK_EQ(plane, 0);
      return 4;
    case gfx::BufferFormat::RGBA_F16:
      DCHECK_EQ(plane, 0);
      return 8;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      static int32_t bytes_per_element[] = {1, 2};
      DCHECK_LT(static_cast<size_t>(plane), arraysize(bytes_per_element));
      return bytes_per_element[plane];
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::UYVY_422:
      DCHECK_EQ(plane, 0);
      return 2;
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::YVU_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

int32_t PixelFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return 'L008';
    case gfx::BufferFormat::BGRX_1010102:
      return 'l10r';  // little-endian ARGB2101010 full-range ARGB
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
      return 'BGRA';
    case gfx::BufferFormat::RGBA_F16:
      return 'RGhA';
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return '420v';
    case gfx::BufferFormat::UYVY_422:
      return '2vuy';
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBX_1010102:
    // Technically RGBX_1010102 should be accepted as 'R10k', but then it won't
    // be supported by CGLTexImageIOSurface2D(), so it's best to reject it here.
    case gfx::BufferFormat::YVU_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
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
  kern_return_t kr =
      mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, -1);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "IOSurfaceMachPortTraits::Release mach_port_mod_refs";
}

}  // namespace internal

IOSurfaceRef CreateIOSurface(const gfx::Size& size,
                             gfx::BufferFormat format,
                             bool should_clear) {
  TRACE_EVENT0("ui", "CreateIOSurface");
  base::TimeTicks start_time = base::TimeTicks::Now();

  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);
  base::ScopedCFTypeRef<CFMutableArrayRef> planes(CFArrayCreateMutable(
      kCFAllocatorDefault, num_planes, &kCFTypeArrayCallBacks));

  // Don't specify plane information unless there are indeed multiple planes
  // because DisplayLink drivers do not support this.
  // http://crbug.com/527556
  if (num_planes > 1) {
    for (size_t plane = 0; plane < num_planes; ++plane) {
      size_t factor = gfx::SubsamplingFactorForBufferFormat(format, plane);

      base::ScopedCFTypeRef<CFMutableDictionaryRef> plane_info(
          CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                    &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks));
      AddIntegerValue(plane_info, kIOSurfacePlaneWidth, size.width() / factor);
      AddIntegerValue(plane_info, kIOSurfacePlaneHeight,
                      size.height() / factor);
      AddIntegerValue(plane_info, kIOSurfacePlaneBytesPerElement,
                      BytesPerElement(format, plane));

      CFArrayAppendValue(planes, plane_info);
    }
  }

  base::ScopedCFTypeRef<CFMutableDictionaryRef> properties(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties, kIOSurfaceWidth, size.width());
  AddIntegerValue(properties, kIOSurfaceHeight, size.height());
  AddIntegerValue(properties, kIOSurfacePixelFormat, PixelFormat(format));
  if (num_planes > 1) {
    CFDictionaryAddValue(properties, kIOSurfacePlaneInfo, planes);
  } else {
    AddIntegerValue(properties, kIOSurfaceBytesPerElement,
                    BytesPerElement(format, 0));
  }

  IOSurfaceRef surface = IOSurfaceCreate(properties);
  if (!surface) {
    LOG(ERROR) << "Failed to allocate IOSurface of size " << size.ToString()
               << ".";
    return nullptr;
  }

  // IOSurface clearing causes significant performance regression on about half
  // of all devices running Yosemite. https://crbug.com/606850#c22.
  if (base::mac::IsOS10_10())
    should_clear = false;

  if (should_clear) {
    // Zero-initialize the IOSurface. Calling IOSurfaceLock/IOSurfaceUnlock
    // appears to be sufficient. https://crbug.com/584760#c17
    IOReturn r = IOSurfaceLock(surface, 0, nullptr);
    DCHECK_EQ(kIOReturnSuccess, r);
    r = IOSurfaceUnlock(surface, 0, nullptr);
    DCHECK_EQ(kIOReturnSuccess, r);
  }

  // Ensure that all IOSurfaces start as sRGB.
  CGColorSpaceRef color_space = base::mac::GetSRGBColorSpace();
  base::ScopedCFTypeRef<CFDataRef> color_space_icc(
      CGColorSpaceCopyICCProfile(color_space));
  IOSurfaceSetValue(surface, CFSTR("IOSurfaceColorSpace"), color_space_icc);

  UMA_HISTOGRAM_TIMES("GPU.IOSurface.CreateTime",
                      base::TimeTicks::Now() - start_time);
  return surface;
}

void IOSurfaceSetColorSpace(IOSurfaceRef io_surface,
                            const ColorSpace& color_space) {
  // Special-case sRGB.
  if (color_space == ColorSpace::CreateSRGB()) {
    base::ScopedCFTypeRef<CFDataRef> srgb_icc(
        CGColorSpaceCopyICCProfile(base::mac::GetSRGBColorSpace()));
    IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorSpace"), srgb_icc);
    return;
  }

  // Special-case BT2020_NCL.
  if (__builtin_available(macos 10.12, *)) {
    const ColorSpace kBt2020(
        ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::SMPTEST2084,
        ColorSpace::MatrixID::BT2020_NCL, ColorSpace::RangeID::LIMITED);
    if (color_space == kBt2020) {
      base::ScopedCFTypeRef<CGColorSpaceRef> cg_color_space(
          CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020));
      DCHECK(cg_color_space);

      base::ScopedCFTypeRef<CFDataRef> cf_data_icc_profile(
          CGColorSpaceCopyICCData(cg_color_space));
      DCHECK(cf_data_icc_profile);
      IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorSpace"),
                        cf_data_icc_profile);
      return;
    }
  }

  // Generate an ICCProfile from the parametric color space.
  ICCProfile icc_profile =
      ICCProfile::FromParametricColorSpace(color_space.GetAsFullRangeRGB());
  if (!icc_profile.IsValid()) {
    DLOG(ERROR) << "Failed to set color space for IOSurface: no ICC profile: "
                << color_space.ToString();
    return;
  }

  // Package it as a CFDataRef and send it to the IOSurface.
  std::vector<char> icc_profile_data = icc_profile.GetData();
  base::ScopedCFTypeRef<CFDataRef> cf_data_icc_profile(CFDataCreate(
      nullptr, reinterpret_cast<const UInt8*>(icc_profile_data.data()),
      icc_profile_data.size()));
  IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorSpace"),
                    cf_data_icc_profile);
}

}  // namespace gfx
