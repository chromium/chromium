// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"

#include <fcntl.h>
#include <linux/version.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/memory.h"
#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/switches.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/dma-buf.h>
#else
#include <linux/types.h>

struct dma_buf_sync {
  __u64 flags;
};

#define DMA_BUF_SYNC_READ (1 << 0)
#define DMA_BUF_SYNC_WRITE (2 << 0)
#define DMA_BUF_SYNC_RW (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START (0 << 2)
#define DMA_BUF_SYNC_END (1 << 2)

#define DMA_BUF_BASE 'b'
#define DMA_BUF_IOCTL_SYNC _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)
#endif

namespace gfx {

namespace {

void PrimeSyncStart(int dmabuf_fd) {
  struct dma_buf_sync sync_start = {0};

  sync_start.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
  int rv = HANDLE_EINTR(ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync_start));
  PLOG_IF(ERROR, rv) << "Failed DMA_BUF_SYNC_START";
}

void PrimeSyncEnd(int dmabuf_fd) {
  struct dma_buf_sync sync_end = {0};

  sync_end.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
  int rv = HANDLE_EINTR(ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync_end));
  PLOG_IF(ERROR, rv) << "Failed DMA_BUF_SYNC_END";
}

bool AllowCpuMappableBuffers() {
  static bool result = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNativeGpuMemoryBuffers);
  return result;
}

}  // namespace

ClientNativePixmapDmaBuf::PlaneInfo::PlaneInfo() {}

ClientNativePixmapDmaBuf::PlaneInfo::PlaneInfo(PlaneInfo&& info)
    : data(info.data), offset(info.offset), size(info.size) {
  // Set nullptr to info.data in order not to call munmap in |info| dtor.
  info.data = nullptr;
}

ClientNativePixmapDmaBuf::PlaneInfo::~PlaneInfo() {
  if (data) {
    int ret = munmap(data, size);
    DCHECK(!ret);
  }
}

// static
bool ClientNativePixmapDmaBuf::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
#if defined(CHROMECAST_BUILD)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      // TODO(spang): Fix b/121148905 and turn these back on.
      return false;
    default:
      break;
  }
#endif

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
      // TODO(crbug.com/954233): RG_88 is enabled only with
      // --enable-native-gpu-memory-buffers . Otherwise it breaks some telemetry
      // tests. Fix that issue and enable it again.
      if (format == gfx::BufferFormat::RG_88 && !AllowCpuMappableBuffers())
        return false;

      return
#if defined(ARCH_CPU_X86_FAMILY)
          // Currently only Intel driver (i.e. minigbm and Mesa) supports
          // R_8 RG_88, NV12 and XB30/XR30.
          format == gfx::BufferFormat::R_8 ||
          format == gfx::BufferFormat::RG_88 ||
          format == gfx::BufferFormat::YUV_420_BIPLANAR ||
          format == gfx::BufferFormat::RGBX_1010102 ||
          format == gfx::BufferFormat::BGRX_1010102 ||
#endif

          format == gfx::BufferFormat::BGRX_8888 ||
          format == gfx::BufferFormat::BGRA_8888 ||
          format == gfx::BufferFormat::RGBX_8888 ||
          format == gfx::BufferFormat::RGBA_8888;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      return false;

    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      if (!AllowCpuMappableBuffers())
        return false;
      return
#if defined(ARCH_CPU_X86_FAMILY)
          // Only the Intel stack (i.e. minigbm and Mesa) supports the formats
          // below.
          format == gfx::BufferFormat::R_8 ||
          format == gfx::BufferFormat::RG_88 ||
          format == gfx::BufferFormat::YUV_420_BIPLANAR ||
          format == gfx::BufferFormat::P010 ||
#endif
          format == gfx::BufferFormat::BGRA_8888;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      // Each platform only supports one camera buffer type. We list the
      // supported buffer formats on all platforms here. When allocating a
      // camera buffer the caller is responsible for making sure a buffer is
      // successfully allocated. For example, allocating YUV420_BIPLANAR
      // for SCANOUT_CAMERA_READ_WRITE may only work on Intel boards.
      return format == gfx::BufferFormat::YUV_420_BIPLANAR;
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      // R_8 is used as the underlying pixel format for BLOB buffers.
      return format == gfx::BufferFormat::R_8;
    case gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return format == gfx::BufferFormat::YVU_420 ||
             format == gfx::BufferFormat::YUV_420_BIPLANAR;
  }
  NOTREACHED();
  return false;
}

// static
std::unique_ptr<gfx::ClientNativePixmap>
ClientNativePixmapDmaBuf::ImportFromDmabuf(gfx::NativePixmapHandle handle,
                                           const gfx::Size& size,
                                           gfx::BufferFormat format) {
  std::array<PlaneInfo, kMaxPlanes> plane_info;

  size_t expected_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);
  if (expected_planes == 0 || handle.planes.size() != expected_planes) {
    return nullptr;
  }

  const size_t page_size = base::GetPageSize();
  for (size_t i = 0; i < handle.planes.size(); ++i) {
    // Verify that the plane buffer has appropriate size.
    size_t min_stride = 0;
    size_t subsample_factor = SubsamplingFactorForBufferFormat(format, i);
    base::CheckedNumeric<size_t> plane_height =
        (base::CheckedNumeric<size_t>(size.height()) + subsample_factor - 1) /
        subsample_factor;
    if (!gfx::RowSizeForBufferFormatChecked(size.width(), format, i,
                                            &min_stride) ||
        handle.planes[i].stride < min_stride) {
      return nullptr;
    }
    base::CheckedNumeric<size_t> min_size =
        base::CheckedNumeric<size_t>(handle.planes[i].stride) * plane_height;
    if (!min_size.IsValid() || handle.planes[i].size < min_size.ValueOrDie())
      return nullptr;

    // mmap() fails if the offset argument is not page-aligned.
    // Since handle.planes[i].offset is possibly not page-aligned, we
    // have to map with an additional offset to be aligned to the page.
    const size_t extra_offset = handle.planes[i].offset % page_size;
    size_t map_size =
        base::checked_cast<size_t>(handle.planes[i].size + extra_offset);
    plane_info[i].offset = extra_offset;
    plane_info[i].size = map_size;

    void* data =
        mmap(nullptr, map_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
             handle.planes[i].fd.get(), handle.planes[i].offset - extra_offset);
    if (data == MAP_FAILED) {
      logging::SystemErrorCode mmap_error = logging::GetLastSystemErrorCode();
      if (mmap_error == ENOMEM)
        base::TerminateBecauseOutOfMemory(map_size);
      LOG(ERROR) << "Failed to mmap dmabuf: "
                 << logging::SystemErrorCodeToString(mmap_error);
      return nullptr;
    }
    plane_info[i].data = data;
  }

  return base::WrapUnique(new ClientNativePixmapDmaBuf(std::move(handle), size,
                                                       std::move(plane_info)));
}

ClientNativePixmapDmaBuf::ClientNativePixmapDmaBuf(
    gfx::NativePixmapHandle handle,
    const gfx::Size& size,
    std::array<PlaneInfo, kMaxPlanes> plane_info)
    : pixmap_handle_(std::move(handle)),
      size_(size),
      plane_info_(std::move(plane_info)) {
  TRACE_EVENT0("drm", "ClientNativePixmapDmaBuf");
}

ClientNativePixmapDmaBuf::~ClientNativePixmapDmaBuf() {
  TRACE_EVENT0("drm", "~ClientNativePixmapDmaBuf");
}

bool ClientNativePixmapDmaBuf::Map() {
  TRACE_EVENT0("drm", "DmaBuf:Map");
  for (size_t i = 0; i < pixmap_handle_.planes.size(); ++i)
    PrimeSyncStart(pixmap_handle_.planes[i].fd.get());
  return true;
}

void ClientNativePixmapDmaBuf::Unmap() {
  TRACE_EVENT0("drm", "DmaBuf:Unmap");
  for (size_t i = 0; i < pixmap_handle_.planes.size(); ++i)
    PrimeSyncEnd(pixmap_handle_.planes[i].fd.get());
}

void* ClientNativePixmapDmaBuf::GetMemoryAddress(size_t plane) const {
  DCHECK_LT(plane, pixmap_handle_.planes.size());
  return static_cast<uint8_t*>(plane_info_[plane].data) +
         plane_info_[plane].offset;
}

int ClientNativePixmapDmaBuf::GetStride(size_t plane) const {
  DCHECK_LT(plane, pixmap_handle_.planes.size());
  return pixmap_handle_.planes[plane].stride;
}

}  // namespace gfx
