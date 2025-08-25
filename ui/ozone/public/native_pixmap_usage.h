// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_NATIVE_PIXMAP_USAGE_H_
#define UI_OZONE_PUBLIC_NATIVE_PIXMAP_USAGE_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/enum_set.h"

namespace ui {

// Corresponds to how ui/ozone platforms "use" a native pixmap buffer.
enum class NativePixmapUsage : uint32_t {
  // Buffer will be used as scanout buffer.
  kScanout,
  // Buffer will be used for rendering.
  kRendering,
  // Buffer will be used as a texture that will be sampled from.
  kTexturing,
  // Buffer is read by CPU and also linear i.e. not tiled.
  kCpuRead,
  // Buffer will be written to by a camera subsystem.
  kCamera,
  // Buffer will be protected i.e. inaccessible to unprivileged users.
  kProtected,
  // Buffer will be written by a video decode accelerator.
  kHWVideoDecoder,
  // Buffer will be read by a video encode accelerator.
  kHWVideoEncoder,
  // Buffer will be read often by the CPU software.
  kSWReadOften,
  // Buffer will be used for front rendering.
  kFrontRendering,

  kLastNativePixmapUsage = kFrontRendering
};

using NativePixmapUsageSet =
    base::EnumSet<NativePixmapUsage,
                  NativePixmapUsage::kScanout,
                  NativePixmapUsage::kLastNativePixmapUsage>;

// Define NativePixmapUsageSet constants corresponding to gfx::BufferUsage.
// This will help make sure the usages match as we transition away from
// gfx::BufferUsage.
// TODO(crbug.com/404958317): Remove NativePixmapBufferUsage once migration to
// SharedImageUsage is completed.
class NativePixmapBufferUsage {
 public:
  static constexpr NativePixmapUsageSet kGpuRead = {
      NativePixmapUsage::kTexturing};
  static constexpr NativePixmapUsageSet kScanout = {
      NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
      NativePixmapUsage::kRendering};
  static constexpr NativePixmapUsageSet kScanoutCameraCpuReadWrite = {
      NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
      NativePixmapUsage::kCpuRead, NativePixmapUsage::kCamera};
  static constexpr NativePixmapUsageSet kCameraCpuReadWrite = {
      NativePixmapUsage::kCpuRead, NativePixmapUsage::kCamera};
  static constexpr NativePixmapUsageSet kScanoutCpuReadWrite = {
      NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
      NativePixmapUsage::kCpuRead};
  static constexpr NativePixmapUsageSet kScanoutVDAWrite = {
      NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
      NativePixmapUsage::kHWVideoDecoder};
  static constexpr NativePixmapUsageSet kProtectedScanout = {
      NativePixmapUsage::kScanout, NativePixmapUsage::kProtected};
  static constexpr NativePixmapUsageSet kProtectedScanoutVDAWrite = {
      NativePixmapUsage::kScanout, NativePixmapUsage::kProtected,
      NativePixmapUsage::kHWVideoDecoder};
  static constexpr NativePixmapUsageSet kGpuReadCpuReadWrite = {
      NativePixmapUsage::kTexturing, NativePixmapUsage::kCpuRead};
  static constexpr NativePixmapUsageSet kScanoutVEACpuRead = {
      NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
      NativePixmapUsage::kCpuRead, NativePixmapUsage::kHWVideoEncoder};
  static constexpr NativePixmapUsageSet kVEAReadCameraCpuReadWrite = {
      NativePixmapUsage::kTexturing, NativePixmapUsage::kCpuRead,
      NativePixmapUsage::kCamera, NativePixmapUsage::kHWVideoEncoder,
      NativePixmapUsage::kSWReadOften};
  static constexpr NativePixmapUsageSet kScanoutFrontRendering = {
      NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
      NativePixmapUsage::kFrontRendering};
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_NATIVE_PIXMAP_USAGE_H_
