// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
#define UI_OZONE_PUBLIC_OZONE_PLATFORM_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"

namespace display {
class NativeDisplayDelegate;
}

namespace IPC {
class MessageFilter;
}

namespace service_manager {
class Connector;
}

namespace ui {

class CursorFactoryOzone;
class InputController;
class GpuPlatformSupportHost;
class OverlayManagerOzone;
class PlatformScreen;
class PlatformWindow;
class PlatformWindowDelegate;
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
class OZONE_EXPORT OzonePlatform {
 public:
  OzonePlatform();
  virtual ~OzonePlatform();

  // Additional initialization params for the platform. Platforms must not
  // retain a reference to this structure.
  struct InitParams {
    // Ozone may retain this pointer for later use. An Ozone platform embedder
    // may set this value if operating in the idiomatic mojo fashion with a
    // service manager. Mojo transport does not require a service manager but in
    // that case ozone will not be able to connect to the DRM and cursor
    // services. Instead the host must invoke |OnGpuServiceLaunched| as
    // described in ui/ozone/public/gpu_platform_support_host.h to inform the
    // ozone host that a process containing these services is running.
    service_manager::Connector* connector = nullptr;

    // Setting this to true indicates that the platform implementation should
    // operate as a single process for platforms (i.e. drm) that are usually
    // split between a host and viz specific portion.
    bool single_process = false;

    // Setting this to true indicates that the platform implementation should
    // use mojo. Setting this to true requires calling |AddInterfaces|
    // afterwards in the Viz process and providing a connector as part.
    bool using_mojo = false;
  };

  // Struct used to indicate platform properties.
  struct PlatformProperties {
    PlatformProperties();
    PlatformProperties(bool needs_request,
                       bool custom_frame_default,
                       bool can_use_system_title_bar,
                       bool requires_mojo_for_ipc,
                       std::vector<gfx::BufferFormat> buffer_formats);
    ~PlatformProperties();
    PlatformProperties(const PlatformProperties& other);

    // Fuchsia only: set to true when the platforms requires
    // |view_owner_request| field in PlatformWindowInitProperties when creating
    // a window.
    bool needs_view_owner_request = false;

    // Determine whether we should default to native decorations or the custom
    // frame based on the currently-running window manager.
    bool custom_frame_pref_default = false;

    // Determine whether switching between system and custom frames is
    // supported.
    bool use_system_title_bar = false;

    // Determines if the platform requires mojo communication for the IPC.
    // Currently used only by the Ozone/Wayland platform.
    bool requires_mojo = false;

    // Wayland only: carries buffer formats supported by a Wayland server.
    std::vector<gfx::BufferFormat> supported_buffer_formats;
  };

  using StartupCallback = base::OnceCallback<void(OzonePlatform*)>;

  // Ensures the OzonePlatform instance without doing any initialization.
  // No-op in case the instance is already created.
  // This is useful in order call virtual methods that depend on the ozone
  // platform selected at runtime, e.g. ::GetMessageLoopTypeForGpu.
  static OzonePlatform* EnsureInstance();

  // Initializes the subsystems/resources necessary for the UI process (e.g.
  // events) with additional properties to customize the ozone platform
  // implementation. Ozone will not retain InitParams after returning from
  // InitalizeForUI.
  static void InitializeForUI(const InitParams& args);

  // Initializes the subsystems for rendering but with additional properties
  // provided by |args| as with InitalizeForUI.
  static void InitializeForGPU(const InitParams& args);

  static OzonePlatform* GetInstance();

  // Registers a callback to be run when the OzonePlatform is initialized. Note
  // that if an instance already exists, then the callback is called
  // immediately. If an instance does not exist, and is created later, then the
  // callback is called once the instance is created and initialized, on the
  // thread it is initialized on. If the caller requires the callback to run on
  // a specific thread, then it needs to do ensure that by itself.
  static void RegisterStartupCallback(StartupCallback callback);

  // Factory getters to override in subclasses. The returned objects will be
  // injected into the appropriate layer at startup. Subclasses should not
  // inject these objects themselves. Ownership is retained by OzonePlatform.
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() = 0;
  virtual ui::OverlayManagerOzone* GetOverlayManager() = 0;
  virtual ui::CursorFactoryOzone* GetCursorFactoryOzone() = 0;
  virtual ui::InputController* GetInputController() = 0;
  virtual IPC::MessageFilter* GetGpuMessageFilter();
  virtual ui::GpuPlatformSupportHost* GetGpuPlatformSupportHost() = 0;
  virtual std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() = 0;
  virtual std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) = 0;
  virtual std::unique_ptr<display::NativeDisplayDelegate>
  CreateNativeDisplayDelegate() = 0;
  virtual std::unique_ptr<PlatformScreen> CreateScreen();

  // Returns a struct that contains configuration and requirements for the
  // current platform implementation.
  virtual const PlatformProperties& GetPlatformProperties();

  // Returns the message loop type required for OzonePlatform instance that
  // will be initialized for the GPU process.
  virtual base::MessageLoop::Type GetMessageLoopTypeForGpu();

  // Ozone platform implementations may also choose to expose mojo interfaces to
  // internal functionality. Embedders wishing to take advantage of ozone mojo
  // implementations must invoke AddInterfaces with a valid
  // service_manager::BinderRegistry* pointer to export all Mojo interfaces
  // defined within Ozone.
  //
  // Requests arriving before they can be immediately handled will be queued and
  // executed later.
  //
  // A default do-nothing implementation is provided to permit platform
  // implementations to opt out of implementing any Mojo interfaces.
  virtual void AddInterfaces(service_manager::BinderRegistry* registry);

  // The GPU-specific portion of Ozone would typically run in a sandboxed
  // process for additional security. Some startup might need to wait until
  // after the sandbox has been configured. The embedder should use this method
  // to specify that the sandbox is configured and that GPU-side setup should
  // complete. A default do-nothing implementation is provided to permit
  // platform implementations to ignore sandboxing and any associated launch
  // ordering issues.
  virtual void AfterSandboxEntry();

 private:
  virtual void InitializeUI(const InitParams& params) = 0;
  virtual void InitializeGPU(const InitParams& params) = 0;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatform);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OZONE_PLATFORM_H_
