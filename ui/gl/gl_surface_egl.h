// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_H_
#define UI_GL_GL_SURFACE_EGL_H_

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/egl_timestamps.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_overlay.h"

namespace gl {

class GLSurfacePresentationHelper;

// If adding a new type, also add it to EGLDisplayType in
// tools/metrics/histograms/histograms.xml. Don't remove or reorder entries.
enum DisplayType {
  DEFAULT = 0,
  SWIFT_SHADER = 1,
  ANGLE_WARP = 2,
  ANGLE_D3D9 = 3,
  ANGLE_D3D11 = 4,
  ANGLE_OPENGL = 5,
  ANGLE_OPENGLES = 6,
  ANGLE_NULL = 7,
  ANGLE_D3D11_NULL = 8,
  ANGLE_OPENGL_NULL = 9,
  ANGLE_OPENGLES_NULL = 10,
  ANGLE_VULKAN = 11,
  ANGLE_VULKAN_NULL = 12,
  ANGLE_D3D11on12 = 13,
  ANGLE_SWIFTSHADER = 14,
  DISPLAY_TYPE_MAX = 15,
};

GL_EXPORT void GetEGLInitDisplays(bool supports_angle_d3d,
                                  bool supports_angle_opengl,
                                  bool supports_angle_null,
                                  bool supports_angle_vulkan,
                                  bool supports_angle_swiftshader,
                                  const base::CommandLine* command_line,
                                  std::vector<DisplayType>* init_displays);

// Interface for EGL surface.
class GL_EXPORT GLSurfaceEGL : public GLSurface {
 public:
  GLSurfaceEGL();

  // Implement GLSurface.
  EGLDisplay GetDisplay() override;
  EGLConfig GetConfig() override;
  GLSurfaceFormat GetFormat() override;

  static bool InitializeOneOff(EGLNativeDisplayType native_display);
  static bool InitializeOneOffForTesting();
  static bool InitializeExtensionSettingsOneOff();
  static void ShutdownOneOff();
  static EGLDisplay GetHardwareDisplay();
  static EGLDisplay InitializeDisplay(EGLNativeDisplayType native_display);
  static EGLNativeDisplayType GetNativeDisplay();

  // These aren't particularly tied to surfaces, but since we already
  // have the static InitializeOneOff here, it's easiest to reuse its
  // initialization guards.
  static const char* GetEGLExtensions();
  static bool HasEGLExtension(const char* name);
  static bool IsCreateContextRobustnessSupported();
  static bool IsCreateContextBindGeneratesResourceSupported();
  static bool IsCreateContextWebGLCompatabilitySupported();
  static bool IsEGLSurfacelessContextSupported();
  static bool IsEGLContextPrioritySupported();
  static bool IsEGLFlexibleSurfaceCompatibilitySupported();
  static bool IsRobustResourceInitSupported();
  static bool IsDisplayTextureShareGroupSupported();
  static bool IsCreateContextClientArraysSupported();
  static bool IsAndroidNativeFenceSyncSupported();
  static bool IsPixelFormatFloatSupported();
  static bool IsANGLEFeatureControlSupported();

 protected:
  ~GLSurfaceEGL() override;

  EGLConfig config_ = nullptr;
  GLSurfaceFormat format_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceEGL);
  static bool InitializeOneOffCommon();
  static bool initialized_;
};

// Encapsulates an EGL surface bound to a view.
class GL_EXPORT NativeViewGLSurfaceEGL : public GLSurfaceEGL,
                                         public EGLTimestampClient,
                                         public ui::PlatformEventDispatcher {
 public:
  NativeViewGLSurfaceEGL(EGLNativeWindowType window,
                         std::unique_ptr<gfx::VSyncProvider> vsync_provider);

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  bool SupportsSwapTimestamps() const override;
  void SetEnableSwapTimestamps() override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
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
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  bool FlipsVertically() const override;
  EGLTimestampClient* GetEGLTimestampClient() override;

  // EGLTimestampClient implementation.
  bool IsEGLTimestampSupported() const override;

  bool GetFrameTimestampInfoIfAvailable(base::TimeTicks* presentation_time,
                                        base::TimeDelta* composite_interval,
                                        uint32_t* presentation_flags,
                                        int frame_id) override;

  // Takes care of the platform dependant bits, of any, for creating the window.
  virtual bool InitializeNativeWindow();

 protected:
  ~NativeViewGLSurfaceEGL() override;

  EGLNativeWindowType window_ = 0;
  std::vector<EGLNativeWindowType> children_;
  gfx::Size size_ = gfx::Size(1, 1);
  bool enable_fixed_size_angle_ = true;

  gfx::SwapResult SwapBuffersWithDamage(const std::vector<int>& rects,
                                        PresentationCallback callback);

 private:
  struct SwapInfo {
    bool frame_id_is_valid;
    EGLuint64KHR frame_id;
  };

  // Commit the |pending_overlays_| and clear the vector. Returns false if any
  // fail to be committed.
  bool CommitAndClearPendingOverlays();
  void UpdateSwapEvents(EGLuint64KHR newFrameId, bool newFrameIdIsValid);
  void TraceSwapEvents(EGLuint64KHR oldFrameId);

  // PlatformEventDispatcher implementation.
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  EGLSurface surface_ = nullptr;
  bool supports_post_sub_buffer_ = false;
  bool supports_swap_buffer_with_damage_ = false;
  bool flips_vertically_ = false;

#if defined(USE_X11)
  bool has_swapped_buffers_ = false;
#endif

  std::unique_ptr<gfx::VSyncProvider> vsync_provider_external_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_internal_;

  std::vector<GLSurfaceOverlay> pending_overlays_;

  // Stored in separate vectors so we can pass the egl timestamps
  // directly to the EGL functions.
  bool use_egl_timestamps_ = false;
  std::vector<EGLint> supported_egl_timestamps_;
  std::vector<const char*> supported_event_names_;

  // PresentationFeedback support.
  int presentation_feedback_index_ = -1;
  int composition_start_index_ = -1;
  uint32_t presentation_flags_ = 0;

  base::queue<SwapInfo> swap_info_queue_;

  bool vsync_enabled_ = true;
  std::unique_ptr<GLSurfacePresentationHelper> presentation_helper_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceEGL);
};

// Encapsulates a pbuffer EGL surface.
class GL_EXPORT PbufferGLSurfaceEGL : public GLSurfaceEGL {
 public:
  explicit PbufferGLSurfaceEGL(const gfx::Size& size);

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  EGLSurface GetHandle() override;
  void* GetShareHandle() override;

 protected:
  ~PbufferGLSurfaceEGL() override;

 private:
  gfx::Size size_;
  EGLSurface surface_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLSurfaceEGL);
};

// SurfacelessEGL is used as Offscreen surface when platform supports
// KHR_surfaceless_context and GL_OES_surfaceless_context. This would avoid the
// need to create a dummy EGLsurface in case we render to client API targets.
class GL_EXPORT SurfacelessEGL : public GLSurfaceEGL {
 public:
  explicit SurfacelessEGL(const gfx::Size& size);

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  bool IsSurfaceless() const override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  EGLSurface GetHandle() override;
  void* GetShareHandle() override;

 protected:
  ~SurfacelessEGL() override;

 private:
  gfx::Size size_;
  DISALLOW_COPY_AND_ASSIGN(SurfacelessEGL);
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_H_
