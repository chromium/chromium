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
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/memory.h"
#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
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
    int ret = munmap(data, offset + size);
    DCHECK(!ret);
  }
}

// static
bool ClientNativePixmapDmaBuf::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
#if BUILDFLAG(IS_CHROMECAST)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      // TODO(spang): Fix b/121148905 and turn these back on.
      return false;
    default:
      break;
  }
#endif

  bool disable_yuv_biplanar = true;
#if defined(OS_CHROMEOS) || BUILDFLAG(IS_CHROMECAST)
  // IsConfigurationSupported(SCANOUT_CPU_READ_WRITE) is used by the renderer
  // to tell whether the platform supports sampling a given format. Zero-copy
  // video capture and encoding requires gfx::BufferFormat::YUV_420_BIPLANAR to
  // be supported by the renderer. Most of Chrome OS platforms support it, so
  // enable it by default, with a switch that allows an explicit disable on
  // platforms known to have problems, e.g. the Tegra-based nyan."
  // TODO(crbug.com/982201): move gfx::BufferFormat::YUV_420_BIPLANAR out
  // of if defined(ARCH_CPU_X86_FAMLIY) when Tegra is no longer supported.
  disable_yuv_biplanar = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableYuv420Biplanar);
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
             format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::RGBA_1010102 ||
             format == gfx::BufferFormat::BGRA_1010102;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      // TODO(crbug.com/954233): RG_88 is enabled only with
      // --enable-native-gpu-memory-buffers . Otherwise it breaks some telemetry
      // tests. Fix that issue and enable it again.
      if (format == gfx::BufferFormat::RG_88 && !AllowCpuMappableBuffers())
        return false;

      if (!disable_yuv_biplanar &&
          format == gfx::BufferFormat::YUV_420_BIPLANAR) {
        return true;
      }

      return
#if defined(ARCH_CPU_X86_FAMILY)
          // The minigbm backends and Mesa drivers commonly used on x86 systems
          // support the following formats.
          format == gfx::BufferFormat::R_8 ||
          format == gfx::BufferFormat::RG_88 ||
          format == gfx::BufferFormat::YUV_420_BIPLANAR ||
          format == gfx::BufferFormat::RGBA_1010102 ||
          format == gfx::BufferFormat::BGRA_1010102 ||
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

      if (!disable_yuv_biplanar &&
          format == gfx::BufferFormat::YUV_420_BIPLANAR) {
        return true;
      }

      return
#if defined(ARCH_CPU_X86_FAMILY)
          // The minigbm backends and Mesa drivers commonly used on x86 systems
          // support the following formats.
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

  for (size_t i = 0; i < handle.planes.size(); ++i) {
    // Verify that the plane buffer has appropriate size.
    const size_t plane_stride =
        base::strict_cast<size_t>(handle.planes[i].stride);
    size_t min_stride = 0;
    size_t subsample_factor = SubsamplingFactorForBufferFormat(format, i);
    base::CheckedNumeric<size_t> plane_height =
        (base::CheckedNumeric<size_t>(size.height()) + subsample_factor - 1) /
        subsample_factor;
    if (!gfx::RowSizeForBufferFormatChecked(size.width(), format, i,
                                            &min_stride) ||
        plane_stride < min_stride) {
      return nullptr;
    }
    base::CheckedNumeric<size_t> min_size =
        base::CheckedNumeric<size_t>(plane_stride) * plane_height;
    if (!min_size.IsValid() || handle.planes[i].size < min_size.ValueOrDie())
      return nullptr;

    // The stride must be a valid integer in order to be consistent with the
    // GpuMemoryBuffer::stride() API. Also, refer to http://crbug.com/1093644#c1
    // for some comments on this check and others in this method.
    if (!base::IsValueInRangeForNumericType<int>(plane_stride))
      return nullptr;

    const size_t map_size = base::checked_cast<size_t>(handle.planes[i].size);
    plane_info[i].offset = handle.planes[i].offset;
    plane_info[i].size = map_size;

    void* data = mmap(nullptr, map_size + handle.planes[i].offset,
                      (PROT_READ | PROT_WRITE), MAP_SHARED,
                      handle.planes[i].fd.get(), 0);

    if (data == MAP_FAILED) {
      logging::SystemErrorCode mmap_error = logging::GetLastSystemErrorCode();
      if (mmap_error == ENOMEM)
        base::TerminateBecauseOutOfMemory(map_size +
                                          handle.planes[i].offset);
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
