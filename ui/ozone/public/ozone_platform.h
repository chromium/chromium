// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
#define UI_OZONE_PUBLIC_OZONE_PLATFORM_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace display {
class NativeDisplayDelegate;
}

namespace ui {
class CursorFactory;
class InputController;
class GpuPlatformSupportHost;
class OverlayManagerOzone;
class PlatformScreen;
class SurfaceFactoryOzone;
class SystemInputInjector;
class PlatformClipboard;
class PlatformGLEGLUtility;

namespace internal {
class InputMethodDelegate;
}  // namespace internal
class InputMethod;

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
  virtual ~OzonePlatform();

  // Additional initialization params for the platform. Platforms must not
  // retain a reference to this structure.
  struct InitParams {
    // Setting this to true indicates that the platform implementation should
    // operate as a single process for platforms (i.e. drm) that are usually
    // split between a host and viz specific portion.
    bool single_process = false;
  };

  // Struct used to indicate platform properties.
  struct PlatformProperties {
    PlatformProperties();
    PlatformProperties(const PlatformProperties& other) = delete;
    PlatformProperties& operator=(const PlatformProperties& other) = delete;
    ~PlatformProperties();

    // Fuchsia only: set to true when the platforms requires |view_token| field
    // in PlatformWindowInitProperties when creating a window.
    bool needs_view_token = false;

    // Determines whether we should default to native decorations or the custom
    // frame based on the currently-running window manager.
    bool custom_frame_pref_default = false;

    // Determines whether switching between system and custom frames is
    // supported.
    bool use_system_title_bar = false;

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

    // Wayland only: determines if the client must ignore the screen bounds when
    // calculating bounds of menu windows.
    bool ignore_screen_bounds_for_menus = false;

    // If true, the platform shows and updates the drag image.
    bool platform_shows_drag_image = true;

    // Linux only, but see a TODO in BrowserDesktopWindowTreeHostLinux.
    // Determines whether the platform supports the global application menu.
    bool supports_global_application_menus = false;

    // Determines if the application modal dialogs should use the event blocker
    // to allow the only browser window receiving UI events.
    bool app_modal_dialogs_use_event_blocker = false;
  };

  // Properties available in the host process after initialization.
  struct InitializedHostProperties {
    // Whether the underlying platform supports deferring compositing of buffers
    // via overlays. If overlays are not supported the promotion and validation
    // logic can be skipped.
    bool supports_overlays = false;
  };

  // Corresponds to chrome_browser_main_extra_parts.h.
  //
  // The browser process' initialization involves several steps -
  // PreEarlyInitialization, PostMainMessageLoopStart, PostMainMessageLoopRun,
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
  // Sets error handlers if supported for the browser process after the message
  // loop started. It's required to call this so that we can exit cleanly if the
  // server can exit before we do.
  virtual void PostMainMessageLoopStart(base::OnceCallback<void()> shutdown_cb);
  // Resets the error handlers if set.
  virtual void PostMainMessageLoopRun();

  // Initializes the subsystems/resources necessary for the UI process (e.g.
  // events) with additional properties to customize the ozone platform
  // implementation. Ozone will not retain InitParams after returning from
  // InitalizeForUI.
  static void InitializeForUI(const InitParams& args);

  // Initializes the subsystems for rendering but with additional properties
  // provided by |args| as with InitalizeForUI.
  static void InitializeForGPU(const InitParams& args);

  static OzonePlatform* GetInstance();

  // Returns the current ozone platform name.
  // TODO(crbug.com/1002674): This is temporary and meant to make it possible
  // for higher level components to take run-time actions depending on the
  // current ozone platform selected. Which implies in layering violations,
  // which are tolerated during the X11 migration to Ozone and must be fixed
  // once it is done.
  static const char* GetPlatformName();

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
  virtual std::unique_ptr<PlatformScreen> CreateScreen() = 0;
  virtual PlatformClipboard* GetPlatformClipboard();
  virtual std::unique_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate,
      gfx::AcceleratedWidget widget) = 0;
  virtual PlatformGLEGLUtility* GetPlatformGLEGLUtility();

  // Returns a bitmask of EventFlags showing the state of Alt, Shift and Ctrl
  // keys that came with the most recent UI event.
  virtual int GetKeyModifiers() const;

  // Returns true if the specified buffer format is supported.
  virtual bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                             gfx::BufferUsage usage) const;

  // Returns a struct that contains configuration and requirements for the
  // current platform implementation. This can be called from either host or GPU
  // process at any time.
  virtual const PlatformProperties& GetPlatformProperties();

  // Returns a struct that contains properties available in the host process
  // after InitializeForUI() runs.
  virtual const InitializedHostProperties& GetInitializedHostProperties();

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

 protected:
  bool has_initialized_ui() const { return initialized_ui_; }
  bool has_initialized_gpu() const { return initialized_gpu_; }

  bool single_process() const { return single_process_; }

 private:
  // Optional method for pre-early initialization. In case of X11, sets X11
  // error handlers so that errors can be caught if early initialization fails.
  virtual void PreEarlyInitialize();

  virtual void InitializeUI(const InitParams& params) = 0;
  virtual void InitializeGPU(const InitParams& params) = 0;

  bool initialized_ui_ = false;
  bool initialized_gpu_ = false;
  bool prearly_initialized_ = false;

  // This value is checked on multiple threads. Declaring it volatile makes
  // modifications to |single_process_| visible by other threads. Mutex is not
  // needed since it's set before other threads are started.
  volatile bool single_process_ = false;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatform);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
