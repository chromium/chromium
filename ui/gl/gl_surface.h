// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_H_
#define UI_GL_GL_SURFACE_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/surface_origin.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_format.h"
#include "ui/gl/gpu_preference.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/gfx/native_pixmap.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "ui/gfx/mac/io_surface.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#endif

namespace gfx {
class ColorSpace;
class VSyncProvider;
}  // namespace gfx

namespace gl {

class GLContext;
class EGLTimestampClient;

// Encapsulates a surface that can be rendered to with GL, hiding platform
// specific management.
class GL_EXPORT GLSurface : public base::RefCounted<GLSurface> {
 public:
  GLSurface();

  GLSurface(const GLSurface&) = delete;
  GLSurface& operator=(const GLSurface&) = delete;

  // Non-virtual initialization, this always calls Initialize with a
  // default GLSurfaceFormat. Subclasses should override the format-
  // specific Initialize method below and interpret the default format
  // as appropriate.
  bool Initialize();

  // (Re)create the surface. TODO(apatrick): This is an ugly hack to allow the
  // EGL surface associated to be recreated without destroying the associated
  // context. The implementation of this function for other GLSurface derived
  // classes is in a pending changelist.
  virtual bool Initialize(GLSurfaceFormat format);

  // Destroys the surface.
  virtual void Destroy() = 0;

  // Resizes the surface, returning success. If failed, it is possible that the
  // context is no longer current.
  virtual bool Resize(const gfx::Size& size,
                      float scale_factor,
                      const gfx::ColorSpace& color_space,
                      bool has_alpha);

  // Recreate the surface without changing the size, returning success. If
  // failed, it is possible that the context is no longer current.
  virtual bool Recreate();

  // Returns true if this surface is offscreen.
  virtual bool IsOffscreen() = 0;

  // The callback is for receiving presentation feedback from |SwapBuffers|,
  // |PostSubBuffer|, |CommitOverlayPlanes|, etc.
  // See |PresentationFeedback| for detail.
  using PresentationCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;

  // Swaps front and back buffers. This has no effect for off-screen
  // contexts. If it returns SWAP_FAILED, it is possible that the context is no
  // longer current.
  virtual gfx::SwapResult SwapBuffers(PresentationCallback callback,
                                      gfx::FrameData data) = 0;

  // Get the size of the surface.
  virtual gfx::Size GetSize() = 0;

  // Get the underlying platform specific surface "handle".
  virtual void* GetHandle() = 0;

  // Returns whether or not the surface supports PostSubBuffer.
  virtual bool SupportsPostSubBuffer();

  // Returns whether SwapBuffersAsync() is supported.
  virtual bool SupportsAsyncSwap();

  // Returns the internal frame buffer object name if the surface is backed by
  // FBO. Otherwise returns 0.
  virtual unsigned int GetBackingFramebufferObject();

  // The SwapCompletionCallback is used to receive notification about the
  // completion of the swap operation from |SwapBuffersAsync|,
  // |PostSubBufferAsync|, |CommitOverlayPlanesAsync|, etc. If a null gpu fence
  // is returned, then the swap is guaranteed to have already completed. If a
  // non-null gpu fence is returned, then the swap operation may still be in
  // progress when this callback is invoked, and the signaling of the gpu fence
  // will mark the completion of the swap operation.
  using SwapCompletionCallback =
      base::OnceCallback<void(gfx::SwapCompletionResult)>;

  // Swaps front and back buffers. This has no effect for off-screen
  // contexts. On some platforms, we want to send SwapBufferAck only after the
  // surface is displayed on screen. The callback can be used to delay sending
  // SwapBufferAck till that data is available. The callback should be run on
  // the calling thread (i.e. same thread SwapBuffersAsync is called)
  virtual void SwapBuffersAsync(SwapCompletionCallback completion_callback,
                                PresentationCallback presentation_callback,
                                gfx::FrameData data);

  // Copy part of the backbuffer to the frontbuffer. If it returns SWAP_FAILED,
  // it is possible that the context is no longer current.
  virtual gfx::SwapResult PostSubBuffer(int x,
                                        int y,
                                        int width,
                                        int height,
                                        PresentationCallback callback,
                                        gfx::FrameData data);

  // Copy part of the backbuffer to the frontbuffer. On some platforms, we want
  // to send SwapBufferAck only after the surface is displayed on screen. The
  // callback can be used to delay sending SwapBufferAck till that data is
  // available. The callback should be run on the calling thread (i.e. same
  // thread PostSubBufferAsync is called)
  virtual void PostSubBufferAsync(int x,
                                  int y,
                                  int width,
                                  int height,
                                  SwapCompletionCallback completion_callback,
                                  PresentationCallback presentation_callback,
                                  gfx::FrameData data);

  // Called after a context is made current with this surface. Returns false
  // on error.
  virtual bool OnMakeCurrent(GLContext* context);

  // Get a handle used to share the surface with another process. Returns null
  // if this is not possible.
  virtual void* GetShareHandle();

  // Get the platform specific display on which this surface resides, if
  // available.
  virtual GLDisplay* GetGLDisplay();

  // Get the platfrom specific configuration for this surface, if available.
  virtual void* GetConfig();

  // Get the GL pixel format of the surface. Must be implemented in a
  // subclass, though it's ok to just "return GLSurfaceFormat()" if
  // the default is appropriate.
  virtual GLSurfaceFormat GetFormat() = 0;

  // Get access to a helper providing time of recent refresh and period
  // of screen refresh. If unavailable, returns NULL.
  virtual gfx::VSyncProvider* GetVSyncProvider();

  // Set vsync to enabled or disabled. If supported, vsync is enabled by
  // default. Does nothing if vsync cannot be changed.
  virtual void SetVSyncEnabled(bool enabled);

  virtual bool IsSurfaceless() const;

  virtual gfx::SurfaceOrigin GetOrigin() const;

  // Returns true if SwapBuffers or PostSubBuffers causes a flip, such that
  // the next buffer may be 2 frames old.
  virtual bool BuffersFlipped() const;

  // Returns true if we are allowed to adopt a size different from the
  // platform's proposed surface size.
  virtual bool SupportsOverridePlatformSize() const;

  // Support for eglGetFrameTimestamps.
  virtual bool SupportsSwapTimestamps() const;
  virtual void SetEnableSwapTimestamps();

  virtual bool SupportsPlaneGpuFences() const;

  // Returns the number of buffers the surface uses in the swap chain. For
  // example, most surfaces are double-buffered, so this would return 2. For
  // triple-buffered surfaces this would return 3, etc.
  virtual int GetBufferCount() const;

  // Return the interface used for querying EGL timestamps.
  virtual EGLTimestampClient* GetEGLTimestampClient();

  static GLSurface* GetCurrent();

  virtual void SetCurrent();
  virtual bool IsCurrent();

  base::WeakPtr<GLSurface> AsWeakPtr();

  static bool ExtensionsContain(const char* extensions, const char* name);

  // This should be called at most once at GPU process startup time.
  static void SetForcedGpuPreference(GpuPreference gpu_preference);
  // If a gpu preference is forced (by GPU driver bug workaround, etc), return
  // it. Otherwise, return the original input preference.
  static GpuPreference AdjustGpuPreference(GpuPreference gpu_preference);

 protected:
  virtual ~GLSurface();

  void InvalidateWeakPtrs();
  bool HasWeakPtrs();

  static GpuPreference forced_gpu_preference_;

 private:
  static void ClearCurrent();

  friend class base::RefCounted<GLSurface>;
  friend class GLContext;

  base::WeakPtrFactory<GLSurface> weak_ptr_factory_{this};
};

// Wraps GLSurface in scoped_refptr and tries to initializes it. Returns a
// scoped_refptr containing the initialized GLSurface or nullptr if
// initialization fails.
GL_EXPORT scoped_refptr<GLSurface> InitializeGLSurface(
    scoped_refptr<GLSurface> surface);

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_H_
