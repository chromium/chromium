// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_aura.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/buildflags.h"
#include "ui/views/drag_utils.h"
#include "ui/views/views_delegate.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/focus_manager_event_handler.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager_aura.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/window_reorderer.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/scoped_animation_disabler.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/window_move_client.h"

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_AURA) && BUILDFLAG(IS_OZONE)
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#endif

DEFINE_UI_CLASS_PROPERTY_TYPE(views::internal::NativeWidgetPrivate*)

namespace views {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(internal::NativeWidgetPrivate*,
                             kNativeWidgetPrivateKey,
                             nullptr)

void SetRestoreBounds(aura::Window* window, const gfx::Rect& bounds) {
  window->SetProperty(aura::client::kRestoreBoundsKey, bounds);
}

void SetIcon(aura::Window* window,
             const aura::WindowProperty<gfx::ImageSkia*>* key,
             const gfx::ImageSkia& value) {
  if (value.isNull())
    window->ClearProperty(key);
  else
    window->SetProperty(key, value);
}

bool FindLayersInOrder(
    const std::vector<raw_ptr<ui::Layer, VectorExperimental>>& children,
    const ui::Layer** first,
    const ui::Layer** second) {
  for (const ui::Layer* child : children) {
    if (child == *second) {
      *second = nullptr;
      return *first == nullptr;
    }

    if (child == *first)
      *first = nullptr;

    if (FindLayersInOrder(child->children(), first, second))
      return true;

    // If second is cleared without success, exit early with failure.
    if (!*second)
      return false;
  }
  return false;
}

// Adds `window` as a child of `parent`. If `parent` is nullptr, find an
// appropriate parent by consulting an implementation of WindowParentingClient
// attached at the root Window of the tree where `window` lives.
void ReparentAuraWindow(aura::Window* window, aura::Window* parent) {
  if (parent) {
    parent->AddChild(window);
  } else {
    // The following looks weird, but it's the equivalent of what aura has
    // always done. (The previous behaviour of aura::Window::SetParent() used
    // NULL as a special value that meant ask the WindowParentingClient where
    // things should go.)
    //
    // This probably isn't strictly correct, but its an invariant that a Window
    // in use will be attached to a RootWindow, so we can't just call
    // RemoveChild here. The only possible thing that could assign a RootWindow
    // in this case is the stacking client of the current RootWindow. This
    // matches our previous behaviour; the global stacking client would almost
    // always reattach the window to the same RootWindow.
    aura::Window* root_window = window->GetRootWindow();
    aura::client::ParentWindowWithContext(window, root_window,
                                          root_window->GetBoundsInScreen(),
                                          display::kInvalidDisplayId);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, public:

NativeWidgetAura::NativeWidgetAura(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate->AsWidget()->GetWeakPtr()),
      window_(new aura::Window(this, aura::client::WINDOW_TYPE_UNKNOWN)) {
  aura::client::SetFocusChangeObserver(window_, this);
  wm::SetActivationChangeObserver(window_, this);
}

// static
void NativeWidgetAura::RegisterNativeWidgetForWindow(
    internal::NativeWidgetPrivate* native_widget,
    aura::Window* window) {
  window->SetProperty(kNativeWidgetPrivateKey, native_widget);
}

// static
void NativeWidgetAura::AssignIconToAuraWindow(aura::Window* window,
                                              const gfx::ImageSkia& window_icon,
                                              const gfx::ImageSkia& app_icon) {
  if (window) {
    SetIcon(window, aura::client::kWindowIconKey, window_icon);
    SetIcon(window, aura::client::kAppIconKey, app_icon);
  }
}

// static
void NativeWidgetAura::SetShadowElevationFromInitParams(
    aura::Window* window,
    const Widget::InitParams& params) {
  if (params.shadow_type == Widget::InitParams::ShadowType::kNone) {
    wm::SetShadowElevation(window, wm::kShadowElevationNone);
  } else if (params.shadow_type == Widget::InitParams::ShadowType::kDrop &&
             params.shadow_elevation) {
    wm::SetShadowElevation(window, *params.shadow_elevation);
  }
}

// static
void NativeWidgetAura::SetResizeBehaviorFromDelegate(WidgetDelegate* delegate,
                                                     aura::Window* window) {
  if (!window)
    return;

  int behavior = aura::client::kResizeBehaviorNone;
  if (delegate) {
    if (delegate->CanResize())
      behavior |= aura::client::kResizeBehaviorCanResize;
    if (delegate->CanMaximize())
      behavior |= aura::client::kResizeBehaviorCanMaximize;
    if (delegate->CanMinimize())
      behavior |= aura::client::kResizeBehaviorCanMinimize;
    if (delegate->CanFullscreen()) {
      behavior |= aura::client::kResizeBehaviorCanFullscreen;
    }
  }
  window->SetProperty(aura::client::kResizeBehaviorKey, behavior);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, internal::NativeWidgetPrivate implementation:

void NativeWidgetAura::InitNativeWidget(Widget::InitParams params) {
  // See Widget::InitParams::context for details.
  DCHECK(params.parent || params.context);

  ownership_ = params.ownership;
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    owned_delegate_ = base::WrapUnique(delegate_.get());

  window_->AcquireAllPropertiesFrom(
      std::move(params.init_properties_container));

  RegisterNativeWidgetForWindow(this, window_);
  window_->SetType(GetAuraWindowTypeForWidgetType(params.type));
  if (params.corner_radius) {
    window_->SetProperty(aura::client::kWindowCornerRadiusKey,
                         *params.corner_radius);
  }
  window_->SetProperty(aura::client::kShowStateKey, params.show_state);

  int desk_index;
  // Set workspace property of this window created with a specified workspace
  // in InitParams. The desk index can be kActiveWorkspace=-1, representing
  // an active desk. If the window is visible on all workspaces, it belongs on
  // the active desk.
  if (params.visible_on_all_workspaces) {
    window_->SetProperty(aura::client::kWindowWorkspaceKey,
                         aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  } else if (const base::Uuid& desk_uuid =
                 base::Uuid::ParseLowercase(params.workspace);
             desk_uuid.is_valid()) {
    window_->SetProperty(aura::client::kDeskUuidKey,
                         desk_uuid.AsLowercaseString());
  } else if (base::StringToInt(params.workspace, &desk_index)) {
    // `params.workspace` used to be the desk index, it now stores the desk
    // Uuid. We still check to see if it is the index for compatibility.
    window_->SetProperty(aura::client::kWindowWorkspaceKey, desk_index);
  }

  if (params.type == Widget::InitParams::TYPE_BUBBLE)
    wm::SetHideOnDeactivate(window_, true);
  window_->SetTransparent(params.opacity ==
                          Widget::InitParams::WindowOpacity::kTranslucent);

  // Check for ShadowType::kNone before aura::Window::Init() to ensure observers
  // do not add useless shadow layers by deriving one from the window type.
  SetShadowElevationFromInitParams(window_, params);

  window_->Init(params.layer_type);
  // Set name after layer init so it propagates to layer.
  window_->SetName(params.name.empty() ? "NativeWidgetAura" : params.name);
  if (params.type == Widget::InitParams::TYPE_CONTROL)
    window_->Show();

  delegate_->OnNativeWidgetCreated();

  gfx::Rect window_bounds = params.bounds;
  gfx::NativeView parent = params.parent;
  gfx::NativeView context = params.context;
  if (!params.child) {
    wm::TransientWindowManager::GetOrCreate(window_)->AddObserver(this);

    // Set up the transient child before the window is added. This way the
    // LayoutManager knows the window has a transient parent.
    if (parent && parent->GetType() != aura::client::WINDOW_TYPE_UNKNOWN) {
      wm::AddTransientChild(parent, window_);
      if (!context)
        context = parent;
      parent = nullptr;

      // Generally transient bubbles are showing state associated to the parent
      // window. Make sure the transient bubble is only visible if the parent is
      // visible, otherwise the bubble may not make sense by itself.
      if (params.type == Widget::InitParams::TYPE_BUBBLE) {
        wm::TransientWindowManager::GetOrCreate(window_)
            ->set_parent_controls_visibility(true);
      }
    }
    // SetZOrderLevel before SetParent so that always-on-top container is used.
    SetZOrderLevel(params.EffectiveZOrderLevel());

    // Make sure we have a real |window_bounds|.
    aura::Window* parent_or_context = parent ? parent : context;
    if (parent_or_context && window_bounds == gfx::Rect()) {
      // If a parent or context is specified but no bounds are given, use the
      // origin of the display so that the widget will be added to the same
      // display as the parent or context.
      gfx::Rect bounds = display::Screen::GetScreen()
                             ->GetDisplayNearestWindow(parent_or_context)
                             .bounds();
      window_bounds.set_origin(bounds.origin());
    }
  }

  // Set properties before adding to the parent so that its layout manager sees
  // the correct values.
  OnSizeConstraintsChanged();

  std::optional<int64_t> target_display;
#if BUILDFLAG(IS_CHROMEOS)
  target_display = params.display_id;
#endif
  if (parent) {
    parent->AddChild(window_);
  } else {
    aura::client::ParentWindowWithContext(
        window_, context->GetRootWindow(), window_bounds,
        target_display.value_or(display::kInvalidDisplayId));
  }

  window_->AddObserver(this);

  // Wait to set the bounds until we have a parent. That way we can know our
  // true state/bounds (the LayoutManager may enforce a particular
  // state/bounds).
  if (IsMaximized() || IsMinimized()) {
    SetRestoreBounds(window_, window_bounds);
  } else {
    SetBoundsInternal(window_bounds, target_display);
  }
  window_->SetEventTargetingPolicy(
      params.accept_events ? aura::EventTargetingPolicy::kTargetAndDescendants
                           : aura::EventTargetingPolicy::kNone);
  DCHECK(GetWidget()->GetRootView());
  if (params.type != Widget::InitParams::TYPE_TOOLTIP)
    tooltip_manager_ = std::make_unique<views::TooltipManagerAura>(this);

  drop_helper_ = std::make_unique<DropHelper>(GetWidget()->GetRootView());
  if (params.type != Widget::InitParams::TYPE_TOOLTIP &&
      params.type != Widget::InitParams::TYPE_POPUP) {
    aura::client::SetDragDropDelegate(window_, this);
  }

  if (params.type == Widget::InitParams::TYPE_WINDOW) {
    focus_manager_event_handler_ =
        std::make_unique<FocusManagerEventHandler>(GetWidget(), window_);
  }

  wm::SetActivationDelegate(window_, this);

  window_reorderer_ =
      std::make_unique<WindowReorderer>(window_, GetWidget()->GetRootView());
}

void NativeWidgetAura::OnWidgetInitDone() {}

void NativeWidgetAura::ReparentNativeViewImpl(gfx::NativeView new_parent) {
  ReparentAuraWindow(GetNativeView(), new_parent);
}

std::unique_ptr<NonClientFrameView>
NativeWidgetAura::CreateNonClientFrameView() {
  return nullptr;
}

bool NativeWidgetAura::ShouldUseNativeFrame() const {
  // There is only one frame type for aura.
  return false;
}

bool NativeWidgetAura::ShouldWindowContentsBeTransparent() const {
  return false;
}

void NativeWidgetAura::FrameTypeChanged() {
  // This is called when the Theme has changed; forward the event to the root
  // widget.
  GetWidget()->ThemeChanged();
  GetWidget()->GetRootView()->SchedulePaint();
}

Widget* NativeWidgetAura::GetWidget() {
  return delegate_ ? delegate_->AsWidget() : nullptr;
}

const Widget* NativeWidgetAura::GetWidget() const {
  return delegate_ ? delegate_->AsWidget() : nullptr;
}

gfx::NativeView NativeWidgetAura::GetNativeView() const {
  return window_;
}

gfx::NativeWindow NativeWidgetAura::GetNativeWindow() const {
  return window_;
}

Widget* NativeWidgetAura::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : nullptr;
}

const ui::Compositor* NativeWidgetAura::GetCompositor() const {
  return window_ ? window_->layer()->GetCompositor() : nullptr;
}

const ui::Layer* NativeWidgetAura::GetLayer() const {
  return window_ ? window_->layer() : nullptr;
}

void NativeWidgetAura::ReorderNativeViews() {
  window_reorderer_->ReorderChildWindows();
}

void NativeWidgetAura::ViewRemoved(View* view) {
  DCHECK(drop_helper_.get() != nullptr);
  drop_helper_->ResetTargetViewIfEquals(view);
}

void NativeWidgetAura::SetNativeWindowProperty(const char* name, void* value) {
  if (window_)
    window_->SetNativeWindowProperty(name, value);
}

void* NativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  return window_ ? window_->GetNativeWindowProperty(name) : nullptr;
}

TooltipManager* NativeWidgetAura::GetTooltipManager() const {
  return tooltip_manager_.get();
}

void NativeWidgetAura::SetCapture() {
  if (window_)
    window_->SetCapture();
}

void NativeWidgetAura::ReleaseCapture() {
  if (window_)
    window_->ReleaseCapture();
}

bool NativeWidgetAura::HasCapture() const {
  return window_ && window_->HasCapture();
}

ui::InputMethod* NativeWidgetAura::GetInputMethod() {
  if (!window_)
    return nullptr;
  aura::Window* root_window = window_->GetRootWindow();
  return root_window ? root_window->GetHost()->GetInputMethod() : nullptr;
}

void NativeWidgetAura::CenterWindow(const gfx::Size& size) {
  if (!window_)
    return;

  window_->SetProperty(aura::client::kPreferredSize, size);

  gfx::Rect parent_bounds(window_->parent()->GetBoundsInRootWindow());
  // When centering window, we take the intersection of the host and
  // the parent. We assume the root window represents the visible
  // rect of a single screen.
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestWindow(window_)
                            .work_area();

  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window_->GetRootWindow());
  if (screen_position_client) {
    gfx::Point origin = work_area.origin();
    screen_position_client->ConvertPointFromScreen(window_->GetRootWindow(),
                                                   &origin);
    work_area.set_origin(origin);
  }

  parent_bounds.Intersect(work_area);

  // If |window_|'s transient parent's bounds are big enough to fit it, then we
  // center it with respect to the transient parent.
  if (wm::GetTransientParent(window_)) {
    gfx::Rect transient_parent_rect =
        wm::GetTransientParent(window_)->GetBoundsInRootWindow();
    transient_parent_rect.Intersect(work_area);
    if (transient_parent_rect.height() >= size.height() &&
        transient_parent_rect.width() >= size.width())
      parent_bounds = transient_parent_rect;
  }

  gfx::Rect window_bounds(
      parent_bounds.x() + (parent_bounds.width() - size.width()) / 2,
      parent_bounds.y() + (parent_bounds.height() - size.height()) / 2,
      size.width(), size.height());
  // Don't size the window bigger than the parent, otherwise the user may not be
  // able to close or move it.
  window_bounds.AdjustToFit(parent_bounds);

  // Convert the bounds back relative to the parent.
  gfx::Point origin = window_bounds.origin();
  aura::Window::ConvertPointToTarget(window_->GetRootWindow(),
                                     window_->parent(), &origin);
  window_bounds.set_origin(origin);
  window_->SetBounds(window_bounds);
}

void NativeWidgetAura::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  // The interface specifies returning restored bounds, not current bounds.
  *bounds = GetRestoredBounds();
  *show_state = window_ ? window_->GetProperty(aura::client::kShowStateKey)
                        : ui::mojom::WindowShowState::kDefault;
}

bool NativeWidgetAura::SetWindowTitle(const std::u16string& title) {
  if (!window_)
    return false;
  if (window_->GetTitle() == title)
    return false;
  window_->SetTitle(title);
  return true;
}

void NativeWidgetAura::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                      const gfx::ImageSkia& app_icon) {
  AssignIconToAuraWindow(window_, window_icon, app_icon);
}

const gfx::ImageSkia* NativeWidgetAura::GetWindowIcon() {
  return window_->GetProperty(aura::client::kWindowIconKey);
}

const gfx::ImageSkia* NativeWidgetAura::GetWindowAppIcon() {
  return window_->GetProperty(aura::client::kAppIconKey);
}

void NativeWidgetAura::InitModalType(ui::mojom::ModalType modal_type) {
  if (modal_type != ui::mojom::ModalType::kNone) {
    window_->SetProperty(aura::client::kModalKey, modal_type);
  }
  if (modal_type == ui::mojom::ModalType::kWindow) {
    wm::TransientWindowManager::GetOrCreate(window_)
        ->set_parent_controls_visibility(true);
  }
}

gfx::Rect NativeWidgetAura::GetWindowBoundsInScreen() const {
  return window_ ? window_->GetBoundsInScreen() : gfx::Rect();
}

gfx::Rect NativeWidgetAura::GetClientAreaBoundsInScreen() const {
  // View-to-screen coordinate system transformations depend on this returning
  // the full window bounds, for example View::ConvertPointToScreen().
  return window_ ? window_->GetBoundsInScreen() : gfx::Rect();
}

gfx::Rect NativeWidgetAura::GetRestoredBounds() const {
  if (!window_)
    return gfx::Rect();

  // Restored bounds should only be relevant if the window is minimized,
  // maximized, or fullscreen. However, in some places the code expects
  // GetRestoredBounds() to return the current window bounds if the window is
  // not in either state.
  if (IsMinimized() || IsMaximized() || IsFullscreen()) {
    // Restore bounds are in screen coordinates, no need to convert.
    gfx::Rect* restore_bounds =
        window_->GetProperty(aura::client::kRestoreBoundsKey);
    if (restore_bounds)
      return *restore_bounds;
  }

  // Prefer getting the window bounds and converting them to screen bounds since
  // Window::GetBoundsInScreen takes into the account the window transform.
  auto* screen_position_client =
      aura::client::GetScreenPositionClient(window_->GetRootWindow());
  if (screen_position_client) {
    // |window_|'s bounds are in parent's coordinate system so use that when
    // converting.
    gfx::Rect bounds = window_->bounds();
    gfx::Point origin = bounds.origin();
    screen_position_client->ConvertPointToScreenIgnoringTransforms(
        window_->parent(), &origin);
    return gfx::Rect(origin, bounds.size());
  }

  return window_->GetBoundsInScreen();
}

std::string NativeWidgetAura::GetWorkspace() const {
  int desk_index = window_->GetProperty(aura::client::kWindowWorkspaceKey);
  if (desk_index == aura::client::kWindowWorkspaceUnassignedWorkspace ||
      desk_index == aura::client::kWindowWorkspaceVisibleOnAllWorkspaces) {
    return std::string();
  }

  return base::NumberToString(desk_index);
}

void NativeWidgetAura::SetBounds(const gfx::Rect& bounds) {
  if (!window_)
    return;
  SetBoundsInternal(bounds, std::nullopt);
}

void NativeWidgetAura::SetBoundsInternal(const gfx::Rect& bounds,
                                         std::optional<int64_t> display_id) {
  display::Display dst_display;
  auto* screen = display::Screen::GetScreen();
  // TODO(crbug.com/40281188): Call SetBoundsInScreen directly.
  if (!display_id ||
      !screen->GetDisplayWithDisplayId(display_id.value(), &dst_display)) {
    dst_display = screen->GetDisplayMatching(bounds);
  }
#if BUILDFLAG(IS_CHROMEOS)
  // `dst_display` is not used on desktop chrome, and `GetDisplayMatching` above
  // may return invalid display on Windows.
  CHECK(dst_display.is_valid());
#endif
  window_->SetBoundsInScreen(bounds, dst_display);
}

void NativeWidgetAura::SetBoundsConstrained(const gfx::Rect& bounds) {
  if (!window_)
    return;

  gfx::Rect new_bounds(bounds);
  if (window_->parent()) {
    if (window_->parent()->GetProperty(wm::kUsesScreenCoordinatesKey)) {
      new_bounds =
          NativeWidgetPrivate::ConstrainBoundsToDisplayWorkArea(new_bounds);
    } else {
      new_bounds.AdjustToFit(gfx::Rect(window_->parent()->bounds().size()));
    }
  }
  SetBounds(new_bounds);
}

void NativeWidgetAura::SetSize(const gfx::Size& size) {
  if (window_)
    window_->SetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void NativeWidgetAura::StackAbove(gfx::NativeView native_view) {
  if (window_ && window_->parent() &&
      window_->parent() == native_view->parent())
    window_->parent()->StackChildAbove(window_, native_view);
}

void NativeWidgetAura::StackAtTop() {
  if (window_)
    window_->parent()->StackChildAtTop(window_);
}

bool NativeWidgetAura::IsStackedAbove(gfx::NativeView native_view) {
  if (!window_)
    return false;

  // If the root windows are not shared between two native views
  // it is likely that they are child windows of different top level windows.
  // In that scenario, just check the top level windows.
  if (GetNativeWindow()->GetRootWindow() != native_view->GetRootWindow()) {
    return GetTopLevelWidget()->IsStackedAbove(
        native_view->GetToplevelWindow());
  }

  const ui::Layer* first = native_view->layer();      // below
  const ui::Layer* second = GetWidget()->GetLayer();  // above
  return FindLayersInOrder(
      GetNativeWindow()->GetRootWindow()->layer()->children(), &first, &second);
}

void NativeWidgetAura::SetShape(std::unique_ptr<Widget::ShapeRects> shape) {
  if (window_)
    window_->layer()->SetAlphaShape(std::move(shape));
}

void NativeWidgetAura::Close() {
  // |window_| may already be deleted by parent window. This can happen
  // when this widget is child widget or has transient parent
  // and ownership is WIDGET_OWNS_NATIVE_WIDGET or CLIENT_OWNS_WIDGET.
  DCHECK(window_ ||
         ownership_ == Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET ||
         ownership_ == Widget::InitParams::CLIENT_OWNS_WIDGET);
  if (window_) {
    Hide();
    window_->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kNone);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeWidgetAura::CloseNow, weak_factory.GetWeakPtr()));
}

void NativeWidgetAura::CloseNow() {
  // We cannot use `raw_ptr::ClearAndDelete` here:
  // In `window_` destructor, `OnWindowDestroying` will be called on this
  // instance and `OnWindowDestroying` contains logic that still needs reference
  // in `window_`. `ClearAndDelete` would have cleared the value in `window_`
  // first before deleting `window_` causing problem in `OnWindowDestroying`.

  if (window_)
    delete window_;

  // `window_` destructor may delete this `NativeWidgetAura` instance. Therefore
  // we must NOT access anything through `this` after `delete window_`.
  // Therefore, we should NOT attempt to set `window_` to `nullptr`.
}

void NativeWidgetAura::Show(ui::mojom::WindowShowState show_state,
                            const gfx::Rect& restore_bounds) {
  if (!window_)
    return;

  if ((show_state == ui::mojom::WindowShowState::kMaximized ||
       show_state == ui::mojom::WindowShowState::kMinimized) &&
      !restore_bounds.IsEmpty()) {
    SetRestoreBounds(window_, restore_bounds);
  }
  if (show_state == ui::mojom::WindowShowState::kMaximized ||
      show_state == ui::mojom::WindowShowState::kFullscreen) {
    window_->SetProperty(aura::client::kShowStateKey, show_state);
  }
  // Disable the window animation for an initially minimized widget, because it
  // will create a detached layer tree for minimizing animation, which can be
  // briefly visible.
  std::optional<wm::ScopedAnimationDisabler> disabler;
  if (show_state == ui::mojom::WindowShowState::kMinimized) {
    disabler.emplace(window_);
  }

  window_->Show();
  if (delegate_->CanActivate()) {
    if (show_state != ui::mojom::WindowShowState::kInactive) {
      Activate();
    }
    // SetInitialFocus() should be always be called, even for
    // SHOW_STATE_INACTIVE. If the window has to stay inactive, the method will
    // do the right thing.
    // Activate() might fail if the window is non-activatable. In this case, we
    // should pass SHOW_STATE_INACTIVE to SetInitialFocus() to stop the initial
    // focused view from getting focused. See crbug.com/515594 for example.
    SetInitialFocus(IsActive() ? show_state
                               : ui::mojom::WindowShowState::kInactive);
  }

  // On desktop aura, a window is activated first even when it is shown as
  // minimized. Do the same for consistency.
  if (show_state == ui::mojom::WindowShowState::kMinimized) {
    Minimize();
  }
}

void NativeWidgetAura::Hide() {
  if (window_)
    window_->Hide();
}

bool NativeWidgetAura::IsVisible() const {
  return window_ && window_->IsVisible();
}

void NativeWidgetAura::Activate() {
  if (!window_)
    return;

  // We don't necessarily have a root window yet. This can happen with
  // constrained windows.
  if (window_->GetRootWindow())
    wm::GetActivationClient(window_->GetRootWindow())->ActivateWindow(window_);
  if (window_->GetProperty(aura::client::kDrawAttentionKey))
    window_->SetProperty(aura::client::kDrawAttentionKey, false);
}

void NativeWidgetAura::Deactivate() {
  if (!window_)
    return;
  wm::GetActivationClient(window_->GetRootWindow())->DeactivateWindow(window_);
}

bool NativeWidgetAura::IsActive() const {
  return window_ && wm::IsActiveWindow(window_);
}

void NativeWidgetAura::SetZOrderLevel(ui::ZOrderLevel order) {
  if (window_)
    window_->SetProperty(aura::client::kZOrderingKey, order);
}

ui::ZOrderLevel NativeWidgetAura::GetZOrderLevel() const {
  if (window_)
    return window_->GetProperty(aura::client::kZOrderingKey);

  return ui::ZOrderLevel::kNormal;
}

void NativeWidgetAura::SetVisibleOnAllWorkspaces(bool always_visible) {
  window_->SetProperty(
      aura::client::kWindowWorkspaceKey,
      always_visible ? aura::client::kWindowWorkspaceVisibleOnAllWorkspaces
                     : aura::client::kWindowWorkspaceUnassignedWorkspace);
}

bool NativeWidgetAura::IsVisibleOnAllWorkspaces() const {
  return window_ && window_->GetProperty(aura::client::kWindowWorkspaceKey) ==
                        aura::client::kWindowWorkspaceVisibleOnAllWorkspaces;
}

void NativeWidgetAura::Maximize() {
  if (window_)
    window_->SetProperty(aura::client::kShowStateKey,
                         ui::mojom::WindowShowState::kMaximized);
}

void NativeWidgetAura::Minimize() {
  if (window_)
    window_->SetProperty(aura::client::kShowStateKey,
                         ui::mojom::WindowShowState::kMinimized);
}

bool NativeWidgetAura::IsMaximized() const {
  return window_ && window_->GetProperty(aura::client::kShowStateKey) ==
                        ui::mojom::WindowShowState::kMaximized;
}

bool NativeWidgetAura::IsMinimized() const {
  return window_ && window_->GetProperty(aura::client::kShowStateKey) ==
                        ui::mojom::WindowShowState::kMinimized;
}

void NativeWidgetAura::Restore() {
  if (window_)
    wm::Restore(window_);
}

void NativeWidgetAura::SetFullscreen(bool fullscreen,
                                     int64_t target_display_id) {
  if (!window_) {
    return;
  }
  wm::SetWindowFullscreen(window_, fullscreen, target_display_id);
}

bool NativeWidgetAura::IsFullscreen() const {
  return window_ && window_->GetProperty(aura::client::kShowStateKey) ==
                        ui::mojom::WindowShowState::kFullscreen;
}

void NativeWidgetAura::SetCanAppearInExistingFullscreenSpaces(
    bool can_appear_in_existing_fullscreen_spaces) {}

void NativeWidgetAura::SetOpacity(float opacity) {
  if (window_)
    window_->layer()->SetOpacity(opacity);
}

void NativeWidgetAura::SetAspectRatio(const gfx::SizeF& aspect_ratio,
                                      const gfx::Size& excluded_margin) {
  DCHECK(!aspect_ratio.IsEmpty());
  if (excluded_margin.width() > 0 || excluded_margin.height() > 0) {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  if (window_) {
    // aura::client::kAspectRatio is owned, which allows for passing by value.
    window_->SetProperty(aura::client::kAspectRatio, aspect_ratio);
    // TODO(crbug.com/40887946): send `excluded_margin`.
  }
}

void NativeWidgetAura::FlashFrame(bool flash) {
  if (window_)
    window_->SetProperty(aura::client::kDrawAttentionKey, flash);
}

void NativeWidgetAura::RunShellDrag(std::unique_ptr<ui::OSExchangeData> data,
                                    const gfx::Point& location,
                                    int operation,
                                    ui::mojom::DragEventSource source) {
  if (window_)
    views::RunShellDrag(window_, std::move(data), location, operation, source);
}

void NativeWidgetAura::CancelShellDrag(View* view) {
  if (window_) {
    views::CancelShellDrag(window_);
  }
}

void NativeWidgetAura::SchedulePaintInRect(const gfx::Rect& rect) {
  if (window_)
    window_->SchedulePaintInRect(rect);
}

void NativeWidgetAura::ScheduleLayout() {
  // ScheduleDraw() triggers a callback to WindowDelegate::UpdateVisualState().
  if (window_)
    window_->ScheduleDraw();
}

void NativeWidgetAura::SetCursor(const ui::Cursor& cursor) {
  cursor_ = cursor;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client)
    cursor_client->SetCursor(cursor);
}

bool NativeWidgetAura::IsMouseEventsEnabled() const {
  if (!window_)
    return false;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  return cursor_client ? cursor_client->IsMouseEventsEnabled() : true;
}

bool NativeWidgetAura::IsMouseButtonDown() const {
  return aura::Env::GetInstance()->IsMouseButtonDown();
}

void NativeWidgetAura::ClearNativeFocus() {
  aura::client::FocusClient* client = aura::client::GetFocusClient(window_);
  if (window_ && client && window_->Contains(client->GetFocusedWindow()))
    client->ResetFocusWithinActiveWindow(window_);
}

gfx::Rect NativeWidgetAura::GetWorkAreaBoundsInScreen() const {
  if (!window_)
    return gfx::Rect();
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(window_)
      .work_area();
}

Widget::MoveLoopResult NativeWidgetAura::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  // |escape_behavior| is only needed on windows when running the native message
  // loop.
  if (!window_ || !window_->GetRootWindow())
    return Widget::MoveLoopResult::kCanceled;
  wm::WindowMoveClient* move_client =
      wm::GetWindowMoveClient(window_->GetRootWindow());
  if (!move_client)
    return Widget::MoveLoopResult::kCanceled;

  SetCapture();
  wm::WindowMoveSource window_move_source =
      source == Widget::MoveLoopSource::kMouse ? wm::WINDOW_MOVE_SOURCE_MOUSE
                                               : wm::WINDOW_MOVE_SOURCE_TOUCH;
  if (move_client->RunMoveLoop(window_, drag_offset, window_move_source) ==
      wm::MOVE_SUCCESSFUL) {
    return Widget::MoveLoopResult::kSuccessful;
  }
  return Widget::MoveLoopResult::kCanceled;
}

void NativeWidgetAura::EndMoveLoop() {
  if (!window_ || !window_->GetRootWindow())
    return;
  wm::WindowMoveClient* move_client =
      wm::GetWindowMoveClient(window_->GetRootWindow());
  if (move_client)
    move_client->EndMoveLoop();
}

void NativeWidgetAura::SetVisibilityChangedAnimationsEnabled(bool value) {
  if (window_)
    window_->SetProperty(aura::client::kAnimationsDisabledKey, !value);
}

void NativeWidgetAura::SetVisibilityAnimationDuration(
    const base::TimeDelta& duration) {
  wm::SetWindowVisibilityAnimationDuration(window_, duration);
}

void NativeWidgetAura::SetVisibilityAnimationTransition(
    Widget::VisibilityTransition transition) {
  wm::WindowVisibilityAnimationTransition wm_transition = wm::ANIMATE_NONE;
  switch (transition) {
    case Widget::ANIMATE_SHOW:
      wm_transition = wm::ANIMATE_SHOW;
      break;
    case Widget::ANIMATE_HIDE:
      wm_transition = wm::ANIMATE_HIDE;
      break;
    case Widget::ANIMATE_BOTH:
      wm_transition = wm::ANIMATE_BOTH;
      break;
    case Widget::ANIMATE_NONE:
      wm_transition = wm::ANIMATE_NONE;
      break;
  }
  wm::SetWindowVisibilityAnimationTransition(window_, wm_transition);
}

ui::GestureRecognizer* NativeWidgetAura::GetGestureRecognizer() {
  return aura::Env::GetInstance()->gesture_recognizer();
}

ui::GestureConsumer* NativeWidgetAura::GetGestureConsumer() {
  return window_;
}

void NativeWidgetAura::OnSizeConstraintsChanged() {
  SetResizeBehaviorFromDelegate(GetWidget()->widget_delegate(), window_);
}

void NativeWidgetAura::OnNativeViewHierarchyWillChange() {}

void NativeWidgetAura::OnNativeViewHierarchyChanged() {}

bool NativeWidgetAura::SetAllowScreenshots(bool allow) {
  // TODO(crbug.com/322519161): Revisit this to delegate the call to
  // `WindowTreeHost`.
  NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetAura::AreScreenshotsAllowed() {
  // TODO(crbug.com/322519161): Revisit this to delegate the call to
  // `WindowTreeHost`. For now, this function simply returns true as the
  // screenshot blocking logic is handled in desktop_native_widget_aura.cc
  NOTIMPLEMENTED();
  return true;
}

std::string NativeWidgetAura::GetName() const {
  return window_ ? window_->GetName() : std::string();
}

base::WeakPtr<internal::NativeWidgetPrivate> NativeWidgetAura::GetWeakPtr() {
  return weak_factory.GetWeakPtr();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::WindowDelegate implementation:

gfx::Size NativeWidgetAura::GetMinimumSize() const {
  return delegate_ ? delegate_->GetMinimumSize() : gfx::Size();
}

gfx::Size NativeWidgetAura::GetMaximumSize() const {
  // Do no check maximizability as EXO clients can have maximum size and be
  // maximizable at the same time.
  return delegate_ ? delegate_->GetMaximumSize() : gfx::Size();
}

void NativeWidgetAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds) {
  // Assume that if the old bounds was completely empty a move happened. This
  // handles the case of a maximize animation acquiring the layer (acquiring a
  // layer results in clearing the bounds).
  if (delegate_ &&
      (old_bounds.origin() != new_bounds.origin() ||
       (old_bounds == gfx::Rect(0, 0, 0, 0) && !new_bounds.IsEmpty()))) {
    delegate_->OnNativeWidgetMove();
  }
  if (delegate_ && (old_bounds.size() != new_bounds.size()))
    delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

gfx::NativeCursor NativeWidgetAura::GetCursor(const gfx::Point& point) {
  return cursor_;
}

int NativeWidgetAura::GetNonClientComponent(const gfx::Point& point) const {
  return delegate_ ? delegate_->GetNonClientComponent(point) : 0;
}

bool NativeWidgetAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return delegate_ ? delegate_->ShouldDescendIntoChildForEventHandling(
                         window_->layer(), child, child->layer(), location)
                   : false;
}

bool NativeWidgetAura::CanFocus() {
  return ShouldActivate();
}

void NativeWidgetAura::OnCaptureLost() {
  if (delegate_)
    delegate_->OnMouseCaptureLost();
}

void NativeWidgetAura::OnPaint(const ui::PaintContext& context) {
  if (delegate_)
    delegate_->OnNativeWidgetPaint(context);
}

void NativeWidgetAura::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  if (Widget* widget = GetWidget()) {
    widget->DeviceScaleFactorChanged(old_device_scale_factor,
                                     new_device_scale_factor);
  }
}

void NativeWidgetAura::OnWindowDestroying(aura::Window* window) {
  window_->RemoveObserver(this);
  if (wm::TransientWindowManager::GetIfExists(window_)) {
    wm::TransientWindowManager::GetOrCreate(window_)->RemoveObserver(this);
  }
  if (delegate_)
    delegate_->OnNativeWidgetDestroying();

  // If the aura::Window is destroyed, we can no longer show tooltips.
  tooltip_manager_.reset();

  focus_manager_event_handler_.reset();
}

void NativeWidgetAura::OnWindowDestroyed(aura::Window* window) {
  window_ = nullptr;
  // |OnNativeWidgetDestroyed| may delete |this| if the object does not own
  // itself.
  bool should_delete_this =
      (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) ||
      (ownership_ == Widget::InitParams::CLIENT_OWNS_WIDGET);
  if (delegate_)
    delegate_->OnNativeWidgetDestroyed();
  if (should_delete_this)
    delete this;
}

void NativeWidgetAura::OnWindowTargetVisibilityChanged(bool visible) {
  if (delegate_)
    delegate_->OnNativeWidgetVisibilityChanged(visible);
}

bool NativeWidgetAura::HasHitTestMask() const {
  return delegate_ ? delegate_->HasHitTestMask() : false;
}

void NativeWidgetAura::GetHitTestMask(SkPath* mask) const {
  DCHECK(mask);
  if (delegate_)
    delegate_->GetHitTestMask(mask);
}

void NativeWidgetAura::UpdateVisualState() {
  if (delegate_)
    delegate_->LayoutRootViewIfNecessary();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::WindowObserver implementation:

void NativeWidgetAura::OnWindowPropertyChanged(aura::Window* window,
                                               const void* key,
                                               intptr_t old) {
  if (delegate_ && key == aura::client::kShowStateKey)
    delegate_->OnNativeWidgetWindowShowStateChanged();

  if (delegate_ && key == aura::client::kWindowWorkspaceKey)
    delegate_->OnNativeWidgetWorkspaceChanged();
}

void NativeWidgetAura::OnResizeLoopStarted(aura::Window* window) {
  if (delegate_)
    delegate_->OnNativeWidgetBeginUserBoundsChange();
}

void NativeWidgetAura::OnResizeLoopEnded(aura::Window* window) {
  if (delegate_)
    delegate_->OnNativeWidgetEndUserBoundsChange();
}

void NativeWidgetAura::OnWindowAddedToRootWindow(aura::Window* window) {
  if (delegate_)
    delegate_->OnNativeWidgetAddedToCompositor();
}

void NativeWidgetAura::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                      aura::Window* new_root) {
  if (delegate_)
    delegate_->OnNativeWidgetRemovingFromCompositor();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, ui::EventHandler implementation:

void NativeWidgetAura::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(window_);
  // Renderer may send a key event back to us if the key event wasn't handled,
  // and the window may be invisible by that time.
  if (!window_->IsVisible())
    return;

  if (delegate_)
    delegate_->OnKeyEvent(event);
}

void NativeWidgetAura::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(window_);
  DCHECK(window_->IsVisible());
  if (delegate_ && event->type() == ui::EventType::kMousewheel) {
    delegate_->OnMouseEvent(event);
    return;
  }

  if (tooltip_manager_.get())
    tooltip_manager_->UpdateTooltip();
  TooltipManagerAura::UpdateTooltipManagerForCapture(this);

  if (delegate_)
    delegate_->OnMouseEvent(event);
}

void NativeWidgetAura::OnScrollEvent(ui::ScrollEvent* event) {
  if (delegate_)
    delegate_->OnScrollEvent(event);
}

void NativeWidgetAura::OnGestureEvent(ui::GestureEvent* event) {
  DCHECK(window_);
  DCHECK(window_->IsVisible() || event->IsEndingEvent());
  if (delegate_)
    delegate_->OnGestureEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, wm::ActivationDelegate implementation:

bool NativeWidgetAura::ShouldActivate() const {
  return delegate_ ? delegate_->CanActivate() : false;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, wm::ActivationChangeObserver implementation:

void NativeWidgetAura::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  DCHECK(window_ == gained_active || window_ == lost_active);
  if (GetWidget() && GetWidget()->GetFocusManager()) {
    if (window_ == gained_active)
      GetWidget()->GetFocusManager()->RestoreFocusedView();
    else if (window_ == lost_active)
      GetWidget()->GetFocusManager()->StoreFocusedView(true);
  }
  if (delegate_)
    delegate_->OnNativeWidgetActivationChanged(window_ == gained_active);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::client::FocusChangeObserver:

void NativeWidgetAura::OnWindowFocused(aura::Window* gained_focus,
                                       aura::Window* lost_focus) {
  if (delegate_ && window_ == gained_focus)
    delegate_->OnNativeFocus();
  else if (delegate_ && window_ == lost_focus)
    delegate_->OnNativeBlur();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::WindowDragDropDelegate implementation:

void NativeWidgetAura::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != nullptr);
  last_drop_operation_ = drop_helper_->OnDragOver(
      event.data(), event.location(), event.source_operations());
}

aura::client::DragUpdateInfo NativeWidgetAura::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != nullptr);
  last_drop_operation_ = drop_helper_->OnDragOver(
      event.data(), event.location(), event.source_operations());
  return aura::client::DragUpdateInfo(
      last_drop_operation_,
      ui::DataTransferEndpoint(ui::EndpointType::kDefault));
}

void NativeWidgetAura::OnDragExited() {
  DCHECK(drop_helper_.get() != nullptr);
  drop_helper_->OnDragExit();
}

aura::client::DragDropDelegate::DropCallback NativeWidgetAura::GetDropCallback(
    const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_);
  return drop_helper_->GetDropCallback(event.data(), event.location(),
                                       last_drop_operation_);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, wm::TransientWindowObserver implementation:

void NativeWidgetAura::OnTransientParentChanged(aura::Window* new_parent) {
  if (delegate_)
    delegate_->OnNativeWidgetParentChanged(new_parent);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, protected:

NativeWidgetAura::~NativeWidgetAura() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
    // `drop_helper_` and `window_reorderer_` hold a pointer to `delegate_`'s
    // root view. Reset them before deleting `delegate_` to avoid holding a
    // briefly dangling ptr.
    drop_helper_.reset();
    window_reorderer_.reset();
    owned_delegate_.reset();
  } else {
    CloseNow();
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, private:

void NativeWidgetAura::SetInitialFocus(ui::mojom::WindowShowState show_state) {
  // The window does not get keyboard messages unless we focus it.
  if (!GetWidget()->SetInitialFocus(show_state))
    window_->Focus();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

namespace {
#if BUILDFLAG(ENABLE_DESKTOP_AURA) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE))
void CloseWindow(aura::Window* window) {
  if (window) {
    Widget* widget = Widget::GetWidgetForNativeView(window);
    if (widget && widget->is_secondary_widget())
      // To avoid the delay in shutdown caused by using Close which may wait
      // for animations, use CloseNow. Because this is only used on secondary
      // widgets it seems relatively safe to skip the extra processing of
      // Close.
      widget->CloseNow();
  }
}
#endif

#if BUILDFLAG(IS_WIN)
BOOL CALLBACK WindowCallbackProc(HWND hwnd, LPARAM lParam) {
  aura::Window* root_window =
      DesktopWindowTreeHostWin::GetContentWindowForHWND(hwnd);
  CloseWindow(root_window);
  return TRUE;
}
#endif
}  // namespace

// static
void Widget::CloseAllSecondaryWidgets() {
#if BUILDFLAG(IS_WIN)
  EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc, 0);
#endif

#if BUILDFLAG(ENABLE_DESKTOP_AURA) && BUILDFLAG(IS_OZONE)
  DesktopWindowTreeHostPlatform::CleanUpWindowList(CloseWindow);
#endif
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetAura(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  return native_view->GetProperty(kNativeWidgetPrivateKey);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  return native_window->GetProperty(kNativeWidgetPrivateKey);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  aura::Window* window = native_view;
  while (window) {
    NativeWidgetPrivate* native_widget = GetNativeWidgetForNativeView(window);
    Widget* widget = native_widget ? native_widget->GetWidget() : nullptr;
    if (widget && widget->is_top_level()) {
      return native_widget;
    }
    window = window->parent();
  }
  return nullptr;
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  {
    // Code expects widget for |native_view| to be added to |children|.
    NativeWidgetPrivate* native_widget = static_cast<NativeWidgetPrivate*>(
        GetNativeWidgetForNativeView(native_view));
    if (native_widget && native_widget->GetWidget())
      children->insert(native_widget->GetWidget());
  }

  for (aura::Window* child_window : native_view->children()) {
    GetAllChildWidgets(child_window, children);
  }
}

// static
void NativeWidgetPrivate::GetAllOwnedWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* owned) {
  // Add all owned widgets.
  for (aura::Window* transient_child : wm::GetTransientChildren(native_view)) {
    NativeWidgetPrivate* native_widget = static_cast<NativeWidgetPrivate*>(
        GetNativeWidgetForNativeView(transient_child));
    if (native_widget && native_widget->GetWidget())
      owned->insert(native_widget->GetWidget());
    GetAllOwnedWidgets(transient_child, owned);
  }

  // Add all child windows.
  for (aura::Window* child : native_view->children())
    GetAllChildWidgets(child, owned);
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  DCHECK(native_view != new_parent);

  gfx::NativeView previous_parent = native_view->parent();
  if (previous_parent == new_parent)
    return;

  Widget::Widgets widgets;
  GetAllChildWidgets(native_view, &widgets);

  // First notify all the widgets that they are being disassociated
  // from their previous parent.
  for (Widget* widget : widgets) {
    widget->NotifyNativeViewHierarchyWillChange();
  }

  Widget* child_widget = Widget::GetWidgetForNativeView(native_view);

  if (child_widget) {
    child_widget->native_widget_private()->ReparentNativeViewImpl(new_parent);
  } else {
    ReparentAuraWindow(native_view, new_parent);
  }

  // And now, notify them that they have a brand new parent.
  for (Widget* widget : widgets) {
    widget->NotifyNativeViewHierarchyChanged();
  }
}

// static
gfx::NativeView NativeWidgetPrivate::GetGlobalCapture(
    gfx::NativeView native_view) {
  aura::client::CaptureClient* capture_client =
      aura::client::GetCaptureClient(native_view->GetRootWindow());
  if (!capture_client)
    return nullptr;
  return capture_client->GetGlobalCaptureWindow();
}

}  // namespace internal
}  // namespace views
