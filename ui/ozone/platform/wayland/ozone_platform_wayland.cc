// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/ozone_platform_wayland.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/base/buildflags.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/ime/linux/input_method_auralinux.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/gpu/drm_render_node_path_finder.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_gl_egl_utility.h"
#include "ui/ozone/platform/wayland/gpu/wayland_overlay_manager.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_connector.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_menu_utils.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_menu_utils.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

#include "ui/gfx/buffer_format_util.h"

#if defined(WAYLAND_GBM)
#include "ui/base/ui_base_features.h"
#include "ui/gfx/linux/gbm_wrapper.h"  // nogncheck
#include "ui/ozone/platform/wayland/gpu/drm_render_node_handle.h"
#endif

#if BUILDFLAG(USE_GTK)
#include "ui/ozone/platform/wayland/host/linux_ui_delegate_wayland.h"  // nogncheck
#endif

#if defined(OS_CHROMEOS)
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#else
#include "ui/ozone/platform/wayland/host/wayland_cursor_factory.h"
#endif

namespace ui {

namespace {

class OzonePlatformWayland : public OzonePlatform {
 public:
  OzonePlatformWayland()
      : old_synthesize_key_repeat_enabled_(
            KeyEvent::IsSynthesizeKeyRepeatEnabled()) {
    CHECK(features::IsUsingOzonePlatform());
    // Disable key-repeat flag synthesizing. On Wayland, key repeat events are
    // generated inside Chrome, and the flag is properly set.
    // See also WaylandEventSource.
    KeyEvent::SetSynthesizeKeyRepeatEnabled(false);
  }

  OzonePlatformWayland(const OzonePlatformWayland&) = delete;
  OzonePlatformWayland& operator=(const OzonePlatformWayland&) = delete;

  ~OzonePlatformWayland() override {
    KeyEvent::SetSynthesizeKeyRepeatEnabled(old_synthesize_key_repeat_enabled_);
  }

  // OzonePlatform
  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
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
    return buffer_manager_connector_ ? buffer_manager_connector_.get()
                                     : gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    return WaylandWindow::Create(delegate, connection_.get(),
                                 std::move(properties));
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return nullptr;
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    // The WaylandConnection and the WaylandOutputManager must be created
    // before PlatformScreen.
    DCHECK(connection_ && connection_->wayland_output_manager());
    return connection_->wayland_output_manager()->CreateWaylandScreen();
  }

  void InitScreen(PlatformScreen* screen) override {
    DCHECK(connection_ && connection_->wayland_output_manager());
    // InitScreen is always called with the same screen that CreateScreen
    // hands back, so it is safe to cast here.
    connection_->wayland_output_manager()->InitWaylandScreen(
        static_cast<WaylandScreen*>(screen));
  }

  PlatformClipboard* GetPlatformClipboard() override {
    DCHECK(connection_);
    return connection_->clipboard();
  }

  PlatformGLEGLUtility* GetPlatformGLEGLUtility() override {
    if (!gl_egl_utility_)
      gl_egl_utility_ = std::make_unique<WaylandGLEGLUtility>();
    return gl_egl_utility_.get();
  }

  std::unique_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate,
      gfx::AcceleratedWidget widget) override {
    // Instantiate and set LinuxInputMethodContextFactory unless it is already
    // set (e.g: tests may have already set it).
    if (!LinuxInputMethodContextFactory::instance() &&
        !input_method_context_factory_) {
      input_method_context_factory_ =
          std::make_unique<WaylandInputMethodContextFactory>(connection_.get());
      LinuxInputMethodContextFactory::SetInstance(
          input_method_context_factory_.get());
    }

    return std::make_unique<InputMethodAuraLinux>(delegate);
  }

  PlatformMenuUtils* GetPlatformMenuUtils() override {
    return menu_utils_.get();
  }

  bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                     gfx::BufferUsage usage) const override {
    // If there is no drm render node device available, native pixmaps are not
    // supported.
    if (path_finder_.GetDrmRenderNodePath().empty())
      return false;

    if (supported_buffer_formats_.find(format) ==
        supported_buffer_formats_.end()) {
      return false;
    }

    return gfx::ClientNativePixmapDmaBuf::IsConfigurationSupported(format,
                                                                   usage);
  }

  bool ShouldUseCustomFrame() override {
    return connection_->xdg_decoration_manager_v1() == nullptr;
  }

  void InitializeUI(const InitParams& args) override {
    // Initialize DeviceDataManager early as devices are set during
    // WaylandConnection::Initialize().
    DeviceDataManager::CreateInstance();
#if BUILDFLAG(USE_XKBCOMMON)
    keyboard_layout_engine_ =
        std::make_unique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter_);
#else
    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
#endif
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());
    connection_ = std::make_unique<WaylandConnection>();
    if (!connection_->Initialize())
      LOG(FATAL) << "Failed to initialize Wayland platform";

    buffer_manager_connector_ = std::make_unique<WaylandBufferManagerConnector>(
        connection_->buffer_manager_host());
#if defined(OS_CHROMEOS)
    cursor_factory_ = std::make_unique<BitmapCursorFactoryOzone>();
#else
    cursor_factory_ = std::make_unique<WaylandCursorFactory>(connection_.get());
#endif
    input_controller_ = CreateStubInputController();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    supported_buffer_formats_ =
        connection_->buffer_manager_host()->GetSupportedBufferFormats();
#if BUILDFLAG(USE_GTK)
    gtk_ui_platform_ =
        std::make_unique<LinuxUiDelegateWayland>(connection_.get());
#endif

    menu_utils_ = std::make_unique<WaylandMenuUtils>(connection_.get());
  }

  void InitializeGPU(const InitParams& args) override {
    buffer_manager_ = std::make_unique<WaylandBufferManagerGpu>();
    surface_factory_ = std::make_unique<WaylandSurfaceFactory>(
        connection_.get(), buffer_manager_.get());
    overlay_manager_ = std::make_unique<WaylandOverlayManager>();
#if defined(WAYLAND_GBM)
    const base::FilePath drm_node_path = path_finder_.GetDrmRenderNodePath();
    if (drm_node_path.empty()) {
      LOG(WARNING) << "Failed to find drm render node path.";
    } else {
      DrmRenderNodeHandle handle;
      if (!handle.Initialize(drm_node_path)) {
        LOG(WARNING) << "Failed to initialize drm render node handle.";
      } else {
        auto gbm = CreateGbmDevice(handle.PassFD().release());
        if (!gbm)
          LOG(WARNING) << "Failed to initialize gbm device.";
        buffer_manager_->set_gbm_device(std::move(gbm));
      }
    }
#endif
  }

  const PlatformProperties& GetPlatformProperties() override {
    static base::NoDestructor<OzonePlatform::PlatformProperties> properties;
    static bool initialised = false;
    if (!initialised) {
      // Supporting server-side decorations requires a support of
      // xdg-decorations. But this protocol has been accepted into the upstream
      // recently, and it will take time before it is taken by compositors. For
      // now, always use custom frames and disallow switching to server-side
      // frames.
      // https://github.com/wayland-project/wayland-protocols/commit/76d1ae8c65739eff3434ef219c58a913ad34e988
      properties->custom_frame_pref_default = true;

      properties->uses_external_vulkan_image_factory = true;

      // Wayland doesn't provide clients with global screen coordinates.
      // Instead, it forces clients to position windows relative to their top
      // level windows if the have child-parent relationship. In case of
      // toplevel windows, clients simply don't know their position on screens
      // and always assume they are located at some arbitrary position.
      properties->ignore_screen_bounds_for_menus = true;
      // Wayland uses sub-surfaces to show tooltips, and sub-surfaces must be
      // bound to their root surfaces always, but finding the correct root
      // surface at the moment of creating the tooltip is not always possible
      // due to how Wayland handles focus and activation.
      // Therefore, the platform should be given a hint at the moment when the
      // surface is initialised, where it is known for sure which root surface
      // shows the tooltip.
      properties->set_parent_for_non_top_level_windows = true;
      properties->app_modal_dialogs_use_event_blocker = true;

      // Primary planes can be transluscent due to underlay strategy. As a
      // result Wayland server draws contents occluded by an accelerated widget.
      // To prevent this, an opaque background image is stacked below the
      // accelerated widget to occlude contents below.
      properties->needs_background_image =
          ui::IsWaylandOverlayDelegationEnabled();

      initialised = true;
    }

    return *properties;
  }

  const PlatformRuntimeProperties& GetPlatformRuntimeProperties() override {
    static OzonePlatform::PlatformRuntimeProperties properties;
    if (connection_) {
      properties.supports_server_side_window_decorations =
          (connection_->xdg_decoration_manager_v1() != nullptr);
    }
    return properties;
  }

  const InitializedHostProperties& GetInitializedHostProperties() override {
    static OzonePlatform::InitializedHostProperties properties;
    static bool initialized = false;
    if (!initialized) {
      properties.supports_overlays =
          ui::IsWaylandOverlayDelegationEnabled() && connection_->viewporter();
      initialized = true;
    }
    return properties;
  }

  void AddInterfaces(mojo::BinderMap* binders) override {
    binders->Add<ozone::mojom::WaylandBufferManagerGpu>(
        base::BindRepeating(
            &OzonePlatformWayland::CreateWaylandBufferManagerGpuBinding,
            base::Unretained(this)),
        base::SequencedTaskRunnerHandle::Get());
  }

  void CreateWaylandBufferManagerGpuBinding(
      mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver) {
    buffer_manager_->AddBindingWaylandBufferManagerGpu(std::move(receiver));
  }

  void PostCreateMainMessageLoop(
      base::OnceCallback<void()> shutdown_cb) override {
    DCHECK(connection_);
    connection_->SetShutdownCb(std::move(shutdown_cb));
  }

 private:
  // Keeps the old value of KeyEvent::IsSynthesizeKeyRepeatEnabled(), to
  // restore it on destruction.
  const bool old_synthesize_key_repeat_enabled_;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<WaylandConnection> connection_;
  std::unique_ptr<WaylandSurfaceFactory> surface_factory_;
  std::unique_ptr<CursorFactory> cursor_factory_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<WaylandInputMethodContextFactory>
      input_method_context_factory_;
  std::unique_ptr<WaylandBufferManagerConnector> buffer_manager_connector_;
  std::unique_ptr<WaylandMenuUtils> menu_utils_;

  // Objects, which solely live in the GPU process.
  std::unique_ptr<WaylandBufferManagerGpu> buffer_manager_;
  std::unique_ptr<WaylandOverlayManager> overlay_manager_;
  std::unique_ptr<WaylandGLEGLUtility> gl_egl_utility_;

  // Provides supported buffer formats for native gpu memory buffers
  // framework.
  wl::BufferFormatsWithModifiersMap supported_buffer_formats_;

  // This is used both in the gpu and browser processes to find out if a drm
  // render node is available.
  DrmRenderNodePathFinder path_finder_;

#if BUILDFLAG(USE_GTK)
  std::unique_ptr<LinuxUiDelegateWayland> gtk_ui_platform_;
#endif
};

}  // namespace

OzonePlatform* CreateOzonePlatformWayland() {
  return new OzonePlatformWayland;
}

}  // namespace ui
