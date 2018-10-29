// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/mus_client.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/cpp/gpu/gpu.h"
#include "services/ws/public/cpp/input_devices/input_device_client.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/capture_synchronizer.h"
#include "ui/aura/mus/mus_context_factory.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/mojo/clipboard_client.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/ax_remote_host.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/mus_property_mirror.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/wm_state.h"

#if defined(USE_OZONE)
#include "ui/base/cursor/ozone/cursor_data_factory_ozone.h"
#endif

// Widget::InitParams::Type must match that of ws::mojom::WindowType.
#define WINDOW_TYPES_MATCH(NAME)                                      \
  static_assert(                                                      \
      static_cast<int32_t>(views::Widget::InitParams::TYPE_##NAME) == \
          static_cast<int32_t>(ws::mojom::WindowType::NAME),          \
      "Window type constants must match")

WINDOW_TYPES_MATCH(WINDOW);
WINDOW_TYPES_MATCH(PANEL);
WINDOW_TYPES_MATCH(WINDOW_FRAMELESS);
WINDOW_TYPES_MATCH(CONTROL);
WINDOW_TYPES_MATCH(POPUP);
WINDOW_TYPES_MATCH(MENU);
WINDOW_TYPES_MATCH(TOOLTIP);
WINDOW_TYPES_MATCH(BUBBLE);
WINDOW_TYPES_MATCH(DRAG);
// ws::mojom::WindowType::UNKNOWN does not correspond to a value in
// Widget::InitParams::Type.

namespace views {

// static
MusClient* MusClient::instance_ = nullptr;

MusClient::InitParams::InitParams() = default;

MusClient::InitParams::~InitParams() = default;

MusClient::MusClient(const InitParams& params) : identity_(params.identity) {
  DCHECK(!instance_);
  DCHECK(aura::Env::GetInstance());
  instance_ = this;

#if defined(USE_OZONE)
  // If we're in a mus client, we aren't going to have all of ozone initialized
  // even though we're in an ozone build. All the hard coded USE_OZONE ifdefs
  // that handle cursor code expect that there will be a CursorFactoryOzone
  // instance. Partially initialize the ozone cursor internals here, like we
  // partially initialize other ozone subsystems in
  // ChromeBrowserMainExtraPartsViews.
  if (params.create_cursor_factory)
    cursor_factory_ozone_ = std::make_unique<ui::CursorDataFactoryOzone>();
#endif

  property_converter_ = std::make_unique<aura::PropertyConverter>();
  property_converter_->RegisterPrimitiveProperty(
      ::wm::kShadowElevationKey,
      ws::mojom::WindowManager::kShadowElevation_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());

  if (params.create_wm_state)
    wm_state_ = std::make_unique<wm::WMState>();

  service_manager::Connector* connector = params.connector;

  if (!params.window_tree_client) {
    // If this process is running in the WindowService, then discardable memory
    // should have already been created.
    const bool create_discardable_memory = !params.running_in_ws_process;
    owned_window_tree_client_ =
        aura::WindowTreeClient::CreateForWindowTreeFactory(
            connector, this, create_discardable_memory,
            std::move(params.io_task_runner));
    window_tree_client_ = owned_window_tree_client_.get();
    aura::Env::GetInstance()->SetWindowTreeClient(window_tree_client_);
  } else {
    window_tree_client_ = params.window_tree_client;
  }

  if (connector && !params.running_in_ws_process) {
    input_device_client_ = std::make_unique<ws::InputDeviceClient>();
    ws::mojom::InputDeviceServerPtr input_device_server;
    connector->BindInterface(ws::mojom::kServiceName, &input_device_server);
    input_device_client_->Connect(std::move(input_device_server));

    screen_ = std::make_unique<ScreenMus>(this);
    display::Screen::SetScreenInstance(screen_.get());

    // NOTE: this deadlocks if |running_in_ws_process| is true (because the main
    // thread is running the WindowService).
    window_tree_client_->WaitForDisplays();

    ui::mojom::ClipboardHostPtr clipboard_host_ptr;
    connector->BindInterface(ws::mojom::kServiceName, &clipboard_host_ptr);
    ui::Clipboard::SetClipboardForCurrentThread(
        std::make_unique<ui::ClipboardClient>(std::move(clipboard_host_ptr)));

    if (params.use_accessibility_host) {
      ax_remote_host_ = std::make_unique<AXRemoteHost>();
      ax_remote_host_->Init(connector);
    }
  }

  ViewsDelegate::GetInstance()->set_native_widget_factory(
      base::Bind(&MusClient::CreateNativeWidget, base::Unretained(this)));
  ViewsDelegate::GetInstance()->set_desktop_window_tree_host_factory(base::Bind(
      &MusClient::CreateDesktopWindowTreeHost, base::Unretained(this)));
}

MusClient::~MusClient() {
  // Tear down accessibility before WindowTreeClient to ensure window tree
  // cleanup doesn't trigger accessibility events.
  ax_remote_host_.reset();

  // ~WindowTreeClient calls back to us (we're its delegate), destroy it while
  // we are still valid.
  owned_window_tree_client_.reset();
  window_tree_client_ = nullptr;
  ui::OSExchangeDataProviderFactory::SetFactory(nullptr);
  ui::Clipboard::DestroyClipboardForCurrentThread();

  if (ViewsDelegate::GetInstance()) {
    ViewsDelegate::GetInstance()->set_native_widget_factory(
        ViewsDelegate::NativeWidgetFactory());
    ViewsDelegate::GetInstance()->set_desktop_window_tree_host_factory(
        ViewsDelegate::DesktopWindowTreeHostFactory());
  }

  if (screen_) {
    display::Screen::SetScreenInstance(nullptr);
    screen_.reset();
  }

  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
  DCHECK(aura::Env::GetInstance());
}

// static
bool MusClient::ShouldCreateDesktopNativeWidgetAura(
    const Widget::InitParams& init_params) {
  const bool from_window_service =
      (init_params.context &&
       init_params.context->env()->mode() == aura::Env::Mode::LOCAL) ||
      (init_params.parent &&
       init_params.parent->env()->mode() == aura::Env::Mode::LOCAL);
  // |from_window_service| is true if the aura::Env has a mode of LOCAL. If
  // the mode is LOCAL there are two envs, one used by the window service
  // (LOCAL), and the other for non-window-service code. Windows created with
  // LOCAL should use NativeWidgetAura (which happens if false is returned
  // here).
  if (from_window_service)
    return false;

  // TYPE_CONTROL and child widgets require a NativeWidgetAura.
  return init_params.type != Widget::InitParams::TYPE_CONTROL &&
         !init_params.child;
}

// static
bool MusClient::ShouldMakeWidgetWindowsTranslucent(
    const Widget::InitParams& params) {
  // |TYPE_WINDOW| and |TYPE_PANEL| are forced to translucent so that the
  // window manager can draw the client decorations.
  return params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW ||
         params.type == Widget::InitParams::TYPE_WINDOW ||
         params.type == Widget::InitParams::TYPE_PANEL;
}

// static
std::map<std::string, std::vector<uint8_t>>
MusClient::ConfigurePropertiesFromParams(
    const Widget::InitParams& init_params) {
  using PrimitiveType = aura::PropertyConverter::PrimitiveType;
  using WindowManager = ws::mojom::WindowManager;
  using TransportType = std::vector<uint8_t>;

  std::map<std::string, TransportType> properties = init_params.mus_properties;

  // Widget::InitParams::Type matches ws::mojom::WindowType.
  properties[WindowManager::kWindowType_InitProperty] =
      mojo::ConvertTo<TransportType>(static_cast<int32_t>(init_params.type));

  properties[WindowManager::kFocusable_InitProperty] =
      mojo::ConvertTo<TransportType>(init_params.CanActivate());

  properties[WindowManager::kTranslucent_InitProperty] =
      mojo::ConvertTo<TransportType>(
          ShouldMakeWidgetWindowsTranslucent(init_params));

  if (!init_params.bounds.IsEmpty()) {
    properties[WindowManager::kBounds_InitProperty] =
        mojo::ConvertTo<TransportType>(init_params.bounds);
  }

  if (!init_params.name.empty()) {
    properties[WindowManager::kName_Property] =
        mojo::ConvertTo<TransportType>(init_params.name);
  }

  properties[WindowManager::kAlwaysOnTop_Property] =
      mojo::ConvertTo<TransportType>(
          static_cast<PrimitiveType>(init_params.keep_on_top));

  properties[WindowManager::kRemoveStandardFrame_InitProperty] =
      mojo::ConvertTo<TransportType>(init_params.remove_standard_frame);

  if (init_params.corner_radius) {
    properties[WindowManager::kWindowCornerRadius_Property] =
        mojo::ConvertTo<TransportType>(
            static_cast<PrimitiveType>(*init_params.corner_radius));
  }

  if (!Widget::RequiresNonClientView(init_params.type))
    return properties;

  if (init_params.delegate) {
    if (properties.count(WindowManager::kResizeBehavior_Property) == 0) {
      properties[WindowManager::kResizeBehavior_Property] =
          mojo::ConvertTo<TransportType>(static_cast<PrimitiveType>(
              init_params.delegate->GetResizeBehavior()));
    }

    if (init_params.delegate->ShouldShowWindowTitle()) {
      properties[WindowManager::kWindowTitleShown_Property] =
          mojo::ConvertTo<TransportType>(static_cast<PrimitiveType>(
              init_params.delegate->ShouldShowWindowTitle()));
    }

    if (!init_params.delegate->GetWindowTitle().empty()) {
      properties[WindowManager::kWindowTitle_Property] =
          mojo::ConvertTo<TransportType>(
              init_params.delegate->GetWindowTitle());
    }

    // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
    gfx::ImageSkia app_icon = init_params.delegate->GetWindowAppIcon();
    SkBitmap app_bitmap = app_icon.GetRepresentation(1.f).GetBitmap();
    if (!app_bitmap.isNull()) {
      properties[WindowManager::kAppIcon_Property] =
          mojo::ConvertTo<TransportType>(app_bitmap);
    }

    // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
    gfx::ImageSkia window_icon = init_params.delegate->GetWindowIcon();
    SkBitmap window_bitmap = window_icon.GetRepresentation(1.f).GetBitmap();
    if (!window_bitmap.isNull()) {
      properties[WindowManager::kWindowIcon_Property] =
          mojo::ConvertTo<TransportType>(window_bitmap);
    }
  }

  return properties;
}

NativeWidget* MusClient::CreateNativeWidget(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
  if (!ShouldCreateDesktopNativeWidgetAura(init_params)) {
    // A null return value results in creating NativeWidgetAura.
    return nullptr;
  }

  DesktopNativeWidgetAura* native_widget =
      new DesktopNativeWidgetAura(delegate);
  if (init_params.desktop_window_tree_host) {
    native_widget->SetDesktopWindowTreeHost(
        base::WrapUnique(init_params.desktop_window_tree_host));
  } else {
    native_widget->SetDesktopWindowTreeHost(
        CreateDesktopWindowTreeHost(init_params, delegate, native_widget));
  }
  return native_widget;
}

void MusClient::OnWidgetInitDone(Widget* widget) {
  // Start tracking the widget for accessibility.
  if (ax_remote_host_)
    ax_remote_host_->StartMonitoringWidget(widget);
}

void MusClient::OnCaptureClientSet(
    aura::client::CaptureClient* capture_client) {
  window_tree_client_->capture_synchronizer()->AttachToCaptureClient(
      capture_client);
}

void MusClient::OnCaptureClientUnset(
    aura::client::CaptureClient* capture_client) {
  window_tree_client_->capture_synchronizer()->DetachFromCaptureClient(
      capture_client);
}

void MusClient::AddObserver(MusClientObserver* observer) {
  observer_list_.AddObserver(observer);
}

void MusClient::RemoveObserver(MusClientObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void MusClient::SetMusPropertyMirror(
    std::unique_ptr<MusPropertyMirror> mirror) {
  mus_property_mirror_ = std::move(mirror);
}

void MusClient::CloseAllWidgets() {
  for (aura::Window* root : window_tree_client_->GetRoots()) {
    Widget* widget = Widget::GetWidgetForNativeView(root);
    if (widget)
      widget->CloseNow();
  }
}

std::unique_ptr<DesktopWindowTreeHost> MusClient::CreateDesktopWindowTreeHost(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  std::map<std::string, std::vector<uint8_t>> mus_properties =
      ConfigurePropertiesFromParams(init_params);
  aura::WindowTreeHostMusInitParams window_tree_host_init_params =
      aura::CreateInitParamsForTopLevel(MusClient::Get()->window_tree_client(),
                                        std::move(mus_properties));
  return std::make_unique<DesktopWindowTreeHostMus>(
      std::move(window_tree_host_init_params), delegate,
      desktop_native_widget_aura);
}

void MusClient::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  NOTREACHED();
}

void MusClient::OnLostConnection(aura::WindowTreeClient* client) {}

void MusClient::OnEmbedRootDestroyed(
    aura::WindowTreeHostMus* window_tree_host) {
  static_cast<DesktopWindowTreeHostMus*>(window_tree_host)
      ->ServerDestroyedWindow();
}

void MusClient::OnDisplaysChanged(
    std::vector<ws::mojom::WsDisplayPtr> ws_displays,
    int64_t primary_display_id,
    int64_t internal_display_id,
    int64_t display_id_for_new_windows) {
  if (screen_) {
    screen_->OnDisplaysChanged(std::move(ws_displays), primary_display_id,
                               internal_display_id, display_id_for_new_windows);
  }
}

void MusClient::OnWindowManagerFrameValuesChanged() {
  for (auto& observer : observer_list_)
    observer.OnWindowManagerFrameValuesChanged();
}

aura::PropertyConverter* MusClient::GetPropertyConverter() {
  return property_converter_.get();
}

aura::Window* MusClient::GetWindowAtScreenPoint(const gfx::Point& point) {
  for (aura::Window* root : window_tree_client_->GetRoots()) {
    aura::WindowTreeHost* window_tree_host = root->GetHost();
    if (!window_tree_host)
      continue;
    // TODO: this likely gets z-order wrong. http://crbug.com/663606.
    gfx::Point relative_point(point);
    window_tree_host->ConvertScreenInPixelsToDIP(&relative_point);
    if (gfx::Rect(root->bounds().size()).Contains(relative_point))
      return root->GetEventHandlerForPoint(relative_point);
  }
  return nullptr;
}

}  // namespace views
