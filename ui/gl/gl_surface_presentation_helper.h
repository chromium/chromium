// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_PRESENTATION_HELPER_H_
#define UI_GL_GL_SURFACE_PRESENTATION_HELPER_H_

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"

namespace gfx {
class VSyncProvider;
}

namespace gl {

class GLContext;
class GLFence;
class GPUTimingClient;
class GPUTimer;

// Helper class for managering and invoke presentation callbacks for GLSurface
// implementations.
class GL_EXPORT GLSurfacePresentationHelper {
 public:
  class GL_EXPORT ScopedSwapBuffers {
   public:
    ScopedSwapBuffers(GLSurfacePresentationHelper* helper,
                      GLSurface::PresentationCallback callback);
    ScopedSwapBuffers(GLSurfacePresentationHelper* helper,
                      GLSurface::PresentationCallback callback,
                      int frame_id);

    ScopedSwapBuffers(const ScopedSwapBuffers&) = delete;
    ScopedSwapBuffers& operator=(const ScopedSwapBuffers&) = delete;

    ~ScopedSwapBuffers();

    void set_result(gfx::SwapResult result) { result_ = result; }
    gfx::SwapResult result() const { return result_; }

   private:
    const raw_ptr<GLSurfacePresentationHelper> helper_;
    gfx::SwapResult result_ = gfx::SwapResult::SWAP_ACK;
  };

  explicit GLSurfacePresentationHelper(gfx::VSyncProvider* vsync_provider);

  // For using fixed VSync provider.
  GLSurfacePresentationHelper(const base::TimeTicks timebase,
                              const base::TimeDelta interval);

  GLSurfacePresentationHelper(const GLSurfacePresentationHelper&) = delete;
  GLSurfacePresentationHelper& operator=(const GLSurfacePresentationHelper&) =
      delete;

  ~GLSurfacePresentationHelper();

  void OnMakeCurrent(GLContext* context, GLSurface* surface);
  void PreSwapBuffers(GLSurface::PresentationCallback callback, int frame_id);
  void PostSwapBuffers(gfx::SwapResult result);

 private:
  struct Frame {
    Frame(Frame&& other);
    Frame(int frame_id, GLSurface::PresentationCallback callback);
    Frame(std::unique_ptr<GPUTimer>&& timer,
          GLSurface::PresentationCallback callback);
    Frame(std::unique_ptr<GLFence>&& fence,
          GLSurface::PresentationCallback callback);
    explicit Frame(GLSurface::PresentationCallback callback);
    ~Frame();
    Frame& operator=(Frame&& other);

    void Destroy(bool has_context = false);

    std::unique_ptr<GPUTimer> timer;
    // GLFence is used only if gpu timers are not available.
    std::unique_ptr<GLFence> fence;
    int frame_id = -1;
    GLSurface::PresentationCallback callback;
    gfx::SwapResult result = gfx::SwapResult::SWAP_ACK;
  };

  bool GetFrameTimestampInfoIfAvailable(const Frame& frame,
                                        base::TimeTicks* timestamp,
                                        base::TimeDelta* interval,
                                        base::TimeTicks* writes_done,
                                        uint32_t* flags);

  // Check |pending_frames_| and run presentation callbacks.
  void CheckPendingFrames();

  // Callback used by PostDelayedTask for running CheckPendingFrames().
  void CheckPendingFramesCallback();

  void UpdateVSyncCallback(bool should_check_pending_frames,
                           const base::TimeTicks timebase,
                           const base::TimeDelta interval);

  void ScheduleCheckPendingFrames(bool align_with_next_vsync);

  const raw_ptr<gfx::VSyncProvider> vsync_provider_;
  scoped_refptr<GLContext> gl_context_;
  raw_ptr<GLSurface> surface_ = nullptr;
  scoped_refptr<GPUTimingClient> gpu_timing_client_;
  base::circular_deque<Frame> pending_frames_;
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;
  bool check_pending_frame_scheduled_ = false;
  bool gl_fence_supported_ = false;
  raw_ptr<EGLTimestampClient> egl_timestamp_client_ = nullptr;
  bool update_vsync_pending_ = false;

  base::WeakPtrFactory<GLSurfacePresentationHelper> weak_ptr_factory_{this};
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_PRESENTATION_HELPER_H_
