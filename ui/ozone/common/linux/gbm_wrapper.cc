// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/linux/gbm_wrapper.h"

#include <gbm.h>

#include "base/posix/eintr_wrapper.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/common/linux/gbm_buffer.h"
#include "ui/ozone/common/linux/gbm_device.h"

namespace gbm_wrapper {

namespace {

// Temporary defines while we migrate to GBM_BO_IMPORT_FD_MODIFIER.
#define GBM_BO_IMPORT_FD_PLANAR_5504 0x5504
#define GBM_BO_IMPORT_FD_PLANAR_5505 0x5505

}  // namespace

class Buffer final : public ui::GbmBuffer {
 public:
  Buffer(struct gbm_bo* bo,
         uint32_t format,
         uint32_t flags,
         uint64_t modifier,
         std::vector<base::ScopedFD> fds,
         const gfx::Size& size,
         std::vector<gfx::NativePixmapPlane> planes)
      : bo_(bo),
        format_(format),
        format_modifier_(modifier),
        flags_(flags),
        fds_(std::move(fds)),
        size_(size),
        planes_(std::move(planes)) {}

  ~Buffer() override {
    DCHECK(!mmap_data_);
    gbm_bo_destroy(bo_);
  }

  uint32_t GetFormat() const override { return format_; }
  uint64_t GetFormatModifier() const override { return format_modifier_; }
  uint32_t GetFlags() const override { return flags_; }
  size_t GetFdCount() const override { return fds_.size(); }
  // TODO(reveman): This should not be needed once crbug.com/597932 is fixed,
  // as the size would be queried directly from the underlying bo.
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetBufferFormat() const override {
    return ui::GetBufferFormatFromFourCCFormat(format_);
  }
  bool AreFdsValid() const override {
    if (fds_.empty())
      return false;

    for (const auto& fd : fds_) {
      if (fd.get() == -1)
        return false;
    }
    return true;
  }
  size_t GetNumPlanes() const override { return planes_.size(); }
  int GetPlaneFd(size_t plane) const override {
    DCHECK_LT(plane, fds_.size());
    return fds_[plane].get();
  }
  int GetPlaneStride(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return planes_[plane].stride;
  }
  int GetPlaneOffset(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return planes_[plane].offset;
  }
  size_t GetPlaneSize(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return planes_[plane].size;
  }
  uint32_t GetPlaneHandle(size_t plane) const override {
    DCHECK_LT(plane, planes_.size());
    return gbm_bo_get_plane_handle(bo_, plane).u32;
  }
  uint32_t GetHandle() const override { return gbm_bo_get_handle(bo_).u32; }
  gfx::NativePixmapHandle ExportHandle() const override {
    gfx::NativePixmapHandle handle;
    gfx::BufferFormat format = ui::GetBufferFormatFromFourCCFormat(format_);
    // TODO(dcastagna): Use gbm_bo_get_num_planes once all the formats we use
    // are supported by gbm.
    for (size_t i = 0; i < gfx::NumberOfPlanesForBufferFormat(format); ++i) {
      // Some formats (e.g: YVU_420) might have less than one fd per plane.
      if (i < fds_.size()) {
        base::ScopedFD scoped_fd(HANDLE_EINTR(dup(GetPlaneFd(i))));
        if (!scoped_fd.is_valid()) {
          PLOG(ERROR) << "dup";
          return gfx::NativePixmapHandle();
        }
        handle.fds.emplace_back(
            base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
      }
      handle.planes.emplace_back(GetPlaneStride(i), GetPlaneOffset(i),
                                 GetPlaneSize(i), GetFormatModifier());
    }
    return handle;
  }

  sk_sp<SkSurface> GetSurface() override {
    DCHECK(!mmap_data_);
    uint32_t stride;
    void* addr;
    addr = gbm_bo_map(bo_, 0, 0, gbm_bo_get_width(bo_), gbm_bo_get_height(bo_),
                      GBM_BO_TRANSFER_READ_WRITE, &stride, &mmap_data_, 0);

    if (!addr)
      return nullptr;
    SkImageInfo info =
        SkImageInfo::MakeN32Premul(size_.width(), size_.height());
    return SkSurface::MakeRasterDirectReleaseProc(info, addr, stride,
                                                  &Buffer::UnmapGbmBo, this);
  }

 private:
  gbm_bo* bo_ = nullptr;
  void* mmap_data_ = nullptr;

  uint32_t format_ = 0;
  uint64_t format_modifier_ = 0;
  uint32_t flags_ = 0;

  std::vector<base::ScopedFD> fds_;

  gfx::Size size_;

  std::vector<gfx::NativePixmapPlane> planes_;

  static void UnmapGbmBo(void* pixels, void* context) {
    Buffer* buffer = static_cast<Buffer*>(context);
    gbm_bo_unmap(buffer->bo_, buffer->mmap_data_);
    buffer->mmap_data_ = nullptr;
  }

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

std::unique_ptr<Buffer> CreateBufferForBO(struct gbm_bo* bo,
                                          uint32_t format,
                                          const gfx::Size& size,
                                          uint32_t flags) {
  DCHECK(bo);
  std::vector<base::ScopedFD> fds;
  std::vector<gfx::NativePixmapPlane> planes;

  const uint64_t modifier = gbm_bo_get_format_modifier(bo);
  for (size_t i = 0; i < gbm_bo_get_num_planes(bo); ++i) {
    // The fd returned by gbm_bo_get_fd is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    base::ScopedFD fd(gbm_bo_get_plane_fd(bo, i));

    // TODO(dcastagna): support multiple fds.
    // crbug.com/642410
    if (!i) {
      if (!fd.is_valid()) {
        PLOG(ERROR) << "Failed to export buffer to dma_buf";
        gbm_bo_destroy(bo);
        return nullptr;
      }
      fds.emplace_back(std::move(fd));
    }

    planes.emplace_back(gbm_bo_get_plane_stride(bo, i),
                        gbm_bo_get_plane_offset(bo, i),
                        gbm_bo_get_plane_size(bo, i), modifier);
  }
  return std::make_unique<Buffer>(bo, format, flags, modifier, std::move(fds),
                                  size, std::move(planes));
}

class Device final : public ui::GbmDevice {
 public:
  Device(gbm_device* device) : device_(device) {}
  ~Device() override { gbm_device_destroy(device_); }

  std::unique_ptr<ui::GbmBuffer> CreateBuffer(uint32_t format,
                                              const gfx::Size& size,
                                              uint32_t flags) override {
    struct gbm_bo* bo =
        gbm_bo_create(device_, size.width(), size.height(), format, flags);
    if (!bo)
      return nullptr;

    return CreateBufferForBO(bo, format, size, flags);
  }

  std::unique_ptr<ui::GbmBuffer> CreateBufferWithModifiers(
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags,
      const std::vector<uint64_t>& modifiers) override {
    if (modifiers.empty())
      return CreateBuffer(format, size, flags);
    struct gbm_bo* bo = gbm_bo_create_with_modifiers(
        device_, size.width(), size.height(), format, modifiers.data(),
        modifiers.size());
    if (!bo)
      return nullptr;

    return CreateBufferForBO(bo, format, size, flags);
  }

  std::unique_ptr<ui::GbmBuffer> CreateBufferFromFds(
      uint32_t format,
      const gfx::Size& size,
      std::vector<base::ScopedFD> fds,
      const std::vector<gfx::NativePixmapPlane>& planes) override {
    DCHECK_LE(fds.size(), planes.size());
    DCHECK_EQ(planes[0].offset, 0);

    // Try to use scanout if supported.
    int gbm_flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
    if (!gbm_device_is_format_supported(device_, format, gbm_flags))
      gbm_flags &= ~GBM_BO_USE_SCANOUT;

    struct gbm_bo* bo = nullptr;
    if (!gbm_device_is_format_supported(device_, format, gbm_flags)) {
      LOG(ERROR) << "gbm format not supported: " << format;
      return nullptr;
    }

    struct gbm_import_fd_planar_data fd_data;
    fd_data.width = size.width();
    fd_data.height = size.height();
    fd_data.format = format;

    DCHECK_LE(planes.size(), 3u);
    for (size_t i = 0; i < planes.size(); ++i) {
      fd_data.fds[i] = fds[i < fds.size() ? i : 0].get();
      fd_data.strides[i] = planes[i].stride;
      fd_data.offsets[i] = planes[i].offset;
      fd_data.format_modifiers[i] = planes[i].modifier;
    }

    // The fd passed to gbm_bo_import is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    //
    // See the comment regarding the GBM_BO_IMPORT_FD_PLANAR_550X above.
    bo = gbm_bo_import(device_, GBM_BO_IMPORT_FD_PLANAR_5505, &fd_data,
                       gbm_flags);
    if (!bo) {
      bo = gbm_bo_import(device_, GBM_BO_IMPORT_FD_PLANAR_5504, &fd_data,
                         gbm_flags);
      if (!bo) {
        LOG(ERROR) << "nullptr returned from gbm_bo_import";
        return nullptr;
      }
    }

    return std::make_unique<Buffer>(bo, format, gbm_flags, planes[0].modifier,
                                    std::move(fds), size, std::move(planes));
  }

 private:
  gbm_device* device_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace gbm_wrapper

namespace ui {

std::unique_ptr<GbmDevice> CreateGbmDevice(int fd) {
  gbm_device* device = gbm_create_device(fd);
  if (!device)
    return nullptr;
  return std::make_unique<gbm_wrapper::Device>(device);
}

}  // namespace ui
