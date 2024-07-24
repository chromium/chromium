// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"

#include <fcntl.h>
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
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/linux/dmabuf_uapi.h"
#include "ui/gfx/switches.h"

namespace gfx {

namespace {

void* MapPlane(const NativePixmapPlane& plane) {
  // The |size_to_map| computation has been determined to be valid in
  // ClientNativePixmapFactoryDmabuf::ImportFromHandle().
  const size_t size_to_map =
      base::CheckAdd(plane.size, plane.offset).ValueOrDie<size_t>();
  void* data = mmap(nullptr, size_to_map, (PROT_READ | PROT_WRITE), MAP_SHARED,
                    plane.fd.get(), 0);
  if (data == MAP_FAILED) {
    logging::SystemErrorCode mmap_error = logging::GetLastSystemErrorCode();
    if (mmap_error == ENOMEM)
      base::TerminateBecauseOutOfMemory(size_to_map);
    LOG(ERROR) << "Failed to mmap dmabuf: "
               << logging::SystemErrorCodeToString(mmap_error);
    return nullptr;
  }
  return data;
}

void PrimeSyncStart(int dmabuf_fd) {
  struct dma_buf_sync sync_start;

  // Do memset() instead of aggregate initialization because the latter can
  // behave unintuitively with unions in C++, and we probably should not assume
  // that dma_buf_sync will never contain a union.
  memset(&sync_start, 0, sizeof(sync_start));

  sync_start.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
  int rv = HANDLE_EINTR(ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync_start));
  PLOG_IF(ERROR, rv) << "Failed DMA_BUF_SYNC_START";
}

void PrimeSyncEnd(int dmabuf_fd) {
  struct dma_buf_sync sync_end = {0};

  // Do memset() instead of aggregate initialization because the latter can
  // behave unintuitively with unions in C++, and we probably should not assume
  // that dma_buf_sync will never contain a union.
  memset(&sync_end, 0, sizeof(sync_end));

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
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      // TODO(crbug.com/954233): RG_88 is enabled only with
      // --enable-native-gpu-memory-buffers . Otherwise it breaks some telemetry
      // tests. Fix that issue and enable it again.
      if (format == gfx::BufferFormat::RG_88 && !AllowCpuMappableBuffers())
        return false;

      if (format == gfx::BufferFormat::YUV_420_BIPLANAR)
        return true;

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
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:  // fallthrough
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
      return false;

    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      if (!AllowCpuMappableBuffers())
        return false;

      if (format == gfx::BufferFormat::YUV_420_BIPLANAR)
        return true;

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
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return format == gfx::BufferFormat::YVU_420 ||
             format == gfx::BufferFormat::YUV_420_BIPLANAR;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
std::unique_ptr<gfx::ClientNativePixmap>
ClientNativePixmapDmaBuf::ImportFromDmabuf(gfx::NativePixmapHandle handle,
                                           const gfx::Size& size,
                                           gfx::BufferFormat format) {
  if (handle.planes.size() > kMaxPlanes)
    return nullptr;

  std::array<PlaneInfo, kMaxPlanes> plane_info;
  for (size_t i = 0; i < handle.planes.size(); ++i) {
    plane_info[i].offset = base::checked_cast<size_t>(handle.planes[i].offset);
    plane_info[i].size = base::checked_cast<size_t>(handle.planes[i].size);
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
  if (!mapped_) {
    TRACE_EVENT0("drm", "DmaBuf:InitialMap");
    for (size_t i = 0; i < pixmap_handle_.planes.size(); ++i) {
      void* data = MapPlane(pixmap_handle_.planes[i]);
      if (!data)
        return false;
      plane_info_[i].data = data;
    }
    mapped_ = true;
  }

  for (const auto& plane : pixmap_handle_.planes)
    PrimeSyncStart(plane.fd.get());

  return true;
}

void ClientNativePixmapDmaBuf::Unmap() {
  TRACE_EVENT0("drm", "DmaBuf:Unmap");
  DCHECK(mapped_);
  for (const auto& plane : pixmap_handle_.planes)
    PrimeSyncEnd(plane.fd.get());
}

size_t ClientNativePixmapDmaBuf::GetNumberOfPlanes() const {
  return pixmap_handle_.planes.size();
}

void* ClientNativePixmapDmaBuf::GetMemoryAddress(size_t plane) const {
  DCHECK_LT(plane, pixmap_handle_.planes.size());
  CHECK(mapped_);
  return static_cast<uint8_t*>(plane_info_[plane].data) +
         plane_info_[plane].offset;
}

int ClientNativePixmapDmaBuf::GetStride(size_t plane) const {
  DCHECK_LT(plane, pixmap_handle_.planes.size());
  return base::checked_cast<int>(pixmap_handle_.planes[plane].stride);
}

NativePixmapHandle ClientNativePixmapDmaBuf::CloneHandleForIPC() const {
  return gfx::CloneHandleForIPC(pixmap_handle_);
}
}  // namespace gfx
