// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_H_
#define UI_GL_GL_SURFACE_EGL_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/egl_timestamps.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_overlay.h"

namespace gl {

class GLSurfacePresentationHelper;

GL_EXPORT void GetEGLInitDisplays(bool supports_angle_d3d,
                                  bool supports_angle_opengl,
                                  bool supports_angle_null,
                                  bool supports_angle_vulkan,
                                  bool supports_angle_swiftshader,
                                  bool supports_angle_egl,
                                  bool supports_angle_metal,
                                  const base::CommandLine* command_line,
                                  std::vector<DisplayType>* init_displays);

// Interface for EGL surface.
class GL_EXPORT GLSurfaceEGL : public GLSurface {
 public:
  explicit GLSurfaceEGL(GLDisplayEGL* display);
  GLSurfaceEGL(const GLSurfaceEGL&) = delete;
  GLSurfaceEGL& operator=(const GLSurfaceEGL&) = delete;
  virtual EGLint GetNativeVisualID() const;

  // Implement GLSurface.
  GLDisplay* GetGLDisplay() override;
  EGLConfig GetConfig() override;
  GLSurfaceFormat GetFormat() override;

  EGLDisplay GetEGLDisplay();

  static GLDisplayEGL* GetGLDisplayEGL();

  // |system_device_id| specifies which GPU to use on a multi-GPU system.
  // If its value is 0, use the default GPU of the system.
  static bool InitializeOneOff(EGLDisplayPlatform native_display,
                               uint64_t system_device_id);
  static bool InitializeOneOffForTesting();
  static bool InitializeExtensionSettingsOneOff();
  static void ShutdownOneOff();
  // |system_device_id| specifies which GPU to use on a multi-GPU system.
  // If its value is 0, use the default GPU of the system.
  static GLDisplayEGL* InitializeDisplay(EGLDisplayPlatform native_display,
                                         uint64_t system_device_id);

 protected:
  ~GLSurfaceEGL() override;

  EGLConfig config_ = nullptr;
  GLSurfaceFormat format_;
  GLDisplayEGL* display_ = nullptr;

 private:
  static bool InitializeOneOffCommon(GLDisplayEGL* display);
  static bool initialized_;
};

// Encapsulates an EGL surface bound to a view.
class GL_EXPORT NativeViewGLSurfaceEGL : public GLSurfaceEGL,
                                         public EGLTimestampClient {
 public:
  NativeViewGLSurfaceEGL(GLDisplayEGL* display,
                         EGLNativeWindowType window,
                         std::unique_ptr<gfx::VSyncProvider> vsync_provider);

  NativeViewGLSurfaceEGL(const NativeViewGLSurfaceEGL&) = delete;
  NativeViewGLSurfaceEGL& operator=(const NativeViewGLSurfaceEGL&) = delete;

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  bool SupportsSwapTimestamps() const override;
  void SetEnableSwapTimestamps() override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool Recreate() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  EGLSurface GetHandle() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback) override;
  bool SupportsCommitOverlayPlanes() override;
  gfx::SwapResult CommitOverlayPlanes(PresentationCallback callback) override;
  bool OnMakeCurrent(GLContext* context) override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  void SetVSyncEnabled(bool enabled) override;
  bool ScheduleOverlayPlane(
      GLImage* image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override;
  gfx::SurfaceOrigin GetOrigin() const override;
  EGLTimestampClient* GetEGLTimestampClient() override;

  // EGLTimestampClient implementation.
  bool IsEGLTimestampSupported() const override;

  bool GetFrameTimestampInfoIfAvailable(base::TimeTicks* presentation_time,
                                        base::TimeDelta* composite_interval,
                                        base::TimeTicks* writes_done_time,
                                        uint32_t* presentation_flags,
                                        int frame_id) override;

  // Takes care of the platform dependant bits, of any, for creating the window.
  virtual bool InitializeNativeWindow();

 protected:
  ~NativeViewGLSurfaceEGL() override;

  EGLNativeWindowType window_ = 0;
  gfx::Size size_ = gfx::Size(1, 1);
  bool enable_fixed_size_angle_ = true;

  GLSurfacePresentationHelper* presentation_helper() const {
    return presentation_helper_.get();
  }

  gfx::SwapResult SwapBuffersWithDamage(const std::vector<int>& rects,
                                        PresentationCallback callback);

 private:
  struct SwapInfo {
    bool frame_id_is_valid;
    EGLuint64KHR frame_id;
  };

  void UpdateSwapEvents(EGLuint64KHR newFrameId, bool newFrameIdIsValid);
  void TraceSwapEvents(EGLuint64KHR oldFrameId);

  // Some platforms may provide a custom implementation of vsync provider.
  virtual std::unique_ptr<gfx::VSyncProvider> CreateVsyncProviderInternal();

  EGLSurface surface_ = nullptr;
  bool supports_post_sub_buffer_ = false;
  bool supports_swap_buffer_with_damage_ = false;
  gfx::SurfaceOrigin surface_origin_ = gfx::SurfaceOrigin::kBottomLeft;

  std::unique_ptr<gfx::VSyncProvider> vsync_provider_external_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_internal_;

  // Stored in separate vectors so we can pass the egl timestamps
  // directly to the EGL functions.
  bool use_egl_timestamps_ = false;
  std::vector<EGLint> supported_egl_timestamps_;
  std::vector<const char*> supported_event_names_;

  // PresentationFeedback support.
  int presentation_feedback_index_ = -1;
  int composition_start_index_ = -1;
  int writes_done_index_ = -1;
  uint32_t presentation_flags_ = 0;

  base::queue<SwapInfo> swap_info_queue_;

  bool vsync_enabled_ = true;
  std::unique_ptr<GLSurfacePresentationHelper> presentation_helper_;
};

// Encapsulates a pbuffer EGL surface.
class GL_EXPORT PbufferGLSurfaceEGL : public GLSurfaceEGL {
 public:
  PbufferGLSurfaceEGL(GLDisplayEGL* display, const gfx::Size& size);

  PbufferGLSurfaceEGL(const PbufferGLSurfaceEGL&) = delete;
  PbufferGLSurfaceEGL& operator=(const PbufferGLSurfaceEGL&) = delete;

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  EGLSurface GetHandle() override;
  void* GetShareHandle() override;

 protected:
  ~PbufferGLSurfaceEGL() override;

 private:
  gfx::Size size_;
  EGLSurface surface_;
};

// SurfacelessEGL is used as Offscreen surface when platform supports
// KHR_surfaceless_context and GL_OES_surfaceless_context. This would avoid the
// need to create a dummy EGLsurface in case we render to client API targets.
class GL_EXPORT SurfacelessEGL : public GLSurfaceEGL {
 public:
  SurfacelessEGL(GLDisplayEGL* display, const gfx::Size& size);

  SurfacelessEGL(const SurfacelessEGL&) = delete;
  SurfacelessEGL& operator=(const SurfacelessEGL&) = delete;

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  bool IsSurfaceless() const override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  EGLSurface GetHandle() override;
  void* GetShareHandle() override;

 protected:
  ~SurfacelessEGL() override;

 private:
  gfx::Size size_;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_H_
