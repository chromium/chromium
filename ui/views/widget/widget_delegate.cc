// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_delegate.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"

namespace views {

namespace {

std::unique_ptr<ClientView> CreateDefaultClientView(WidgetDelegate* delegate,
                                                    Widget* widget) {
  return std::make_unique<ClientView>(
      widget, delegate->TransferOwnershipOfContentsView());
}

std::unique_ptr<View> CreateDefaultOverlayView() {
  return nullptr;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WidgetDelegate:

WidgetDelegate::Params::Params() = default;
WidgetDelegate::Params::~Params() = default;

WidgetDelegate::WidgetDelegate()
    : widget_initialized_callbacks_(std::make_unique<ClosureVector>()),
      client_view_factory_(
          base::BindOnce(&CreateDefaultClientView, base::Unretained(this))),
      overlay_view_factory_(base::BindOnce(&CreateDefaultOverlayView)) {}

WidgetDelegate::~WidgetDelegate() {
  CHECK(can_delete_this_) << "A WidgetDelegate must outlive its Widget";
  if (!contents_view_taken_ && default_contents_view_ &&
      !default_contents_view_->parent()) {
    delete default_contents_view_;
    default_contents_view_ = nullptr;
  }
  if (destructor_ran_) {
    DCHECK(!*destructor_ran_);
    *destructor_ran_ = true;
  }
}

void WidgetDelegate::SetCanActivate(bool can_activate) {
  can_activate_ = can_activate;
}

void WidgetDelegate::OnWidgetMove() {}

void WidgetDelegate::OnDisplayChanged() {}

void WidgetDelegate::OnWorkAreaChanged() {}

bool WidgetDelegate::OnCloseRequested(Widget::ClosedReason close_reason) {
  return true;
}

View* WidgetDelegate::GetInitiallyFocusedView() {
  return params_.initially_focused_view.value_or(nullptr);
}

bool WidgetDelegate::HasConfiguredInitiallyFocusedView() const {
  return params_.initially_focused_view.has_value();
}

BubbleDialogDelegate* WidgetDelegate::AsBubbleDialogDelegate() {
  return nullptr;
}

DialogDelegate* WidgetDelegate::AsDialogDelegate() {
  return nullptr;
}

bool WidgetDelegate::CanResize() const {
  return params_.can_resize;
}

bool WidgetDelegate::CanMaximize() const {
  return params_.can_maximize;
}

bool WidgetDelegate::CanMinimize() const {
  return params_.can_minimize;
}

bool WidgetDelegate::CanActivate() const {
  return can_activate_;
}

ui::ModalType WidgetDelegate::GetModalType() const {
  return params_.modal_type;
}

ax::mojom::Role WidgetDelegate::GetAccessibleWindowRole() {
  return params_.accessible_role;
}

std::u16string WidgetDelegate::GetAccessibleWindowTitle() const {
  return params_.accessible_title.empty() ? GetWindowTitle()
                                          : params_.accessible_title;
}

std::u16string WidgetDelegate::GetWindowTitle() const {
  return params_.title;
}

bool WidgetDelegate::ShouldShowWindowTitle() const {
  return params_.show_title;
}

bool WidgetDelegate::ShouldCenterWindowTitleText() const {
#if defined(USE_AURA)
  return params_.center_title;
#else
  return false;
#endif
}

// TODO(ffred): refactor this method.
bool WidgetDelegate::RotatePaneFocusFromView(View* focused_view,
                                             bool forward,
                                             bool enable_wrapping) {
  // Get the list of all accessible panes.
  std::vector<View*> panes;
  GetAccessiblePanes(&panes);

  // Count the number of panes and set the default index if no pane
  // is initially focused.
  const size_t count = panes.size();
  if (!count) {
    return false;
  }

  // Initialize |index| to an appropriate starting index if nothing is
  // focused initially.
  size_t index = forward ? (count - 1) : 0;

  // Check to see if a pane already has focus and update the index accordingly.
  if (focused_view) {
    const auto i =
        base::ranges::find_if(panes, [focused_view](const auto* pane) {
          return pane && pane->Contains(focused_view);
        });
    if (i != panes.cend()) {
      index = static_cast<size_t>(i - panes.cbegin());
    }
  }

  // Rotate focus.
  for (const size_t start_index = index;;) {
    index = (!forward ? (index + count - 1) : (index + 1)) % count;

    if (!enable_wrapping && (index == (forward ? 0 : (count - 1)))) {
      return false;
    }

    // Ensure that we don't loop more than once.
    if (index == start_index) {
      return false;
    }

    views::View* pane = panes[index];
    DCHECK(pane);
    if (pane->GetVisible()) {
      pane->RequestFocus();
      // |pane| may be in a different widget, so don't assume its focus manager
      // is |this|.
      focused_view = pane->GetWidget()->GetFocusManager()->GetFocusedView();
      if (pane == focused_view || pane->Contains(focused_view)) {
        return true;
      }
    }
  }
}

bool WidgetDelegate::ShouldShowCloseButton() const {
  return params_.show_close_button;
}

ui::ImageModel WidgetDelegate::GetWindowAppIcon() {
  // Prefer app icon if available.
  if (!params_.app_icon.IsEmpty())
    return params_.app_icon;
  // Fall back to the window icon.
  return GetWindowIcon();
}

// Returns the icon to be displayed in the window.
ui::ImageModel WidgetDelegate::GetWindowIcon() {
  return params_.icon;
}

bool WidgetDelegate::ShouldShowWindowIcon() const {
  return params_.show_icon;
}

bool WidgetDelegate::ExecuteWindowsCommand(int command_id) {
  return false;
}

std::string WidgetDelegate::GetWindowName() const {
  return std::string();
}

void WidgetDelegate::SaveWindowPlacement(const gfx::Rect& bounds,
                                         ui::WindowShowState show_state) {
  std::string window_name = GetWindowName();
  if (!window_name.empty()) {
    ViewsDelegate::GetInstance()->SaveWindowPlacement(GetWidget(), window_name,
                                                      bounds, show_state);
  }
}

bool WidgetDelegate::ShouldSaveWindowPlacement() const {
  return !GetWindowName().empty();
}

bool WidgetDelegate::GetSavedWindowPlacement(
    const Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  std::string window_name = GetWindowName();
  if (window_name.empty() ||
      !ViewsDelegate::GetInstance()->GetSavedWindowPlacement(
          widget, window_name, bounds, show_state))
    return false;
  // Try to find a display intersecting the saved bounds.
  const auto& display =
      display::Screen::GetScreen()->GetDisplayMatching(*bounds);
  return display.bounds().Intersects(*bounds);
}

void WidgetDelegate::WidgetInitializing(Widget* widget) {
  widget_ = widget;
}

void WidgetDelegate::WidgetInitialized() {
  for (auto&& callback : *widget_initialized_callbacks_)
    std::move(callback).Run();
  widget_initialized_callbacks_.reset();
  OnWidgetInitialized();
}

void WidgetDelegate::WidgetDestroying() {
  widget_ = nullptr;
}

void WidgetDelegate::WindowWillClose() {
  // TODO(ellyjones): For this and the other callback methods, establish whether
  // any other code calls these methods. If not, DCHECK here and below that
  // these methods are only called once.
  for (auto&& callback : window_will_close_callbacks_)
    std::move(callback).Run();
}

void WidgetDelegate::WindowClosing() {
  for (auto&& callback : window_closing_callbacks_)
    std::move(callback).Run();
}

void WidgetDelegate::DeleteDelegate() {
  bool owned_by_widget = params_.owned_by_widget;
  ClosureVector delete_callbacks;
  delete_callbacks.swap(delete_delegate_callbacks_);

  bool destructor_ran = false;
  destructor_ran_ = &destructor_ran;
  for (auto&& callback : delete_callbacks)
    std::move(callback).Run();

  // TODO(kylixrd): Eventually the widget will never own the delegate, so much
  // of this code will need to be reworked.
  //
  // If the WidgetDelegate is owned by the Widget, it is illegal for the
  // DeleteDelegate callbacks to destruct it; if it is not owned by the Widget,
  // the DeleteDelete callbacks are allowed but not required to destroy it.
  if (owned_by_widget) {
    DCHECK(!destructor_ran);
    // TODO(kylxird): Rework this once the Widget stops being able to "own" the
    // delegate.
    // Only delete this if this delegate was never actually initialized wth a
    // Widget or the delegate isn't "owned" by the Widget.
    if (can_delete_this_) {
      delete this;
      return;
    }
    destructor_ran_ = nullptr;
  } else {
    // If the destructor didn't get run, reset destructor_ran_ so that when it
    // does run it doesn't try to scribble over where our stack was.
    if (!destructor_ran)
      destructor_ran_ = nullptr;
  }
}

Widget* WidgetDelegate::GetWidget() {
  return widget_;
}

const Widget* WidgetDelegate::GetWidget() const {
  return widget_;
}

View* WidgetDelegate::GetContentsView() {
  if (unowned_contents_view_)
    return unowned_contents_view_;
  if (!default_contents_view_)
    default_contents_view_ = new View;
  return default_contents_view_;
}

View* WidgetDelegate::TransferOwnershipOfContentsView() {
  DCHECK(!contents_view_taken_);
  contents_view_taken_ = true;
  if (owned_contents_view_)
    owned_contents_view_.release();
  return GetContentsView();
}

ClientView* WidgetDelegate::CreateClientView(Widget* widget) {
  DCHECK(client_view_factory_);
  return std::move(client_view_factory_).Run(widget).release();
}

std::unique_ptr<NonClientFrameView> WidgetDelegate::CreateNonClientFrameView(
    Widget* widget) {
  return nullptr;
}

View* WidgetDelegate::CreateOverlayView() {
  DCHECK(overlay_view_factory_);
  return std::move(overlay_view_factory_).Run().release();
}

bool WidgetDelegate::WidgetHasHitTestMask() const {
  return false;
}

void WidgetDelegate::GetWidgetHitTestMask(SkPath* mask) const {
  DCHECK(mask);
}

bool WidgetDelegate::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
  return true;
}

void WidgetDelegate::SetAccessibleWindowRole(ax::mojom::Role role) {
  params_.accessible_role = role;
}

void WidgetDelegate::SetAccessibleTitle(std::u16string title) {
  params_.accessible_title = std::move(title);
}

void WidgetDelegate::SetCanMaximize(bool can_maximize) {
  bool old_can_maximize = std::exchange(params_.can_maximize, can_maximize);
  if (GetWidget() && params_.can_maximize != old_can_maximize)
    GetWidget()->OnSizeConstraintsChanged();
}

void WidgetDelegate::SetCanMinimize(bool can_minimize) {
  bool old_can_minimize = std::exchange(params_.can_minimize, can_minimize);
  if (GetWidget() && params_.can_minimize != old_can_minimize)
    GetWidget()->OnSizeConstraintsChanged();
}

void WidgetDelegate::SetCanResize(bool can_resize) {
  bool old_can_resize = std::exchange(params_.can_resize, can_resize);
  if (GetWidget() && params_.can_resize != old_can_resize)
    GetWidget()->OnSizeConstraintsChanged();
}

// TODO (kylixrd): This will be removed once Widget no longer "owns" the
// WidgetDelegate.
void WidgetDelegate::SetOwnedByWidget(bool owned) {
  if (params_.owned_by_widget == owned)
    return;
  params_.owned_by_widget = owned;
  if (widget_ && widget_->widget_delegate_.get() == this) {
    if (params_.owned_by_widget)
      widget_->owned_widget_delegate_ = base::WrapUnique(this);
    else
      widget_->owned_widget_delegate_.release();
  }
}

void WidgetDelegate::SetFocusTraversesOut(bool focus_traverses_out) {
  params_.focus_traverses_out = focus_traverses_out;
}

void WidgetDelegate::SetEnableArrowKeyTraversal(
    bool enable_arrow_key_traversal) {
  params_.enable_arrow_key_traversal = enable_arrow_key_traversal;
}

void WidgetDelegate::SetIcon(ui::ImageModel icon) {
  params_.icon = std::move(icon);
  if (GetWidget())
    GetWidget()->UpdateWindowIcon();
}

void WidgetDelegate::SetAppIcon(ui::ImageModel icon) {
  params_.app_icon = std::move(icon);
  if (GetWidget())
    GetWidget()->UpdateWindowIcon();
}

void WidgetDelegate::SetInitiallyFocusedView(View* initially_focused_view) {
  DCHECK(!GetWidget());
  params_.initially_focused_view = initially_focused_view;
}

void WidgetDelegate::SetModalType(ui::ModalType modal_type) {
  DCHECK(!GetWidget());
  params_.modal_type = modal_type;
}

void WidgetDelegate::SetShowCloseButton(bool show_close_button) {
  params_.show_close_button = show_close_button;
}

void WidgetDelegate::SetShowIcon(bool show_icon) {
  params_.show_icon = show_icon;
  if (GetWidget())
    GetWidget()->UpdateWindowIcon();
}

void WidgetDelegate::SetShowTitle(bool show_title) {
  params_.show_title = show_title;
}

void WidgetDelegate::SetTitle(const std::u16string& title) {
  if (params_.title == title)
    return;
  params_.title = title;
  if (GetWidget())
    GetWidget()->UpdateWindowTitle();
}

void WidgetDelegate::SetTitle(int title_message_id) {
  SetTitle(l10n_util::GetStringUTF16(title_message_id));
}

#if defined(USE_AURA)
void WidgetDelegate::SetCenterTitle(bool center_title) {
  params_.center_title = center_title;
}
#endif

void WidgetDelegate::SetHasWindowSizeControls(bool has_controls) {
  SetCanMaximize(has_controls);
  SetCanMinimize(has_controls);
  SetCanResize(has_controls);
}

void WidgetDelegate::RegisterWidgetInitializedCallback(
    base::OnceClosure callback) {
  DCHECK(widget_initialized_callbacks_);
  widget_initialized_callbacks_->emplace_back(std::move(callback));
}

void WidgetDelegate::RegisterWindowWillCloseCallback(
    base::OnceClosure callback) {
  window_will_close_callbacks_.emplace_back(std::move(callback));
}

void WidgetDelegate::RegisterWindowClosingCallback(base::OnceClosure callback) {
  window_closing_callbacks_.emplace_back(std::move(callback));
}

void WidgetDelegate::RegisterDeleteDelegateCallback(
    base::OnceClosure callback) {
  delete_delegate_callbacks_.emplace_back(std::move(callback));
}

void WidgetDelegate::SetClientViewFactory(ClientViewFactory factory) {
  DCHECK(!GetWidget());
  client_view_factory_ = std::move(factory);
}

void WidgetDelegate::SetOverlayViewFactory(OverlayViewFactory factory) {
  DCHECK(!GetWidget());
  overlay_view_factory_ = std::move(factory);
}

void WidgetDelegate::SetContentsViewImpl(std::unique_ptr<View> contents) {
  DCHECK(!contents->owned_by_client());
  DCHECK(!unowned_contents_view_);
  owned_contents_view_ = std::move(contents);
  unowned_contents_view_ = owned_contents_view_.get();
}

////////////////////////////////////////////////////////////////////////////////
// WidgetDelegateView:

WidgetDelegateView::WidgetDelegateView() {
  // TODO (kylixrd): Remove once the Widget ceases to "own" the WidgetDelegate.
  // A WidgetDelegate should be deleted on DeleteDelegate.
  SetOwnedByWidget(true);
}

WidgetDelegateView::~WidgetDelegateView() = default;

Widget* WidgetDelegateView::GetWidget() {
  return View::GetWidget();
}

const Widget* WidgetDelegateView::GetWidget() const {
  return View::GetWidget();
}

views::View* WidgetDelegateView::GetContentsView() {
  return this;
}

BEGIN_METADATA(WidgetDelegateView, View)
END_METADATA

}  // namespace views
