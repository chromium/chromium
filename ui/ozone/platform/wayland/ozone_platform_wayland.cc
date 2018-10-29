// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/ozone_platform_wayland.h"

#include "base/memory/ptr_util.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/ui_features.h"
#include "ui/display/manager/fake_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/system_input_injector.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/wayland/gpu/wayland_connection_proxy.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_connection_connector.h"
#include "ui/ozone/platform/wayland/wayland_input_method_context_factory.h"
#include "ui/ozone/platform/wayland/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/ozone/platform/wayland/wayland_xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

#if defined(WAYLAND_GBM)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/common/linux/gbm_wrapper.h"
#include "ui/ozone/platform/wayland/gpu/drm_render_node_handle.h"
#include "ui/ozone/platform/wayland/gpu/drm_render_node_path_finder.h"
#endif

namespace ui {

namespace {

class OzonePlatformWayland : public OzonePlatform {
 public:
  OzonePlatformWayland() {
    // Supporting server-side decorations requires a support of xdg-decorations.
    // But this protocol has been accepted into the upstream recently, and it
    // will take time before it is taken by compositors. For now, always use
    // custom frames and disallow switching to server-side frames.
    // https://github.com/wayland-project/wayland-protocols/commit/76d1ae8c65739eff3434ef219c58a913ad34e988
    properties_.custom_frame_pref_default = true;
    properties_.use_system_title_bar = false;
    // Ozone/Wayland relies on the mojo communication when running in
    // !single_process.
    // TODO(msisov, rjkroege): Remove after http://crbug.com/806092.
    properties_.requires_mojo = true;
  }
  ~OzonePlatformWayland() override {}

  // OzonePlatform
  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }

  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_.get();
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return connector_ ? connector_.get() : gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    // Some unit tests may try to set custom input method context factory
    // after InitializeUI. Thus instead of creating factory in InitializeUI
    // it is set at this point if none exists
    if (!LinuxInputMethodContextFactory::instance() &&
        !wayland_input_method_context_factory_) {
      wayland_input_method_context_factory_.reset(
          new WaylandInputMethodContextFactory(connection_.get()));
    }

    auto window = std::make_unique<WaylandWindow>(delegate, connection_.get());
    if (!window->Initialize(std::move(properties)))
      return nullptr;
    return std::move(window);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return std::make_unique<display::FakeDisplayDelegate>();
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    // The WaylandConnection and the WaylandOutputManager must be created before
    // PlatformScreen.
    DCHECK(connection_ && connection_->wayland_output_manager());
    return connection_->wayland_output_manager()->CreateWaylandScreen();
  }

  void InitializeUI(const InitParams& args) override {
#if BUILDFLAG(USE_XKBCOMMON)
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<WaylandXkbKeyboardLayoutEngine>(
            xkb_evdev_code_converter_));
#else
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<StubKeyboardLayoutEngine>());
#endif
    connection_.reset(new WaylandConnection);
    if (!connection_->Initialize())
      LOG(FATAL) << "Failed to initialize Wayland platform";

#if defined(WAYLAND_GBM)
    if (!args.single_process)
      connector_.reset(new WaylandConnectionConnector(connection_.get()));
#endif

    cursor_factory_.reset(new BitmapCursorFactoryOzone);
    overlay_manager_.reset(new StubOverlayManager);
    input_controller_ = CreateStubInputController();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU(const InitParams& args) override {
    if (!args.single_process) {
      proxy_.reset(new WaylandConnectionProxy(nullptr));
#if defined(WAYLAND_GBM)
      DrmRenderNodePathFinder path_finder;
      const base::FilePath drm_node_path = path_finder.GetDrmRenderNodePath();
      if (drm_node_path.empty())
        LOG(FATAL) << "Failed to find drm render node path.";

      DrmRenderNodeHandle handle;
      if (!handle.Initialize(drm_node_path))
        LOG(FATAL) << "Failed to initialize drm render node handle.";

      auto gbm = CreateGbmDevice(handle.PassFD().release());
      if (!gbm)
        LOG(FATAL) << "Failed to initialize gbm device.";

      proxy_->set_gbm_device(std::move(gbm));
#endif
    } else {
      proxy_.reset(new WaylandConnectionProxy(connection_.get()));
    }
    surface_factory_.reset(new WaylandSurfaceFactory(proxy_.get()));
  }

  const PlatformProperties& GetPlatformProperties() override {
    if (connection_ && properties_.supported_buffer_formats.empty()) {
      properties_.supported_buffer_formats =
          connection_->GetSupportedBufferFormats();
    }
    return properties_;
  }

  void AddInterfaces(service_manager::BinderRegistry* registry) override {
    registry->AddInterface<ozone::mojom::WaylandConnectionClient>(
        base::BindRepeating(
            &OzonePlatformWayland::CreateWaylandConnectionClientBinding,
            base::Unretained(this)));
  }

  void CreateWaylandConnectionClientBinding(
      ozone::mojom::WaylandConnectionClientRequest request) {
    proxy_->AddBindingWaylandConnectionClient(std::move(request));
  }

 private:
  std::unique_ptr<WaylandConnection> connection_;
  std::unique_ptr<WaylandSurfaceFactory> surface_factory_;
  std::unique_ptr<BitmapCursorFactoryOzone> cursor_factory_;
  std::unique_ptr<StubOverlayManager> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<WaylandInputMethodContextFactory>
      wayland_input_method_context_factory_;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  std::unique_ptr<WaylandConnectionProxy> proxy_;
  std::unique_ptr<WaylandConnectionConnector> connector_;

  PlatformProperties properties_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformWayland);
};

}  // namespace

OzonePlatform* CreateOzonePlatformWayland() {
  return new OzonePlatformWayland;
}

}  // namespace ui
