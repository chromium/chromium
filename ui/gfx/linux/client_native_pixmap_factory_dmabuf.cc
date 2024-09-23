// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gfx/native_pixmap_handle.h"

// Although, it's compiled for all linux platforms, it does not mean dmabuf
// will work there. Check the comment below in the
// ClientNativePixmapFactoryDmabuf for more details.
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"

namespace gfx {

namespace {

class ClientNativePixmapOpaque : public ClientNativePixmap {
 public:
  explicit ClientNativePixmapOpaque(NativePixmapHandle pixmap_handle)
      : pixmap_handle_(std::move(pixmap_handle)) {}
  ~ClientNativePixmapOpaque() override = default;

  bool Map() override { return false; }
  void Unmap() override { NOTREACHED_IN_MIGRATION(); }
  size_t GetNumberOfPlanes() const override {
    return pixmap_handle_.planes.size();
  }
  void* GetMemoryAddress(size_t plane) const override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  int GetStride(size_t plane) const override {
    CHECK_LT(plane, pixmap_handle_.planes.size());
    // Even though a ClientNativePixmapOpaque should not be mapped, we may still
    // need to query the stride of each plane. See
    // VideoFrame::WrapExternalGpuMemoryBuffer() for such a use case.
    return base::checked_cast<int>(pixmap_handle_.planes[plane].stride);
  }
  NativePixmapHandle CloneHandleForIPC() const override {
    return gfx::CloneHandleForIPC(pixmap_handle_);
  }

 private:
  NativePixmapHandle pixmap_handle_;
};

}  // namespace

class ClientNativePixmapFactoryDmabuf : public ClientNativePixmapFactory {
 public:
  explicit ClientNativePixmapFactoryDmabuf() {}

  ClientNativePixmapFactoryDmabuf(const ClientNativePixmapFactoryDmabuf&) =
      delete;
  ClientNativePixmapFactoryDmabuf& operator=(
      const ClientNativePixmapFactoryDmabuf&) = delete;

  ~ClientNativePixmapFactoryDmabuf() override {}

  std::unique_ptr<ClientNativePixmap> ImportFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    DCHECK(!handle.planes.empty());
    switch (usage) {
      case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
      case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      case gfx::BufferUsage::SCANOUT_FRONT_RENDERING: {
        if (!CanFitImageForSizeAndFormat(
                handle, size, format, /*assume_single_memory_object=*/false)) {
          DLOG(ERROR) << "Failed to verify the size and format of the handle.";
          return nullptr;
        }

        for (size_t i = 0; i < handle.planes.size(); ++i) {
          if (!base::IsValueInRangeForNumericType<size_t>(
                  handle.planes[i].offset) ||
              !base::IsValueInRangeForNumericType<size_t>(
                  handle.planes[i].size)) {
            return nullptr;
          }

          if (!handle.planes[i].fd.is_valid())
            return nullptr;

          const int fd = handle.planes[i].fd.get();
          const off_t dma_buf_size = lseek(fd, /*offset=*/0, SEEK_END);
          if (dma_buf_size == static_cast<off_t>(-1)) {
            PLOG(ERROR) << "Failed to get the size of the dma-buf";
            return nullptr;
          }
          if (lseek(fd, /*offset=*/0, SEEK_SET) == static_cast<off_t>(-1)) {
            PLOG(ERROR) << "Failed to reset the file offset of the dma-buf";
            return nullptr;
          }
          if (!base::IsValueInRangeForNumericType<size_t>(dma_buf_size))
            return nullptr;

          base::CheckedNumeric<uint64_t> size_to_map =
              base::CheckAdd(handle.planes[i].size, handle.planes[i].offset);
          if (!size_to_map.IsValid<size_t>())
            return nullptr;
          if (size_to_map.ValueOrDie<size_t>() >
              base::checked_cast<size_t>(dma_buf_size)) {
            return nullptr;
          }
        }
        return ClientNativePixmapDmaBuf::ImportFromDmabuf(std::move(handle),
                                                          size, format);
      }
      case gfx::BufferUsage::GPU_READ:
      case gfx::BufferUsage::SCANOUT:
      case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      case gfx::BufferUsage::PROTECTED_SCANOUT:
      case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
        return base::WrapUnique(
            new ClientNativePixmapOpaque(std::move(handle)));
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
};

ClientNativePixmapFactory* CreateClientNativePixmapFactoryDmabuf() {
  return new ClientNativePixmapFactoryDmabuf();
}

}  // namespace gfx
