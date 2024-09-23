// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/test/mock_gbm_device.h"

#include <xf86drm.h>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"

namespace ui {
namespace {

base::ScopedFD MakeFD() {
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path))
    return {};
  auto file =
      base::File(temp_path, base::File::FLAG_READ | base::File::FLAG_WRITE |
                                base::File::FLAG_CREATE_ALWAYS);
  return base::ScopedFD(file.TakePlatformFile());
}

class MockGbmBuffer final : public ui::GbmBuffer {
 public:
  MockGbmBuffer(uint32_t format,
                uint32_t flags,
                uint64_t modifier,
                const gfx::Size& size,
                std::vector<gfx::NativePixmapPlane> planes,
                std::vector<uint32_t> handles)
      : format_(format),
        format_modifier_(modifier),
        flags_(flags),
        size_(size),
        planes_(std::move(planes)),
        handles_(std::move(handles)) {}

  MockGbmBuffer(const MockGbmBuffer&) = delete;
  MockGbmBuffer& operator=(const MockGbmBuffer&) = delete;

  ~MockGbmBuffer() override = default;

  uint32_t GetFormat() const override { return format_; }
  uint64_t GetFormatModifier() const override { return format_modifier_; }
  uint32_t GetFlags() const override { return flags_; }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetBufferFormat() const override {
    return ui::GetBufferFormatFromFourCCFormat(format_);
  }
  bool AreFdsValid() const override {
    if (planes_.empty())
      return false;

    for (const auto& plane : planes_) {
      if (!plane.fd.is_valid())
        return false;
    }
    return true;
  }
  size_t GetNumPlanes() const override { return planes_.size(); }
  int GetPlaneFd(size_t plane) const override {
    return planes_[plane].fd.get();
  }

  bool SupportsZeroCopyWebGPUImport() const override {
    NOTIMPLEMENTED();
    return false;
  }

  uint32_t GetPlaneStride(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return planes_[plane].stride;
  }
  size_t GetPlaneOffset(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return planes_[plane].offset;
  }
  size_t GetPlaneSize(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return static_cast<size_t>(planes_[plane].size);
  }
  uint32_t GetPlaneHandle(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return handles_[plane];
  }
  uint32_t GetHandle() const override { return GetPlaneHandle(0); }
  gfx::NativePixmapHandle ExportHandle() const override {
    NOTIMPLEMENTED();
    return gfx::NativePixmapHandle();
  }

  sk_sp<SkSurface> GetSurface() override { return nullptr; }

 private:
  uint32_t format_ = 0;
  uint64_t format_modifier_ = 0;
  uint32_t flags_ = 0;
  gfx::Size size_;
  std::vector<gfx::NativePixmapPlane> planes_;
  std::vector<uint32_t> handles_;
};

}  // namespace

MockGbmDevice::MockGbmDevice() = default;

MockGbmDevice::~MockGbmDevice() = default;

void MockGbmDevice::set_allocation_failure(bool should_fail_allocations) {
  should_fail_allocations_ = should_fail_allocations;
}

std::vector<uint64_t> MockGbmDevice::GetSupportedModifiers() const {
  return supported_modifiers_;
}

std::unique_ptr<GbmBuffer> MockGbmDevice::CreateBuffer(uint32_t format,
                                                       const gfx::Size& size,
                                                       uint32_t flags) {
  if (should_fail_allocations_)
    return nullptr;

  return CreateBufferWithModifiers(format, size, flags, {});
}

std::unique_ptr<GbmBuffer> MockGbmDevice::CreateBufferWithModifiers(
    uint32_t format,
    const gfx::Size& size,
    uint32_t flags,
    const std::vector<uint64_t>& modifiers) {
  if (should_fail_allocations_)
    return nullptr;

  uint32_t bytes_per_pixel;
  switch (format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
      bytes_per_pixel = 4;
      break;
    case DRM_FORMAT_NV12:
      bytes_per_pixel = 2;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported format: " << format;
      return nullptr;
  }

  uint64_t format_modifier =
      modifiers.empty() ? DRM_FORMAT_MOD_NONE : modifiers.back();

  if (!base::Contains(supported_modifiers_, format_modifier)) {
    PLOG(ERROR) << "Unsupported format modifier: " << std::hex
                << format_modifier;
    return nullptr;
  }

  uint32_t width = base::checked_cast<uint32_t>(size.width());
  uint32_t height = base::checked_cast<uint32_t>(size.height());
  uint32_t plane_stride = base::CheckMul(bytes_per_pixel, width).ValueOrDie();
  uint32_t plane_size = base::CheckMul(plane_stride, height).ValueOrDie();
  uint32_t plane_offset = 0;

  std::vector<gfx::NativePixmapPlane> planes;
  planes.emplace_back(plane_stride, plane_offset, plane_size, MakeFD());
  std::vector<uint32_t> handles;
  handles.push_back(next_handle_++);

  return std::make_unique<MockGbmBuffer>(format, flags, format_modifier, size,
                                         std::move(planes), std::move(handles));
}

std::unique_ptr<GbmBuffer> MockGbmDevice::CreateBufferFromHandle(
    uint32_t format,
    const gfx::Size& size,
    gfx::NativePixmapHandle handle) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool MockGbmDevice::CanCreateBufferForFormat(uint32_t format) {
  return true;
}

}  // namespace ui
