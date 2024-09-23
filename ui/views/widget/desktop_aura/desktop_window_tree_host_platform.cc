// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/native_window_occlusion_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/extensions/workspace_extension.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/window_move_client.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone_linux.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/views/corewm/tooltip_lacros.h"
#else
#include "ui/views/corewm/tooltip_aura.h"
#endif

DEFINE_UI_CLASS_PROPERTY_TYPE(views::DesktopWindowTreeHostPlatform*)

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(DesktopWindowTreeHostPlatform*,
                             kHostForRootWindow,
                             nullptr)

namespace {

// A list of all (top-level) windows that have been created but not yet
// destroyed.
std::list<gfx::AcceleratedWidget>* open_windows_ = nullptr;

bool DetermineInactivity(ui::mojom::WindowShowState show_state) {
  if (show_state != ui::mojom::WindowShowState::kDefault &&
      show_state != ui::mojom::WindowShowState::kNormal &&
      show_state != ui::mojom::WindowShowState::kInactive &&
      show_state != ui::mojom::WindowShowState::kMaximized) {
    // It will behave like SHOW_STATE_NORMAL.
    NOTIMPLEMENTED_LOG_ONCE();
  }

  // See comment in PlatformWindow::Show().
  return show_state == ui::mojom::WindowShowState::kInactive;
}

ui::PlatformWindowOpacity GetPlatformWindowOpacity(
    Widget::InitParams::WindowOpacity opacity) {
  switch (opacity) {
    case Widget::InitParams::WindowOpacity::kInferred:
      return ui::PlatformWindowOpacity::kInferOpacity;
    case Widget::InitParams::WindowOpacity::kOpaque:
      return ui::PlatformWindowOpacity::kOpaqueWindow;
    case Widget::InitParams::WindowOpacity::kTranslucent:
      return ui::PlatformWindowOpacity::kTranslucentWindow;
  }
  return ui::PlatformWindowOpacity::kOpaqueWindow;
}

ui::PlatformWindowType GetPlatformWindowType(
    Widget::InitParams::Type window_type) {
  switch (window_type) {
    case Widget::InitParams::TYPE_WINDOW:
      return ui::PlatformWindowType::kWindow;
    case Widget::InitParams::TYPE_MENU:
      return ui::PlatformWindowType::kMenu;
    case Widget::InitParams::TYPE_TOOLTIP:
      return ui::PlatformWindowType::kTooltip;
    case Widget::InitParams::TYPE_DRAG:
      return ui::PlatformWindowType::kDrag;
    case Widget::InitParams::TYPE_BUBBLE:
      return ui::PlatformWindowType::kBubble;
    default:
      return ui::PlatformWindowType::kPopup;
  }
}

ui::PlatformWindowShadowType GetPlatformWindowShadowType(
    Widget::InitParams::ShadowType shadow_type) {
  switch (shadow_type) {
    case Widget::InitParams::ShadowType::kDefault:
      return ui::PlatformWindowShadowType::kDefault;
    case Widget::InitParams::ShadowType::kNone:
      return ui::PlatformWindowShadowType::kNone;
    case Widget::InitParams::ShadowType::kDrop:
      return ui::PlatformWindowShadowType::kDrop;
  }
  NOTREACHED();
}

ui::PlatformWindowInitProperties ConvertWidgetInitParamsToInitProperties(
    const Widget::InitParams& params) {
  ui::PlatformWindowInitProperties properties;
  properties.type = GetPlatformWindowType(params.type);
  properties.accept_events = params.accept_events;
  properties.activatable =
      params.activatable == Widget::InitParams::Activatable::kYes;
  properties.force_show_in_taskbar = params.force_show_in_taskbar;
  properties.z_order = params.EffectiveZOrderLevel();
  properties.keep_on_top = properties.z_order != ui::ZOrderLevel::kNormal;
  properties.is_security_surface =
      properties.z_order == ui::ZOrderLevel::kSecuritySurface;
  properties.visible_on_all_workspaces = params.visible_on_all_workspaces;
  properties.remove_standard_frame = params.remove_standard_frame;
  properties.workspace = params.workspace;
  properties.opacity = GetPlatformWindowOpacity(params.opacity);
  properties.shadow_type = GetPlatformWindowShadowType(params.shadow_type);

  if (params.parent && params.parent->GetHost())
    properties.parent_widget = params.parent->GetHost()->GetAcceleratedWidget();

#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformProperties()
          .set_parent_for_non_top_level_windows) {
    // If context has been set, use that as the parent_widget so that Wayland
    // creates a correct hierarchy of windows.
    if (params.context) {
      auto* host = params.context->GetHost();
      // Use this context as a parent widget iff the host for a root window is
      // set (this happens during OnNativeWidgetCreated). Otherwise, the context
      // can be a native window of a WindowTreeHost created by
      // WindowTreeHost::Create, which we don't want to have as a context (this
      // happens in tests - called by TestScreen, AuraTestHelper,
      // aura::DemoMain, and SitePerProcessBrowserTest).
      if (host && host->window()->GetProperty(kHostForRootWindow))
        properties.parent_widget = host->GetAcceleratedWidget();
    }
  }
  properties.inhibit_keyboard_shortcuts = params.inhibit_keyboard_shortcuts;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Set restore members for windows to know ids upon creation. See the
  // corresponding comment in Widget::InitParams.
  properties.restore_session_id = params.restore_session_id;
  properties.restore_window_id = params.restore_window_id;
  properties.restore_window_id_source = params.restore_window_id_source;
#endif

#if BUILDFLAG(IS_FUCHSIA)
  properties.enable_keyboard = true;
#endif

  return properties;
}

SkPath GetWindowMask(const Widget* widget) {
  if (!widget || !widget->non_client_view()) {
    return SkPath();
  }

  SkPath window_mask;
  // Some frame views define a custom (non-rectanguar) window mask.
  // If so, use it to define the window shape. If not, fall through.
  const_cast<NonClientView*>(widget->non_client_view())
      ->GetWindowMask(widget->GetWindowBoundsInScreen().size(), &window_mask);
  return window_mask;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostPlatform:

DesktopWindowTreeHostPlatform::DesktopWindowTreeHostPlatform(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : native_widget_delegate_(native_widget_delegate->AsWidget()->GetWeakPtr()),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      window_move_client_(this) {}

DesktopWindowTreeHostPlatform::~DesktopWindowTreeHostPlatform() {
  window()->ClearProperty(kHostForRootWindow);
  DCHECK(!platform_window()) << "The host must be closed before destroying it.";
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
  DestroyDispatcher();
}

// static
aura::Window* DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
    gfx::AcceleratedWidget widget) {
  auto* host = DesktopWindowTreeHostPlatform::GetHostForWidget(widget);
  return host ? host->GetContentWindow() : nullptr;
}

// static
DesktopWindowTreeHostPlatform* DesktopWindowTreeHostPlatform::GetHostForWidget(
    gfx::AcceleratedWidget widget) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(widget);
  return host ? host->window()->GetProperty(kHostForRootWindow) : nullptr;
}

// static
std::vector<aura::Window*> DesktopWindowTreeHostPlatform::GetAllOpenWindows() {
  std::vector<aura::Window*> windows(open_windows().size());
  base::ranges::transform(
      open_windows(), windows.begin(),
      DesktopWindowTreeHostPlatform::GetContentWindowForWidget);
  return windows;
}

// static
void DesktopWindowTreeHostPlatform::CleanUpWindowList(
    void (*func)(aura::Window* window)) {
  if (!open_windows_)
    return;
  while (!open_windows_->empty()) {
    gfx::AcceleratedWidget widget = open_windows_->front();
    func(DesktopWindowTreeHostPlatform::GetContentWindowForWidget(widget));
    if (!open_windows_->empty() && open_windows_->front() == widget)
      open_windows_->erase(open_windows_->begin());
  }

  delete open_windows_;
  open_windows_ = nullptr;
}

aura::Window* DesktopWindowTreeHostPlatform::GetContentWindow() {
  return desktop_native_widget_aura_->content_window();
}

void DesktopWindowTreeHostPlatform::Init(const Widget::InitParams& params) {
  if (params.type == Widget::InitParams::TYPE_WINDOW)
    GetContentWindow()->SetProperty(aura::client::kAnimationsDisabledKey, true);

  ui::PlatformWindowInitProperties properties =
      ConvertWidgetInitParamsToInitProperties(params);
  AddAdditionalInitProperties(params, &properties);

#if BUILDFLAG(IS_CHROMEOS)
  // Set persistable based on whether or not the content window is persistable.
  properties.persistable = GetContentWindow()->GetProperty(wm::kPersistableKey);
#endif

  // If we have a parent, record the parent/child relationship. We use this
  // data during destruction to make sure that when we try to close a parent
  // window, we also destroy all child windows.
  if (properties.parent_widget) {
    window_parent_ = DesktopWindowTreeHostPlatform::GetHostForWidget(
        properties.parent_widget);
    DCHECK(window_parent_);
    window_parent_->window_children_.insert(this);
  }

  // Calculate initial bounds.
  properties.bounds = params.bounds;
#if BUILDFLAG(IS_CHROMEOS)
  properties.display_id = params.display_id;
#endif

  // Set extensions delegate.
  DCHECK(!properties.workspace_extension_delegate);
  properties.workspace_extension_delegate = this;

  CreateAndSetPlatformWindow(std::move(properties));

  // Disable compositing on tooltips as a workaround for
  // https://crbug.com/442111.
  CreateCompositor(params.force_software_compositing ||
                   params.type == Widget::InitParams::TYPE_TOOLTIP);

  WindowTreeHost::OnAcceleratedWidgetAvailable();
  InitHost();
  window()->Show();
}

void DesktopWindowTreeHostPlatform::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  window()->SetProperty(kHostForRootWindow, this);
  // This reroutes RunMoveLoop requests to the DesktopWindowTreeHostPlatform.
  // The availability of this feature depends on a platform (PlatformWindow)
  // that implements RunMoveLoop.
  wm::SetWindowMoveClient(window(), &window_move_client_);
  platform_window()->SetUseNativeFrame(params.type ==
                                           Widget::InitParams::TYPE_WINDOW &&
                                       !params.remove_standard_frame);

  native_widget_delegate_->OnNativeWidgetCreated();
}

void DesktopWindowTreeHostPlatform::OnWidgetInitDone() {
  // Once we can guarantee |NonClientView| is created OnWidgetInitDone,
  // UpdateWindowTransparency and FillsBoundsCompletely accordingly.
  desktop_native_widget_aura_->UpdateWindowTransparency();
  GetContentWindow()->SetFillsBoundsCompletely(
      GetWindowMaskForClipping().isEmpty());
}

void DesktopWindowTreeHostPlatform::OnActiveWindowChanged(bool active) {
#if BUILDFLAG(IS_OZONE)
  // When bubbles are accelerated widgets, `window_children_` can contain a
  // bubble where `bubble->is_active_` is true, while `this->is_active_` is
  // false.
  // When WindowFocusedFromInputEvent makes a window of this tree host
  // active, we enter this condition where `active && !is_active_` because
  // subwindows do not get active state from the OS on some platforms (E.g.
  // Wayland). So we need to ensure Activate is conveyed to the platform_window.
  if (base::FeatureList::IsEnabled(features::kOzoneBubblesUsePlatformWidgets) &&
      active && !is_active_ && !window_children_.empty()) {
    Activate();
  }
#endif
}

std::unique_ptr<corewm::Tooltip>
DesktopWindowTreeHostPlatform::CreateTooltip() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<corewm::TooltipLacros>();
#else
  return std::make_unique<corewm::TooltipAura>();
#endif
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostPlatform::CreateDragDropClient() {
  ui::WmDragHandler* drag_handler = ui::GetWmDragHandler(*(platform_window()));
  std::unique_ptr<DesktopDragDropClientOzone> drag_drop_client =
#if BUILDFLAG(IS_LINUX)
      std::make_unique<DesktopDragDropClientOzoneLinux>(window(), drag_handler);
#else
      std::make_unique<DesktopDragDropClientOzone>(window(), drag_handler);
#endif  // BUILDFLAG(IS_LINUX)
  // Set a class property key, which allows |drag_drop_client| to be used for
  // drop action.
  SetWmDropHandler(platform_window(), drag_drop_client.get());
  return std::move(drag_drop_client);
}

void DesktopWindowTreeHostPlatform::Close() {
  // If we are in process of closing or the PlatformWindow has already been
  // closed, do nothing.
  if (close_widget_factory_.HasWeakPtrs() || !platform_window())
    return;

  GetContentWindow()->Hide();

  // Hide while waiting for the close.
  // Please note that it's better to call WindowTreeHost::Hide, which also calls
  // PlatformWindow::Hide and Compositor::SetVisible(false).
  Hide();

  // And we delay the close so that if we are called from an ATL callback,
  // we don't destroy the window before the callback returned (as the caller
  // may delete ourselves on destroy and the ATL callback would still
  // dereference us when the callback returns).
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DesktopWindowTreeHostPlatform::CloseNow,
                                close_widget_factory_.GetWeakPtr()));
}

void DesktopWindowTreeHostPlatform::CloseNow() {
  if (!platform_window())
    return;

#if BUILDFLAG(IS_OZONE)
  SetWmDropHandler(platform_window(), nullptr);
#endif

  platform_window()->PrepareForShutdown();

  ReleaseCapture();
  if (native_widget_delegate_)
    native_widget_delegate_->OnNativeWidgetDestroying();

  // If we have children, close them. Use a copy for iteration because they'll
  // remove themselves.
  std::set<raw_ptr<DesktopWindowTreeHostPlatform, SetExperimental>>
      window_children_copy = window_children_;
  for (DesktopWindowTreeHostPlatform* child : window_children_copy) {
    child->CloseNow();
  }
  DCHECK(window_children_.empty());

  // If we have a parent, remove ourselves from its children list.
  if (window_parent_) {
    window_parent_->window_children_.erase(this);
    window_parent_ = nullptr;
  }

  // Destroy the compositor before destroying the |platform_window()| since
  // shutdown may try to swap, and the swap without a window may cause an error
  // in X Server or Wayland, which causes a crash with in-process renderer, for
  // example.
  DestroyCompositor();

  platform_window()->Close();
}

aura::WindowTreeHost* DesktopWindowTreeHostPlatform::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostPlatform::Show(ui::mojom::WindowShowState show_state,
                                         const gfx::Rect& restore_bounds) {
  OnAcceleratedWidgetMadeVisible(true);
  if (compositor())
    SetVisible(true);

  platform_window()->Show(DetermineInactivity(show_state));

  switch (show_state) {
    case ui::mojom::WindowShowState::kMaximized:
      platform_window()->Maximize();
      if (!restore_bounds.IsEmpty()) {
        // Enforce |restored_bounds_in_pixels_| since calling Maximize() could
        // have reset it.
        platform_window()->SetRestoredBoundsInDIP(restore_bounds);
      }
      break;
    case ui::mojom::WindowShowState::kMinimized:
      platform_window()->Minimize();
      break;
    case ui::mojom::WindowShowState::kFullscreen:
      SetFullscreen(true, display::kInvalidDisplayId);
      break;
    default:
      break;
  }

  if (native_widget_delegate_->CanActivate()) {
    if (show_state != ui::mojom::WindowShowState::kInactive &&
        show_state != ui::mojom::WindowShowState::kMinimized) {
      Activate();
    }

    // SetInitialFocus() should be always be called, even for
    // SHOW_STATE_INACTIVE. If the window has to stay inactive, the method will
    // do the right thing.
    // Activate() might fail if the window is non-activatable. In this case, we
    // should pass SHOW_STATE_INACTIVE to SetInitialFocus() to stop the initial
    // focused view from getting focused. See https://crbug.com/515594 for
    // example.
    native_widget_delegate_->SetInitialFocus(
        IsActive() ? show_state : ui::mojom::WindowShowState::kInactive);
  }

  // compositor()->SetVisible(true) might have already led to content_window
  // Show() via OnCompositorVisibilityChanging(). Calling Show() a second time
  // has side effects, so skip it.
  if (!GetContentWindow()->IsVisible()) {
    GetContentWindow()->Show();
  }
}

bool DesktopWindowTreeHostPlatform::IsVisible() const {
  return platform_window() && platform_window()->IsVisible();
}

void DesktopWindowTreeHostPlatform::SetSize(const gfx::Size& size) {
  auto bounds_in_dip = platform_window()->GetBoundsInDIP();
  bounds_in_dip.set_size(size);
  platform_window()->SetBoundsInDIP(bounds_in_dip);
}

void DesktopWindowTreeHostPlatform::StackAbove(aura::Window* window) {
  if (!window || !window->GetRootWindow())
    return;

  platform_window()->StackAbove(window->GetHost()->GetAcceleratedWidget());
}

void DesktopWindowTreeHostPlatform::StackAtTop() {
  platform_window()->StackAtTop();
}

bool DesktopWindowTreeHostPlatform::IsStackedAbove(aura::Window* window) {
  // TODO(crbug.com/40238598) Implement Window layer check
  NOTREACHED();
}

void DesktopWindowTreeHostPlatform::CenterWindow(const gfx::Size& size) {
  gfx::Rect parent_bounds = GetWorkAreaBoundsInScreen();

  // If |window_|'s transient parent bounds are big enough to contain |size|,
  // use them instead.
  if (wm::GetTransientParent(GetContentWindow())) {
    gfx::Rect transient_parent_rect =
        wm::GetTransientParent(GetContentWindow())->GetBoundsInScreen();
    // Consider using the intersect of the work area.
    if (transient_parent_rect.height() >= size.height() &&
        transient_parent_rect.width() >= size.width()) {
      parent_bounds = transient_parent_rect;
    }
  }

  gfx::Rect window_bounds_in_screen = parent_bounds;
  window_bounds_in_screen.ClampToCenteredSize(size);

  SetBoundsInDIP(window_bounds_in_screen);
}

void DesktopWindowTreeHostPlatform::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  *bounds = GetRestoredBounds();

  if (IsFullscreen())
    *show_state = ui::mojom::WindowShowState::kFullscreen;
  else if (IsMinimized())
    *show_state = ui::mojom::WindowShowState::kMinimized;
  else if (IsMaximized())
    *show_state = ui::mojom::WindowShowState::kMaximized;
  else if (!IsActive())
    *show_state = ui::mojom::WindowShowState::kInactive;
  else
    *show_state = ui::mojom::WindowShowState::kNormal;
}

gfx::Rect DesktopWindowTreeHostPlatform::GetWindowBoundsInScreen() const {
  return platform_window()->GetBoundsInDIP();
}

gfx::Rect DesktopWindowTreeHostPlatform::GetClientAreaBoundsInScreen() const {
  // Attempts to calculate the rect by asking the NonClientFrameView what it
  // thought its GetBoundsForClientView() were broke combobox drop down
  // placement.
  return GetWindowBoundsInScreen();
}

gfx::Rect DesktopWindowTreeHostPlatform::GetRestoredBounds() const {
  // We can't reliably track the restored bounds of a window, but we can get
  // the 90% case down. When *chrome* is the process that requests maximizing
  // or restoring bounds, we can record the current bounds before we request
  // maximization, and clear it when we detect a state change.
  gfx::Rect restored_bounds = platform_window()->GetRestoredBoundsInDIP();

  // When window is resized, |restored bounds| is not set and empty.
  // If |restored bounds| is empty, it returns the current window size.
  if (!restored_bounds.IsEmpty())
    return restored_bounds;

  return GetWindowBoundsInScreen();
}

std::string DesktopWindowTreeHostPlatform::GetWorkspace() const {
  auto* workspace_extension = ui::GetWorkspaceExtension(*platform_window());
  return workspace_extension ? workspace_extension->GetWorkspace()
                             : std::string();
}

gfx::Rect DesktopWindowTreeHostPlatform::GetWorkAreaBoundsInScreen() const {
  return GetDisplayNearestRootWindow().work_area();
}

void DesktopWindowTreeHostPlatform::SetShape(
    std::unique_ptr<Widget::ShapeRects> native_shape) {
  // TODO(crbug.com/40737127) : When supporting PlatformWindow::SetShape,
  // Calls ui::Layer::SetAlphaShape and sets |is_shape_explicitly_set_| to true.
  platform_window()->SetShape(std::move(native_shape), GetRootTransform());
}

void DesktopWindowTreeHostPlatform::SetParent(gfx::AcceleratedWidget parent) {
  // TODO(crbug.com/40284685): hook parent to the accelerated widget.
  if (window_parent_) {
    window_parent_->window_children_.erase(this);
  }
  window_parent_ = DesktopWindowTreeHostPlatform::GetHostForWidget(parent);
  if (window_parent_) {
    window_parent_->window_children_.insert(this);
  }
}

void DesktopWindowTreeHostPlatform::Activate() {
  platform_window()->Activate();
}

void DesktopWindowTreeHostPlatform::Deactivate() {
  ReleaseCapture();
  platform_window()->Deactivate();
}

bool DesktopWindowTreeHostPlatform::IsActive() const {
  return is_active_;
}

void DesktopWindowTreeHostPlatform::Maximize() {
  platform_window()->Maximize();
  if (IsMinimized())
    Show(ui::mojom::WindowShowState::kNormal, gfx::Rect());
}

void DesktopWindowTreeHostPlatform::Minimize() {
  ReleaseCapture();
  platform_window()->Minimize();
}

void DesktopWindowTreeHostPlatform::Restore() {
  platform_window()->Restore();
  Show(ui::mojom::WindowShowState::kNormal, gfx::Rect());
}

bool DesktopWindowTreeHostPlatform::IsMaximized() const {
  return platform_window()->GetPlatformWindowState() ==
         ui::PlatformWindowState::kMaximized;
}

bool DesktopWindowTreeHostPlatform::IsMinimized() const {
  return platform_window()->GetPlatformWindowState() ==
         ui::PlatformWindowState::kMinimized;
}

bool DesktopWindowTreeHostPlatform::HasCapture() const {
  return platform_window()->HasCapture();
}

void DesktopWindowTreeHostPlatform::SetZOrderLevel(ui::ZOrderLevel order) {
  platform_window()->SetZOrderLevel(order);
}

ui::ZOrderLevel DesktopWindowTreeHostPlatform::GetZOrderLevel() const {
  return platform_window()->GetZOrderLevel();
}

void DesktopWindowTreeHostPlatform::SetVisibleOnAllWorkspaces(
    bool always_visible) {
  auto* workspace_extension = ui::GetWorkspaceExtension(*platform_window());
  if (workspace_extension)
    workspace_extension->SetVisibleOnAllWorkspaces(always_visible);
}

bool DesktopWindowTreeHostPlatform::IsVisibleOnAllWorkspaces() const {
  auto* workspace_extension = ui::GetWorkspaceExtension(*platform_window());
  return workspace_extension ? workspace_extension->IsVisibleOnAllWorkspaces()
                             : false;
}

bool DesktopWindowTreeHostPlatform::SetWindowTitle(
    const std::u16string& title) {
  if (window_title_ == title)
    return false;

  window_title_ = title;
  platform_window()->SetTitle(window_title_);
  return true;
}

void DesktopWindowTreeHostPlatform::ClearNativeFocus() {
  // This method is weird and misnamed. Instead of clearing the native focus,
  // it sets the focus to our content_window, which will trigger a cascade
  // of focus changes into views.
  if (GetContentWindow() && aura::client::GetFocusClient(GetContentWindow()) &&
      GetContentWindow()->Contains(
          aura::client::GetFocusClient(GetContentWindow())
              ->GetFocusedWindow())) {
    aura::client::GetFocusClient(GetContentWindow())
        ->FocusWindow(GetContentWindow());
  }
}

bool DesktopWindowTreeHostPlatform::IsMoveLoopSupported() const {
  return platform_window()->IsClientControlledWindowMovementSupported();
}

Widget::MoveLoopResult DesktopWindowTreeHostPlatform::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  auto* move_loop_handler = ui::GetWmMoveLoopHandler(*platform_window());
  if (move_loop_handler && move_loop_handler->RunMoveLoop(drag_offset))
    return Widget::MoveLoopResult::kSuccessful;
  return Widget::MoveLoopResult::kCanceled;
}

void DesktopWindowTreeHostPlatform::EndMoveLoop() {
  auto* move_loop_handler = ui::GetWmMoveLoopHandler(*platform_window());
  if (move_loop_handler)
    move_loop_handler->EndMoveLoop();
}

void DesktopWindowTreeHostPlatform::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  platform_window()->SetVisibilityChangedAnimationsEnabled(value);
  if (desktop_native_widget_aura_->widget_type() !=
      Widget::InitParams::TYPE_WINDOW) {
    GetContentWindow()->SetProperty(aura::client::kAnimationsDisabledKey,
                                    !value);
  }
}

std::unique_ptr<NonClientFrameView>
DesktopWindowTreeHostPlatform::CreateNonClientFrameView() {
  return ShouldUseNativeFrame() ? std::make_unique<NativeFrameView>(GetWidget())
                                : nullptr;
}

bool DesktopWindowTreeHostPlatform::ShouldUseNativeFrame() const {
  return platform_window()->ShouldUseNativeFrame();
}

bool DesktopWindowTreeHostPlatform::ShouldWindowContentsBeTransparent() const {
  return platform_window()->ShouldWindowContentsBeTransparent() ||
         !(GetWindowMaskForClipping().isEmpty());
}

void DesktopWindowTreeHostPlatform::FrameTypeChanged() {
  if (!native_widget_delegate_) {
    return;
  }
  Widget::FrameType new_type =
      native_widget_delegate_->AsWidget()->frame_type();
  if (new_type == Widget::FrameType::kDefault) {
    // The default is determined by Widget::InitParams::remove_standard_frame
    // and does not change.
    return;
  }
  platform_window()->SetUseNativeFrame(new_type ==
                                       Widget::FrameType::kForceNative);

  // Replace the frame and layout the contents. Even though we don't have a
  // swappable glass frame like on Windows, we still replace the frame because
  // the button assets don't update otherwise.
  if (GetWidget()->non_client_view())
    GetWidget()->non_client_view()->UpdateFrame();
}

void DesktopWindowTreeHostPlatform::SetFullscreen(bool fullscreen,
                                                  int64_t target_display_id) {
  auto weak_ptr = GetWeakPtr();
  platform_window()->SetFullscreen(fullscreen, target_display_id);
  if (!weak_ptr)
    return;

  if (!base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
    // The state must change synchronously to let media react on fullscreen
    // changes.
    DCHECK_EQ(fullscreen, IsFullscreen());

    if (IsFullscreen() == fullscreen) {
      ScheduleRelayout();
    }
    // Else: the widget will be relaid out either when the window bounds change
    // or when |platform_window|'s fullscreen state changes.
  }
}

bool DesktopWindowTreeHostPlatform::IsFullscreen() const {
  return ui::IsPlatformWindowStateFullscreen(
      platform_window()->GetPlatformWindowState());
}

void DesktopWindowTreeHostPlatform::SetOpacity(float opacity) {
  GetContentWindow()->layer()->SetOpacity(opacity);
  platform_window()->SetOpacity(opacity);
}

void DesktopWindowTreeHostPlatform::SetAspectRatio(
    const gfx::SizeF& aspect_ratio,
    const gfx::Size& excluded_margin) {
  // TODO(crbug.com/40887946): send `excluded_margin`.
  if (excluded_margin.width() > 0 || excluded_margin.height() > 0) {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  platform_window()->SetAspectRatio(aspect_ratio);
}

void DesktopWindowTreeHostPlatform::SetWindowIcons(
    const gfx::ImageSkia& window_icon,
    const gfx::ImageSkia& app_icon) {
  platform_window()->SetWindowIcons(window_icon, app_icon);
}

void DesktopWindowTreeHostPlatform::InitModalType(
    ui::mojom::ModalType modal_type) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopWindowTreeHostPlatform::FlashFrame(bool flash_frame) {
  platform_window()->FlashFrame(flash_frame);
}

bool DesktopWindowTreeHostPlatform::IsAnimatingClosed() const {
  return platform_window()->IsAnimatingClosed();
}

void DesktopWindowTreeHostPlatform::SizeConstraintsChanged() {
  platform_window()->SizeConstraintsChanged();
}

bool DesktopWindowTreeHostPlatform::ShouldUpdateWindowTransparency() const {
  return true;
}

bool DesktopWindowTreeHostPlatform::ShouldUseDesktopNativeCursorManager()
    const {
  return true;
}

bool DesktopWindowTreeHostPlatform::ShouldCreateVisibilityController() const {
  return true;
}

void DesktopWindowTreeHostPlatform::UpdateWindowShapeIfNeeded(
    const ui::PaintContext& context) {
  if (is_shape_explicitly_set_)
    return;

  SkPath clip_path = GetWindowMaskForClipping();
  if (clip_path.isEmpty())
    return;

  ui::PaintRecorder recorder(context, GetWindowBoundsInScreen().size());
  recorder.canvas()->ClipPath(clip_path, true);
}

void DesktopWindowTreeHostPlatform::SetBoundsInDIP(const gfx::Rect& bounds) {
  platform_window()->SetBoundsInDIP(bounds);
}

void DesktopWindowTreeHostPlatform::SetAllowScreenshots(bool allow) {
  NOTIMPLEMENTED();
}

bool DesktopWindowTreeHostPlatform::AreScreenshotsAllowed() {
  NOTIMPLEMENTED();
  return true;
}

gfx::Transform DesktopWindowTreeHostPlatform::GetRootTransform() const {
  // TODO(crbug.com/40218466): This can use wrong scale during initialization.
  // Revisit this as a part of 'use dip' work.

  // This might be called before the |platform_window| is created. Thus,
  // explicitly check if that exists before trying to access its visibility and
  // the display where it is shown.
  const aura::Window* root_window = nullptr;
  if (platform_window()) {
    root_window = window();
  } else if (window_parent_) {
    root_window = window_parent_->window();
  }

  auto* const screen = display::Screen::GetScreen();
  const float scale = root_window
                          ? screen
                                ->GetPreferredScaleFactorForWindow(
                                    const_cast<aura::Window*>(root_window))
                                .value_or(1.0f)
                          : screen->GetPrimaryDisplay().device_scale_factor();

  gfx::Transform transform;
  transform.Scale(scale, scale);
  return transform;
}

void DesktopWindowTreeHostPlatform::ShowImpl() {
  Show(ui::mojom::WindowShowState::kNormal, gfx::Rect());
}

void DesktopWindowTreeHostPlatform::HideImpl() {
  WindowTreeHostPlatform::HideImpl();
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(false);
}

gfx::Rect DesktopWindowTreeHostPlatform::CalculateRootWindowBounds() const {
  return gfx::Rect(platform_window()->GetBoundsInDIP().size());
}

gfx::Rect DesktopWindowTreeHostPlatform::GetBoundsInDIP() const {
  return platform_window()->GetBoundsInDIP();
}

void DesktopWindowTreeHostPlatform::OnCompositorVisibilityChanging(
    ui::Compositor* compositor,
    bool visible) {
  // Make sure to show the content window before the compositor has become
  // visible.
  if (visible) {
    // The UI compositor may not have a valid local surface ID if it was set to
    // invalid during eviction. This can happen if native occlusion is enabled.
    // Here we ensure the invariant that a visible UI compositor will always
    // have a valid local surface ID.
    if (!window()->GetLocalSurfaceId().is_valid()) {
      window()->AllocateLocalSurfaceId();
      compositor->SetLocalSurfaceIdFromParent(window()->GetLocalSurfaceId());
    }
    GetContentWindow()->Show();
  }
}

void DesktopWindowTreeHostPlatform::OnCompositorVisibilityChanged(
    ui::Compositor* compositor,
    bool visible) {
  // Make sure to hide the content window after the compositor has become
  // not visible.
  if (!visible) {
    GetContentWindow()->Hide();
  }
}

gfx::Insets DesktopWindowTreeHostPlatform::CalculateInsetsInDIP(
    ui::PlatformWindowState window_state) const {
  return GetWidget()->GetCustomInsetsInDIP();
}

void DesktopWindowTreeHostPlatform::OnClosed() {
  open_windows().remove(GetAcceleratedWidget());
  wm::SetWindowMoveClient(window(), nullptr);
  SetWmDropHandler(platform_window(), nullptr);
  desktop_native_widget_aura_->OnHostWillClose();
  SetPlatformWindow(nullptr);
  desktop_native_widget_aura_->OnHostClosed();
}

void DesktopWindowTreeHostPlatform::OnWindowStateChanged(
    ui::PlatformWindowState old_state,
    ui::PlatformWindowState new_state) {
  bool was_minimized = old_state == ui::PlatformWindowState::kMinimized;
  bool is_minimized = new_state == ui::PlatformWindowState::kMinimized;

  // Propagate minimization/restore to compositor to avoid drawing 'blank'
  // frames that could be treated as previews, which show content even if a
  // window is minimized.
  if (!aura::NativeWindowOcclusionTracker::
          IsNativeWindowOcclusionTrackingAlwaysEnabled(this) &&
      is_minimized != was_minimized) {
    if (is_minimized) {
      SetVisible(false);
    } else {
      SetVisible(true);
    }
  }

  // Now that we have different window properties, we may need to relayout the
  // window. (The windows code doesn't need this because their window change is
  // synchronous.)
  ScheduleRelayout();
  GetWidget()->OnNativeWidgetWindowShowStateChanged();
}

void DesktopWindowTreeHostPlatform::OnCloseRequest() {
  GetWidget()->Close();
}

void DesktopWindowTreeHostPlatform::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  DCHECK(!base::Contains(open_windows(), widget));
  open_windows().push_front(widget);
  aura::WindowTreeHostPlatform::OnAcceleratedWidgetAvailable(widget);
}

void DesktopWindowTreeHostPlatform::OnWillDestroyAcceleratedWidget() {
  desktop_native_widget_aura_->OnHostWillClose();
}

bool DesktopWindowTreeHostPlatform::OnRotateFocus(
    ui::PlatformWindowDelegate::RotateDirection direction,
    bool reset) {
  return DesktopWindowTreeHostPlatform::RotateFocusForWidget(*GetWidget(),
                                                             direction, reset);
}

void DesktopWindowTreeHostPlatform::OnActivationChanged(bool active) {
  if (active) {
    auto widget = GetAcceleratedWidget();
    open_windows().remove(widget);
    open_windows().insert(open_windows().begin(), widget);
  }
  if (is_active_ == active)
    return;
  is_active_ = active;
  aura::WindowTreeHostPlatform::OnActivationChanged(active);
  desktop_native_widget_aura_->HandleActivationChanged(active);
  ScheduleRelayout();
}

std::optional<gfx::Size>
DesktopWindowTreeHostPlatform::GetMinimumSizeForWindow() const {
  return native_widget_delegate_->GetMinimumSize();
}

std::optional<gfx::Size>
DesktopWindowTreeHostPlatform::GetMaximumSizeForWindow() const {
  return native_widget_delegate_->GetMaximumSize();
}

bool DesktopWindowTreeHostPlatform::CanMaximize() const {
  return GetWidget()->widget_delegate()->CanMaximize();
}

bool DesktopWindowTreeHostPlatform::CanFullscreen() const {
  return GetWidget()->widget_delegate()->CanFullscreen();
}

SkPath DesktopWindowTreeHostPlatform::GetWindowMaskForWindowShapeInPixels() {
  SkPath window_mask = GetWindowMask(GetWidget());
  // Convert SkPath in DIPs to pixels.
  if (!window_mask.isEmpty()) {
    window_mask.transform(
        gfx::TransformToFlattenedSkMatrix(GetRootTransform()));
  }
  return window_mask;
}

std::optional<ui::OwnedWindowAnchor>
DesktopWindowTreeHostPlatform::GetOwnedWindowAnchorAndRectInDIP() {
  const auto* anchor =
      GetContentWindow()->GetProperty(aura::client::kOwnedWindowAnchor);
  return anchor ? std::make_optional(*anchor) : std::nullopt;
}

gfx::Rect DesktopWindowTreeHostPlatform::ConvertRectToPixels(
    const gfx::Rect& rect_in_dip) const {
  return ToPixelRect(rect_in_dip);
}

gfx::Rect DesktopWindowTreeHostPlatform::ConvertRectToDIP(
    const gfx::Rect& rect_in_pixels) const {
  return ToDIPRect(rect_in_pixels);
}

gfx::PointF DesktopWindowTreeHostPlatform::ConvertScreenPointToLocalDIP(
    const gfx::Point& screen_in_pixels) const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // lacros should not use this.
  NOTREACHED();
#else
  // TODO(crbug.com/40222832): DIP should use gfx::PointF. Fix this as
  // a part of cleanup work(crbug.com/1318279).
  gfx::Point local_dip(screen_in_pixels);
  ConvertScreenInPixelsToDIP(&local_dip);
  return gfx::PointF(local_dip);
#endif
}

gfx::Insets DesktopWindowTreeHostPlatform::ConvertInsetsToPixels(
    const gfx::Insets& insets_dip) const {
  auto scale = GetRootTransform().To2dScale();
  return gfx::ScaleToCeiledInsets(insets_dip, scale.x(), scale.y());
}

void DesktopWindowTreeHostPlatform::OnWorkspaceChanged() {
  OnHostWorkspaceChanged();
}

gfx::Rect DesktopWindowTreeHostPlatform::ToDIPRect(
    const gfx::Rect& rect_in_pixels) const {
  return GetRootTransform()
      .InverseMapRect(rect_in_pixels)
      .value_or(rect_in_pixels);
}

gfx::Rect DesktopWindowTreeHostPlatform::ToPixelRect(
    const gfx::Rect& rect_in_dip) const {
  gfx::RectF rect_in_pixels_f =
      GetRootTransform().MapRect(gfx::RectF(rect_in_dip));
  // Due to the limitation of IEEE floating point representation and rounding
  // error, the converted result may become slightly larger than expected value,
  // such as 3000.0005. Allow error to round down in such case. This is
  // also used in cc/viz. See crbug.com/1418606.
  return gfx::ToEnclosingRectIgnoringError(rect_in_pixels_f);
}

Widget* DesktopWindowTreeHostPlatform::GetWidget() {
  return native_widget_delegate_ ? native_widget_delegate_->AsWidget()
                                 : nullptr;
}

const Widget* DesktopWindowTreeHostPlatform::GetWidget() const {
  return native_widget_delegate_ ? native_widget_delegate_->AsWidget()
                                 : nullptr;
}

views::corewm::TooltipController*
DesktopWindowTreeHostPlatform::tooltip_controller() {
  return desktop_native_widget_aura_->tooltip_controller();
}

void DesktopWindowTreeHostPlatform::ScheduleRelayout() {
  if (!native_widget_delegate_) {
    return;
  }
  Widget* widget = native_widget_delegate_->AsWidget();
  NonClientView* non_client_view = widget->non_client_view();
  // non_client_view may be NULL, especially during creation.
  if (non_client_view) {
    if (non_client_view->frame_view())
      non_client_view->frame_view()->InvalidateLayout();
    non_client_view->client_view()->InvalidateLayout();
    non_client_view->InvalidateLayout();
    // Once |NonClientView| is invalidateLayout,
    // UpdateWindowTransparency and FillsBoundsCompletely accordingly.
    desktop_native_widget_aura_->UpdateWindowTransparency();
    GetContentWindow()->SetFillsBoundsCompletely(
        GetWindowMaskForClipping().isEmpty());
  }
}

void DesktopWindowTreeHostPlatform::SetVisible(bool visible) {
  if (compositor())
    compositor()->SetVisible(visible);

  native_widget_delegate_->OnNativeWidgetVisibilityChanged(visible);
}

void DesktopWindowTreeHostPlatform::AddAdditionalInitProperties(
    const Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {}

SkPath DesktopWindowTreeHostPlatform::GetWindowMaskForClipping() const {
  if (!platform_window()->ShouldUpdateWindowShape())
    return SkPath();
  return GetWindowMask(GetWidget());
}

display::Display DesktopWindowTreeHostPlatform::GetDisplayNearestRootWindow()
    const {
  DCHECK(window());
  DCHECK(window()->IsRootWindow());
  // TODO(sky): GetDisplayNearestWindow() should take a const aura::Window*.
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      const_cast<aura::Window*>(window()));
}

bool DesktopWindowTreeHostPlatform::RotateFocusForWidget(
    Widget& widget,
    ui::PlatformWindowDelegate::RotateDirection direction,
    bool reset) {
  if (reset) {
    widget.GetFocusManager()->ClearFocus();
  }
  auto focus_manager_direction =
      direction == RotateDirection::kForward
          ? views::FocusManager::Direction::kForward
          : views::FocusManager::Direction::kBackward;
  auto wrapping = reset ? views::FocusManager::FocusCycleWrapping::kEnabled
                        : views::FocusManager::FocusCycleWrapping::kDisabled;
  return widget.GetFocusManager()->RotatePaneFocus(focus_manager_direction,
                                                   wrapping);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHost:

// Linux subclasses this host and adds some Linux specific bits.
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostPlatform(native_widget_delegate,
                                           desktop_native_widget_aura);
}
#endif

// static
std::list<gfx::AcceleratedWidget>&
DesktopWindowTreeHostPlatform::open_windows() {
  if (!open_windows_)
    open_windows_ = new std::list<gfx::AcceleratedWidget>();
  return *open_windows_;
}

// static
bool DesktopWindowTreeHostPlatform::has_open_windows() {
  return !!open_windows_;
}

}  // namespace views
