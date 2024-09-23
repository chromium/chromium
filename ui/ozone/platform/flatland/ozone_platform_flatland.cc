// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/ozone_platform_flatland.h"

#include <fidl/fuchsia.ui.views/cpp/hlcpp_conversion.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/ime/fuchsia/input_method_fuchsia.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/bitmap_cursor_factory.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/flatland/flatland_gpu_host.h"
#include "ui/ozone/platform/flatland/flatland_gpu_service.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"
#include "ui/ozone/platform/flatland/flatland_window.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"
#include "ui/ozone/platform/flatland/mojom/scenic_gpu_service.mojom.h"
#include "ui/ozone/platform/flatland/overlay_manager_flatland.h"
#include "ui/ozone/platform_selection.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/fuchsia/view_ref_pair.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"
#endif

namespace ui {

namespace {

class FlatlandPlatformEventSource : public ui::PlatformEventSource {
 public:
  FlatlandPlatformEventSource() = default;
  ~FlatlandPlatformEventSource() override = default;
  FlatlandPlatformEventSource(const FlatlandPlatformEventSource&) = delete;
  FlatlandPlatformEventSource& operator=(const FlatlandPlatformEventSource&) =
      delete;
};

// OzonePlatform for Flatland.
class OzonePlatformFlatland : public OzonePlatform,
                              public base::CurrentThread::DestructionObserver {
 public:
  OzonePlatformFlatland() = default;
  ~OzonePlatformFlatland() override = default;
  OzonePlatformFlatland(const OzonePlatformFlatland&) = delete;
  OzonePlatformFlatland& operator=(const OzonePlatformFlatland&) = delete;

  // OzonePlatform implementation.
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
    return flatland_gpu_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    BindInMainProcessIfNecessary();

    if (!properties.view_creation_token.value) {
      ::fuchsia::ui::views::ViewportCreationToken parent_token;
      ::fuchsia::ui::views::ViewCreationToken child_token;
      auto status =
          zx::channel::create(0, &parent_token.value, &child_token.value);
      CHECK_EQ(ZX_OK, status) << "zx_channel_create";
      properties.view_creation_token = std::move(child_token);
      properties.view_ref_pair = ::ui::ViewRefPair::New();
      properties.view_controller =
          ::ui::fuchsia::GetFlatlandViewPresenter().Run(
              std::move(parent_token));
    }

    CHECK(properties.view_creation_token.value.is_valid());
    return std::make_unique<FlatlandWindow>(window_manager_.get(), delegate,
                                            std::move(properties));
  }

  const PlatformProperties& GetPlatformProperties() override {
    static base::NoDestructor<OzonePlatform::PlatformProperties> properties;
    static bool initialised = false;
    if (!initialised) {
      properties->message_pump_type_for_gpu = base::MessagePumpType::IO;
      properties->supports_vulkan_swap_chain = false;

      initialised = true;
    }

    return *properties;
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    return window_manager_->CreateScreen();
  }

  void InitScreen(PlatformScreen* screen) override {}

  std::unique_ptr<InputMethod> CreateInputMethod(
      ImeKeyEventDispatcher* ime_key_event_dispatcher,
      gfx::AcceleratedWidget widget) override {
    return std::make_unique<InputMethodFuchsia>(
        window_manager_->GetWindow(widget)->virtual_keyboard_enabled(),
        ime_key_event_dispatcher,
        fidl::HLCPPToNatural(
            window_manager_->GetWindow(widget)->CloneViewRef()));
  }

  bool InitializeUI(const InitParams& params) override {
    if (!PlatformEventSource::GetInstance())
      platform_event_source_ = std::make_unique<FlatlandPlatformEventSource>();
    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());

    window_manager_ = std::make_unique<FlatlandWindowManager>();
    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    cursor_factory_ = std::make_unique<BitmapCursorFactory>();

    flatland_gpu_host_ =
        std::make_unique<FlatlandGpuHost>(window_manager_.get());

    // SurfaceFactory is configured here to use a ui-process remote for software
    // output.
    if (!surface_factory_)
      surface_factory_ = std::make_unique<FlatlandSurfaceFactory>();

    if (base::SingleThreadTaskRunner::HasCurrentDefault())
      BindInMainProcessIfNecessary();

    return true;
  }

  void InitializeGPU(const InitParams& params) override {
    DCHECK(!surface_factory_ || params.single_process);

    if (!surface_factory_)
      surface_factory_ = std::make_unique<FlatlandSurfaceFactory>();

    if (!params.single_process) {
      mojo::PendingRemote<mojom::ScenicGpuHost> flatland_gpu_host_remote;
      flatland_gpu_service_ = std::make_unique<FlatlandGpuService>(
          flatland_gpu_host_remote.InitWithNewPipeAndPassReceiver());

      // SurfaceFactory is configured here to use a gpu-process remote. The
      // other end of the pipe will be attached through FlatlandGpuService.
      surface_factory_->Initialize(std::move(flatland_gpu_host_remote));
    }

    overlay_manager_ = std::make_unique<OverlayManagerFlatland>();
  }

  const PlatformRuntimeProperties& GetPlatformRuntimeProperties() override {
    static OzonePlatform::PlatformRuntimeProperties properties;
    properties.supports_native_pixmaps = true;
    properties.supports_overlays = true;
    return properties;
  }

  void AddInterfaces(mojo::BinderMap* binders) override {
    binders->Add<mojom::ScenicGpuService>(
        flatland_gpu_service_->GetBinderCallback(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                     gfx::BufferUsage usage) const override {
    return FlatlandSysmemBufferCollection::IsNativePixmapConfigSupported(format,
                                                                         usage);
  }

  bool IsWindowCompositingSupported() const override { return true; }

 private:
  // Binds main process surface factory to main process FlatlandGpuHost
  void BindInMainProcessIfNecessary() {
    if (bound_in_main_process_)
      return;

    mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host_remote;
    flatland_gpu_host_->Initialize(
        gpu_host_remote.InitWithNewPipeAndPassReceiver());
    surface_factory_->Initialize(std::move(gpu_host_remote));
    bound_in_main_process_ = true;

    base::CurrentThread::Get()->AddDestructionObserver(this);
  }

  void ShutdownInMainProcess() {
    DCHECK(bound_in_main_process_);
    surface_factory_->Shutdown();
    flatland_gpu_host_->Shutdown();
    window_manager_->Shutdown();
    bound_in_main_process_ = false;
  }

  // base::CurrentThread::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override { ShutdownInMainProcess(); }

  std::unique_ptr<FlatlandWindowManager> window_manager_;

  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<PlatformEventSource> platform_event_source_;
  std::unique_ptr<CursorFactory> cursor_factory_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<FlatlandGpuHost> flatland_gpu_host_;
  std::unique_ptr<FlatlandGpuService> flatland_gpu_service_;
  std::unique_ptr<FlatlandSurfaceFactory> surface_factory_;

  // Whether the main process has initialized mojo bindings.
  bool bound_in_main_process_ = false;
};

}  // namespace

OzonePlatform* CreateOzonePlatformFlatland() {
  return new OzonePlatformFlatland();
}

}  // namespace ui
