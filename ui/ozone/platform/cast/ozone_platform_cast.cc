// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/ozone_platform_cast.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/public/cast_egl_platform.h"
#include "chromecast/public/cast_egl_platform_shlib.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/cast/overlay_manager_cast.h"
#include "ui/ozone/platform/cast/platform_window_cast.h"
#include "ui/ozone/platform/cast/surface_factory_cast.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_screen.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_init_properties.h"

using chromecast::CastEglPlatform;

namespace ui {
namespace {

// Ozone platform implementation for Cast.  Implements functionality
// common to all Cast implementations:
//  - Always one window with window size equal to display size
//  - No input, cursor support
//  - Relinquish GPU resources flow for switching to external applications
// Meanwhile, platform-specific implementation details are abstracted out
// to the CastEglPlatform interface.
class OzonePlatformCast : public OzonePlatform {
 public:
  explicit OzonePlatformCast(std::unique_ptr<CastEglPlatform> egl_platform)
      : egl_platform_(std::move(egl_platform)) {}

  OzonePlatformCast(const OzonePlatformCast&) = delete;
  OzonePlatformCast& operator=(const OzonePlatformCast&) = delete;

  ~OzonePlatformCast() override {}

  // OzonePlatform implementation:
  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    if (!surface_factory_) {
      // The browser process is trying to create a surface (only the GPU
      // process should try to do that) for falling back on software
      // rendering because it failed to create a channel to the GPU process.
      // Returning a null pointer will crash via a null-pointer dereference,
      // so instead perform a controlled crash.

      // TODO(servolk): Odroid EGL implementation says there are no valid
      // configs when HDMI is not connected. This command-line switch will allow
      // us to avoid crashes in this situation and work in headless mode when
      // HDMI is disconnected. But this means that graphics won't work later, if
      // HDMI is reconnected, until the device is rebooted. We'll need to look
      // into better way to handle dynamic GPU process restarts on HDMI
      // connect/disconnect events.
      bool allow_dummy_software_rendering =
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              "allow-dummy-software-rendering");
      if (allow_dummy_software_rendering) {
        LOG(INFO) << "Using dummy SurfaceFactoryCast";
        surface_factory_ = std::make_unique<SurfaceFactoryCast>();
        return surface_factory_.get();
      }

      LOG(FATAL) << "Unable to create a GPU graphics context, and Cast doesn't "
                    "support software compositing.";
    }
    return surface_factory_.get();
  }
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }
  CursorFactory* GetCursorFactory() override { return cursor_factory_.get(); }
  InputController* GetInputController() override {
    return event_factory_ozone_->input_controller();
  }
  std::unique_ptr<PlatformScreen> CreateScreen() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  void InitScreen(PlatformScreen* screen) override {
    NOTREACHED_IN_MIGRATION();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return event_factory_ozone_->CreateSystemInputInjector();
  }
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    return std::make_unique<PlatformWindowCast>(delegate, properties.bounds);
  }
  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    // On Cast platform the display is initialized by low-level non-Ozone code.
    return nullptr;
  }
  std::unique_ptr<InputMethod> CreateInputMethod(
      ImeKeyEventDispatcher* ime_key_event_dispatcher,
      gfx::AcceleratedWidget) override {
    return std::make_unique<InputMethodMinimal>(ime_key_event_dispatcher);
  }

  bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                     gfx::BufferUsage usage) const override {
    return format == gfx::BufferFormat::BGRA_8888 &&
           usage == gfx::BufferUsage::SCANOUT;
  }

  bool IsWindowCompositingSupported() const override { return true; }

  bool InitializeUI(const InitParams& params) override {
    device_manager_ = CreateDeviceManager();
    cursor_factory_ = std::make_unique<CursorFactory>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    // Enable dummy software rendering support if GPU process disabled
    // or if we're an audio-only build.
    // Note: switch is kDisableGpu from content/public/common/content_switches.h
    bool enable_dummy_software_rendering = true;
#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
    enable_dummy_software_rendering =
        base::CommandLine::ForCurrentProcess()->HasSwitch("disable-gpu");
#endif  // BUILDFLAG(IS_CAST_AUDIO_ONLY)

    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());

    event_factory_ozone_ = std::make_unique<EventFactoryEvdev>(
        nullptr, device_manager_.get(),
        KeyboardLayoutEngineManager::GetKeyboardLayoutEngine());

    if (enable_dummy_software_rendering)
      surface_factory_ = std::make_unique<SurfaceFactoryCast>();

    return true;
  }

  void InitializeGPU(const InitParams& params) override {
    overlay_manager_ = std::make_unique<OverlayManagerCast>();
    surface_factory_ =
        std::make_unique<SurfaceFactoryCast>(std::move(egl_platform_));
  }

  void PostCreateMainMessageLoop(base::OnceCallback<void()> shutdown_cb,
                                 scoped_refptr<base::SingleThreadTaskRunner>
                                     user_input_task_runner) override {
    event_factory_ozone_->SetUserInputTaskRunner(
        std::move(user_input_task_runner));
  }

 private:
  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<DeviceManager> device_manager_;
  std::unique_ptr<CastEglPlatform> egl_platform_;
  std::unique_ptr<SurfaceFactoryCast> surface_factory_;
  std::unique_ptr<CursorFactory> cursor_factory_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<EventFactoryEvdev> event_factory_ozone_;
};

}  // namespace

OzonePlatform* CreateOzonePlatformCast() {
  const std::vector<std::string>& argv =
      base::CommandLine::ForCurrentProcess()->argv();
  std::unique_ptr<chromecast::CastEglPlatform> platform(
      chromecast::CastEglPlatformShlib::Create(argv));
  return new OzonePlatformCast(std::move(platform));
}

}  // namespace ui
