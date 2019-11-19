// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/ozone_platform_x11.h"

#include <memory>
#include <utility>

#include "base/message_loop/message_pump_type.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/fake/fake_display_delegate.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/platform/x11/x11_event_source_default.h"
#include "ui/gfx/x/x11.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/x11/x11_clipboard_ozone.h"
#include "ui/ozone/platform/x11/x11_cursor_factory_ozone.h"
#include "ui/ozone/platform/x11/x11_screen_ozone.h"
#include "ui/ozone/platform/x11/x11_surface_factory.h"
#include "ui/ozone/platform/x11/x11_window_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_base.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/input_method_chromeos.h"
#else
#include "ui/base/ime/linux/input_method_auralinux.h"
#endif

namespace ui {

namespace {

constexpr OzonePlatform::PlatformProperties kX11PlatformProperties{
    /*needs_view_token=*/false,
    /*custom_frame_pref_default=*/false,
    /*use_system_title_bar=*/true,
    /*requires_mojo=*/false,

    // When the Ozone X11 backend is running, use a UI loop to grab Expose
    // events. See GLSurfaceGLX and https://crbug.com/326995.
    /*message_pump_type_for_gpu=*/base::MessagePumpType::UI};

// Singleton OzonePlatform implementation for X11 platform.
class OzonePlatformX11 : public OzonePlatform {
 public:
  OzonePlatformX11() {}

  ~OzonePlatformX11() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }

  ui::OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<PlatformWindowBase> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    std::unique_ptr<X11WindowOzone> window =
        std::make_unique<X11WindowOzone>(delegate);
    window->Initialize(std::move(properties));
    window->SetTitle(base::ASCIIToUTF16("Ozone X11"));
    return std::move(window);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return std::make_unique<display::FakeDisplayDelegate>();
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    auto screen = std::make_unique<X11ScreenOzone>();
    screen->Init();
    return screen;
  }

  PlatformClipboard* GetPlatformClipboard() override {
    return clipboard_.get();
  }

  std::unique_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate) override {
#if defined(OS_CHROMEOS)
    return std::make_unique<InputMethodChromeOS>(delegate);
#else
    return std::make_unique<InputMethodAuraLinux>(delegate);
#endif
  }

  const PlatformProperties& GetPlatformProperties() override {
    return kX11PlatformProperties;
  }

  void InitializeUI(const InitParams& params) override {
    InitializeCommon(params);
    CreatePlatformEventSource();
    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    clipboard_ = std::make_unique<X11ClipboardOzone>();
    cursor_factory_ozone_ = std::make_unique<X11CursorFactoryOzone>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    TouchFactory::SetTouchDeviceListFromCommandLine();
  }

  void InitializeGPU(const InitParams& params) override {
    InitializeCommon(params);

    // In single process mode either the UI thread will create an event source
    // or it's a test and an event source isn't desired.
    if (!params.single_process)
      CreatePlatformEventSource();

    surface_factory_ozone_ = std::make_unique<X11SurfaceFactory>();
  }

 private:
  // Performs initialization steps need by both UI and GPU.
  void InitializeCommon(const InitParams& params) {
    if (common_initialized_)
      return;

    // Always initialize in multi-thread mode, since this is used only during
    // development.
    XInitThreads();

    // If XOpenDisplay() failed there is nothing we can do. Crash here instead
    // of crashing later. If you are crashing here, make sure there is an X
    // server running and $DISPLAY is set.
    CHECK(gfx::GetXDisplay()) << "Missing X server or $DISPLAY";

    ui::SetDefaultX11ErrorHandlers();

    common_initialized_ = true;
  }

  // Creates |event_source_| if it doesn't already exist.
  void CreatePlatformEventSource() {
    if (event_source_)
      return;

    XDisplay* display = gfx::GetXDisplay();
    event_source_ = std::make_unique<X11EventSourceDefault>(display);
  }

  bool common_initialized_ = false;

  // Objects in the UI process.
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<X11ClipboardOzone> clipboard_;
  std::unique_ptr<X11CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

  // Objects in the GPU process.
  std::unique_ptr<X11SurfaceFactory> surface_factory_ozone_;

  // Objects in both UI and GPU process.
  std::unique_ptr<X11EventSourceDefault> event_source_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformX11);
};

}  // namespace

OzonePlatform* CreateOzonePlatformX11() {
  return new OzonePlatformX11;
}

}  // namespace ui
