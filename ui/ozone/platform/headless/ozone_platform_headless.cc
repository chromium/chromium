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
#include "build/chromeos_buildflags.h"
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
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "ui/base/ime/fuchsia/input_method_fuchsia.h"
#endif

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

// OzonePlatform for headless mode
class OzonePlatformHeadless : public OzonePlatform {
 public:
  explicit OzonePlatformHeadless(const base::FilePath& dump_file)
      : file_path_(dump_file) {}

  OzonePlatformHeadless(const OzonePlatformHeadless&) = delete;
  OzonePlatformHeadless& operator=(const OzonePlatformHeadless&) = delete;

  ~OzonePlatformHeadless() override = default;

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

    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    cursor_factory_ = std::make_unique<BitmapCursorFactory>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    return true;
  }

  void InitializeGPU(const InitParams& params) override {
    if (!surface_factory_)
      surface_factory_ = std::make_unique<HeadlessSurfaceFactory>(file_path_);
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
  base::FilePath file_path_;
};

}  // namespace

OzonePlatform* CreateOzonePlatformHeadless() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  base::FilePath location;
  if (cmd->HasSwitch(switches::kOzoneDumpFile))
    location = cmd->GetSwitchValuePath(switches::kOzoneDumpFile);
  return new OzonePlatformHeadless(location);
}

}  // namespace ui
