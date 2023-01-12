// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SURFACE_OZONE_CANVAS_H_
#define UI_OZONE_PUBLIC_SURFACE_OZONE_CANVAS_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/frame_data.h"

class SkCanvas;

namespace gfx {
class Rect;
class Size;
class VSyncProvider;
}  // namespace gfx

namespace ui {

// The platform-specific part of an software output. The class is intended
// for use when no EGL/GLES2 acceleration is possible.
// This class owns any bits that the ozone implementation needs freed when
// the software output is destroyed.
class COMPONENT_EXPORT(OZONE_BASE) SurfaceOzoneCanvas {
 public:
  virtual ~SurfaceOzoneCanvas();

  // Returns an SkCanvas for drawing on the window. The SurfaceOzoneCanvas keeps
  // the SkCanvas alive until the client finishes writing contents and calls
  // PresentCanvas. Additionally, the SkCanvas becomes invalid after
  // ResizeCanvas is called. See comment at ResizeCanvas.
  virtual SkCanvas* GetCanvas() = 0;

  // Attempts to resize the canvas to match the viewport size. After
  // resizing, the compositor must call GetSurface() to get the next
  // surface - this invalidates any previous surface from GetSurface().
  // |viewport_size| is the size of viewport in pixels that is the multiple
  // of a size in screen dips * |scale|.
  virtual void ResizeCanvas(const gfx::Size& viewport_size, float scale) = 0;

  // Present the current surface. After presenting, the compositor must
  // call GetSurface() to get the next surface - this invalidates any
  // previous surface from GetSurface().
  //
  // The implementation may assume that any pixels outside the damage
  // rectangle are unchanged since the previous call to PresentCanvas().
  virtual void PresentCanvas(const gfx::Rect& damage) = 0;

  // Returns a gfx::VsyncProvider for this surface. Note that this may be
  // called after we have entered the sandbox so if there are operations (e.g.
  // opening a file descriptor providing vsync events) that must be done
  // outside of the sandbox, they must have been completed in
  // InitializeHardware. Returns an empty scoped_ptr on error.
  virtual std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() = 0;

  // If asynchronous buffer swap is supported, the implementation must override
  // OnSwapBuffers and run the callback that is passed in an OnSwapBuffers
  // call once the swap is completed.
  virtual bool SupportsAsyncBufferSwap() const;

  // Returns true if we are allowed to adopt a size different from the
  // platform's proposed surface size.
  virtual bool SupportsOverridePlatformSize() const;

  // Corresponds to SoftwareOutputDevice::SwapBuffersCallback.
  using SwapBuffersCallback = base::OnceCallback<void(const gfx::Size&)>;
  // The implementations may want to handle the buffer swap callback by
  // themselves if the buffer swap is asynchronous, for example, or it needs to
  // do something else before the callback is called. Also check the comment
  // near the SupportsAsyncBufferSwap.
  virtual void OnSwapBuffers(SwapBuffersCallback swap_ack_callback,
                             gfx::FrameData data);

  // Returns the maximum number of pending frames.
  virtual int MaxFramesPending() const;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SURFACE_OZONE_CANVAS_H_
