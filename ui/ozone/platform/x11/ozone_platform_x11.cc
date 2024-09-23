// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/ozone_platform_x11.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/buildflags.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory_ozone.h"
#include "ui/base/x/x11_cursor_factory.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/linux/gpu_memory_buffer_support_x11.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/visual_manager.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/x11/gl_egl_utility_x11.h"
#include "ui/ozone/platform/x11/linux_ui_delegate_x11.h"
#include "ui/ozone/platform/x11/x11_clipboard_ozone.h"
#include "ui/ozone/platform/x11/x11_global_shortcut_listener_ozone.h"
#include "ui/ozone/platform/x11/x11_keyboard_hook.h"
#include "ui/ozone/platform/x11/x11_menu_utils.h"
#include "ui/ozone/platform/x11/x11_screen_ozone.h"
#include "ui/ozone/platform/x11/x11_surface_factory.h"
#include "ui/ozone/platform/x11/x11_user_input_monitor.h"
#include "ui/ozone/platform/x11/x11_utils.h"
#include "ui/ozone/platform/x11/x11_window.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/base/ime/ash/input_method_ash.h"
#else
#include "ui/base/ime/linux/input_method_auralinux.h"
#include "ui/ozone/platform/x11/os_exchange_data_provider_x11.h"
#endif

namespace ui {

namespace {

// Singleton OzonePlatform implementation for X11 platform.
class OzonePlatformX11 : public OzonePlatform,
                         public OSExchangeDataProviderFactoryOzone {
 public:
  OzonePlatformX11() { SetInstance(this); }

  OzonePlatformX11(const OzonePlatformX11&) = delete;
  OzonePlatformX11& operator=(const OzonePlatformX11&) = delete;

  ~OzonePlatformX11() override = default;

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }

  ui::OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactory* GetCursorFactory() override { return cursor_factory_.get(); }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    auto window = std::make_unique<X11Window>(delegate);
    window->Initialize(std::move(properties));
    return std::move(window);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return nullptr;
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    return std::make_unique<X11ScreenOzone>();
  }

  void InitScreen(PlatformScreen* screen) override {
    // InitScreen is always called with the same screen that CreateScreen
    // hands back, so it is safe to cast here.
    static_cast<X11ScreenOzone*>(screen)->Init();
  }

  PlatformClipboard* GetPlatformClipboard() override {
    return clipboard_.get();
  }

  PlatformGLEGLUtility* GetPlatformGLEGLUtility() override {
    if (!gl_egl_utility_)
      gl_egl_utility_ = std::make_unique<GLEGLUtilityX11>();
    return gl_egl_utility_.get();
  }

  std::unique_ptr<InputMethod> CreateInputMethod(
      ImeKeyEventDispatcher* ime_key_event_dispatcher,
      gfx::AcceleratedWidget) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return std::make_unique<ash::InputMethodAsh>(ime_key_event_dispatcher);
#else
    return std::make_unique<InputMethodAuraLinux>(ime_key_event_dispatcher);
#endif
  }

  PlatformMenuUtils* GetPlatformMenuUtils() override {
    return menu_utils_.get();
  }

  PlatformUtils* GetPlatformUtils() override { return x11_utils_.get(); }

  PlatformGlobalShortcutListener* GetPlatformGlobalShortcutListener(
      PlatformGlobalShortcutListenerDelegate* delegate) override {
    if (!global_shortcut_listener_) {
      global_shortcut_listener_ =
          std::make_unique<X11GlobalShortcutListenerOzone>(delegate);
    }
    return global_shortcut_listener_.get();
  }

  std::unique_ptr<PlatformKeyboardHook> CreateKeyboardHook(
      PlatformKeyboardHookTypes type,
      base::RepeatingCallback<void(KeyEvent* event)> callback,
      std::optional<base::flat_set<DomCode>> dom_codes,
      gfx::AcceleratedWidget accelerated_widget) override {
    switch (type) {
      case PlatformKeyboardHookTypes::kModifier:
        return std::make_unique<X11KeyboardHook>(
            std::move(dom_codes), std::move(callback), accelerated_widget);
      case PlatformKeyboardHookTypes::kMedia:
        return nullptr;
    }
  }

  std::unique_ptr<OSExchangeDataProvider> CreateProvider() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return std::make_unique<OSExchangeDataProviderNonBacked>();
#else
    return std::make_unique<OSExchangeDataProviderX11>();
#endif
  }

  const PlatformProperties& GetPlatformProperties() override {
    static base::NoDestructor<OzonePlatform::PlatformProperties> properties;
    static bool initialised = false;
    if (!initialised) {
      properties->custom_frame_pref_default = ui::GetCustomFramePrefDefault();

      // When the Ozone X11 backend is running, use a UI loop to grab Expose
      // events. See GLSurfaceGLX and https://crbug.com/326995.
      properties->message_pump_type_for_gpu = base::MessagePumpType::UI;
      // When the Ozone X11 backend is running, use a UI loop to dispatch
      // SHM completion events.
      properties->message_pump_type_for_viz_compositor =
          base::MessagePumpType::UI;
      properties->supports_vulkan_swap_chain = true;
      properties->skia_can_fall_back_to_x11 = true;
      properties->platform_shows_drag_image = false;
      properties->supports_global_application_menus = true;
      properties->app_modal_dialogs_use_event_blocker = true;
      properties->fetch_buffer_formats_for_gmb_on_gpu = true;

      initialised = true;
    }

    return *properties;
  }

  const PlatformRuntimeProperties& GetPlatformRuntimeProperties() override {
    static OzonePlatform::PlatformRuntimeProperties properties;

    if (has_initialized_gpu() &&
        ui::GpuMemoryBufferSupportX11::GetInstance()->has_gbm_device()) {
      // This property is set when the GetPlatformRuntimeProperties is
      // called on the gpu process side.
      properties.supports_native_pixmaps = true;
    }
    properties.supports_subwindows_as_accelerated_widgets = true;

    return properties;
  }

  bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                     gfx::BufferUsage usage) const override {
    // Native pixmap support is determined on gpu process via gpu extra info
    // that gets this information from GpuMemoryBufferSupportX11.
    return false;
  }

  bool IsWindowCompositingSupported() const override {
    return x11::Connection::Get()
        ->GetOrCreateVisualManager()
        .ArgbVisualAvailable();
  }

  bool InitializeUI(const InitParams& params) override {
    if (ShouldFailInitializeUIForTest()) {
      LOG(ERROR) << "Failing for test";
      return false;
    }
    // If opening the connection failed, we can not do anything.  The platform
    // cannot initialise.
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kHeadless) &&
        !x11::Connection::Get()->Ready()) {
      LOG(ERROR) << "Missing X server or $DISPLAY";
      return false;
    }

    InitializeCommon(params);
    CreatePlatformEventSource();
    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    clipboard_ = std::make_unique<X11ClipboardOzone>();
    cursor_factory_ = std::make_unique<X11CursorFactory>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    // TODO(crbug.com/41472924): Support XKB.
    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());

    TouchFactory::SetTouchDeviceListFromCommandLine();

#if BUILDFLAG(USE_GTK)
    linux_ui_delegate_ = std::make_unique<LinuxUiDelegateX11>();
#endif

    menu_utils_ = std::make_unique<X11MenuUtils>();
    x11_utils_ = std::make_unique<X11Utils>();

    base::UmaHistogramEnumeration("Linux.WindowManager", GetWindowManagerUMA());

    return true;
  }

  void InitializeGPU(const InitParams& params) override {
    InitializeCommon(params);
    if (params.enable_native_gpu_memory_buffers) {
      base::ThreadPool::PostTask(FROM_HERE, base::BindOnce([]() {
                                   ui::GpuMemoryBufferSupportX11::GetInstance();
                                 }));
    }
    // In single process mode either the UI thread will create an event source
    // or it's a test and an event source isn't desired.
    if (!params.single_process)
      CreatePlatformEventSource();

    // Set up the X11 connection before the sandbox gets set up.  This cannot be
    // done later since opening the connection requires socket() and connect().
    auto connection = x11::Connection::Get()->Clone();
    connection->DetachFromSequence();
    surface_factory_ozone_ =
        std::make_unique<X11SurfaceFactory>(std::move(connection));
  }

  void PostCreateMainMessageLoop(
      base::OnceCallback<void()> shutdown_cb,
      scoped_refptr<base::SingleThreadTaskRunner>) override {
    // Installs the X11 error handlers for the UI process after the
    // main message loop has started. This will allow us to exit cleanly
    // if X exits before we do.
    x11::Connection::Get()->SetIOErrorHandler(std::move(shutdown_cb));
  }

  std::unique_ptr<PlatformUserInputMonitor> GetPlatformUserInputMonitor(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      override {
    return std::make_unique<X11UserInputMonitor>(std::move(io_task_runner));
  }

 private:
  // Performs initialization steps need by both UI and GPU.
  void InitializeCommon(const InitParams& params) {
    if (common_initialized_)
      return;

    common_initialized_ = true;
  }

  // Creates |event_source_| if it doesn't already exist.
  void CreatePlatformEventSource() {
    if (event_source_)
      return;

    auto* connection = x11::Connection::Get();
    event_source_ = std::make_unique<X11EventSource>(connection);
  }

  bool common_initialized_ = false;

  // Objects in the UI process.
  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<X11ClipboardOzone> clipboard_;
  std::unique_ptr<CursorFactory> cursor_factory_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<X11MenuUtils> menu_utils_;
  std::unique_ptr<X11Utils> x11_utils_;
  std::unique_ptr<PlatformGlobalShortcutListener> global_shortcut_listener_;

  // Objects in the GPU process.
  std::unique_ptr<X11SurfaceFactory> surface_factory_ozone_;
  std::unique_ptr<GLEGLUtilityX11> gl_egl_utility_;

  // Objects in both UI and GPU process.
  std::unique_ptr<X11EventSource> event_source_;

#if BUILDFLAG(USE_GTK)
  std::unique_ptr<LinuxUiDelegate> linux_ui_delegate_;
#endif
};

}  // namespace

OzonePlatform* CreateOzonePlatformX11() {
  return new OzonePlatformX11;
}

}  // namespace ui
