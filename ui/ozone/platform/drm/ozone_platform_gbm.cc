// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/ozone_platform_gbm.h"

#include <gbm.h>
#include <stdlib.h>
#include <xf86drm.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/buildflags.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_manager_gpu.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_message_proxy.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/drm_device_connector.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/platform/drm/host/host_drm_device.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/mojom/drm_device.mojom.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/platform_screen.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/input_method_chromeos.h"
#else
#include "ui/base/ime/input_method_minimal.h"
#endif

namespace ui {

namespace {

class OzonePlatformGbm : public OzonePlatform {
 public:
  OzonePlatformGbm() = default;
  ~OzonePlatformGbm() override = default;

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }
  InputController* GetInputController() override {
    return event_factory_ozone_->input_controller();
  }
  IPC::MessageFilter* GetGpuMessageFilter() override {
    if (using_mojo_) {
      return nullptr;
    } else {
      return gpu_message_filter_.get();
    }
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    NOTREACHED();
    return nullptr;
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    if (using_mojo_) {
      return drm_device_connector_.get();
    } else {
      return gpu_platform_support_host_.get();
    }
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return event_factory_ozone_->CreateSystemInputInjector();
  }

  // In multi-process mode, this function must be executed in Viz as it sets up
  // the callbacks needed for Mojo receivers. In single process mode, it may be
  // called on any thread. It must follow one of |InitializeUI| or
  // |InitializeGPU|. Invocations of this method when not using mojo will be
  // ignored. While the caller may choose to invoke this method before entering
  // the sandbox, the actual interface adding has to happen on the DRM Device
  // thread and so will be deferred until the DRM thread is running.
  void AddInterfaces(mojo::BinderMap* binders) override {
    if (!using_mojo_)
      return;

    binders->Add<ozone::mojom::DrmDevice>(
        base::BindRepeating(&OzonePlatformGbm::CreateDrmDeviceReceiver,
                            weak_factory_.GetWeakPtr()),
        base::ThreadTaskRunnerHandle::Get());
  }

  // Runs on the thread where AddInterfaces was invoked. But the endpoint is
  // always bound on the DRM thread.
  void CreateDrmDeviceReceiver(
      mojo::PendingReceiver<ozone::mojom::DrmDevice> receiver) {
    if (drm_thread_started_)
      drm_thread_proxy_->AddDrmDeviceReceiver(std::move(receiver));
    else
      pending_gpu_adapter_receivers_.push_back(std::move(receiver));
  }

  // Runs on the thread that invoked |AddInterfaces| to drain the queue of
  // receiver requests that could not be satisfied until the DRM thread is
  // available (i.e. if waiting until the sandbox has been entered.)
  void DrainReceiverRequests() {
    for (auto& receiver : pending_gpu_adapter_receivers_)
      drm_thread_proxy_->AddDrmDeviceReceiver(std::move(receiver));
    pending_gpu_adapter_receivers_.clear();

    drm_thread_started_ = true;
  }

  std::unique_ptr<PlatformWindowBase> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    GpuThreadAdapter* adapter = gpu_platform_support_host_.get();
    if (using_mojo_) {
      adapter = host_drm_device_.get();
    }

    auto platform_window = std::make_unique<DrmWindowHost>(
        delegate, properties.bounds, adapter, event_factory_ozone_.get(),
        cursor_.get(), window_manager_.get(), display_manager_.get(),
        overlay_manager_.get());
    platform_window->Initialize();
    return std::move(platform_window);
  }
  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return std::make_unique<DrmNativeDisplayDelegate>(display_manager_.get());
  }
  std::unique_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate) override {
#if defined(OS_CHROMEOS)
    return std::make_unique<InputMethodChromeOS>(delegate);
#else
    return std::make_unique<InputMethodMinimal>(delegate);
#endif
  }

  bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                     gfx::BufferUsage usage) const override {
    return gfx::ClientNativePixmapDmaBuf::IsConfigurationSupported(format,
                                                                   usage);
  }

  void InitializeUI(const InitParams& args) override {
    // Ozone drm can operate in three modes configured at runtime.
    //   1. legacy mode where host and viz components communicate
    //      via param traits IPC. This will be soon deprecated in favor of 3.
    //   2. single-process mode where host and viz components
    //      communicate via in-process mojo. Single-process mode can be single
    //      or multi-threaded.
    //   3. multi-process mode where host and viz components communicate
    //      via mojo IPC.

    using_mojo_ = args.using_mojo;
    host_thread_ = base::PlatformThread::CurrentRef();

    device_manager_ = CreateDeviceManager();
    window_manager_ = std::make_unique<DrmWindowHostManager>();
    cursor_ = std::make_unique<DrmCursor>(window_manager_.get());

#if BUILDFLAG(USE_XKBCOMMON)
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter_));
#else
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<StubKeyboardLayoutEngine>());
#endif

    event_factory_ozone_ = std::make_unique<EventFactoryEvdev>(
        cursor_.get(), device_manager_.get(),
        KeyboardLayoutEngineManager::GetKeyboardLayoutEngine());

    GpuThreadAdapter* adapter;

    if (using_mojo_) {
      host_drm_device_ = base::MakeRefCounted<HostDrmDevice>(cursor_.get());
      drm_device_connector_ =
          std::make_unique<DrmDeviceConnector>(host_drm_device_);
      adapter = host_drm_device_.get();
    } else {
      gpu_platform_support_host_ =
          std::make_unique<DrmGpuPlatformSupportHost>(cursor_.get());
      adapter = gpu_platform_support_host_.get();
    }

    std::unique_ptr<DrmOverlayManagerHost> overlay_manager_host;
    if (!args.viz_display_compositor) {
      overlay_manager_host = std::make_unique<DrmOverlayManagerHost>(
          adapter, window_manager_.get());
    }

    display_manager_ = std::make_unique<DrmDisplayHostManager>(
        adapter, device_manager_.get(), &host_properties_,
        overlay_manager_host.get(), event_factory_ozone_->input_controller());
    cursor_factory_ozone_ = std::make_unique<BitmapCursorFactoryOzone>();

    if (using_mojo_) {
      host_drm_device_->ProvideManagers(display_manager_.get(),
                                        overlay_manager_host.get());
    }

    overlay_manager_ = std::move(overlay_manager_host);
  }

  void InitializeGPU(const InitParams& args) override {
    using_mojo_ = args.using_mojo;
    gpu_task_runner_ = base::ThreadTaskRunnerHandle::Get();

    InterThreadMessagingProxy* itmp;
    if (!using_mojo_) {
      scoped_refptr<DrmThreadMessageProxy> message_proxy(
          new DrmThreadMessageProxy());
      itmp = message_proxy.get();
      gpu_message_filter_ = std::move(message_proxy);
    }

    // NOTE: Can't start the thread here since this is called before sandbox
    // initialization in multi-process Chrome.
    drm_thread_proxy_ = std::make_unique<DrmThreadProxy>();

    surface_factory_ =
        std::make_unique<GbmSurfaceFactory>(drm_thread_proxy_.get());
    if (!using_mojo_) {
      drm_thread_proxy_->BindThreadIntoMessagingProxy(itmp);
    }

    if (args.viz_display_compositor) {
      overlay_manager_ =
          std::make_unique<DrmOverlayManagerGpu>(drm_thread_proxy_.get());
    }

    // If gpu is in a separate process, rest of the initialization happens after
    // entering the sandbox.
    if (!single_process())
      return;

    // In single process/mojo mode we need to make sure DrainReceiverRequest is
    // executed on this thread before we start the drm device.
    const bool block_for_drm_thread = using_mojo_;
    StartDrmThread(block_for_drm_thread);

    if (using_mojo_ && host_thread_ == base::PlatformThread::CurrentRef()) {
      CHECK(has_initialized_ui()) << "Mojo single-thread mode requires "
                                     "InitializeUI to be called first.";
      // Connect host and gpu here since OnGpuServiceLaunched() is not called in
      // the single-threaded mode.
      mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device;
      drm_thread_proxy_->AddDrmDeviceReceiver(
          drm_device.InitWithNewPipeAndPassReceiver());
      drm_device_connector_->ConnectSingleThreaded(std::move(drm_device));
    }
  }

  const InitializedHostProperties& GetInitializedHostProperties() override {
    DCHECK(has_initialized_ui());
    return host_properties_;
  }

  // The DRM thread needs to be started late because we need to wait for the
  // sandbox to start. This entry point in the Ozone API gives platforms
  // flexibility in handing this requirement.
  void AfterSandboxEntry() override {
    DCHECK(!single_process());
    CHECK(has_initialized_gpu()) << "AfterSandboxEntry before InitializeForGPU "
                                    "is invalid startup order.";

    const bool block_for_drm_thread = false;
    StartDrmThread(block_for_drm_thread);
  }

 private:
  // Starts the DRM thread. |blocking| determines if the call should be blocked
  // until the thread is started.
  void StartDrmThread(bool blocking) {
    if (blocking) {
      base::WaitableEvent done_event;
      drm_thread_proxy_->StartDrmThread(base::BindOnce(
          &base::WaitableEvent::Signal, base::Unretained(&done_event)));
      done_event.Wait();
      DrainReceiverRequests();
    } else {
      auto safe_receiver_request_drainer = CreateSafeOnceCallback(
          base::BindOnce(&OzonePlatformGbm::DrainReceiverRequests,
                         weak_factory_.GetWeakPtr()));
      drm_thread_proxy_->StartDrmThread(
          std::move(safe_receiver_request_drainer));
    }
  }

  bool using_mojo_ = false;

  // Objects in the GPU process.
  std::unique_ptr<DrmThreadProxy> drm_thread_proxy_;
  std::unique_ptr<GbmSurfaceFactory> surface_factory_;
  scoped_refptr<IPC::MessageFilter> gpu_message_filter_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // TODO(rjkroege,sadrul): Provide a more elegant solution for this issue when
  // running in single process mode.
  std::vector<mojo::PendingReceiver<ozone::mojom::DrmDevice>>
      pending_gpu_adapter_receivers_;
  bool drm_thread_started_ = false;

  // gpu_platform_support_host_ is the IPC bridge to the GPU process while
  // host_drm_device_ is the mojo bridge to the Viz process. Only one can be in
  // use at any time.
  // TODO(rjkroege): Remove gpu_platform_support_host_ once ozone/drm with mojo
  // has reached the stable channel.
  // A raw pointer to either |gpu_platform_support_host_| or |host_drm_device_|
  // is passed to |display_manager_| and |overlay_manager_| in IntializeUI.
  // To avoid a use after free, the following two members should be declared
  // before the two managers, so that they're deleted after them.
  std::unique_ptr<DrmGpuPlatformSupportHost> gpu_platform_support_host_;

  // Objects in the host process.
  std::unique_ptr<DrmDeviceConnector> drm_device_connector_;
  scoped_refptr<HostDrmDevice> host_drm_device_;
  base::PlatformThreadRef host_thread_;
  std::unique_ptr<DeviceManager> device_manager_;
  std::unique_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<DrmWindowHostManager> window_manager_;
  std::unique_ptr<DrmCursor> cursor_;
  std::unique_ptr<EventFactoryEvdev> event_factory_ozone_;
  std::unique_ptr<DrmDisplayHostManager> display_manager_;
  std::unique_ptr<DrmOverlayManager> overlay_manager_;
  InitializedHostProperties host_properties_;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  base::WeakPtrFactory<OzonePlatformGbm> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformGbm);
};

}  // namespace

OzonePlatform* CreateOzonePlatformGbm() {
  return new OzonePlatformGbm;
}

}  // namespace ui
