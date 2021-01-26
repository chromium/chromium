// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_delegate.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/metadata/metadata_impl_macros.h"
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

std::unique_ptr<NonClientFrameView> CreateDefaultNonClientFrameView(
    Widget* widget) {
  return nullptr;
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
    : widget_initializing_callbacks_(std::make_unique<ClosureVector>()),
      widget_initialized_callbacks_(std::make_unique<ClosureVector>()),
      client_view_factory_(
          base::BindOnce(&CreateDefaultClientView, base::Unretained(this))),
      non_client_frame_view_factory_(
          base::BindRepeating(&CreateDefaultNonClientFrameView)),
      overlay_view_factory_(base::BindOnce(&CreateDefaultOverlayView)) {}
WidgetDelegate::~WidgetDelegate() {
  CHECK(can_delete_this_) << "A WidgetDelegate must outlive its Widget";
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

base::string16 WidgetDelegate::GetAccessibleWindowTitle() const {
  return params_.accessible_title.empty() ? GetWindowTitle()
                                          : params_.accessible_title;
}

base::string16 WidgetDelegate::GetWindowTitle() const {
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

bool WidgetDelegate::ShouldShowCloseButton() const {
  return params_.show_close_button;
}

gfx::ImageSkia WidgetDelegate::GetWindowAppIcon() {
  // Prefer app icon if available.
  if (!params_.app_icon.isNull())
    return params_.app_icon;
  // Fall back to the window icon.
  return GetWindowIcon();
}

// Returns the icon to be displayed in the window.
gfx::ImageSkia WidgetDelegate::GetWindowIcon() {
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
  for (auto&& callback : *widget_initializing_callbacks_)
    std::move(callback).Run();
  widget_initializing_callbacks_.reset();
  OnWidgetInitializing();
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
  for (auto&& callback : delete_delegate_callbacks_)
    std::move(callback).Run();
  if (params_.owned_by_widget)
    delete this;
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
  DCHECK(non_client_frame_view_factory_);
  return non_client_frame_view_factory_.Run(widget);
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

void WidgetDelegate::SetAccessibleRole(ax::mojom::Role role) {
  params_.accessible_role = role;
}

void WidgetDelegate::SetAccessibleTitle(base::string16 title) {
  params_.accessible_title = std::move(title);
}

void WidgetDelegate::SetCanMaximize(bool can_maximize) {
  std::exchange(params_.can_maximize, can_maximize);
  if (GetWidget() && params_.can_maximize != can_maximize)
    GetWidget()->OnSizeConstraintsChanged();
}

void WidgetDelegate::SetCanMinimize(bool can_minimize) {
  std::exchange(params_.can_minimize, can_minimize);
  if (GetWidget() && params_.can_minimize != can_minimize)
    GetWidget()->OnSizeConstraintsChanged();
}

void WidgetDelegate::SetCanResize(bool can_resize) {
  std::exchange(params_.can_resize, can_resize);
  if (GetWidget() && params_.can_resize != can_resize)
    GetWidget()->OnSizeConstraintsChanged();
}

void WidgetDelegate::SetOwnedByWidget(bool owned) {
  params_.owned_by_widget = owned;
}

void WidgetDelegate::SetFocusTraversesOut(bool focus_traverses_out) {
  params_.focus_traverses_out = focus_traverses_out;
}

void WidgetDelegate::SetEnableArrowKeyTraversal(
    bool enable_arrow_key_traversal) {
  params_.enable_arrow_key_traversal = enable_arrow_key_traversal;
}

void WidgetDelegate::SetIcon(const gfx::ImageSkia& icon) {
  params_.icon = icon;
  if (GetWidget())
    GetWidget()->UpdateWindowIcon();
}

void WidgetDelegate::SetAppIcon(const gfx::ImageSkia& icon) {
  params_.app_icon = icon;
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

void WidgetDelegate::SetTitle(const base::string16& title) {
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

void WidgetDelegate::RegisterWidgetInitializingCallback(
    base::OnceClosure callback) {
  DCHECK(widget_initializing_callbacks_);
  widget_initializing_callbacks_->emplace_back(std::move(callback));
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

void WidgetDelegate::SetNonClientFrameViewFactory(
    NonClientFrameViewFactory factory) {
  DCHECK(!GetWidget());
  non_client_frame_view_factory_ = std::move(factory);
}

void WidgetDelegate::SetOverlayViewFactory(OverlayViewFactory factory) {
  DCHECK(!GetWidget());
  overlay_view_factory_ = std::move(factory);
}

void WidgetDelegate::SetContentsViewImpl(View* contents) {
  // Note: DCHECKing the ownership of contents is done in the public setters,
  // which are inlined in the header.
  DCHECK(!unowned_contents_view_);
  if (!contents->owned_by_client())
    owned_contents_view_ = base::WrapUnique(contents);
  unowned_contents_view_ = contents;
}

////////////////////////////////////////////////////////////////////////////////
// WidgetDelegateView:

WidgetDelegateView::WidgetDelegateView() {
  // A WidgetDelegate should be deleted on DeleteDelegate.
  set_owned_by_client();
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
