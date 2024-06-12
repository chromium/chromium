// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_DISPLAY_H_
#define UI_GL_GL_DISPLAY_H_

#include <EGL/egl.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "ui/gl/gl_export.h"
#include "ui/gl/gpu_switching_manager.h"

#if BUILDFLAG(IS_APPLE)
#if __OBJC__
@protocol MTLSharedEvent;
#endif  // __OBJC__
#endif

namespace gl {
struct DisplayExtensionsEGL;
template <typename GLDisplayPlatform>
class GLDisplayManager;

class EGLDisplayPlatform {
 public:
  constexpr EGLDisplayPlatform()
      : display_(EGL_DEFAULT_DISPLAY), platform_(0), valid_(false) {}
  explicit constexpr EGLDisplayPlatform(EGLNativeDisplayType display,
                                        int platform = 0)
      : display_(display), platform_(platform), valid_(true) {}

  bool Valid() const { return valid_; }
  int GetPlatform() const { return platform_; }
  EGLNativeDisplayType GetDisplay() const { return display_; }

 private:
  EGLNativeDisplayType display_;
  // 0 for default, or EGL_PLATFORM_* enum.
  int platform_;
  bool valid_;
};

// If adding a new type, also add it to EGLDisplayType in
// tools/metrics/histograms/enums.xml. Don't remove or reorder entries.
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
  ANGLE_OPENGL_EGL = 15,
  ANGLE_OPENGLES_EGL = 16,
  ANGLE_METAL = 17,
  ANGLE_METAL_NULL = 18,
  DISPLAY_TYPE_MAX = 19,
};

enum DisplayPlatform {
  NONE = 0,
  EGL = 1,
};

class GL_EXPORT GLDisplay {
 public:
  GLDisplay(const GLDisplay&) = delete;
  GLDisplay& operator=(const GLDisplay&) = delete;

  uint64_t system_device_id() const { return system_device_id_; }
  DisplayKey display_key() const { return display_key_; }
  DisplayPlatform type() const { return type_; }

  virtual ~GLDisplay();

  virtual void* GetDisplay() const = 0;
  virtual void Shutdown() = 0;
  virtual bool IsInitialized() const = 0;
  virtual bool Initialize(GLDisplay* display) = 0;

  template <typename GLDisplayPlatform>
  GLDisplayPlatform* GetAs();

 protected:
  GLDisplay(uint64_t system_device_id,
            DisplayKey display_key,
            DisplayPlatform type);

  uint64_t system_device_id_ = 0;
  DisplayKey display_key_ = DisplayKey::kDefault;
  DisplayPlatform type_ = NONE;
};

// TODO(344606399): Consider merging GLDisplayEGL into GLDisplay.
class GL_EXPORT GLDisplayEGL : public GLDisplay {
 public:
  GLDisplayEGL(const GLDisplayEGL&) = delete;
  GLDisplayEGL& operator=(const GLDisplayEGL&) = delete;

  ~GLDisplayEGL() override;

  static GLDisplayEGL* GetDisplayForCurrentContext();

  static void EnableANGLEDebugLayer();

  EGLDisplay GetDisplay() const override;
  void Shutdown() override;
  bool IsInitialized() const override;

  void SetDisplay(EGLDisplay display);
  EGLDisplayPlatform GetNativeDisplay() const;
  DisplayType GetDisplayType() const;

  bool IsEGLSurfacelessContextSupported();
  bool IsEGLContextPrioritySupported();
  bool IsAndroidNativeFenceSyncSupported();
  bool IsANGLEExternalContextAndSurfaceSupported();

  bool Initialize(bool supports_angle,
                  std::vector<DisplayType> init_displays,
                  EGLDisplayPlatform native_display);
  bool Initialize(GLDisplay* other_display) override;
  void InitializeForTesting();
  bool InitializeExtensionSettings();

  std::unique_ptr<DisplayExtensionsEGL> ext;

#if BUILDFLAG(IS_APPLE)
#if __OBJC__
  bool CreateMetalSharedEvent(id<MTLSharedEvent>* shared_event_out,
                              uint64_t* signal_value_out);
  void WaitForMetalSharedEvent(id<MTLSharedEvent> shared_event,
                               uint64_t signal_value);
#endif  // __OBJC__

  // Call periodically to clean up resources.
  void CleanupTempEGLSyncObjects();

  // Call during Initialize/Shutdown to clean initialize/delete the objective C
  // shared event storage
  void InitMetalSharedEventStorage();
  void CleanupMetalSharedEventStorage();
#endif

 private:
  friend class GLDisplayManager<GLDisplayEGL>;
  friend class EGLApiTest;

  class EGLGpuSwitchingObserver final : public ui::GpuSwitchingObserver {
   public:
    explicit EGLGpuSwitchingObserver(EGLDisplay display);
    ~EGLGpuSwitchingObserver() override = default;
    void OnGpuSwitched(GpuPreference active_gpu_heuristic) override;

   private:
    EGLDisplay display_ = EGL_NO_DISPLAY;
  };

  GLDisplayEGL(uint64_t system_device_id, DisplayKey display_key);

  bool InitializeDisplay(bool supports_angle,
                         std::vector<DisplayType> init_displays,
                         EGLDisplayPlatform native_display,
                         gl::GLDisplayEGL* existing_display);
  void InitializeCommon(bool for_testing);

  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLDisplayPlatform native_display_ = EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);
  DisplayType display_type_ = DisplayType::DEFAULT;

  bool egl_surfaceless_context_supported_ = false;
  bool egl_context_priority_supported_ = false;
  bool egl_android_native_fence_sync_supported_ = false;

  std::unique_ptr<EGLGpuSwitchingObserver> gpu_switching_observer_;

#if BUILDFLAG(IS_APPLE)
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
#endif
};

}  // namespace gl

#endif  // UI_GL_GL_DISPLAY_H_
