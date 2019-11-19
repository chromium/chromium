// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_
#define UI_GL_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_

#include <memory>

#include <android/hardware_buffer.h>
#include <android/native_window.h>

#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_export.h"

extern "C" {
typedef struct ASurfaceControl ASurfaceControl;
typedef struct ASurfaceTransaction ASurfaceTransaction;
}

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace gl {

class GL_EXPORT SurfaceControl {
 public:
  static bool IsSupported();

  // Returns true if overlays with |color_space| are supported by the platform.
  static bool SupportsColorSpace(const gfx::ColorSpace& color_space);

  // Returns the usage flags required for using an AHardwareBuffer with the
  // SurfaceControl API, if it is supported.
  static uint64_t RequiredUsage();

  // Enables usage bits requires for getting UBWC on Qualcomm devices. Must be
  // called early at process startup, before any buffer allocations are made.
  static void EnableQualcommUBWC();

  class GL_EXPORT Surface : public base::RefCounted<Surface> {
   public:
    Surface();
    Surface(const Surface& parent, const char* name);
    Surface(ANativeWindow* parent, const char* name);

    ASurfaceControl* surface() const { return surface_; }

   private:
    friend class base::RefCounted<Surface>;
    ~Surface();

    ASurfaceControl* surface_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Surface);
  };

  struct GL_EXPORT SurfaceStats {
    SurfaceStats();
    ~SurfaceStats();

    SurfaceStats(SurfaceStats&& other);
    SurfaceStats& operator=(SurfaceStats&& other);

    ASurfaceControl* surface = nullptr;

    // The fence which is signaled when the reads for the previous buffer for
    // the given |surface| are finished.
    base::ScopedFD fence;
  };

  struct GL_EXPORT TransactionStats {
   public:
    TransactionStats();
    ~TransactionStats();

    TransactionStats(TransactionStats&& other);
    TransactionStats& operator=(TransactionStats&& other);

    // The fence which is signaled when this transaction is presented by the
    // display.
    base::ScopedFD present_fence;
    std::vector<SurfaceStats> surface_stats;
    base::TimeTicks latch_time;

   private:
    DISALLOW_COPY_AND_ASSIGN(TransactionStats);
  };

  class GL_EXPORT Transaction {
   public:
    Transaction();
    ~Transaction();

    Transaction(Transaction&& other);
    Transaction& operator=(Transaction&& other);

    void SetVisibility(const Surface& surface, bool show);
    void SetZOrder(const Surface& surface, int32_t z);
    void SetBuffer(const Surface& surface,
                   AHardwareBuffer* buffer,
                   base::ScopedFD fence_fd);
    void SetGeometry(const Surface& surface,
                     const gfx::Rect& src,
                     const gfx::Rect& dst,
                     gfx::OverlayTransform transform);
    void SetOpaque(const Surface& surface, bool opaque);
    void SetDamageRect(const Surface& surface, const gfx::Rect& rect);
    void SetColorSpace(const Surface& surface,
                       const gfx::ColorSpace& color_space);

    // Sets the callback which will be dispatched when the transaction is acked
    // by the framework.
    // |task_runner| provides an optional task runner on which the callback
    // should be run.
    using OnCompleteCb = base::OnceCallback<void(TransactionStats stats)>;
    void SetOnCompleteCb(
        OnCompleteCb cb,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    void Apply();

   private:
    int id_;
    ASurfaceTransaction* transaction_;

    DISALLOW_COPY_AND_ASSIGN(Transaction);
  };
};
}  // namespace gl

#endif  // UI_GL_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_
