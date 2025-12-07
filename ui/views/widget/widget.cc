// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget.h"

#include <algorithm>
#include <optional>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/default_style.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/accessibility/tree/widget_ax_manager.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/drag_controller.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/focus/native_view_focus_manager.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/any_widget_observer_singleton.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/sublevel_manager.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_deletion_observer.h"
#include "ui/views/widget/widget_enumerator.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views {

namespace {

// If `view` has a layer the layer is added to `layers`. Else this recurses
// through the children. This is used to build a list of the layers in reverse
// z-order (i.e views later in the returned vector have a higher z-order)
// created by views that are direct children of the Widgets layer.
void BuildViewsWithLayersInZOrder(View* view, View::Views* views) {
  if (view->layer()) {
    views->push_back(view);
  } else {
    for (View* child : view->GetChildrenInZOrder()) {
      BuildViewsWithLayersInZOrder(child, views);
    }
  }
}

// Create a native widget implementation.
// First, use the supplied one if non-NULL.
// Finally, make a default one.
NativeWidget* CreateNativeWidget(const Widget::InitParams& params,
                                 internal::NativeWidgetDelegate* delegate) {
  if (params.native_widget) {
    return params.native_widget;
  }

  const auto& factory = ViewsDelegate::GetInstance()->native_widget_factory();
  if (!factory.is_null()) {
    NativeWidget* native_widget = factory.Run(params, delegate);
    if (native_widget) {
      return native_widget;
    }
  }
  return internal::NativeWidgetPrivate::CreateNativeWidget(delegate);
}

void NotifyCaretBoundsChanged(ui::InputMethod* input_method) {
  if (!input_method) {
    return;
  }
  ui::TextInputClient* client = input_method->GetTextInputClient();
  if (client) {
    input_method->OnCaretBoundsChanged(client);
  }
}

#if BUILDFLAG(IS_WIN)
ui::mojom::WindowShowState GetShowState(views::Widget* widget) {
  if (widget->IsMaximized()) [[unlikely]] {
    return ui::mojom::WindowShowState::kMaximized;
  } else if (widget->IsMinimized()) [[unlikely]] {
    return ui::mojom::WindowShowState::kMinimized;
  } else if (widget->IsFullscreen()) [[unlikely]] {
    return ui::mojom::WindowShowState::kFullscreen;
  }
  return ui::mojom::WindowShowState::kNormal;
}
#endif

}  // namespace

// static
Widget::DisableActivationChangeHandlingType
    Widget::g_disable_activation_change_handling_ =
        Widget::DisableActivationChangeHandlingType::kNone;

// A default implementation of WidgetDelegate, used by Widget when no
// WidgetDelegate is supplied.
class DefaultWidgetDelegate : public WidgetDelegate {
 public:
  DefaultWidgetDelegate() {
    // In most situations where a Widget is used without a delegate the Widget
    // is used as a container, so that we want focus to advance to the top-level
    // widget. A good example of this is the find bar.
    SetFocusTraversesOut(true);
    RegisterDeleteDelegateCallback(
        RegisterDeleteCallbackPassKey(),
        base::BindOnce(&DefaultWidgetDelegate::Destroy,
                       base::Unretained(this)));
  }

  DefaultWidgetDelegate(const DefaultWidgetDelegate&) = delete;
  DefaultWidgetDelegate& operator=(const DefaultWidgetDelegate&) = delete;

  ~DefaultWidgetDelegate() override = default;

 private:
  void Destroy() { delete this; }
};

////////////////////////////////////////////////////////////////////////////////
// Widget, PaintAsActiveLock:

Widget::PaintAsActiveLock::PaintAsActiveLock() = default;
Widget::PaintAsActiveLock::~PaintAsActiveLock() = default;

////////////////////////////////////////////////////////////////////////////////
// Widget, PaintAsActiveLockImpl:

class Widget::PaintAsActiveLockImpl : public Widget::PaintAsActiveLock {
 public:
  explicit PaintAsActiveLockImpl(base::WeakPtr<Widget>&& widget)
      : widget_(widget) {}

  ~PaintAsActiveLockImpl() override {
    Widget* const widget = widget_.get();
    if (widget) {
      widget->UnlockPaintAsActive();
    }
  }

 private:
  base::WeakPtr<Widget> widget_;
};

////////////////////////////////////////////////////////////////////////////////
// Widget, InitParams:

Widget::InitParams::InitParams(Ownership ownership, Type type)
    : type(type), ownership(ownership) {}

Widget::InitParams::InitParams(InitParams&& other) = default;

Widget::InitParams::~InitParams() = default;

bool Widget::InitParams::CanActivate() const {
  if (activatable != InitParams::Activatable::kDefault) {
    return activatable == InitParams::Activatable::kYes;
  }
  return type != InitParams::TYPE_CONTROL && type != InitParams::TYPE_POPUP &&
         type != InitParams::TYPE_MENU && type != InitParams::TYPE_TOOLTIP &&
         type != InitParams::TYPE_DRAG;
}

ui::ZOrderLevel Widget::InitParams::EffectiveZOrderLevel() const {
  if (z_order.has_value()) {
    return z_order.value();
  }

  switch (type) {
    case TYPE_MENU:
      return ui::ZOrderLevel::kFloatingWindow;
    case TYPE_DRAG:
      return ui::ZOrderLevel::kFloatingUIElement;
    default:
      return ui::ZOrderLevel::kNormal;
  }
}

void Widget::InitParams::SetParent(Widget* parent_widget) {
  SetParent(parent_widget->GetNativeView());
}

void Widget::InitParams::SetParent(gfx::NativeView parent_view) {
  parent = parent_view;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

Widget::Widget() = default;

Widget::Widget(InitParams params) {
  Init(std::move(params));
}

Widget::~Widget() {
  // DestroyRootView() will cause InvalidateLayout() to ScheduleLayout() which
  // is unnecessary.
  is_destroying_ = true;
  widget_closed_ = true;
  autosize_task_factory_.InvalidateWeakPtrs();

  // The following Notification order is preserved here:
  //   1. WidgetObserver::OnWidgetDestroying
  //   2. WidgetObserver::OnWidgetDestroyed
  //   3. WidgetDelegate::WidgetDestroying
  // Under WIDGET_OWNS_NATIVE_WIDGET and NATIVE_WIDGET_OWNS_WIDGET, the observer
  // notifications are initiated by native widget prior to ~Widget. In
  // CLIENT_OWNS_WIDGET, all events are emitted in ~Widget.

  if (widget_delegate_ && ownership_ != InitParams::CLIENT_OWNS_WIDGET) {
    widget_delegate_->WidgetDestroying();
  }
  if (ownership_ == InitParams::WIDGET_OWNS_NATIVE_WIDGET) {
    owned_native_widget_.reset();
    DCHECK(!native_widget_);
  } else if (ownership_ == InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
    // TODO(crbug.com/41444457): Revert to DCHECK once we figure out the reason.
    CHECK(!native_widget_)
        << "Destroying a widget with a live native widget. "
        << "Widget probably should use WIDGET_OWNS_NATIVE_WIDGET ownership.";
  } else {
    DCHECK_EQ(ownership_, InitParams::CLIENT_OWNS_WIDGET);
    // Specifically in the case of CLIENT_OWNS_WIDGET the native widget is
    // notified to allow clearing of any widget-associated state. Do so before
    // the call to `HandleWidgetDestroyed()` below which will invalidate
    // `native_widget_`.
    if (native_widget_) {
      native_widget_->ClientDestroyedWidget();
    }

    HandleWidgetDestroying();
    if (native_widget_) {
      native_widget_->Close();
    }

    HandleWidgetDestroyed();
    if (widget_delegate_) {
      widget_delegate_->WidgetDestroying();
    }
  }

  RemoveObserver(&root_view_->GetViewAccessibility());
  // Destroy RootView after the native widget, so in case the WidgetDelegate is
  // a View in the RootView hierarchy it gets destroyed as a WidgetDelegate
  // first.
  // This makes destruction order for WidgetDelegate consistent between
  // different Widget/NativeWidget ownership models (WidgetDelegate is always
  // deleted before here, which may have removed it as a View from the
  // View hierarchy).
  DestroyRootView();
}

// static
Widget* Widget::CreateWindowWithParent(WidgetDelegate* delegate,
                                       gfx::NativeView parent,
                                       const gfx::Rect& bounds) {
  Widget::InitParams params(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.delegate = delegate;
  params.parent = parent;
  params.bounds = bounds;
  return new Widget(std::move(params));
}

Widget* Widget::CreateWindowWithParent(std::unique_ptr<WidgetDelegate> delegate,
                                       gfx::NativeView parent,
                                       const gfx::Rect& bounds) {
  return CreateWindowWithParent(delegate.release(), parent, bounds);
}

// static
Widget* Widget::CreateWindowWithContext(WidgetDelegate* delegate,
                                        gfx::NativeWindow context,
                                        const gfx::Rect& bounds) {
  Widget::InitParams params(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.delegate = delegate;
  params.context = context;
  params.bounds = bounds;
  return new Widget(std::move(params));
}

// static
Widget* Widget::CreateWindowWithContext(
    std::unique_ptr<WidgetDelegate> delegate,
    gfx::NativeWindow context,
    const gfx::Rect& bounds) {
  return CreateWindowWithContext(delegate.release(), context, bounds);
}

// static
Widget* Widget::GetWidgetForNativeView(gfx::NativeView native_view) {
  if (!native_view) {
    return nullptr;
  }

  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetNativeWidgetForNativeView(native_view);
  return native_widget ? native_widget->GetWidget() : nullptr;
}

// static
Widget* Widget::GetWidgetForNativeWindow(gfx::NativeWindow native_window) {
  if (!native_window) {
    return nullptr;
  }

  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
          native_window);
  return native_widget ? native_widget->GetWidget() : nullptr;
}

// static
Widget* Widget::GetTopLevelWidgetForNativeView(gfx::NativeView native_view) {
  if (!native_view) {
    return nullptr;
  }

  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetTopLevelNativeWidget(native_view);
  return native_widget ? native_widget->GetWidget() : nullptr;
}

// static
Widget::Widgets Widget::GetAllChildWidgets(gfx::NativeView native_view) {
  return native_view
             ? internal::NativeWidgetPrivate::GetAllChildWidgets(native_view)
             : Widget::Widgets();
}

// static
Widget::Widgets Widget::GetAllOwnedWidgets(gfx::NativeView native_view) {
  return native_view
             ? internal::NativeWidgetPrivate::GetAllOwnedWidgets(native_view)
             : Widget::Widgets();
}

// static
void Widget::ForEachOwnedWidget(gfx::NativeView native_view,
                                base::FunctionRef<void(Widget*)> on_widget) {
  WidgetEnumerator widget_iterator(GetAllOwnedWidgets(native_view));
  while (!widget_iterator.IsEmpty()) {
    on_widget(widget_iterator.Next());
  }
}

// static
void Widget::ReparentNativeView(gfx::NativeView native_view,
                                gfx::NativeView new_parent) {
  DCHECK(native_view);
  internal::NativeWidgetPrivate::ReparentNativeView(native_view, new_parent);
  Widget* child_widget = GetWidgetForNativeView(native_view);
  Widget* parent_widget =
      new_parent ? GetWidgetForNativeView(new_parent) : nullptr;
  if (child_widget) {
    child_widget->HandleNativeWidgetReparented(parent_widget);
  }
}

// static
int Widget::GetLocalizedContentsWidth(int col_resource_id) {
  return ui::GetLocalizedContentsWidthForFontList(
      col_resource_id,
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          ui::kMessageFontSizeDelta));
}

// static
int Widget::GetLocalizedContentsHeight(int row_resource_id) {
  return ui::GetLocalizedContentsHeightForFontList(
      row_resource_id,
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          ui::kMessageFontSizeDelta));
}

// static
gfx::Size Widget::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  return gfx::Size(GetLocalizedContentsWidth(col_resource_id),
                   GetLocalizedContentsHeight(row_resource_id));
}

// static
bool Widget::RequiresNonClientView(InitParams::Type type) {
  return type == InitParams::TYPE_WINDOW || type == InitParams::TYPE_BUBBLE;
}

// static
bool Widget::IsWindowCompositingSupported() {
#if BUILDFLAG(IS_OZONE)
  return ui::OzonePlatform::GetInstance()->IsWindowCompositingSupported();
#else
  return true;
#endif
}

void Widget::Init(InitParams params) {
  TRACE_EVENT0("views", "Widget::Init");

  DCHECK(!native_widget_initialized_)
      << "This widget has already been initialized";

  if (params.name.empty() && params.delegate) {
    params.name = params.delegate->internal_name();
    // If an internal name was not provided the class name of the contents view
    // is a reasonable default.
    if (params.name.empty() && params.delegate->GetContentsView()) {
      params.name = params.delegate->GetContentsView()->GetClassName();
    }
  }

  if (params.parent && GetWidgetForNativeView(params.parent)) {
    parent_ = GetWidgetForNativeView(params.parent)->GetWeakPtr();
  }

  // Subscripbe to parent's paint-as-active change.
  if (parent_) {
    parent_paint_as_active_subscription_ =
        parent_->RegisterPaintAsActiveChangedCallback(
            base::BindRepeating(&Widget::OnParentShouldPaintAsActiveChanged,
                                base::Unretained(this)));
  }

  params.child |= (params.type == InitParams::TYPE_CONTROL);
  is_top_level_ = !params.child;
  is_autosized_ = params.autosize;

  if (params.opacity == views::Widget::InitParams::WindowOpacity::kInferred &&
      params.type != views::Widget::InitParams::TYPE_WINDOW) {
    params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  }

  // ViewsDelegate::OnBeforeWidgetInit() may change `params.delegate` either by
  // setting it to null or assigning a different value to it, so handle both
  // cases.
  ViewsDelegate::GetInstance()->OnBeforeWidgetInit(&params, this);

  if (params.delegate) {
    widget_delegate_ = params.delegate->AttachWidgetAndGetHandle(this);
  } else {
    auto default_delegate = std::make_unique<DefaultWidgetDelegate>();
    widget_delegate_ =
        default_delegate.release()->AttachWidgetAndGetHandle(this);
  }

  DCHECK(widget_delegate_);

  if (params.opacity == views::Widget::InitParams::WindowOpacity::kInferred) {
    params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  }

  bool can_activate = params.CanActivate();
  params.activatable = can_activate ? InitParams::Activatable::kYes
                                    : InitParams::Activatable::kNo;

  widget_delegate_->SetCanActivate(can_activate);

  ownership_ = params.ownership;

  sublevel_manager_ = std::make_unique<SublevelManager>(this, params.sublevel);

  internal::NativeWidgetPrivate* native_widget_raw_ptr =
      CreateNativeWidget(params, this)->AsNativeWidgetPrivate();
  native_widget_ = native_widget_raw_ptr->GetWeakPtr();
  if (params.ownership == InitParams::WIDGET_OWNS_NATIVE_WIDGET) {
    owned_native_widget_ = base::WrapUnique(native_widget_raw_ptr);
  }

  // The WidgetAXManager must be initialized *before* RootView is created,
  // because RootView's constructor may access it (e.g., to fire events).
  // However, the rest of InitAccessibility() depends on `root_view_`, so we
  // defer calling it until after `root_view_` is initialized.
  if (::features::IsAccessibilityTreeForViewsEnabled()) {
    CHECK(!ax_manager_)
        << "Widget::InitAccessibility() should only be called once";
    ax_manager_ = std::make_unique<WidgetAXManager>(this);
  }

  root_view_.reset(CreateRootView());
  InitAccessibility();  // Requires `root_view_`.

  // Copy the elements of params that will be used after it is moved.
  const InitParams::Type type = params.type;
  const gfx::Rect bounds = params.bounds;
  const ui::mojom::WindowShowState show_state = params.show_state;
  WidgetDelegate* delegate = params.delegate;
  bool should_set_initial_bounds = true;
#if BUILDFLAG(IS_CHROMEOS)
  // If the target display is specified on ChromeOS, the initial bounds will be
  // set based on the display.
  should_set_initial_bounds = !params.display_id.has_value();
#endif
#if BUILDFLAG(IS_WIN)
  // force_system_menu_for_frameless only applies to frameless windows.
  CHECK(!params.force_system_menu_for_frameless ||
        params.type == Widget::InitParams::TYPE_WINDOW_FRAMELESS);
#endif  // BUILDFLAG(IS_WIN)
  background_color_ = params.background_color;
  native_widget_->InitNativeWidget(std::move(params));
  if (type == InitParams::TYPE_MENU) {
    is_mouse_button_pressed_ = native_widget_->IsMouseButtonDown();
  }
  if (RequiresNonClientView(type)) {
    non_client_view_ =
        new NonClientView(widget_delegate_->CreateClientView(this));
    non_client_view_->SetFrameView(CreateFrameView());
    non_client_view_->SetOverlayView(widget_delegate_->CreateOverlayView());

    // Bypass the layout that happens in Widget::SetContentsView().
    // LayoutImmediately() will occur after setting the initial bounds below.
    // The RootView's size is not valid until that happens.
    root_view_->SetContentsView(non_client_view_);

    // Initialize the window's icon and title before setting the window's
    // initial bounds; the frame view's preferred height may depend on the
    // presence of an icon or a title.
    UpdateWindowIcon();
    UpdateWindowTitle();
    non_client_view_->ResetWindowControls();
    if (should_set_initial_bounds) {
      SetInitialBounds(bounds);
    }

    // Perform the initial layout. This handles the case where the size might
    // not actually change when setting the initial bounds. If it did, child
    // views won't have a dirty Layout state, so won't do any work.
    root_view_->LayoutImmediately();

    if (show_state == ui::mojom::WindowShowState::kMaximized) {
      Maximize();
    } else if (show_state == ui::mojom::WindowShowState::kMinimized) {
      Minimize();
      saved_show_state_ = ui::mojom::WindowShowState::kMinimized;
    }

#if BUILDFLAG(IS_CHROMEOS)
    // In ChromeOS, rounding window can involve rounding its client view and the
    // contents. Therefore, wait till the contents are set.
    // Since on ChromeOS, window can be square or rounded based on the window
    // state, wait till window is maximized or minimized.
    non_client_view_->frame_view()->UpdateWindowRoundedCorners();
#endif
  } else if (delegate) {
    SetContentsView(delegate->TransferOwnershipOfContentsView());
    if (should_set_initial_bounds) {
      SetInitialBoundsForFramelessWindow(bounds);
    }
  }

  if (parent_) {
    parent_->OnChildAdded(this);
  }

  native_widget_->SetBackgroundColor(
      GetColorProvider()->GetColor(GetBackgroundColorId()));

  UpdateAccessibleNameForRootView();
  native_theme_observation_.Observe(GetNativeTheme());
  native_widget_initialized_ = true;
  native_widget_->OnWidgetInitDone();

  if (delegate) {
    delegate->WidgetInitialized();
  }

  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetInitialized(
      this);
}

void Widget::InitAccessibility() {
  // The root view must always be fully initialized so we at least expose one
  // accessible element to the platform APIs. This is necessary for us to detect
  // accessibility API usage and fully enable accessibility support for all
  // views.
  root_view_->GetViewAccessibility().CompleteCacheInitialization();

  // The root view must always be initialized as it is being added to widget,
  // like setting kClassName correctly.
  root_view_->GetViewAccessibility().OnViewAddedToWidget();

  // Once the root view is added to the widget, it should be marked as ready to
  // send accessible event notifications. From that point on, any view that is
  // connected to the RootView will be able to send accessible events.
  root_view_->GetViewAccessibility().SetRootViewIsReadyToNotifyEvents();

  // We need to add the RootView's ViewAccessibility as an observer of the
  // widget, so that when the widget is closed, the accessible data is set
  // accordingly.
  AddObserver(&root_view_->GetViewAccessibility());

  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());

  // Must be called after `root_view_` is initialized.
  if (ax_manager_) {
    ax_manager_->Init();
  }
}

void Widget::ShowEmojiPanel() {
  if (native_widget_) {
    native_widget_->ShowEmojiPanel();
  }
}

// Unconverted methods (see header) --------------------------------------------

gfx::NativeView Widget::GetNativeView() const {
  return native_widget_ ? native_widget_->GetNativeView() : gfx::NativeView();
}

gfx::NativeWindow Widget::GetNativeWindow() const {
  return native_widget_ ? native_widget_->GetNativeWindow()
                        : gfx::NativeWindow();
}

std::optional<display::Display> Widget::GetNearestDisplay() {
  if (auto native_view = GetNativeView()) {
    return display::Screen::Get()->GetDisplayNearestView(native_view);
  }
  return std::nullopt;
}

void Widget::AddObserver(WidgetObserver* observer) {
  // Make sure that there is no nullptr in observer list. crbug.com/471649.
  CHECK(observer);
  observers_.AddObserver(observer);
}

void Widget::RemoveObserver(WidgetObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Widget::HasObserver(const WidgetObserver* observer) const {
  return observers_.HasObserver(observer);
}

void Widget::AddRemovalsObserver(WidgetRemovalsObserver* observer) {
  removals_observers_.AddObserver(observer);
}

void Widget::RemoveRemovalsObserver(WidgetRemovalsObserver* observer) {
  removals_observers_.RemoveObserver(observer);
}

bool Widget::HasRemovalsObserver(const WidgetRemovalsObserver* observer) const {
  return removals_observers_.HasObserver(observer);
}

bool Widget::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) const {
  return false;
}

void Widget::Reparent(Widget* parent) {
  gfx::NativeView child_view = GetNativeView();
  gfx::NativeView parent_view =
      parent ? parent->GetNativeView() : gfx::NativeView();
  internal::NativeWidgetPrivate::ReparentNativeView(child_view, parent_view);
  HandleNativeWidgetReparented(parent);
}

void Widget::ViewHierarchyChanged(const ViewHierarchyChangedDetails& details) {
  if (!details.is_add) {
    if (details.child == dragged_view_) {
      dragged_view_ = nullptr;
    }
    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager) {
      focus_manager->ViewRemoved(details.child);
    }
    if (native_widget_) {
      native_widget_->ViewRemoved(details.child);
    }
  }
}

void Widget::NotifyNativeViewHierarchyWillChange() {
  if (!native_widget_) {
    return;
  }
  // During tear-down the top-level focus manager becomes unavailable to
  // GTK tabbed panes and their children, so normal deregistration via
  // |FocusManager::ViewRemoved()| calls are fouled.  We clear focus here
  // to avoid these redundant steps and to avoid accessing deleted views
  // that may have been in focus.
  ClearFocusManagerFromWidget();
  native_widget_->OnNativeViewHierarchyWillChange();
}

void Widget::NotifyNativeViewHierarchyChanged() {
  if (!native_widget_) {
    return;
  }
  native_widget_->OnNativeViewHierarchyChanged();
  root_view_->NotifyNativeViewHierarchyChanged();
}

void Widget::NotifyWillRemoveView(View* view) {
  removals_observers_.Notify(&WidgetRemovalsObserver::OnWillRemoveView, this,
                             view);
}

// Converted methods (see header) ----------------------------------------------

Widget* Widget::GetTopLevelWidget() {
  return const_cast<Widget*>(
      static_cast<const Widget*>(this)->GetTopLevelWidget());
}

const Widget* Widget::GetTopLevelWidget() const {
  // GetTopLevelNativeWidget doesn't work during destruction because
  // property is gone after gobject gets deleted. Short circuit here
  // for toplevel so that InputMethod can remove itself from
  // focus manager.
  if (is_top_level()) {
    return this;
  }
  return native_widget_ ? native_widget_->GetTopLevelWidget() : nullptr;
}

Widget* Widget::GetPrimaryWindowWidget() {
  return GetTopLevelWidget();
}

const Widget* Widget::GetPrimaryWindowWidget() const {
  return const_cast<Widget*>(this)->GetPrimaryWindowWidget();
}

void Widget::SetContentsView(View* view) {
  // Do not SetContentsView() again if it is already set to the same view.
  if (view == GetContentsView()) {
    return;
  }

  // |non_client_view_| can only be non-null here if RequiresNonClientView() was
  // true when the widget was initialized. Creating widgets with non-client
  // views and then setting the contents view can cause subtle problems on
  // Windows, where the native widget thinks there is still a
  // |non_client_view_|. If you get this error, either use a different type when
  // initializing the widget, or don't call SetContentsView().
  DCHECK(!non_client_view_);

  root_view_->SetContentsView(view);

  // Force a layout now, since the attached hierarchy won't be ready for the
  // containing window's bounds. Note that we call Layout directly rather than
  // calling the widget's size changed handler, since the RootView's bounds may
  // not have changed, which will cause the Layout not to be done otherwise.
  root_view_->LayoutImmediately();
}

View* Widget::GetContentsView() const {
  return root_view_->GetContentsView();
}

View* Widget::GetClientContentsView() const {
  if (non_client_view_) {
    auto* client_view = non_client_view_->client_view();
    return (!client_view->children().empty()) ? client_view->children().front()
                                              : nullptr;
  }
  return GetContentsView();
}

gfx::Rect Widget::GetWindowBoundsInScreen() const {
  return native_widget_ ? native_widget_->GetWindowBoundsInScreen()
                        : gfx::Rect();
}

gfx::Rect Widget::GetClientAreaBoundsInScreen() const {
  return native_widget_ ? native_widget_->GetClientAreaBoundsInScreen()
                        : gfx::Rect();
}

gfx::Rect Widget::GetRestoredBounds() const {
  return native_widget_ ? native_widget_->GetRestoredBounds() : gfx::Rect();
}

std::string Widget::GetWorkspace() const {
  return native_widget_ ? native_widget_->GetWorkspace() : "";
}

void Widget::SetBounds(const gfx::Rect& bounds) {
  if (native_widget_) {
    native_widget_->SetBounds(bounds);
  }
}

void Widget::SetSize(const gfx::Size& size) {
  if (native_widget_) {
    native_widget_->SetSize(size);
  }
}

gfx::Size Widget::GetSize() const {
  return GetRestoredBounds().size();
}

gfx::Insets Widget::GetCustomInsetsInDIP() const {
  return gfx::Insets();
}

void Widget::CenterWindow(const gfx::Size& size) {
  if (native_widget_) {
    native_widget_->CenterWindow(size);
  }
}

void Widget::SetBoundsConstrained(const gfx::Rect& bounds) {
  if (native_widget_) {
    native_widget_->SetBoundsConstrained(bounds);
  }
}

void Widget::SetVisibilityChangedAnimationsEnabled(bool value) {
  if (native_widget_) {
    native_widget_->SetVisibilityChangedAnimationsEnabled(value);
  }
}

void Widget::SetVisibilityAnimationDuration(const base::TimeDelta& duration) {
  if (native_widget_) {
    native_widget_->SetVisibilityAnimationDuration(duration);
  }
}

void Widget::SetVisibilityAnimationTransition(VisibilityTransition transition) {
  if (native_widget_) {
    native_widget_->SetVisibilityAnimationTransition(transition);
  }
}

bool Widget::IsMoveLoopSupported() const {
  return native_widget_ ? native_widget_->IsMoveLoopSupported() : false;
}

Widget::MoveLoopResult Widget::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    MoveLoopSource source,
    MoveLoopEscapeBehavior escape_behavior) {
  if (!native_widget_) {
    return MoveLoopResult::kCanceled;
  }

  return native_widget_->RunMoveLoop(drag_offset, source, escape_behavior);
}

void Widget::EndMoveLoop() {
  if (native_widget_) {
    native_widget_->EndMoveLoop();
  }
}

void Widget::StackAboveWidget(Widget* widget) {
  if (native_widget_) {
    native_widget_->StackAbove(widget->GetNativeView());
  }
}

void Widget::StackAbove(gfx::NativeView native_view) {
  if (native_widget_) {
    native_widget_->StackAbove(native_view);
  }
}

void Widget::StackAtTop() {
  if (native_widget_) {
    native_widget_->StackAtTop();
  }
}

bool Widget::IsStackedAbove(gfx::NativeView native_view) {
  return native_widget_ ? native_widget_->IsStackedAbove(native_view) : false;
}

void Widget::SetShape(std::unique_ptr<ShapeRects> shape) {
  if (native_widget_) {
    native_widget_->SetShape(std::move(shape));
  }
}

void Widget::CloseWithReason(ClosedReason closed_reason) {
  // This logic intentionally exists before the 5 early returns below. If a
  // client does not want a Widget to Close(), the client should not call
  // Close(). Each of the cases below is to handle client-specific cases that
  // added logic into this class, rather than modifying the client to not call
  // Close().
  if (override_close_) {
    std::move(override_close_).Run(closed_reason);
    return;
  }

  if (widget_closed_) {
    // It appears we can hit this code path if you close a modal dialog then
    // close the last browser before the destructor is hit, which triggers
    // invoking Close again.
    return;
  }
  if (block_close_) {
    return;
  }
  if (non_client_view_ && non_client_view_->OnWindowCloseRequested() ==
                              CloseRequestResult::kCannotClose) {
    return;
  }

  // Cancel widget close on focus lost. This is used in UI Devtools to lock
  // bubbles and in some tests where we want to ignore spurious deactivation.
  if (closed_reason == ClosedReason::kLostFocus &&
      (g_disable_activation_change_handling_ ==
           DisableActivationChangeHandlingType::kIgnore ||
       g_disable_activation_change_handling_ ==
           DisableActivationChangeHandlingType::kIgnoreDeactivationOnly)) {
    return;
  }

  // This is the last chance to cancel closing.
  if (widget_delegate_ && !widget_delegate_->OnCloseRequested(closed_reason)) {
    return;
  }

  // The actions below can cause this function to be called again, so mark
  // |this| as closed early. See crbug.com/714334
  widget_closed_ = true;
  closed_reason_ = closed_reason;
  SaveWindowPlacement();
  ClearFocusManagerFromWidget();

  ax_mode_observation_.Reset();

  observers_.Notify(&WidgetObserver::OnWidgetClosing, this);

  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetClosing(this);

  if (widget_delegate_) {
    widget_delegate_->WindowWillClose();
  }

  if (native_widget_) {
    native_widget_->Close();
  }
}

void Widget::MakeCloseSynchronous(
    base::OnceCallback<void(ClosedReason)> override_close) {
  override_close_ = std::move(override_close);
}

void Widget::Close() {
  CloseWithReason(ClosedReason::kUnspecified);
}

void Widget::CloseNow() {
  // Set this so that Widget::Close() early outs. In general this operation is
  // a one-way and can't be undone.
  widget_closed_ = true;

  ax_mode_observation_.Reset();

  observers_.Notify(&WidgetObserver::OnWidgetClosing, this);
  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetClosing(this);

  DCHECK(native_widget_initialized_) << "Native widget is never initialized.";

  if (native_widget_) {
    native_widget_->CloseNow();
  }
}

bool Widget::IsClosed() const {
  return widget_closed_;
}

void Widget::Show() {
  if (!native_widget_) {
    return;
  }
  const ui::Layer* layer = GetLayer();
  TRACE_EVENT1("views", "Widget::Show", "layer",
               layer ? layer->name() : "none");
  ui::mojom::WindowShowState preferred_show_state =
      CanActivate() ? ui::mojom::WindowShowState::kNormal
                    : ui::mojom::WindowShowState::kInactive;
  if (non_client_view_) {
    // While initializing, the kiosk mode will go to full screen before the
    // widget gets shown. In that case we stay in full screen mode, regardless
    // of the |saved_show_state_| member.
    if (saved_show_state_ == ui::mojom::WindowShowState::kMaximized &&
        !initial_restored_bounds_.IsEmpty() && !IsFullscreen()) {
      native_widget_->Show(ui::mojom::WindowShowState::kMaximized,
                           initial_restored_bounds_);
    } else {
      native_widget_->Show(saved_show_state_, gfx::Rect());
    }
    // |saved_show_state_| only applies the first time the window is shown.
    // If we don't reset the value the window may be shown maximized every time
    // it is subsequently shown after being hidden.
    saved_show_state_ = preferred_show_state;
  } else {
    native_widget_->Show(preferred_show_state, gfx::Rect());
  }

  HandleShowRequested();
}

void Widget::Hide() {
  if (!native_widget_) {
    return;
  }
  native_widget_->Hide();
  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetHidden(this);
}

void Widget::ShowInactive() {
  if (!native_widget_) {
    return;
  }
  // If this gets called with saved_show_state_ ==
  // ui::mojom::WindowShowState::kMaximized, call SetBounds()with the restored
  // bounds to set the correct size. This normally should not happen, but if it
  // does we should avoid showing unsized windows.
  if (saved_show_state_ == ui::mojom::WindowShowState::kMaximized &&
      !initial_restored_bounds_.IsEmpty()) {
    SetBounds(initial_restored_bounds_);
    saved_show_state_ = ui::mojom::WindowShowState::kNormal;
  }
  native_widget_->Show(ui::mojom::WindowShowState::kInactive, gfx::Rect());

  HandleShowRequested();
}

void Widget::Activate() {
  if (CanActivate() && native_widget_) {
    native_widget_->Activate();
  }
}

void Widget::Deactivate() {
  if (native_widget_) {
    native_widget_->Deactivate();
  }
}

bool Widget::IsActive() const {
  return native_widget_ ? native_widget_->IsActive() : false;
}

bool Widget::ShouldViewsStyleFollowWidgetActivation() const {
  return CanActivate();
}

void Widget::SetZOrderLevel(ui::ZOrderLevel order) {
  if (native_widget_) {
    native_widget_->SetZOrderLevel(order);
  }
}

ui::ZOrderLevel Widget::GetZOrderLevel() const {
  return native_widget_ ? native_widget_->GetZOrderLevel()
                        : ui::ZOrderLevel::kNormal;
}

void Widget::SetZOrderSublevel(int sublevel) {
  sublevel_manager_->SetSublevel(sublevel);
}

int Widget::GetZOrderSublevel() const {
  if (!sublevel_manager_) {
    return 0;
  }

  return sublevel_manager_->GetSublevel();
}

#if BUILDFLAG(IS_MAC)
void Widget::SetActivationIndependence(bool independence) {
  CHECK(
      (independence && GetZOrderLevel() == ui::ZOrderLevel::kFloatingWindow) ||
      (!independence && GetZOrderLevel() == ui::ZOrderLevel::kNormal));
  if (native_widget_) {
    native_widget_->SetActivationIndependence(independence);
  }
}
#endif

void Widget::SetVisibleOnAllWorkspaces(bool always_visible) {
  if (native_widget_) {
    native_widget_->SetVisibleOnAllWorkspaces(always_visible);
  }
}

bool Widget::IsVisibleOnAllWorkspaces() const {
  return native_widget_ ? native_widget_->IsVisibleOnAllWorkspaces() : false;
}

void Widget::Maximize() {
  if (native_widget_) {
    native_widget_->Maximize();
  }
}

void Widget::Minimize() {
  if (native_widget_) {
    native_widget_->Minimize();
  }
}

void Widget::Restore() {
  if (native_widget_) {
    native_widget_->Restore();
  }
}

void Widget::ShowWindowControlsMenu(const gfx::Point& point) {
  if (native_widget_) {
    native_widget_->ShowWindowControlsMenu(point);
  }
}

bool Widget::IsMaximized() const {
  return native_widget_ ? native_widget_->IsMaximized() : false;
}

bool Widget::IsMinimized() const {
  return native_widget_ ? native_widget_->IsMinimized() : false;
}

void Widget::SetFullscreen(bool fullscreen, int64_t target_display_id) {
  if (!native_widget_) {
    return;
  }
  // It isn't valid to specify `target_display_id` when exiting fullscreen.
  if (!fullscreen) {
    DCHECK(target_display_id == display::kInvalidDisplayId);
  }
  if (IsFullscreen() == fullscreen &&
      target_display_id == display::kInvalidDisplayId) {
    return;
  }

  auto weak_ptr = GetWeakPtr();
  native_widget_->SetFullscreen(fullscreen, target_display_id);
  if (!weak_ptr) {
    return;
  }

  if (non_client_view_) {
    non_client_view_->InvalidateLayout();
  }
}

bool Widget::IsFullscreen() const {
  if (native_widget_ && native_widget_->IsFullscreen()) {
    return true;
  }
  // Some widgets are logically the same window as their parent, and thus their
  // parent must also be checked for fullscreen.
  if (parent() && check_parent_for_fullscreen_) {
    return parent()->IsFullscreen();
  }
  return false;
}

void Widget::SetCanAppearInExistingFullscreenSpaces(
    bool can_appear_in_existing_fullscreen_spaces) {
  if (native_widget_) {
    native_widget_->SetCanAppearInExistingFullscreenSpaces(
        can_appear_in_existing_fullscreen_spaces);
  }
}

void Widget::SetOpacity(float opacity) {
  DCHECK(opacity >= 0.0f);
  DCHECK(opacity <= 1.0f);
  if (native_widget_) {
    native_widget_->SetOpacity(opacity);
  }
}

void Widget::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  if (!native_widget_) {
    return;
  }

  // The aspect ratio affects the client view only, so figure out how much of
  // the widget isn't taken up by the client view.
  gfx::Size excluded_margin;
  if (non_client_view() && non_client_view()->frame_view()) {
    excluded_margin =
        non_client_view()->bounds().size() -
        non_client_view()->frame_view()->GetBoundsForClientView().size();
  }
  native_widget_->SetAspectRatio(aspect_ratio, excluded_margin);
}

void Widget::FlashFrame(bool flash) {
  if (native_widget_) {
    native_widget_->FlashFrame(flash);
  }
}

View* Widget::GetRootView() {
  return root_view_.get();
}

const View* Widget::GetRootView() const {
  return root_view_.get();
}

bool Widget::IsVisible() const {
  return native_widget_ ? native_widget_->IsVisible() : false;
}

bool Widget::IsVisibleOnScreen() const {
  return native_widget_ ? native_widget_->IsVisibleOnScreen() : false;
}

const ui::ThemeProvider* Widget::GetThemeProvider() const {
  // The theme provider is provided by the very top widget in the ownership
  // chain, which may include parenting, anchoring, etc. Use
  // GetPrimaryWindowWidget() rather than GetTopLevelWidget() for this purpose
  // (see description of those methods to learn more).
  const Widget* const root_widget = GetPrimaryWindowWidget();
  return (root_widget && root_widget != this) ? root_widget->GetThemeProvider()
                                              : nullptr;
}

ui::ColorProviderKey::ThemeInitializerSupplier* Widget::GetCustomTheme() const {
  return nullptr;
}

FocusManager* Widget::GetFocusManager() {
  Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->focus_manager_.get() : nullptr;
}

const FocusManager* Widget::GetFocusManager() const {
  const Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->focus_manager_.get() : nullptr;
}

ui::InputMethod* Widget::GetInputMethod() {
  if (is_top_level() && native_widget_) {
    // Only creates the shared the input method instance on top level widget.
    return native_widget_private()->GetInputMethod();
  } else {
    Widget* toplevel = GetTopLevelWidget();
    // If GetTopLevelWidget() returns itself which is not toplevel,
    // the widget is detached from toplevel widget.
    // TODO(oshima): Fix GetTopLevelWidget() to return NULL
    // if there is no toplevel. We probably need to add GetTopMostWidget()
    // to replace some use cases.
    return (toplevel && toplevel != this) ? toplevel->GetInputMethod()
                                          : nullptr;
  }
}

SublevelManager* Widget::GetSublevelManager() {
  return sublevel_manager_.get();
}

void Widget::RunShellDrag(View* view,
                          std::unique_ptr<ui::OSExchangeData> data,
                          const gfx::Point& location,
                          int operation,
                          ui::mojom::DragEventSource source) {
  if (view) {
    CHECK_EQ(view->GetWidget(), this);
  }

  if (!native_widget_) {
    return;
  }
  dragged_view_ = view;
  OnDragWillStart();

  observers_.Notify(&WidgetObserver::OnWidgetDragWillStart, this);

  if (view && view->drag_controller()) {
    view->drag_controller()->OnWillStartDragForView(view);
  }

  WidgetDeletionObserver widget_deletion_observer(this);
  {
    // Since application tasks are needed in drag-induced nested message loops
    // which occur here, (notably bookmark and download dragging), application
    // tasks need to run. Only views:: and ui::EventDispatcher stacks are
    // present, which expect this re-entrancy.
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
    native_widget_->RunShellDrag(std::move(data), location, operation, source);
  }

  // The widget may be destroyed during the drag operation.
  if (!widget_deletion_observer.IsWidgetAlive()) {
    return;
  }

  // TODO(crbug.com/375959961): On X11, the native widget's mouse button state
  // is not updated when the mouse button is released to end a drag.
#if !BUILDFLAG(IS_OZONE_X11)
  is_mouse_button_pressed_ = native_widget_->IsMouseButtonDown();
#endif

  // If the view is removed during the drag operation, dragged_view_ is set to
  // NULL.
  if (view && dragged_view_ == view) {
    dragged_view_ = nullptr;
    view->OnDragDone();
  }
  OnDragComplete();

  observers_.Notify(&WidgetObserver::OnWidgetDragComplete, this);
}

void Widget::CancelShellDrag(View* view) {
  if (!native_widget_) {
    return;
  }

  native_widget_->CancelShellDrag(view);
}

void Widget::SchedulePaintInRect(const gfx::Rect& rect) {
  // This happens when DestroyRootView removes all children from the
  // RootView which triggers a SchedulePaint that ends up here. This happens
  // after in ~Widget after native_widget_ is destroyed.
  if (native_widget_) {
    native_widget_->SchedulePaintInRect(rect);
  }
}

void Widget::OnRootViewLayoutInvalidated() {
  if (IsClosed()) {
    return;
  }

  if (is_autosized()) {
    // There is no need to post another async auto-resize task when there is
    // already one.
    if (!autosize_task_factory_.HasWeakPtrs()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&Widget::ResizeToDelegateDesiredBounds,
                                    autosize_task_factory_.GetWeakPtr()));
    }
  }

  ScheduleLayout();
}

void Widget::ScheduleLayout() {
  if (native_widget_) {
    native_widget_->ScheduleLayout();
  }
}

void Widget::SetCursor(const ui::Cursor& cursor) {
  if (native_widget_) {
    native_widget_->SetCursor(cursor);
  }
}

bool Widget::IsMouseEventsEnabled() const {
  return native_widget_ ? native_widget_->IsMouseEventsEnabled() : false;
}

void Widget::SetNativeWindowProperty(const char* name, void* value) {
  if (native_widget_) {
    native_widget_->SetNativeWindowProperty(name, value);
  }
}

void* Widget::GetNativeWindowProperty(const char* name) const {
  return native_widget_ ? native_widget_->GetNativeWindowProperty(name)
                        : nullptr;
}

void Widget::UpdateWindowTitle() {
  if (!native_widget_ || !non_client_view_) {
    return;
  }

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  std::u16string window_title = widget_delegate_->GetWindowTitle();
  base::i18n::AdjustStringForLocaleDirection(&window_title);
  bool title_changed = native_widget_->SetWindowTitle(window_title);

  // Continue UpdateWindowTitle() only if the title or title visibility changes.
  if (!title_changed) {
    bool has_title = non_client_view()->HasWindowTitle();
    bool title_visibility_changed = non_client_view()->IsWindowTitleVisible() !=
                                    widget_delegate_->ShouldShowWindowTitle();
    if (!has_title || !title_visibility_changed) {
      return;
    }
  }

  non_client_view_->UpdateWindowTitle();
  UpdateAccessibleNameForRootView();
}

void Widget::UpdateWindowIcon() {
  if (!native_widget_) {
    return;
  }

  if (non_client_view_) {
    non_client_view_->UpdateWindowIcon();
  }

  native_widget_->SetWindowIcons(
      widget_delegate_->GetWindowIcon().Rasterize(GetColorProvider()),
      widget_delegate_->GetWindowAppIcon().Rasterize(GetColorProvider()));
}

FocusTraversable* Widget::GetFocusTraversable() {
  return static_cast<internal::RootView*>(root_view_.get());
}

void Widget::ThemeChanged() {
  if (root_view_) {
    root_view_->ThemeChanged();
  }

  observers_.Notify(&WidgetObserver::OnWidgetThemeChanged, this);

  NotifyColorProviderChanged();

  if (native_widget_) {
    native_widget_->SetBackgroundColor(
        GetColorProvider()->GetColor(GetBackgroundColorId()));
  }
}

void Widget::DeviceScaleFactorChanged(float old_device_scale_factor,
                                      float new_device_scale_factor) {
  root_view_->DeviceScaleFactorChanged(old_device_scale_factor,
                                       new_device_scale_factor);
}

void Widget::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

void Widget::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

void Widget::ClearNativeFocus() {
  if (native_widget_) {
    native_widget_->ClearNativeFocus();
  }
}

std::unique_ptr<FrameView> Widget::CreateFrameView() {
  if (!native_widget_) {
    return nullptr;
  }
  auto frame_view = widget_delegate_->CreateFrameView(this);
  if (!frame_view) {
    frame_view = native_widget_->CreateFrameView();
  }
  if (!frame_view) {
    frame_view = ViewsDelegate::GetInstance()->CreateDefaultFrameView(this);
  }
  CHECK(frame_view);
  return frame_view;
}

bool Widget::ShouldUseNativeFrame() const {
  if (frame_type_ != FrameType::kDefault) {
    return frame_type_ == FrameType::kForceNative;
  }
  return native_widget_ ? native_widget_->ShouldUseNativeFrame() : false;
}

bool Widget::ShouldWindowContentsBeTransparent() const {
  return native_widget_ ? native_widget_->ShouldWindowContentsBeTransparent()
                        : false;
}

void Widget::FrameTypeChanged() {
  if (native_widget_) {
    native_widget_->FrameTypeChanged();
  }
}

const ui::Compositor* Widget::GetCompositor() const {
  return native_widget_ ? native_widget_->GetCompositor() : nullptr;
}

const ui::Layer* Widget::GetLayer() const {
  return native_widget_ ? native_widget_->GetLayer() : nullptr;
}

void Widget::ReorderNativeViews() {
  if (native_widget_) {
    native_widget_->ReorderNativeViews();
  }
}

void Widget::LayerTreeChanged() {
  // Calculate the layers requires traversing the tree, and since nearly any
  // mutation of the tree can trigger this call we delay until absolutely
  // necessary.
  views_with_layers_dirty_ = true;
}

const NativeWidget* Widget::native_widget() const {
  return native_widget_.get();
}

NativeWidget* Widget::native_widget() {
  return native_widget_.get();
}

void Widget::SetCapture(View* view) {
  if (!native_widget_) {
    return;
  }

  if (!native_widget_->HasCapture()) {
    native_widget_->SetCapture();

    // Early return if setting capture was unsuccessful.
    if (!native_widget_->HasCapture()) {
      return;
    }
  }

  is_mouse_button_pressed_ = native_widget_->IsMouseButtonDown();
  root_view_->SetMouseAndGestureHandler(view);
}

void Widget::ReleaseCapture() {
  if (native_widget_ && native_widget_->HasCapture()) {
    native_widget_->ReleaseCapture();
  }
}

bool Widget::HasCapture() {
  return native_widget_ ? native_widget_->HasCapture() : false;
}

TooltipManager* Widget::GetTooltipManager() {
  return native_widget_ ? native_widget_->GetTooltipManager() : nullptr;
}

const TooltipManager* Widget::GetTooltipManager() const {
  return native_widget_ ? native_widget_->GetTooltipManager() : nullptr;
}

gfx::Rect Widget::GetWorkAreaBoundsInScreen() const {
  return native_widget_ ? native_widget_->GetWorkAreaBoundsInScreen()
                        : gfx::Rect();
}

void Widget::SynthesizeMouseMoveEvent() {
  // In screen coordinate.
  gfx::Point mouse_location = display::Screen::Get()->GetCursorScreenPoint();
  if (!GetWindowBoundsInScreen().Contains(mouse_location)) {
    return;
  }

  // Convert: screen coordinate -> widget coordinate.
  View::ConvertPointFromScreen(root_view_.get(), &mouse_location);
  last_mouse_event_was_move_ = false;
  ui::MouseEvent mouse_event(ui::EventType::kMouseMoved, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_IS_SYNTHESIZED, 0);
  root_view_->OnMouseMoved(mouse_event);
}

ui::GestureRecognizer* Widget::GetGestureRecognizer() {
  return native_widget_ ? native_widget_->GetGestureRecognizer() : nullptr;
}

ui::GestureConsumer* Widget::GetGestureConsumer() {
  return native_widget_ ? native_widget_->GetGestureConsumer() : nullptr;
}

void Widget::OnSizeConstraintsChanged() {
  if (native_widget_) {
    native_widget_->OnSizeConstraintsChanged();
  }

  if (non_client_view_) {
    non_client_view_->SizeConstraintsChanged();
  }

  observers_.Notify(&WidgetObserver::OnWidgetSizeConstraintsChanged, this);
}

void Widget::OnWindowModalVisibilityChanged(bool visible) {
  // Because there are non-views window modals the initiator of this
  // notification is platform-dependent:
  // - On Mac: initiated by NativeWidget, i.e.,
  //   NativeWidgetMacNSWindowHost::OnSheetModalShown/Closed.
  // - Others: initiated by child Widget, i.e.,
  //   Widget::OnNativeWidgetVisibilityChanged.
  // - all platforms: initiated by a CLIENT_OWNS_WIDGET child Widget when the
  //   client destroys it.
  // TODO(crbug.com/450705434): on Windows and Linux the file select dialog is
  // also non-views dialog. Send the notification on showing and closing such
  // dialogs too.
  observers_.Notify(&WidgetObserver::OnWidgetWindowModalVisibilityChanged, this,
                    visible);
}

void Widget::OnOwnerClosing() {}

bool Widget::GetIsDesktopWidget() const {
  return native_widget_ ? native_widget_->IsDesktopNativeWidget() : false;
}

std::string Widget::GetName() const {
  return native_widget_ ? native_widget_->GetName() : "";
}

base::CallbackListSubscription Widget::RegisterPaintAsActiveChangedCallback(
    PaintAsActiveCallbackList::CallbackType callback) {
  return paint_as_active_callbacks_.Add(std::move(callback));
}

std::unique_ptr<Widget::PaintAsActiveLock> Widget::LockPaintAsActive() {
  const bool was_paint_as_active = ShouldPaintAsActive();
  ++paint_as_active_refcount_;
  if (ShouldPaintAsActive() != was_paint_as_active) {
    NotifyPaintAsActiveChanged();
    if (parent() && !parent_paint_as_active_lock_) {
      parent_paint_as_active_lock_ = parent()->LockPaintAsActive();
    }
  }
  return std::make_unique<PaintAsActiveLockImpl>(
      weak_ptr_factory_.GetWeakPtr());
}

base::WeakPtr<Widget> Widget::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool Widget::ShouldPaintAsActive() const {
  // A transient bubble hits this code path when it loses focus.
  // Return false after Close() is called.
  if (widget_closed_) {
    return false;
  }

  return native_widget_active_ || paint_as_active_refcount_ ||
         (parent() && parent()->ShouldPaintAsActive());
}

void Widget::OnParentShouldPaintAsActiveChanged() {
  // |native_widget_| has already been deleted and |this| is being deleted so
  // that we don't have to handle the event and also it's unsafe to reference
  // |native_widget_| in this case.
  if (!native_widget_) {
    return;
  }

  // |native_widget_active| is being updated in
  // OnNativeWidgetActivationChanged(). Notification will be handled there.
  if (native_widget_active_ != native_widget_->IsActive()) {
    return;
  }

  // this->ShouldPaintAsActive() changes iff the native widget is
  // inactive and there's no lock on this widget.
  if (!(native_widget_active_ || paint_as_active_refcount_)) {
    NotifyPaintAsActiveChanged();
  }
}

void Widget::NotifyPaintAsActiveChanged() {
  // In the case the Widget has closed do not notify paint as active changes to
  // mitigate the risk of UAFs and attempted accesses to torn-down Widget
  // subclass state.
  if (widget_closed_) {
    return;
  }
  paint_as_active_callbacks_.Notify();
  if (native_widget_) {
    native_widget_->PaintAsActiveChanged();
  }
}

void Widget::SetNativeTheme(ui::NativeTheme* native_theme) {
  const bool is_update = native_theme_ && (native_theme_ != native_theme);
  native_theme_ = native_theme;
  native_theme_observation_.Reset();
  if (native_theme) {
    native_theme_observation_.Observe(native_theme);
  }

  if (is_update) {
    OnNativeThemeUpdated(native_theme);
  } else {
    ThemeChanged();
  }
}

int Widget::GetX() const {
  return GetRestoredBounds().x();
}

int Widget::GetY() const {
  return GetRestoredBounds().y();
}

int Widget::GetWidth() const {
  return GetRestoredBounds().width();
}

int Widget::GetHeight() const {
  return GetRestoredBounds().height();
}

bool Widget::GetVisible() const {
  return IsVisible();
}

void Widget::SetX(int x) {
  gfx::Rect bounds = GetRestoredBounds();
  if (x == bounds.x()) {
    return;
  }
  bounds.set_x(x);
  SetBounds(bounds);
}

void Widget::SetY(int y) {
  gfx::Rect bounds = GetRestoredBounds();
  if (y == bounds.y()) {
    return;
  }
  bounds.set_y(y);
  SetBounds(bounds);
}

void Widget::SetWidth(int width) {
  gfx::Rect bounds = GetRestoredBounds();
  if (width == bounds.width()) {
    return;
  }
  bounds.set_width(width);
  SetBounds(bounds);
}

void Widget::SetHeight(int height) {
  gfx::Rect bounds = GetRestoredBounds();
  if (height == bounds.height()) {
    return;
  }
  bounds.set_height(height);
  SetBounds(bounds);
}

void Widget::SetVisible(bool visible) {
  if (visible == IsVisible()) {
    return;
  }
  if (visible) {
    Show();
  } else {
    Hide();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Widget, NativeWidgetDelegate implementation:

bool Widget::IsModal() const {
  if (!widget_delegate_) {
    return false;
  }

  return widget_delegate_->GetModalType() != ui::mojom::ModalType::kNone;
}

bool Widget::IsDialogBox() const {
  if (!widget_delegate_) {
    return false;
  }

  return !!widget_delegate_->AsDialogDelegate();
}

bool Widget::CanActivate() const {
  // This may be called after OnNativeWidgetDestroyed(), which sets
  // |widget_delegate_| to null.
  return widget_delegate_ && widget_delegate_->CanActivate();
}

bool Widget::IsNativeWidgetInitialized() const {
  return native_widget_initialized_;
}

bool Widget::OnNativeWidgetActivationChanged(bool active) {
  if (!ShouldHandleNativeWidgetActivationChanged(active)) {
    return false;
  }

  // On windows we may end up here before we've completed initialization (from
  // an WM_NCACTIVATE). If that happens the WidgetDelegate likely doesn't know
  // the Widget and will crash attempting to access it.
  if (!active && native_widget_initialized_) {
    SaveWindowPlacement();
  }

  observers_.Notify(&WidgetObserver::OnWidgetActivationChanged, this, active);

  if (active) {
    internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetActivated(
        this);

    base::AutoReset<bool> is_traversing_widget_tree(&is_traversing_widget_tree_,
                                                    true);
    Widget* root = nullptr;
    for (Widget* widget = this; widget; widget = widget->parent()) {
      widget->observers_.Notify(&WidgetObserver::OnWidgetTreeActivated, widget,
                                this);
      root = widget;
    }
#if BUILDFLAG(IS_WIN)
    // Windows shuffles child widgets when the application re-gains
    // activation, so re-order to ensure z-order sublevels.
    root->GetSublevelManager()->EnsureOwnerTreeSublevel();
#else
    std::ignore = root;
#endif
  }

  const bool was_paint_as_active = ShouldPaintAsActive();

  // Widgets in a widget tree should share the same ShouldPaintAsActive().
  // Lock the parent as paint-as-active when this widget becomes active.
  // If we're in the process of closing the widget, delay resetting the
  // `parent_paint_as_active_lock_` until the owning native widget destroys this
  // widget (i.e. wait until widget destruction). Do this as closing a widget
  // may result in synchronously calling into this method, which can cause the
  // parent to immediately paint as inactive. This is an issue if, after this
  // widget has been closed, the parent widget is the next widget to receive
  // activation. If using a desktop native widget, the next widget to receive
  // activation may be determined by the system's window manager and this may
  // not happen synchronously with closing the Widget. By waiting for the owning
  // native widget to destroy this widget we ensure that resetting the paint
  // lock happens synchronously with the activation the next widget (see
  // crbug/1303549).
  if (!active && !paint_as_active_refcount_ && !widget_closed_) {
    parent_paint_as_active_lock_.reset();
  } else if (parent()) {
    parent_paint_as_active_lock_ = parent()->LockPaintAsActive();
  }

  native_widget_active_ = active;

  // Notify controls (e.g. LabelButton) and children widgets about the
  // paint-as-active change.
  if (ShouldPaintAsActive() != was_paint_as_active) {
    NotifyPaintAsActiveChanged();
  }

  return true;
}

bool Widget::ShouldHandleNativeWidgetActivationChanged(bool active) {
  return (g_disable_activation_change_handling_ !=
          DisableActivationChangeHandlingType::kIgnore) &&
         (g_disable_activation_change_handling_ !=
              DisableActivationChangeHandlingType::kIgnoreDeactivationOnly ||
          active);
}

void Widget::OnNativeFocus() {
  NativeViewFocusManager::GetInstance()->OnNativeFocusChanged(GetNativeView());
}

void Widget::OnNativeBlur() {
  NativeViewFocusManager::GetInstance()->OnNativeFocusChanged(gfx::NativeView());
}

void Widget::OnNativeWidgetVisibilityChanged(bool visible) {
  View* root = GetRootView();
  if (root) {
    root->PropagateVisibilityNotifications(nullptr, visible);
  }
  observers_.Notify(&WidgetObserver::OnWidgetVisibilityChanged, this, visible);
  if (GetCompositor() && root && root->layer()) {
    root->layer()->SetVisible(visible);
  }

#if !BUILDFLAG(IS_MAC)
  // MacOS sends these notifications through the NativeWidgetMacNSWindowHost's
  // OnSheetModalShown and OnSheetModalClosed methods because there're non-views
  // window modal sheets.
  MaybeNotifyParentAboutWindowModalVisibilityChanged(visible);
#endif
}

void Widget::OnNativeWidgetVisibilityOnScreenChanged(bool visible) {
  observers_.Notify(&WidgetObserver::OnWidgetVisibilityOnScreenChanged, this,
                    visible);
}

void Widget::OnNativeWidgetCreated() {
  if (is_top_level()) {
    focus_manager_ = FocusManagerFactory::Create(this);
  }

  DCHECK(native_widget_);
  DCHECK(widget_delegate_);
  native_widget_->InitModalType(widget_delegate_->GetModalType());

  observers_.Notify(&WidgetObserver::OnWidgetCreated, this);
}

void Widget::OnNativeWidgetDestroying() {
  // Tell the focus manager (if any) that root_view is being removed
  // in case that the focused view is under this root view.
  DCHECK(native_widget_);
  HandleWidgetDestroying();
}

void Widget::OnNativeWidgetDestroyed() {
  // Mark the widget as closed so that DeleteDelegate() won't call
  // InvalidateLayout().
  widget_closed_ = true;
  // HandleWidgetDestroyed() may cause the destruction of `this`. Save `this`
  // as a WeakPtr in order to later check whether `this` has been destroyed.
  auto weak_this = GetWeakPtr();
  HandleWidgetDestroyed();
  // The following will ensure that a Widget is always destroyed synchronously
  // along with the NativeWidget even if the NativeWidget is being destroyed by
  // a parent Widget or the platform. If `override_close_` is set, the client
  // is intending to make the closing process synchronous. If the callback
  // does not reset the Widget, the Widget will be left in a closed, zombie-like
  // state. It is strongly recommended to reset the Widget within the callback.
  if (weak_this && override_close_) {
    std::move(override_close_).Run(closed_reason());
  }
}

void Widget::OnNativeWidgetParentChanged(gfx::NativeView parent) {
  Widget* parent_widget = parent ? GetWidgetForNativeView(parent) : nullptr;
  HandleNativeWidgetReparented(parent_widget);
}

gfx::Size Widget::GetMinimumSize() const {
  return non_client_view_ ? non_client_view_->GetMinimumSize() : gfx::Size();
}

gfx::Size Widget::GetMaximumSize() const {
  return non_client_view_ ? non_client_view_->GetMaximumSize() : gfx::Size();
}

void Widget::OnNativeWidgetMove() {
  TRACE_EVENT0("ui", "Widget::OnNativeWidgetMove");

  if (widget_delegate_) {
    widget_delegate_->OnWidgetMove();
  }
  NotifyCaretBoundsChanged(GetInputMethod());

  observers_.Notify(&WidgetObserver::OnWidgetBoundsChanged, this,
                    GetWindowBoundsInScreen());
}

void Widget::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  TRACE_EVENT0("ui", "Widget::OnNativeWidgetSizeChanged");

  View* root = GetRootView();
  if (root) {
    root->SetSize(new_size);
  }

  NotifyCaretBoundsChanged(GetInputMethod());
  SaveWindowPlacementIfNeeded();

  base::AutoReset auto_reset(&save_window_placement_allowed_, false);

  observers_.Notify(&WidgetObserver::OnWidgetBoundsChanged, this,
                    GetWindowBoundsInScreen());

#if BUILDFLAG(IS_WIN)
  ui::mojom::WindowShowState show_state = GetShowState(this);
  if (saved_show_state_ != show_state) {
    OnNativeWidgetWindowShowStateChanged();
    saved_show_state_ = show_state;
  }
#endif
}

void Widget::OnNativeWidgetUserResizeStarted() {
  observers_.Notify(&WidgetObserver::OnWidgetUserResizeStarted);
}

void Widget::OnNativeWidgetUserResizeEnded() {
  observers_.Notify(&WidgetObserver::OnWidgetUserResizeEnded);
}

void Widget::OnNativeWidgetWorkspaceChanged() {}

void Widget::OnNativeWidgetWindowShowStateChanged() {
  SaveWindowPlacementIfNeeded();

  observers_.Notify(&WidgetObserver::OnWidgetShowStateChanged, this);
}

void Widget::OnNativeWidgetBeginUserBoundsChange() {
  if (widget_delegate_) {
    widget_delegate_->OnWindowBeginUserBoundsChange();
  }
}

void Widget::OnNativeWidgetEndUserBoundsChange() {
  if (widget_delegate_) {
    widget_delegate_->OnWindowEndUserBoundsChange();
  }
}

void Widget::OnNativeWidgetAddedToCompositor() {}

void Widget::OnNativeWidgetRemovingFromCompositor() {}

bool Widget::HasFocusManager() const {
  return !!focus_manager_.get();
}

void Widget::OnNativeWidgetPaint(const ui::PaintContext& context) {
  // On Linux Aura, we can get here during Init() because of the
  // SetInitialBounds call.
  if (!native_widget_initialized_) {
    return;
  }
  GetRootView()->PaintFromPaintRoot(context);
}

int Widget::GetNonClientComponent(const gfx::Point& point) {
  int component =
      non_client_view_ ? non_client_view_->NonClientHitTest(point) : HTNOWHERE;

  if (movement_disabled_ &&
      (component == HTCAPTION || component == HTSYSMENU)) {
    return HTNOWHERE;
  }

  return component;
}

void Widget::OnKeyEvent(ui::KeyEvent* event) {
  SendEventToSink(event);
  if (!event->handled() && GetFocusManager() &&
      !GetFocusManager()->OnKeyEvent(*event)) {
    event->StopPropagation();
  }
}

// TODO(tdanderson): We should not be calling the OnMouse*() functions on
//                   RootView from anywhere in Widget. Use
//                   SendEventToSink() instead. See crbug.com/348087.
void Widget::OnMouseEvent(ui::MouseEvent* event) {
  if (!native_widget_) {
    return;
  }

  TRACE_EVENT0("ui", "Widget::OnMouseEvent");

  View* root_view = GetRootView();
  switch (event->type()) {
    case ui::EventType::kMousePressed: {
      last_mouse_event_was_move_ = false;

      // We may get deleted by the time we return from OnMousePressed. So we
      // use an observer to make sure we are still alive.
      WidgetDeletionObserver widget_deletion_observer(this);

      gfx::NativeView current_capture =
          internal::NativeWidgetPrivate::GetGlobalCapture(
              native_widget_->GetNativeView());
      // Make sure we're still visible before we attempt capture as the mouse
      // press processing may have made the window hide (as happens with menus).
      //
      // It is possible that capture has changed as a result of a mouse-press.
      // In these cases do not update internal state.
      //
      // A mouse-press may trigger a nested message-loop, and absorb the paired
      // release. If so the code returns here. So make sure that that
      // mouse-button is still down before attempting to do a capture.
      if (root_view && root_view->OnMousePressed(*event) &&
          widget_deletion_observer.IsWidgetAlive() && IsVisible() &&
          native_widget_->IsMouseButtonDown() &&
          current_capture == internal::NativeWidgetPrivate::GetGlobalCapture(
                                 native_widget_->GetNativeView())) {
        is_mouse_button_pressed_ = true;
        if (!native_widget_->HasCapture()) {
          native_widget_->SetCapture();
        }
        event->SetHandled();
      }
      return;
    }

    case ui::EventType::kMouseReleased:
      last_mouse_event_was_move_ = false;
      is_mouse_button_pressed_ = false;
      // Release capture first, to avoid confusion if OnMouseReleased blocks.
      if (auto_release_capture_ && native_widget_->HasCapture()) {
        base::AutoReset<bool> resetter(&ignore_capture_loss_, true);
        native_widget_->ReleaseCapture();
      }
      if (root_view) {
        root_view->OnMouseReleased(*event);
      }
      if ((event->flags() & ui::EF_IS_NON_CLIENT) == 0 &&
          // If none of the "normal" buttons are pressed, this event may be from
          // one of the newer mice that have buttons bound to browser forward
          // back actions. Don't squelch the event and let the default handler
          // process it.
          (event->flags() &
           (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON |
            ui::EF_RIGHT_MOUSE_BUTTON)) != 0) {
        event->SetHandled();
      }
      return;

    case ui::EventType::kMouseMoved:
    case ui::EventType::kMouseDragged:
      if (native_widget_->HasCapture() && is_mouse_button_pressed_) {
        last_mouse_event_was_move_ = false;
        if (root_view) {
          root_view->OnMouseDragged(*event);
        }
      } else if (!last_mouse_event_was_move_ ||
                 last_mouse_event_position_ != event->location()) {
        last_mouse_event_position_ = event->location();
        last_mouse_event_was_move_ = true;
        if (root_view) {
          root_view->OnMouseMoved(*event);
        }
      }
      return;

    case ui::EventType::kMouseEntered:
      last_mouse_event_was_move_ = false;
      if (root_view) {
        root_view->OnMouseEntered(*event);
      }
      return;

    case ui::EventType::kMouseExited:
      last_mouse_event_was_move_ = false;
      if (root_view) {
        root_view->OnMouseExited(*event);
      }
      return;

    case ui::EventType::kMousewheel:
      if (root_view && root_view->OnMouseWheel(
                           static_cast<const ui::MouseWheelEvent&>(*event))) {
        event->SetHandled();
      }
      return;

    default:
      return;
  }
}

void Widget::OnMouseCaptureLost() {
  if (ignore_capture_loss_) {
    return;
  }

  View* root_view = GetRootView();
  if (root_view) {
    root_view->OnMouseCaptureLost();
  }
  is_mouse_button_pressed_ = false;
}

void Widget::OnScrollEvent(ui::ScrollEvent* event) {
  ui::ScrollEvent event_copy(*event);
  SendEventToSink(&event_copy);

  // Convert unhandled ui::EventType::kScroll events into
  // ui::EventType::kMousewheel events.
  if (!event_copy.handled() && event_copy.type() == ui::EventType::kScroll) {
    ui::MouseWheelEvent wheel(*event);
    OnMouseEvent(&wheel);
  }
}

void Widget::OnGestureEvent(ui::GestureEvent* event) {
  // We explicitly do not capture here. Not capturing enables multiple widgets
  // to get tap events at the same time. Views (such as tab dragging) may
  // explicitly capture.
  SendEventToSink(event);
}

bool Widget::ExecuteCommand(int command_id) {
  if (!widget_delegate_) {
    return false;
  }

  return widget_delegate_->ExecuteWindowsCommand(command_id);
}

bool Widget::HasHitTestMask() const {
  if (!widget_delegate_) {
    return false;
  }

  return widget_delegate_->WidgetHasHitTestMask();
}

void Widget::GetHitTestMask(SkPath* mask) const {
  if (!widget_delegate_) {
    return;
  }

  DCHECK(mask);
  widget_delegate_->GetWidgetHitTestMask(mask);
}

Widget* Widget::AsWidget() {
  return this;
}

const Widget* Widget::AsWidget() const {
  return this;
}

bool Widget::SetInitialFocus(ui::mojom::WindowShowState show_state) {
  FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager || !widget_delegate_) {
    return false;
  }
  View* v = widget_delegate_->GetInitiallyFocusedView();
  if (!focus_on_creation_ ||
      show_state == ui::mojom::WindowShowState::kInactive ||
      show_state == ui::mojom::WindowShowState::kMinimized) {
    // If not focusing the window now, tell the focus manager which view to
    // focus when the window is restored.
    if (v) {
      focus_manager->SetStoredFocusView(v);
    }
    return true;
  }
  if (v) {
    v->RequestFocus();
    // If the Widget is active (thus allowing its child Views to receive focus),
    // but the request for focus was unsuccessful, fall back to using the first
    // focusable View instead.
    if (focus_manager->GetFocusedView() == nullptr && IsActive()) {
      focus_manager->AdvanceFocus(false);
    }
  }
  return !!focus_manager->GetFocusedView();
}

bool Widget::ShouldDescendIntoChildForEventHandling(
    ui::Layer* root_layer,
    gfx::NativeView child,
    ui::Layer* child_layer,
    const gfx::Point& location) {
  if (widget_delegate_ &&
      !widget_delegate_->ShouldDescendIntoChildForEventHandling(child,
                                                                location)) {
    return false;
  }

  const View::Views& views_with_layers = GetViewsWithLayersInZOrder();
  if (views_with_layers.empty()) {
    return true;
  }

  // Don't descend into |child| if there is a view with a Layer that contains
  // the point and is stacked above |child_layer|.
  auto child_layer_iter =
      std::ranges::find(root_layer->children(), child_layer);
  if (child_layer_iter == root_layer->children().end()) {
    return true;
  }

  for (View* view : base::Reversed(views_with_layers)) {
    // Skip views that don't process events.
    if (!view->GetCanProcessEventsWithinSubtree()) {
      continue;
    }
    ui::Layer* layer = view->layer();
    DCHECK(layer);
    if (layer->visible() && layer->bounds().Contains(location)) {
      auto root_layer_iter = std::ranges::find(root_layer->children(), layer);
      if (child_layer_iter > root_layer_iter) {
        // |child| is on top of the remaining layers, no need to continue.
        return true;
      }

      // TODO(pbos): Does this need to be made more robust through hit testing
      // or using ViewTargeter? This for instance does not take into account
      // whether the view is enabled/drawn/etc.
      //
      // Event targeting uses the visible bounds of the View, which may differ
      // from the bounds of the layer. Verify the view hosting the layer
      // actually contains |location|. Use GetVisibleBounds(), which is
      // effectively what event targetting uses.
      gfx::Rect vis_bounds = view->GetVisibleBounds();
      gfx::Point point_in_view = location;
      View::ConvertPointToTarget(GetRootView(), view, &point_in_view);
      if (vis_bounds.Contains(point_in_view)) {
        return false;
      }
    }
  }
  return true;
}

void Widget::LayoutRootViewIfNecessary() {
  if (is_autosized() && autosize_task_factory_.HasWeakPtrs()) {
    // If there is an autosize task in the task queue, consume it before layout.
    // Otherwise this layout may be incorrect.
    autosize_task_factory_.InvalidateWeakPtrs();
    ResizeToDelegateDesiredBounds();
  }

  if (root_view_ && root_view_->needs_layout()) {
    // Widget name is only collected in local traces.
    TRACE_EVENT1("ui", "Widget::LayoutRootViewIfNecessary", "widget name",
                 GetName());
    root_view_->LayoutImmediately();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Widget, ui::EventSource implementation:
ui::EventSink* Widget::GetEventSink() {
  return root_view_.get();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, FocusTraversable implementation:

FocusSearch* Widget::GetFocusSearch() {
  return root_view_->GetFocusSearch();
}

FocusTraversable* Widget::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
}

View* Widget::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, ui::NativeThemeObserver implementation:

void Widget::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  TRACE_EVENT0("ui", "Widget::OnNativeThemeUpdated");
  ThemeChanged();
}

void Widget::OnAXModeAdded(ui::AXMode mode) {
  if (mode == ui::AXMode::kNativeAPIs) {
    auto* root_view = GetRootView();
    if (root_view) {
      // The root view's accessibility cache is always fully initialized, so we
      // only have to recursively complete for its descendants.
      for (View* child : root_view->children()) {
        child->GetViewAccessibility().CompleteCacheInitialization();
      }
    }
  }
}

void Widget::SetColorModeOverride(
    std::optional<ui::ColorProviderKey::ColorMode> color_mode) {
  if (color_mode != color_mode_override_) {
    color_mode_override_ = color_mode;
    ThemeChanged();
  }
}

void Widget::SetBackgroundColor(std::optional<ui::ColorId> background_color) {
  if (background_color != background_color_) {
    background_color_ = background_color;
    if (native_widget_) {
      native_widget_->SetBackgroundColor(
          GetColorProvider()->GetColor(GetBackgroundColorId()));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Widget, ui::ColorProviderSource:

ui::ColorProviderKey Widget::GetColorProviderKey() const {
  // Generally all Widgets should inherit the key of their parent, falling back
  // to the key set by the NativeTheme otherwise.
  // TODO(crbug.com/40272831): `parent_` does not always resolve to the logical
  // parent as expected here (e.g. bubbles). This should be addressed and the
  // use of parent_ below replaced with something like GetLogicalParent().
  ui::ColorProviderKey key =
      parent_ ? parent_->GetColorProviderKey()
              : GetNativeTheme()->GetColorProviderKey(GetCustomTheme());

  // Widgets may have specific overrides set on the Widget itself that should
  // apply specifically to themselves and their children, apply these here.
  if (color_mode_override_.has_value()) {
    key.color_mode = color_mode_override_.value();
  }

  return key;
}

const ui::ColorProvider* Widget::GetColorProvider() const {
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      GetColorProviderKey());
}

ui::RendererColorMap Widget::GetRendererColorMap(
    ui::ColorProviderKey::ColorMode color_mode,
    ui::ColorProviderKey::ForcedColors forced_colors) const {
  auto key = GetColorProviderKey();
  key.color_mode = color_mode;
  key.forced_colors = forced_colors;
  ui::ColorProvider* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(key);
  CHECK(color_provider);
  return ui::CreateRendererColorMap(*color_provider);
}

ui::ColorProviderKey Widget::GetColorProviderKeyForTesting() const {
  return GetColorProviderKey();
}

void Widget::SetCheckParentForFullscreen() {
  check_parent_for_fullscreen_ = true;
}

void Widget::SetAllowScreenshots(bool allow) {
  if (native_widget_) {
    native_widget_->SetAllowScreenshots(allow);
  }
}

bool Widget::AreScreenshotsAllowed() {
  return native_widget_ ? native_widget_->AreScreenshotsAllowed() : true;
}

void Widget::UpdateAccessibleNameForRootView() {
  if (root_view_) {
    root_view_->UpdateAccessibleName();
  }
}

void Widget::UpdateAccessibleURLForRootView(const GURL& url) {
  if (root_view_) {
    root_view_->UpdateAccessibleURL(url);
  }
}

void Widget::SaveWindowPlacementIfNeeded() {
  if (native_widget_initialized_ && save_window_placement_allowed_) {
    SaveWindowPlacement();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Widget, protected:

internal::RootView* Widget::CreateRootView() {
  return new internal::RootView(this);
}

void Widget::DestroyRootView() {
  NotifyWillRemoveView(root_view_.get());
  non_client_view_ = nullptr;
  // Remove all children before the unique_ptr reset so that
  // GetWidget()->GetRootView() doesn't return nullptr while the views hierarchy
  // is being torn down.
  root_view_->RemoveAllChildViews();
  root_view_.reset();
}

void Widget::OnDragWillStart() {}

void Widget::OnDragComplete() {}

const ui::NativeTheme* Widget::GetNativeTheme() const {
  if (native_theme_) {
    return native_theme_;
  }

  if (parent_) {
    return parent_->GetNativeTheme();
  }

#if BUILDFLAG(IS_LINUX)
  if (auto* linux_ui_theme =
          ui::LinuxUiTheme::GetForWindow(GetNativeWindow())) {
    return linux_ui_theme->GetNativeTheme();
  }
#endif

  return ui::NativeTheme::GetInstanceForNativeUi();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, private:

void Widget::SaveWindowPlacement() {
  // The window delegate does the actual saving for us. It seems like (judging
  // by go/crash) that in some circumstances we can end up here after
  // WM_DESTROY, at which point the window delegate is likely gone. So just
  // bail.
  if (is_destroying_ || !widget_delegate_ ||
      !widget_delegate_->ShouldSaveWindowPlacement() || !native_widget_) {
    return;
  }
  ui::mojom::WindowShowState show_state = ui::mojom::WindowShowState::kNormal;
  gfx::Rect bounds;
  native_widget_->GetWindowPlacement(&bounds, &show_state);
  widget_delegate_->SaveWindowPlacement(bounds, show_state);
}

void Widget::SetInitialBounds(const gfx::Rect& bounds) {
  if (!non_client_view_) {
    return;
  }

  gfx::Rect saved_bounds;
  if (GetSavedWindowPlacement(&saved_bounds, &saved_show_state_)) {
    if (saved_show_state_ == ui::mojom::WindowShowState::kMaximized) {
      // If we're going to maximize, wait until Show is invoked to set the
      // bounds. That way we avoid a noticeable resize.
      initial_restored_bounds_ = saved_bounds;
    } else if (!saved_bounds.IsEmpty()) {
      // If the saved bounds are valid, use them.
      SetBounds(saved_bounds);
    }
  } else {
    if (bounds.IsEmpty()) {
      if (bounds.origin().IsOrigin()) {
        // No initial bounds supplied, so size the window to its content and
        // center over its parent.
        CenterWindow(non_client_view_->GetPreferredSize({}));
      } else {
        // Use the preferred size and the supplied origin.
        gfx::Rect preferred_bounds(bounds);
        preferred_bounds.set_size(non_client_view_->GetPreferredSize({}));
        SetBoundsConstrained(preferred_bounds);
      }
    } else {
      // Use the supplied initial bounds.
      SetBoundsConstrained(bounds);
    }
  }
}

void Widget::SetInitialBoundsForFramelessWindow(const gfx::Rect& bounds) {
  if (bounds.IsEmpty()) {
    View* contents_view = GetContentsView();
    DCHECK(contents_view);
    // No initial bounds supplied, so size the window to its content and
    // center over its parent if preferred size is provided.
    gfx::Size size = contents_view->GetPreferredSize({});
    if (!size.IsEmpty() && native_widget_) {
      native_widget_->CenterWindow(size);
    }
  } else {
    // Use the supplied initial bounds.
    SetBounds(bounds);
  }
}

void Widget::HandleNativeWidgetReparented(Widget* parent) {
  if (parent == parent_.get()) {
    return;
  }

  Widget* old_parent = parent_.get();
  CHECK(!is_traversing_widget_tree_);
  parent_ = parent ? parent->GetWeakPtr() : nullptr;

  // Release the paint-as-active lock on the old parent.
  bool has_lock_on_parent = !!parent_paint_as_active_lock_;
  parent_paint_as_active_lock_.reset();
  parent_paint_as_active_subscription_ = base::CallbackListSubscription();

  // Lock and subscribe to parent's paint-as-active.
  if (parent) {
    if (has_lock_on_parent || native_widget_active_) {
      parent_paint_as_active_lock_ = parent->LockPaintAsActive();
    }
    parent_paint_as_active_subscription_ =
        parent->RegisterPaintAsActiveChangedCallback(
            base::BindRepeating(&Widget::OnParentShouldPaintAsActiveChanged,
                                base::Unretained(this)));
  }

  if (old_parent) {
    old_parent->OnChildRemoved(this);
  }
  if (parent) {
    parent->OnChildAdded(this);
  }
}

bool Widget::GetSavedWindowPlacement(gfx::Rect* bounds,
                                     ui::mojom::WindowShowState* show_state) {
  // First we obtain the window's saved show-style and store it. We need to do
  // this here, rather than in Show() because by the time Show() is called,
  // the window's size will have been reset (below) and the saved maximized
  // state will have been lost. Sadly there's no way to tell on Windows when
  // a window is restored from maximized state, so we can't more accurately
  // track maximized state independently of sizing information.

  if (!widget_delegate_->GetSavedWindowPlacement(this, bounds, show_state)) {
    return false;
  }

  gfx::Size minimum_size = GetMinimumSize();
  // Make sure the bounds are at least the minimum size.
  if (bounds->width() < minimum_size.width()) {
    bounds->set_width(minimum_size.width());
  }

  if (bounds->height() < minimum_size.height()) {
    bounds->set_height(minimum_size.height());
  }
  return true;
}

const View::Views& Widget::GetViewsWithLayersInZOrder() {
  if (views_with_layers_dirty_) {
    views_with_layers_dirty_ = false;
    views_with_layers_.clear();
    BuildViewsWithLayersInZOrder(GetRootView(), &views_with_layers_);
  }
  return views_with_layers_;
}

void Widget::UnlockPaintAsActive() {
  const bool was_paint_as_active = ShouldPaintAsActive();
  DCHECK_GT(paint_as_active_refcount_, 0U);
  --paint_as_active_refcount_;

  if (!paint_as_active_refcount_ && !native_widget_active_) {
    parent_paint_as_active_lock_.reset();
  }

  if (ShouldPaintAsActive() != was_paint_as_active) {
    NotifyPaintAsActiveChanged();
  }
}

void Widget::ClearFocusManagerFromWidget() {
  FocusManager* focus_manager = GetFocusManager();
  // We are being removed from a window hierarchy.  Treat this as
  // the root_view_ being removed.
  if (focus_manager) {
    focus_manager->ViewRemoved(root_view_.get());
    CHECK(root_view_);
    // Also notify the view tree in the child widget
    // to perform actions when focus is cleared.
    if (!is_top_level()) {
      root_view_->PropagateWillClearFocusManager();
    }
  }
}

void Widget::MaybeNotifyParentAboutWindowModalVisibilityChanged(bool visible) {
  if (!widget_delegate()) {
    return;
  }

  if (widget_delegate()->GetModalType() != ui::mojom::ModalType::kWindow) {
    return;
  }

  if (!parent_) {
    return;
  }

  parent_->OnWindowModalVisibilityChanged(visible);
}

void Widget::HandleShowRequested() {
  sublevel_manager_->EnsureOwnerSublevel();
  internal::AnyWidgetObserverSingleton::GetInstance()->OnAnyWidgetShown(this);
}

void Widget::HandleWidgetDestroying() {
  if (native_widget_destroyed_) {
    return;
  }
  ClearFocusManagerFromWidget();
  if (parent_) {
    parent_->OnChildRemoved(this);
  }
  observers_.Notify(&WidgetObserver::OnWidgetDestroying, this);
  if (non_client_view_) {
    non_client_view_->WindowClosing();
  }
  if (widget_delegate_) {
    widget_delegate_->WindowClosing();
  }
}

void Widget::HandleWidgetDestroyed() {
  if (native_widget_destroyed_) {
    return;
  }

  // The widget can still be visible. This happens on macOS when
  // the client destroys a CLIENT_OWNS_WIDGET widget. The OS has no
  // chance to send us a visibility change event.
  if (IsVisible()) {
    MaybeNotifyParentAboutWindowModalVisibilityChanged(false);
  }

  ax_mode_observation_.Reset();

  observers_.Notify(&WidgetObserver::OnWidgetDestroyed, this);

  native_widget_destroyed_ = true;
  auto weak_ptr = GetWeakPtr();
  if (widget_delegate_) {
    widget_delegate_->DeleteDelegate();
  }
  // When the ownership_ is CLIENT_OWNS_WIDGET, the DeleteDelegate() call above
  // *might* also cause the Widget to be destroyed. The following statement
  // checks for this. If this function is called from within the Widget
  // destructor, the delegate has already been notified (WidgetDestroyed() is
  // called from the destructor) that the Widget is being destroyed, so the call
  // to inform the client that the Widget is a "zombie"
  // (WidgetDelegate::WidgetIsZombie()) isn't performed.
  if (!weak_ptr) {
    return;
  }
  // Immediately reset the weak ptr. If NATIVE_WIDGET_OWNS_WIDGET destruction of
  // the NativeWidget can destroy the Widget. We don't want to touch the
  // NativeWidget during the destruction of the Widget either since some member
  // variables on the NativeWidget may already be destroyed. In
  // WIDGET_OWNS_NATIVE_WIDGET the NativeWidget will be cleaned up through
  // |owned_native_widget_|
  native_widget_.reset();
}

void Widget::OnChildAdded(Widget* child_widget) {
  CHECK(child_widget);
  if (ax_manager_ && child_widget->ax_manager_) {
    ax_manager_->OnChildManagerAdded(*child_widget->ax_manager_);
  }
  observers_.Notify(&WidgetObserver::OnWidgetChildAdded, this, child_widget);
}

void Widget::OnChildRemoved(Widget* child_widget) {
  CHECK(child_widget);
  if (ax_manager_ && child_widget->ax_manager_) {
    ax_manager_->OnChildManagerRemoved(*child_widget->ax_manager_);
  }
  observers_.Notify(&WidgetObserver::OnWidgetChildRemoved, this, child_widget);
}

void Widget::ResizeToDelegateDesiredBounds() {
  if (!IsNativeWidgetInitialized() || !widget_delegate_ || !GetContentsView()) {
    return;
  }

  gfx::Rect desired_bounds = widget_delegate_->GetDesiredWidgetBounds();
  if (desired_bounds.IsEmpty() || desired_bounds == GetWindowBoundsInScreen()) {
    return;
  }

  // Size to contents view.
  SetBounds(desired_bounds);
}

void Widget::SetClientContentsViewInternal(std::unique_ptr<View> view) {
  if (non_client_view_) {
    auto* client_view = non_client_view_->client_view();
    // Remove/destroy the existing client contents view(s), if present.
    if (!client_view->children().empty()) {
      client_view->RemoveAllChildViews();
    }
    client_view->set_contents_view(view.get());
    client_view->AddChildView(std::move(view));
  } else {
    SetContentsView(view.release());
  }
  root_view_->LayoutImmediately();
}

ui::ColorId Widget::GetBackgroundColorId() const {
  return background_color_.value_or(ui::kColorWindowBackground);
}

BEGIN_METADATA_BASE(Widget)
ADD_READONLY_PROPERTY_METADATA(std::string_view, ClassName)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, ClientAreaBoundsInScreen)
ADD_READONLY_PROPERTY_METADATA(std::string, Name)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, RestoredBounds)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, WindowBoundsInScreen)
ADD_READONLY_PROPERTY_METADATA(bool, IsDesktopWidget)
ADD_PROPERTY_METADATA(int, X)
ADD_PROPERTY_METADATA(int, Y)
ADD_PROPERTY_METADATA(int, Width)
ADD_PROPERTY_METADATA(int, Height)
ADD_PROPERTY_METADATA(bool, Visible)
ADD_PROPERTY_METADATA(ui::ZOrderLevel, ZOrderLevel)
ADD_PROPERTY_METADATA(gfx::Size, Size)
END_METADATA

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, NativeWidget implementation:

internal::NativeWidgetPrivate* NativeWidgetPrivate::AsNativeWidgetPrivate() {
  return this;
}

}  // namespace internal
}  // namespace views

DEFINE_ENUM_CONVERTERS(ui::ZOrderLevel,
                       {ui::ZOrderLevel::kNormal, u"kNormal"},
                       {ui::ZOrderLevel::kFloatingWindow, u"kFloatingWindow"},
                       {ui::ZOrderLevel::kFloatingUIElement,
                        u"kFloatingUIElement"},
                       {ui::ZOrderLevel::kSecuritySurface, u"kSecuritySurface"})
