// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_
#define UI_GFX_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_

#include <android/hardware_buffer.h>
#include <android/native_window.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/overlay_transform.h"

extern "C" {
typedef struct ASurfaceControl ASurfaceControl;
typedef struct ASurfaceTransaction ASurfaceTransaction;
}

namespace gfx {
class ColorSpace;

class GFX_EXPORT SurfaceControl {
 public:
  // Check if the platform is capable of supporting the low-level SurfaceControl
  // API. See also gpu/config/gpu_util's GetAndroidSurfaceControlFeatureStatus
  // which checks other prerequisites such as Gpufence support before declaring
  // support for the high-level SurfaceControl feature in Chrome.
  static bool IsSupported();

  // Returns true if overlays with |color_space| are supported by the platform.
  static bool SupportsColorSpace(const gfx::ColorSpace& color_space);

  // Translate `color_space` and `desired_brightness_ratio` to an ADataSpace and
  // extended range brightness ratio.
  static bool ColorSpaceToADataSpace(
      const gfx::ColorSpace& color_space,
      float desired_brightness_ratio,
      ADataSpace& out_dataspace,
      float& out_extended_range_brightness_ratio);

  // Returns the usage flags required for using an AHardwareBuffer with the
  // SurfaceControl API, if it is supported.
  static uint64_t RequiredUsage();

  // Enables usage bits requires for getting UBWC on Qualcomm devices. Must be
  // called early at process startup, before any buffer allocations are made.
  static void EnableQualcommUBWC();

  // Returns true if tagging a surface with a frame rate value is supported.
  static bool SupportsSetFrameRate();

  // Returns true if OnCommit callback is supported.
  static bool SupportsOnCommit();

  // Returns true if tagging a transaction with vsync id is supported.
  static GFX_EXPORT bool SupportsSetFrameTimeline();

  // Returns true if APIs to convert Java SurfaceControl to ASurfaceControl.
  static GFX_EXPORT bool SupportsSurfacelessControl();

  // Returns true if API to enable back pressure is supported.
  static GFX_EXPORT bool SupportsSetEnableBackPressure();

  // Applies transaction. Used to emulate webview functor interface, where we
  // pass raw ASurfaceTransaction object. For use inside Chromium use
  // Transaction class below instead.
  static void ApplyTransaction(ASurfaceTransaction* transaction);

  static void SetStubImplementationForTesting();

  class GFX_EXPORT Surface : public base::RefCounted<Surface> {
   public:
    // Wraps ASurfaceControl, but doesn't transfer ownership. Will not release
    // in dtor.
    static scoped_refptr<Surface> WrapUnowned(ASurfaceControl* surface);

    Surface();
    Surface(const Surface& parent, const char* name);
    Surface(ANativeWindow* parent, const char* name);
    Surface(JNIEnv* env,
            const base::android::JavaRef<jobject>& j_surface_control);

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    ASurfaceControl* surface() const { return surface_; }

   private:
    friend class base::RefCounted<Surface>;
    ~Surface();

    raw_ptr<ASurfaceControl> surface_ = nullptr;
    raw_ptr<ASurfaceControl> owned_surface_ = nullptr;
  };

  struct GFX_EXPORT SurfaceStats {
    SurfaceStats();
    ~SurfaceStats();

    SurfaceStats(SurfaceStats&& other);
    SurfaceStats& operator=(SurfaceStats&& other);

    raw_ptr<ASurfaceControl> surface = nullptr;

    // The fence which is signaled when the reads for the previous buffer for
    // the given |surface| are finished.
    base::ScopedFD fence;
  };

  struct GFX_EXPORT TransactionStats {
   public:
    TransactionStats();

    TransactionStats(const TransactionStats&) = delete;
    TransactionStats& operator=(const TransactionStats&) = delete;

    ~TransactionStats();

    TransactionStats(TransactionStats&& other);
    TransactionStats& operator=(TransactionStats&& other);

    // The fence which is signaled when this transaction is presented by the
    // display.
    base::ScopedFD present_fence;
    std::vector<SurfaceStats> surface_stats;
    base::TimeTicks latch_time;
  };

  class GFX_EXPORT Transaction {
   public:
    Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

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
                       const gfx::ColorSpace& color_space,
                       const std::optional<HDRMetadata>& metadata);
    void SetFrameRate(const Surface& surface, float frame_rate);
    void SetParent(const Surface& surface, Surface* new_parent);
    void SetPosition(const Surface& surface, const gfx::Point& position);
    void SetScale(const Surface& surface, float sx, float sy);
    void SetCrop(const Surface& surface, const gfx::Rect& rect);
    void SetFrameTimelineId(int64_t vsync_id);
    void SetEnableBackPressure(const Surface& surface, bool enable);

    // Sets the callback which will be dispatched when the transaction is acked
    // by the framework.
    // |task_runner| provides an optional task runner on which the callback
    // should be run.
    using OnCompleteCb = base::OnceCallback<void(TransactionStats stats)>;
    void SetOnCompleteCb(
        OnCompleteCb cb,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    using OnCommitCb = base::OnceClosure;
    void SetOnCommitCb(OnCommitCb cb,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    void Apply();
    // Caller(e.g.,WebView) must call ASurfaceTransaction_apply(), otherwise
    // SurfaceControl leaks.
    ASurfaceTransaction* GetTransaction();

   private:
    void PrepareCallbacks();
    void DestroyIfNeeded();

    int id_;
    raw_ptr<ASurfaceTransaction> transaction_;
    OnCommitCb on_commit_cb_;
    OnCompleteCb on_complete_cb_;
    bool need_to_apply_ = false;
  };
};
}  // namespace gfx

#endif  // UI_GFX_ANDROID_ANDROID_SURFACE_CONTROL_COMPAT_H_
