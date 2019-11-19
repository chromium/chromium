// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_PIXMAP_H_
#define UI_GFX_NATIVE_PIXMAP_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {
class Rect;
class RectF;
class GpuFence;

// This represents a buffer that can be directly imported via GL for
// rendering, or exported via dma-buf fds.
class NativePixmap : public base::RefCountedThreadSafe<NativePixmap> {
 public:
  NativePixmap() {}

  virtual bool AreDmaBufFdsValid() const = 0;
  virtual int GetDmaBufFd(size_t plane) const = 0;
  virtual uint32_t GetDmaBufPitch(size_t plane) const = 0;
  virtual size_t GetDmaBufOffset(size_t plane) const = 0;
  virtual size_t GetDmaBufPlaneSize(size_t plane) const = 0;
  // Return the number of non-interleaved "color" planes.
  virtual size_t GetNumberOfPlanes() const = 0;

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
  // |plane_z_order| specifies the stacking order of the plane relative to the
  // main framebuffer located at index 0.
  // |plane_transform| specifies how the buffer is to be transformed during
  // composition.
  // |display_bounds| specify where it is supposed to be on the screen.
  // |crop_rect| specifies the region within the buffer to be placed
  // inside |display_bounds|. This is specified in texture coordinates, in the
  // range of [0,1].
  // |enable_blend| specifies if the plane should be alpha blended, with premul
  // apha, when scanned out.
  // |gpu_fence| specifies a gpu fence to wait on before the pixmap is ready
  // to be displayed.
  virtual bool ScheduleOverlayPlane(
      gfx::AcceleratedWidget widget,
      int plane_z_order,
      gfx::OverlayTransform plane_transform,
      const gfx::Rect& display_bounds,
      const gfx::RectF& crop_rect,
      bool enable_blend,
      std::unique_ptr<gfx::GpuFence> gpu_fence) = 0;

  // Export the buffer for sharing across processes.
  // Any file descriptors in the exported handle are owned by the caller.
  virtual gfx::NativePixmapHandle ExportHandle() = 0;

 protected:
  virtual ~NativePixmap() {}

 private:
  friend class base::RefCountedThreadSafe<NativePixmap>;

  DISALLOW_COPY_AND_ASSIGN(NativePixmap);
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_PIXMAP_H_
