// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/ozone_platform_headless.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/common/bitmap_cursor_factory.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/headless/headless_screen.h"
#include "ui/ozone/platform/headless/headless_surface_factory.h"
#include "ui/ozone/platform/headless/headless_window.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/stub_input_controller.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "ui/base/ime/fuchsia/input_method_fuchsia.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include <algorithm>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/platform_gl_egl_utility.h"

// From ANGLE's egl/eglext.h. Follows the same approach as in
// ui/ozone/platform/wayland/gpu/wayland_gl_egl_utility.cc.
#ifndef EGL_ANGLE_platform_angle_device_type_swiftshader
#define EGL_ANGLE_platform_angle_device_type_swiftshader
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE 0x3487
#endif

#ifndef EGL_ANGLE_platform_angle
#define EGL_ANGLE_platform_angle 1
#define EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE 0x348F
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE 0x3209
#endif

#ifndef EGL_KHR_platform_gbm
#define EGL_KHR_platform_gbm 1
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif

#ifndef EGL_ANGLE_platform_angle_vulkan
#define EGL_ANGLE_platform_angle_vulkan 1
#define EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE 0x3450
#define EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE 0x34A5
#endif

#ifndef EGL_ANGLE_platform_angle_device_type_egl_angle
#define EGL_ANGLE_platform_angle_device_type_egl_angle
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE 0x348E
#endif

#ifndef EGL_ANGLE_platform_angle_opengl
#define EGL_ANGLE_platform_angle_opengl 1
#define EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE 0x320D
#define EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE 0x320E
#endif
#endif  // BUILDFLAG(IS_LINUX)

namespace ui {

namespace {

// A headless implementation of PlatformEventSource that we can instantiate to
// make
// sure that the PlatformEventSource has an instance while in unit tests.
class HeadlessPlatformEventSource : public PlatformEventSource {
 public:
  HeadlessPlatformEventSource() = default;

  HeadlessPlatformEventSource(const HeadlessPlatformEventSource&) = delete;
  HeadlessPlatformEventSource& operator=(const HeadlessPlatformEventSource&) =
      delete;

  ~HeadlessPlatformEventSource() override = default;
};

#if BUILDFLAG(IS_LINUX)
class HeadlessGLEGLUtility : public PlatformGLEGLUtility {
 public:
  explicit HeadlessGLEGLUtility(HeadlessSurfaceFactory* surface_factory)
      : surface_factory_(surface_factory) {}

  HeadlessGLEGLUtility(const HeadlessGLEGLUtility&) = delete;
  HeadlessGLEGLUtility& operator=(const HeadlessGLEGLUtility&) = delete;

  ~HeadlessGLEGLUtility() override = default;

  // PlatformGLEGLUtility:
  void GetAdditionalEGLAttributes(
      EGLenum platform_type,
      std::vector<EGLAttrib>* display_attributes) override {
    if (!surface_factory_ || !surface_factory_->GetNativeDisplay().Valid()) {
      return;
    }
    if (std::ranges::contains(
            *display_attributes,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE) ||
        platform_type == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE) {
      display_attributes->push_back(
          EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE);
      display_attributes->push_back(
          EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE);
      return;
    }
    if (platform_type == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE ||
        platform_type == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE) {
      display_attributes->push_back(
          EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE);
      display_attributes->push_back(EGL_PLATFORM_GBM_KHR);
      if (!std::ranges::contains(*display_attributes,
                                 EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE)) {
        display_attributes->push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        display_attributes->push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE);
      }
    }
  }

  void ChooseEGLAlphaAndBufferSize(EGLint* alpha_size,
                                   EGLint* buffer_size) override {}

 private:
  raw_ptr<HeadlessSurfaceFactory> surface_factory_;
};

class HeadlessOverlayCandidates : public OverlayCandidatesOzone {
 public:
  HeadlessOverlayCandidates() = default;

  HeadlessOverlayCandidates(const HeadlessOverlayCandidates&) = delete;
  HeadlessOverlayCandidates& operator=(const HeadlessOverlayCandidates&) =
      delete;

  ~HeadlessOverlayCandidates() override = default;

  // OverlayCandidatesOzone:
  void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces) override {}
};

class HeadlessOverlayManager : public OverlayManagerOzone {
 public:
  HeadlessOverlayManager() = default;

  HeadlessOverlayManager(const HeadlessOverlayManager&) = delete;
  HeadlessOverlayManager& operator=(const HeadlessOverlayManager&) = delete;

  ~HeadlessOverlayManager() override = default;

  // OverlayManagerOzone:
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget widget) override {
    return std::make_unique<HeadlessOverlayCandidates>();
  }
};
#endif  // BUILDFLAG(IS_LINUX)

// OzonePlatform for headless mode.
class OzonePlatformHeadlessImpl : public OzonePlatformHeadless {
 public:
  explicit OzonePlatformHeadlessImpl(const base::FilePath& dump_file)
      : file_path_(dump_file) {}

  OzonePlatformHeadlessImpl(const OzonePlatformHeadlessImpl&) = delete;
  OzonePlatformHeadlessImpl& operator=(const OzonePlatformHeadlessImpl&) =
      delete;

  ~OzonePlatformHeadlessImpl() override = default;

  // OzonePlatformHeadless:
  HeadlessWindowManager* GetHeadlessWindowManager() override {
    return window_manager_.get();
  }

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }
  CursorFactory* GetCursorFactory() override { return cursor_factory_.get(); }
  InputController* GetInputController() override {
    return input_controller_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  const PlatformRuntimeProperties& GetPlatformRuntimeProperties() override {
    static OzonePlatform::PlatformRuntimeProperties properties;
    properties.supports_native_pixmaps =
        has_initialized_gpu() && surface_factory_ &&
        surface_factory_->SupportsNativePixmaps();
    return properties;
  }
  bool IsNativePixmapConfigSupported(viz::SharedImageFormat format,
                                     gfx::BufferUsage usage) const override {
#if BUILDFLAG(IS_LINUX)
    if (!has_initialized_gpu() || !surface_factory_) {
      return false;
    }
    return surface_factory_->IsNativePixmapConfigSupported(format, usage);
#else
    return false;
#endif
  }
#if BUILDFLAG(IS_LINUX)
  PlatformGLEGLUtility* GetPlatformGLEGLUtility() override {
    if (!surface_factory_) {
      return nullptr;
    }
    if (!gl_egl_utility_) {
      gl_egl_utility_ =
          std::make_unique<HeadlessGLEGLUtility>(surface_factory_.get());
    }
    return gl_egl_utility_.get();
  }
#endif
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;  // no input injection support.
  }
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    return std::make_unique<HeadlessWindow>(delegate, window_manager_.get(),
                                            properties.bounds);
  }
  bool IsWindowCompositingSupported() const override { return true; }
  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return nullptr;
  }
  std::unique_ptr<PlatformScreen> CreateScreen() override {
    return std::make_unique<HeadlessScreen>();
  }
  void InitScreen(PlatformScreen* screen) override {}
  std::unique_ptr<InputMethod> CreateInputMethod(
      ImeKeyEventDispatcher* ime_key_event_dispatcher,
      gfx::AcceleratedWidget widget) override {
    return std::make_unique<InputMethodMinimal>(ime_key_event_dispatcher);
  }

// Desktop Linux, not CastOS.
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
  const PlatformProperties& GetPlatformProperties() override {
    static base::NoDestructor<OzonePlatform::PlatformProperties> properties;
    static bool initialized = false;
    if (!initialized) {
      initialized = true;
    }
    return *properties;
  }
#endif

  bool InitializeUI(const InitParams& params) override {
    window_manager_ = std::make_unique<HeadlessWindowManager>();
    surface_factory_ = std::make_unique<HeadlessSurfaceFactory>(file_path_);
    // This unbreaks tests that create their own.
    if (!PlatformEventSource::GetInstance())
      platform_event_source_ = std::make_unique<HeadlessPlatformEventSource>();
    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());

    if (!overlay_manager_) {
      overlay_manager_ = std::make_unique<StubOverlayManager>();
    }
    input_controller_ = std::make_unique<StubInputController>();
    cursor_factory_ = std::make_unique<BitmapCursorFactory>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    return true;
  }

  void InitializeGPU(const InitParams& params) override {
    if (!surface_factory_)
      surface_factory_ = std::make_unique<HeadlessSurfaceFactory>(file_path_);
#if BUILDFLAG(IS_LINUX)
    surface_factory_->InitializeGbm();
    overlay_manager_ = std::make_unique<HeadlessOverlayManager>();
#endif
  }

 private:
  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<HeadlessWindowManager> window_manager_;
  std::unique_ptr<HeadlessSurfaceFactory> surface_factory_;
  std::unique_ptr<PlatformEventSource> platform_event_source_;
  std::unique_ptr<CursorFactory> cursor_factory_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<PlatformGLEGLUtility> gl_egl_utility_;
#endif
  base::FilePath file_path_;
};

}  // namespace

OzonePlatform* CreateOzonePlatformHeadless() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  base::FilePath location;
  if (cmd->HasSwitch(switches::kOzoneDumpFile))
    location = cmd->GetSwitchValuePath(switches::kOzoneDumpFile);
  return new OzonePlatformHeadlessImpl(location);
}

}  // namespace ui
