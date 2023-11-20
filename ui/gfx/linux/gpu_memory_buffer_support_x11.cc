// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/gpu_memory_buffer_support_x11.h"

#include <fcntl.h>
#include <xcb/xcb.h>

#include <memory>

#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/gbm_util.h"
#include "ui/gfx/linux/gbm_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/future.h"

namespace ui {

namespace {

// Obtain an authenticated DRM fd from X11 and create a GbmDevice with it.
std::unique_ptr<ui::GbmDevice> CreateX11GbmDevice() {
  if (getenv("RUNNING_UNDER_RR") != nullptr) {
    LOG(WARNING) << "Running under rr, disabling dri3";
    return nullptr;
  }

  auto* connection = x11::Connection::Get();
  // |connection| may be nullptr in headless mode.
  if (!connection) {
    LOG(WARNING) << "Could not create x11 connection.";
    return nullptr;
  }

  auto& dri3 = connection->dri3();
  if (!dri3.present()) {
    LOG(WARNING) << "dri3 extension not supported.";
    return nullptr;
  }

  // Obtain an authenticated DRM fd.
  auto reply = dri3.Open({connection->default_root(), 0}).Sync();
  if (!reply)
    return nullptr;

  base::ScopedFD fd(HANDLE_EINTR(dup(reply->device_fd.get())));
  if (!fd.is_valid())
    return nullptr;
  if (HANDLE_EINTR(fcntl(fd.get(), F_SETFD, FD_CLOEXEC)) == -1)
    return nullptr;

  return ui::CreateGbmDevice(fd.release());
}

std::vector<gfx::BufferUsageAndFormat> CreateSupportedConfigList(
    ui::GbmDevice* device) {
  if (!device)
    return {};

  std::vector<gfx::BufferUsageAndFormat> configs;
  for (gfx::BufferUsage usage : {
           gfx::BufferUsage::GPU_READ,
           gfx::BufferUsage::SCANOUT,
           gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
           gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
           gfx::BufferUsage::SCANOUT_VDA_WRITE,
       }) {
    for (gfx::BufferFormat format : {
             gfx::BufferFormat::R_8,
             gfx::BufferFormat::RG_88,
             gfx::BufferFormat::RGBA_8888,
             gfx::BufferFormat::RGBX_8888,
             gfx::BufferFormat::BGRA_8888,
             gfx::BufferFormat::BGRX_8888,
             gfx::BufferFormat::BGRA_1010102,

             // On some Intel setups calling gbm_bo_create() with this format
             // results in a crash caused by an integer-divide-by-zero.
             // TODO(thomasanderson): Enable this format.
             // gfx::BufferFormat::RGBA_1010102,
             gfx::BufferFormat::BGR_565,
             gfx::BufferFormat::YUV_420_BIPLANAR,
             gfx::BufferFormat::YVU_420,
             gfx::BufferFormat::P010,
         }) {
      // At least on mesa/amdgpu, gbm_device_is_format_supported() lies.  Test
      // format support by creating a buffer directly.  Use a 2x2 buffer so that
      // YUV420 formats get properly tested.
      if (device->CreateBuffer(GetFourCCFormatFromBufferFormat(format),
                               gfx::Size(2, 2), BufferUsageToGbmFlags(usage))) {
        configs.push_back(gfx::BufferUsageAndFormat(usage, format));
      }
    }
  }
  return configs;
}

}  // namespace

// static
GpuMemoryBufferSupportX11* GpuMemoryBufferSupportX11::GetInstance() {
  static base::NoDestructor<GpuMemoryBufferSupportX11> instance;
  return instance.get();
}

GpuMemoryBufferSupportX11::GpuMemoryBufferSupportX11()
    : device_(CreateX11GbmDevice()),
      supported_configs_(CreateSupportedConfigList(device_.get())) {}

GpuMemoryBufferSupportX11::~GpuMemoryBufferSupportX11() = default;

std::unique_ptr<GbmBuffer> GpuMemoryBufferSupportX11::CreateBuffer(
    gfx::BufferFormat format,
    const gfx::Size& size,
    gfx::BufferUsage usage) {
  if (!device_) {
    LOG(ERROR) << "Can't create buffer -- gbm  device is missing.";
    return nullptr;
  }
  if (!base::Contains(supported_configs_,
                      gfx::BufferUsageAndFormat(usage, format))) {
    LOG(ERROR) << "Can't create buffer -- unsupported config: usage="
               << gfx::BufferUsageToString(usage)
               << ", format=" << gfx::BufferFormatToString(format);
    return nullptr;
  }

  static base::debug::CrashKeyString* crash_key_string =
      base::debug::AllocateCrashKeyString("buffer_usage_and_format",
                                          base::debug::CrashKeySize::Size64);
  std::string buffer_usage_and_format = gfx::BufferFormatToString(format) +
                                        std::string(",") +
                                        gfx::BufferUsageToString(usage);
  base::debug::ScopedCrashKeyString scoped_crash_key(
      crash_key_string, buffer_usage_and_format.c_str());

  return device_->CreateBuffer(GetFourCCFormatFromBufferFormat(format), size,
                               BufferUsageToGbmFlags(usage));
}

bool GpuMemoryBufferSupportX11::CanCreateNativePixmapForFormat(
    gfx::BufferFormat format) {
  return device_ && device_->CanCreateBufferForFormat(
                        GetFourCCFormatFromBufferFormat(format));
}

std::unique_ptr<GbmBuffer> GpuMemoryBufferSupportX11::CreateBufferFromHandle(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  if (!device_) {
    LOG(ERROR) << "Can't create buffer from handle -- gbm  device is missing.";
    return nullptr;
  }

  static base::debug::CrashKeyString* crash_key_string =
      base::debug::AllocateCrashKeyString("buffer_from_handle_format",
                                          base::debug::CrashKeySize::Size64);
  std::string buffer_from_handle_format = gfx::BufferFormatToString(format);
  base::debug::ScopedCrashKeyString scoped_crash_key(
      crash_key_string, buffer_from_handle_format.c_str());

  return device_->CreateBufferFromHandle(
      GetFourCCFormatFromBufferFormat(format), size, std::move(handle));
}

}  // namespace ui
