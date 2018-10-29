// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"

#include <utility>

#include "base/macros.h"
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
  ClientNativePixmapOpaque() {}
  ~ClientNativePixmapOpaque() override {}

  bool Map() override {
    NOTREACHED();
    return false;
  }
  void Unmap() override { NOTREACHED(); }
  void* GetMemoryAddress(size_t plane) const override {
    NOTREACHED();
    return nullptr;
  }
  int GetStride(size_t plane) const override {
    NOTREACHED();
    return 0;
  }
};

}  // namespace

class ClientNativePixmapFactoryDmabuf : public ClientNativePixmapFactory {
 public:
  explicit ClientNativePixmapFactoryDmabuf(
      bool supports_native_pixmap_import_from_dmabuf)
      : supports_native_pixmap_import_from_dmabuf_(
            supports_native_pixmap_import_from_dmabuf) {}
  ~ClientNativePixmapFactoryDmabuf() override {}

  // ClientNativePixmapFactory:
  bool IsConfigurationSupported(gfx::BufferFormat format,
                                gfx::BufferUsage usage) const override {
    switch (usage) {
      case gfx::BufferUsage::GPU_READ:
        return format == gfx::BufferFormat::BGR_565 ||
               format == gfx::BufferFormat::RGBA_8888 ||
               format == gfx::BufferFormat::RGBX_8888 ||
               format == gfx::BufferFormat::BGRA_8888 ||
               format == gfx::BufferFormat::BGRX_8888 ||
               format == gfx::BufferFormat::YVU_420;
      case gfx::BufferUsage::SCANOUT:
        return format == gfx::BufferFormat::BGRX_8888 ||
               format == gfx::BufferFormat::RGBX_8888 ||
               format == gfx::BufferFormat::RGBA_8888 ||
               format == gfx::BufferFormat::BGRA_8888;
      case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
        return
#if defined(ARCH_CPU_X86_FAMILY)
            // Currently only Intel driver (i.e. minigbm and Mesa) supports R_8
            // RG_88, NV12 and XB30. https://crbug.com/356871
            format == gfx::BufferFormat::R_8 ||
            format == gfx::BufferFormat::RG_88 ||
            format == gfx::BufferFormat::YUV_420_BIPLANAR ||
            format == gfx::BufferFormat::RGBX_1010102 ||
#endif

            format == gfx::BufferFormat::BGRX_8888 ||
            format == gfx::BufferFormat::BGRA_8888 ||
            format == gfx::BufferFormat::RGBX_8888 ||
            format == gfx::BufferFormat::RGBA_8888;
      case gfx::BufferUsage::SCANOUT_VDA_WRITE:
        return false;
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT: {
        if (!supports_native_pixmap_import_from_dmabuf_)
          return false;
        return
#if defined(ARCH_CPU_X86_FAMILY)
            // Currently only Intel driver (i.e. minigbm and
            // Mesa) supports R_8 RG_88 and NV12.
            // https://crbug.com/356871
            format == gfx::BufferFormat::R_8 ||
            format == gfx::BufferFormat::RG_88 ||
            format == gfx::BufferFormat::YUV_420_BIPLANAR ||
#endif
            format == gfx::BufferFormat::BGRA_8888;
      }
      case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE: {
        if (!supports_native_pixmap_import_from_dmabuf_)
          return false;
        // Each platform only supports one camera buffer type. We list the
        // supported buffer formats on all platforms here. When allocating a
        // camera buffer the caller is responsible for making sure a buffer is
        // successfully allocated. For example, allocating YUV420_BIPLANAR
        // for SCANOUT_CAMERA_READ_WRITE may only work on Intel boards.
        return format == gfx::BufferFormat::YUV_420_BIPLANAR;
      }
      case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE: {
        if (!supports_native_pixmap_import_from_dmabuf_)
          return false;
        // R_8 is used as the underlying pixel format for BLOB buffers.
        return format == gfx::BufferFormat::R_8;
      }
    }
    NOTREACHED();
    return false;
  }
  std::unique_ptr<ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferUsage usage) override {
    DCHECK(!handle.fds.empty());
    switch (usage) {
      case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
        if (supports_native_pixmap_import_from_dmabuf_)
          return ClientNativePixmapDmaBuf::ImportFromDmabuf(handle, size);
        NOTREACHED()
            << "Native GpuMemoryBuffers are not supported on this platform";
        return nullptr;
      case gfx::BufferUsage::GPU_READ:
      case gfx::BufferUsage::SCANOUT:
      case gfx::BufferUsage::SCANOUT_VDA_WRITE:
        // Close all the fds.
        for (const auto& fd : handle.fds)
          base::ScopedFD scoped_fd(fd.fd);
        return base::WrapUnique(new ClientNativePixmapOpaque);
    }
    NOTREACHED();
    return nullptr;
  }

 private:
  // Says if ClientNativePixmapDmaBuf can be used to import handle from dmabuf.
  const bool supports_native_pixmap_import_from_dmabuf_ = false;

  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFactoryDmabuf);
};

ClientNativePixmapFactory* CreateClientNativePixmapFactoryDmabuf(
    bool supports_native_pixmap_import_from_dmabuf) {
// |supports_native_pixmap_import_from_dmabuf| can be enabled on all linux but
// it is not a requirement to support glCreateImageChromium+Dmabuf since it uses
// gfx::BufferUsage::SCANOUT and the pixmap does not need to be mappable on the
// client side.
//
// At the moment, only Ozone/Wayland platform running on Linux is able to import
// handle from dmabuf in addition to the ChromeOS. This is set in the ozone
// level in the ClientNativePixmapFactoryWayland class.
//
// This is not ideal. The ozone platform should probably set this.
// TODO(rjkroege): do something better here.
#if defined(OS_CHROMEOS)
  supports_native_pixmap_import_from_dmabuf = true;
#endif
  return new ClientNativePixmapFactoryDmabuf(
      supports_native_pixmap_import_from_dmabuf);
}

}  // namespace gfx
