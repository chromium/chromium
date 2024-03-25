// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_PIXMAP_H_
#define UI_GFX_NATIVE_PIXMAP_H_

#include "base/memory/ref_counted.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
struct OverlayPlaneData;
class GpuFence;

// This represents a buffer that can be directly imported via GL for
// rendering, or exported via dma-buf fds.
class NativePixmap : public base::RefCountedThreadSafe<NativePixmap> {
 public:
  NativePixmap() {}

  NativePixmap(const NativePixmap&) = delete;
  NativePixmap& operator=(const NativePixmap&) = delete;

  virtual bool AreDmaBufFdsValid() const = 0;
  virtual int GetDmaBufFd(size_t plane) const = 0;
  virtual uint32_t GetDmaBufPitch(size_t plane) const = 0;
  virtual size_t GetDmaBufOffset(size_t plane) const = 0;
  virtual size_t GetDmaBufPlaneSize(size_t plane) const = 0;
  // Return the number of non-interleaved "color" planes.
  virtual size_t GetNumberOfPlanes() const = 0;
  virtual bool SupportsZeroCopyWebGPUImport() const = 0;

  // The following methods return format, modifier and size of the buffer,
  // respectively.
  virtual gfx::BufferFormat GetBufferFormat() const = 0;
  virtual uint64_t GetBufferFormatModifier() const = 0;
  virtual gfx::Size GetBufferSize() const = 0;

  // Return an id that is guaranteed to be unique and equal for all instances
  // of this NativePixmap backed by the same buffer, for the duration of its
  // lifetime. If such id cannot be generated, 0 (an invalid id) is returned.
  //
  // TODO(posciak): crbug.com/771863, remove this once a different mechanism
  // for protected shared memory buffers is implemented.
  virtual uint32_t GetUniqueId() const = 0;

  // Sets the overlay plane to switch to at the next page flip.
  // |widget| specifies the screen to display this overlay plane on.
  // |acquire_fences| specifies gpu fences to wait on before the pixmap is ready
  // to be displayed. These fence are fired when the gpu has finished writing to
  // the pixmap.
  // |release_fences| specifies gpu fences that are signalled when the pixmap
  // has been displayed and is ready for reuse.
  // |overlay_plane_data| specifies overlay data such as opacity, z_order, size,
  // etc.
  virtual bool ScheduleOverlayPlane(
      gfx::AcceleratedWidget widget,
      const gfx::OverlayPlaneData& overlay_plane_data,
      std::vector<gfx::GpuFence> acquire_fences,
      std::vector<gfx::GpuFence> release_fences) = 0;

  // Export the buffer for sharing across processes.
  // Any file descriptors in the exported handle are owned by the caller.
  virtual gfx::NativePixmapHandle ExportHandle() const = 0;

 protected:
  virtual ~NativePixmap() {}

 private:
  friend class base::RefCountedThreadSafe<NativePixmap>;
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_PIXMAP_H_
