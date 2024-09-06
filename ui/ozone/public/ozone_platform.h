// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
#define UI_OZONE_PUBLIC_OZONE_PLATFORM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace display {
class NativeDisplayDelegate;
}

namespace ui {
enum class DomCode : uint32_t;
enum class PlatformKeyboardHookTypes;

class CursorFactory;
class GpuPlatformSupportHost;
class ImeKeyEventDispatcher;
class InputMethod;
class InputController;
class KeyEvent;
class OverlayManagerOzone;
class PlatformClipboard;
class PlatformGLEGLUtility;
class PlatformGlobalShortcutListener;
class PlatformGlobalShortcutListenerDelegate;
class PlatformKeyboardHook;
class PlatformMenuUtils;
class PlatformScreen;
class PlatformUserInputMonitor;
class PlatformUtils;
class SurfaceFactoryOzone;
class SystemInputInjector;

struct PlatformWindowInitProperties;

// Base class for Ozone platform implementations.
//
// Ozone platforms must override this class and implement the virtual
// GetFooFactoryOzone() methods to provide implementations of the
// various ozone interfaces.
//
// The OzonePlatform subclass can own any state needed by the
// implementation that is shared between the various ozone interfaces,
// such as a connection to the windowing system.
//
// A platform is free to use different implementations of each
// interface depending on the context. You can, for example, create
// different objects depending on the underlying hardware, command
// line flags, or whatever is appropriate for the platform.
class COMPONENT_EXPORT(OZONE) OzonePlatform {
 public:
  OzonePlatform();

  OzonePlatform(const OzonePlatform&) = delete;
  OzonePlatform& operator=(const OzonePlatform&) = delete;

  virtual ~OzonePlatform();

  // Additional initialization params for the platform. Platforms must not
  // retain a reference to this structure.
  struct InitParams {
    // Setting this to true indicates that the platform implementation should
    // operate as a single process for platforms (i.e. drm) that are usually
    // split between a host and viz specific portion.
    bool single_process = false;

    // Setting this to true indicates the the platform can do additional
    // initialization for the GpuMemoryBuffer framework.
    bool enable_native_gpu_memory_buffers = false;

    // The direct VideoDecoder is disallowed on some particular SoC/platforms.
    // This flag is a reflection of whatever the ChromeOS command line builder
    // says. If false, overlay manager will not use synchronous pageflip
    // testing with real buffer.
    // TODO(fangzhoug): Some Chrome OS boards still use the legacy video
    // decoder. Remove this once ChromeOSVideoDecoder is on everywhere.
    bool allow_sync_and_real_buffer_page_flip_testing = false;

    // TODO(b/331237773): Unfortunately, the kHandleOverlaysSwapFailure feature
    // cannot be checked by the overlay manager in ozone/drm directly as it
    // creates a circular dependency that gn complains about. That's why this
    // control bool is here. Remove this once kHandleOverlaysSwapFailure is
    // removed and DrmOverlayManager is always handling swap failures.
    bool handle_overlays_swap_failure = false;
  };

  // Struct used to indicate platform properties.
  struct PlatformProperties {
    PlatformProperties();
    PlatformProperties(const PlatformProperties& other) = delete;
    PlatformProperties& operator=(const PlatformProperties& other) = delete;
    ~PlatformProperties();

    // Determines whether we should default to native decorations or the custom
    // frame based on the currently-running window manager.
    bool custom_frame_pref_default = false;

    // Determines the type of message pump that should be used for GPU main
    // thread.
    base::MessagePumpType message_pump_type_for_gpu =
        base::MessagePumpType::DEFAULT;

    // Determines the type of message pump that should be used for viz
    // compositor thread.
    base::MessagePumpType message_pump_type_for_viz_compositor =
        base::MessagePumpType::DEFAULT;

    // Determines if the platform supports vulkan swap chain.
    bool supports_vulkan_swap_chain = false;

    // Linux only: determines if Skia can fall back to the X11 output device.
    bool skia_can_fall_back_to_x11 = false;

    // Wayland only: determines whether windows which are not top level ones
    // should be given parents explicitly.
    bool set_parent_for_non_top_level_windows = false;

    // If true, the platform shows and updates the drag image.
    bool platform_shows_drag_image = true;

    // Linux only, but see a TODO in BrowserDesktopWindowTreeHostLinux.
    // Determines whether the platform supports the global application menu.
    bool supports_global_application_menus = false;

    // Determines if the application modal dialogs should use the event blocker
    // to allow the only browser window receiving UI events.
    bool app_modal_dialogs_use_event_blocker = false;

    // Determines whether buffer formats should be fetched on GPU and passed
    // back via gpu extra info.
    bool fetch_buffer_formats_for_gmb_on_gpu = false;

    // Indicates that the platform allows client applications to manipulate
    // global screen coordinates. Wayland, for example, disallow it by design.
    bool supports_global_screen_coordinates = true;

    // Whether the platform supports system/shell integrated color picker
    // dialog. An example is XDG Desktop Portal provided PickColor dialog.
    bool supports_color_picker_dialog = true;
  };

  // Groups platform properties that can only be known at run time.
  struct PlatformRuntimeProperties {
    PlatformRuntimeProperties();

    // Values to override the value of a property in tests.
    enum class SupportsForTest {
      kNotSet,  // The property is not overridden.
      kYes,     // The platform should return true.
      kNo,      // The plafrorm should return false.
    };

    // Whether the underlying platform supports deferring compositing of buffers
    // via overlays. If overlays are not supported the promotion and validation
    // logic can be skipped.
    bool supports_overlays = false;
    // Indicates whether the platform supports server-side window decorations.
    bool supports_server_side_window_decorations = true;

    // For platforms that have optional support for server-side decorations,
    // this parameter allows setting the desired state in tests.  The platform
    // must have the appropriate logic in its GetPlatformRuntimeProperties()
    // method.
    static SupportsForTest override_supports_ssd_for_test;

    // Wayland only: determines whether solid color overlays can be delegated
    // without a backing image via a wayland protocol.
    bool supports_non_backed_solid_color_buffers = false;

    // Wayland only: determines whether single pixel buffer protocol is
    // supported.
    bool supports_single_pixel_buffer = false;

    // Indicates whether the platform supports native pixmaps.
    bool supports_native_pixmaps = false;

    // Wayland only: determines whether BufferQueue needs a background image to
    // be stacked below an AcceleratedWidget to make a widget opaque.
    bool needs_background_image = false;

    // Wayland only: determines whether clip rects can be delegated via the
    // wayland protocol when no quad is out of window.
    bool supports_clip_rect = false;

    // Wayland only: determine whether toplevel surfaces can be activated and
    // deactivated.
    bool supports_activation = false;

    // Wayland only: determines whether non axis-aligned 2d transforms can be
    // delegated via the wayland protocol.
    bool supports_affine_transform = false;

    // Wayland only: determines whether clip rects can be delegated via the
    // wayland protocol when some quads are out of window.
    // TODO(crbug.com/40277728): The flag is currently disabled by default since
    // there is a bug. Set this flag to enabled in GPU process when the
    // remaining issues are resolved.
    bool supports_out_of_window_clip_rect = false;

    // Wayland only: whether wayland server has the fix that applies
    // transformations in the correct order.
    bool has_transformation_fix = false;

    // Wayland only: whether bubble widgets can use platform objects.
    bool supports_subwindows_as_accelerated_widgets = false;

    // Indicates whether the platform supports system-controlled per-window
    // scaling.
    bool supports_per_window_scaling = false;

    // Allows overriding whether per window scaling is enabled in tests.
    static SupportsForTest override_supports_per_window_scaling_for_test;
  };

  // Corresponds to chrome_browser_main_extra_parts.h.
  //
  // The browser process' initialization involves several steps -
  // PreEarlyInitialization, PostCreateMainMessageLoop, PostMainMessageLoopRun,
  // etc. In order to be consistent with that and allow platform specific
  // initialization steps, the OzonePlatform has three methods - one static
  // PreEarlyInitialization that is expected to do some early non-ui
  // initialization (like error handlers that X11 sets), and two non-static
  // methods - PostMainmessageLoopStart and PostMainMessageLoopRun. The latter
  // two are supposed to be called on a post start and a post-run of the
  // MessageLoop. Please note that this methods must be run on the browser' UI
  // thread.
  //
  // Creates OzonePlatform and does pre-early initialization (internally, sets
  // error handlers if supported so that we can print errors during the browser
  // process' start up).
  static void PreEarlyInitialization();
  // Sets error handlers if supported for the browser process, and provides a
  // task_runner suitable for handling user input after the message loop
  // started. It's required to call this so that we can exit cleanly if the
  // server can exit before we do.
  virtual void PostCreateMainMessageLoop(
      base::OnceCallback<void()> shutdown_cb,
      scoped_refptr<base::SingleThreadTaskRunner> user_input_task_runner);
  // Resets the error handlers if set.
  virtual void PostMainMessageLoopRun();

  // Initializes the subsystems/resources necessary for the UI process (e.g.
  // events) with additional properties to customize the ozone platform
  // implementation. Ozone will not retain InitParams after returning from
  // InitializeForUI.
  // Returns whether the initialisation completed successfully.  Should this
  // have returned false, the browser must stop the startup and exit because it
  // would not be able to work normally.
  static bool InitializeForUI(const InitParams& args);

  // Initializes the subsystems for rendering but with additional properties
  // provided by |args| as with InitalizeForUI.
  static void InitializeForGPU(const InitParams& args);

  static OzonePlatform* GetInstance();

  static bool IsInitialized();

  // Returns the current ozone platform name.
  // Some tests may skip based on the platform name.
  static std::string GetPlatformNameForTest();

  // Factory getters to override in subclasses. The returned objects will be
  // injected into the appropriate layer at startup. Subclasses should not
  // inject these objects themselves. Ownership is retained by OzonePlatform.
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() = 0;
  virtual ui::OverlayManagerOzone* GetOverlayManager() = 0;
  virtual ui::CursorFactory* GetCursorFactory() = 0;
  virtual ui::InputController* GetInputController() = 0;
  virtual ui::GpuPlatformSupportHost* GetGpuPlatformSupportHost() = 0;
  virtual std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() = 0;
  virtual std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) = 0;
  virtual std::unique_ptr<display::NativeDisplayDelegate>
  CreateNativeDisplayDelegate() = 0;
  // Creates a new PlatformScreen subclass from the factory subclass.
  virtual std::unique_ptr<PlatformScreen> CreateScreen() = 0;
  // This function must be called immediately after CreateScreen with the
  // `screen` that was returned from CreateScreen.  They are separated to avoid
  // observer recursion into display::Screen from inside CreateScreen.
  virtual void InitScreen(PlatformScreen* screen) = 0;
  virtual PlatformClipboard* GetPlatformClipboard();
  virtual std::unique_ptr<InputMethod> CreateInputMethod(
      ImeKeyEventDispatcher* ime_key_event_dispatcher,
      gfx::AcceleratedWidget widget) = 0;
  virtual PlatformGLEGLUtility* GetPlatformGLEGLUtility();
  virtual PlatformMenuUtils* GetPlatformMenuUtils();
  virtual PlatformUtils* GetPlatformUtils();
  virtual PlatformGlobalShortcutListener* GetPlatformGlobalShortcutListener(
      PlatformGlobalShortcutListenerDelegate* delegate);
  // Returns the keyboard hook that captures the specified keys.  See more in
  // ui::KeyboardHook.  However, unlike that interface, Ozone tries to register
  // the hook that it has created, and returns the one only if it was registered
  // successfully.
  // Handles creating both modifier and media keyboard hooks.  |dom_codes| and
  // |accelerated_widget| are only used if |type| is
  // PlatformKeyboardHookTypes::kModifier.
  virtual std::unique_ptr<PlatformKeyboardHook> CreateKeyboardHook(
      PlatformKeyboardHookTypes type,
      base::RepeatingCallback<void(KeyEvent* event)> callback,
      std::optional<base::flat_set<DomCode>> dom_codes,
      gfx::AcceleratedWidget accelerated_widget);

  // Returns true if the specified buffer format is supported.
  virtual bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                             gfx::BufferUsage usage) const;

  // Whether the platform supports compositing windows with transparency.
  virtual bool IsWindowCompositingSupported() const = 0;

  // Returns whether a custom frame should be used for windows.
  // The default behaviour is returning what is suggested by the
  // custom_frame_pref_default property of the platform: if the platform
  // suggests using the custom frame, likely it lacks native decorations.
  // See https://crbug.com/1212555
  virtual bool ShouldUseCustomFrame();

  // Returns a struct that contains configuration and requirements for the
  // current platform implementation. This can be called from either host or GPU
  // process at any time.
  virtual const PlatformProperties& GetPlatformProperties();

  // Returns runtime properties of the current platform implementation available
  // after either InitializeUI() or InitializeGPU() runs. Runtime properties for
  // UI and GPU may be different depending on availability of platform objects.
  virtual const PlatformRuntimeProperties& GetPlatformRuntimeProperties();

  // Ozone platform implementations may also choose to expose mojo interfaces to
  // internal functionality. Embedders wishing to take advantage of ozone mojo
  // implementations must invoke AddInterfaces with a valid mojo::BinderMap
  // pointer to export all Mojo interfaces defined within Ozone.
  //
  // Requests arriving before they can be immediately handled will be queued and
  // executed later.
  //
  // A default do-nothing implementation is provided to permit platform
  // implementations to opt out of implementing any Mojo interfaces.
  virtual void AddInterfaces(mojo::BinderMap* binders);

  // The GPU-specific portion of Ozone would typically run in a sandboxed
  // process for additional security. Some startup might need to wait until
  // after the sandbox has been configured.
  // When the GPU is in a separate process, the embedder should call this method
  // after it has configured (or failed to configure) the sandbox so that the
  // GPU-side setup is completed. If the GPU is in-process, there is no
  // sandboxing and the embedder should not call this method.
  // A default do-nothing implementation is provided to permit platform
  // implementations to ignore sandboxing and any associated launch ordering
  // issues.
  virtual void AfterSandboxEntry();

  // Creates a user input monitor.
  // The user input comes from I/O devices and must be handled on the IO thread.
  // |io_task_runner| must be bound to the IO thread so the implementation could
  // ensure that calls happen on the right thread.
  virtual std::unique_ptr<PlatformUserInputMonitor> GetPlatformUserInputMonitor(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  virtual void DumpState(std::ostream& out) const {}

 protected:
  bool has_initialized_ui() const { return initialized_ui_; }
  bool has_initialized_gpu() const { return initialized_gpu_; }

  bool single_process() const { return single_process_; }

  static bool ShouldFailInitializeUIForTest();

 private:
  friend class OzonePlatformTest;

  // For platforms that may fail at the early stage of initialising, sets so
  // that they fail.
  // See https://crbug.com/1280138.
  static void SetFailInitializeUIForTest(bool fail);

  // Optional method for pre-early initialization. In case of X11, sets X11
  // error handlers so that errors can be caught if early initialization fails.
  virtual void PreEarlyInitialize();

  // Initialises the platform in the UI process.  Returns whether that completed
  // successfully, i. e., the startup process may proceed further.
  // The platform implementation must check all conditions critical for normal
  // operation, and return false if any of them are not met (e. g., the display
  // server is not available).
  virtual bool InitializeUI(const InitParams& params) = 0;
  virtual void InitializeGPU(const InitParams& params) = 0;

  bool initialized_ui_ = false;
  bool initialized_gpu_ = false;
  bool prearly_initialized_ = false;

  // This value is checked on multiple threads. Declaring it volatile makes
  // modifications to |single_process_| visible by other threads. Mutex is not
  // needed since it's set before other threads are started.
  volatile bool single_process_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
