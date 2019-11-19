// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_CONTEXT_H_
#define UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_CONTEXT_H_

#include <OpenGL/OpenGL.h>

#include <map>
#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/gl/gpu_switching_observer.h"
#include "ui/gl/scoped_cgl.h"

namespace ui {

class IOSurfaceContext
    : public base::RefCounted<IOSurfaceContext>,
      public ui::GpuSwitchingObserver {
 public:
  enum Type {
    // The number used to look up the context used for async readback and for
    // initializing the IOSurface.
    kOffscreenContext = -2,
    // The number used to look up the context used by CAOpenGLLayers.
    kCALayerContext = -3,
  };

  // Get or create a GL context of the specified type. Share these GL contexts
  // as much as possible because creating and destroying them can be expensive.
  // http://crbug.com/180463
  ACCELERATED_WIDGET_MAC_EXPORT static scoped_refptr<IOSurfaceContext> Get(
      Type type);

  // Mark that all the GL contexts in the same sharegroup as this context as
  // invalid, so they shouldn't be returned anymore by Get, but rather, new
  // contexts should be created. This is called as a precaution when unexpected
  // GL errors occur, or after a GPU switch.
  void PoisonContextAndSharegroup();
  bool HasBeenPoisoned() const { return poisoned_; }

  CGLContextObj cgl_context() const { return cgl_context_; }

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override;

 private:
  friend class base::RefCounted<IOSurfaceContext>;

  IOSurfaceContext(
      Type type,
      base::ScopedTypeRef<CGLContextObj> clg_context_strong);
  virtual ~IOSurfaceContext();

  Type type_;
  base::ScopedTypeRef<CGLContextObj> cgl_context_;

  bool poisoned_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_IO_SURFACE_CONTEXT_H_
