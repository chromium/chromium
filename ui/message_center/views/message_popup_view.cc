// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_view.h"

#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

#if defined(OS_CHROMEOS)
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#endif

namespace message_center {

MessagePopupView::MessagePopupView(const Notification& notification,
                                   MessagePopupCollection* popup_collection)
    : message_view_(MessageViewFactory::Create(notification)),
      popup_collection_(popup_collection),
      a11y_feedback_on_init_(
          notification.rich_notification_data()
              .should_make_spoken_feedback_for_popup_updates) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  if (!message_view_->IsManuallyExpandedOrCollapsed())
    message_view_->SetExpanded(message_view_->IsAutoExpandingAllowed());
  AddChildView(message_view_);
  SetNotifyEnterExitOnChild(true);
}

MessagePopupView::MessagePopupView(MessagePopupCollection* popup_collection)
    : message_view_(nullptr),
      popup_collection_(popup_collection),
      a11y_feedback_on_init_(false) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

MessagePopupView::~MessagePopupView() {
  popup_collection_->NotifyPopupClosed(this);
}

void MessagePopupView::UpdateContents(const Notification& notification) {
  if (!IsWidgetValid())
    return;
  ui::AXNodeData old_data;
  message_view_->GetAccessibleNodeData(&old_data);
  message_view_->UpdateWithNotification(notification);
  popup_collection_->NotifyPopupResized();
  if (notification.rich_notification_data()
          .should_make_spoken_feedback_for_popup_updates) {
    ui::AXNodeData new_data;
    message_view_->GetAccessibleNodeData(&new_data);

    const std::string& new_name =
        new_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
    const std::string& old_name =
        old_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
    if (new_name.empty()) {
      new_data.SetNameFrom(ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
      return;
    }

    if (old_name != new_name)
      NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

#if !defined(OS_APPLE)
float MessagePopupView::GetOpacity() const {
  if (!IsWidgetValid())
    return 0.f;
  return GetWidget()->GetLayer()->opacity();
}
#endif

void MessagePopupView::SetPopupBounds(const gfx::Rect& bounds) {
  if (!IsWidgetValid())
    return;
  GetWidget()->SetBounds(bounds);
}

void MessagePopupView::SetOpacity(float opacity) {
  if (!IsWidgetValid())
    return;
  GetWidget()->SetOpacity(opacity);
}

void MessagePopupView::AutoCollapse() {
  if (!IsWidgetValid() || is_hovered_ ||
      message_view_->IsManuallyExpandedOrCollapsed()) {
    return;
  }
  message_view_->SetExpanded(false);
}

void MessagePopupView::Show() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Make the widget explicitly activatable as TYPE_POPUP is not activatable by
  // default but we need focus for the inline reply textarea.
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
#else
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
#endif
  params.delegate = this;
  views::Widget* widget = new views::Widget();
  popup_collection_->ConfigureWidgetInitParamsForContainer(widget, &params);
  widget->set_focus_on_creation(false);
  observer_.Add(widget);

#if defined(OS_WIN)
  // We want to ensure that this toast always goes to the native desktop,
  // not the Ash desktop (since there is already another toast contents view
  // there.
  if (!params.parent)
    params.native_widget = new views::DesktopNativeWidgetAura(widget);
#endif

  widget->Init(std::move(params));

#if defined(OS_CHROMEOS)
  // On Chrome OS, this widget is shown in the shelf container. It means this
  // widget would inherit the parent's window targeter (ShelfWindowTarget) by
  // default. But it is not good for popup. So we override it with the normal
  // WindowTargeter.
  gfx::NativeWindow native_window = widget->GetNativeWindow();
  native_window->SetEventTargeter(std::make_unique<aura::WindowTargeter>());
#endif

  widget->SetOpacity(0.0);
  widget->ShowInactive();

  if (a11y_feedback_on_init_)
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

void MessagePopupView::Close() {
  if (!GetWidget()) {
    DeleteDelegate();
    return;
  }

  if (!GetWidget()->IsClosed())
    GetWidget()->CloseNow();
}

void MessagePopupView::OnMouseEntered(const ui::MouseEvent& event) {
  is_hovered_ = true;
  popup_collection_->Update();
}

void MessagePopupView::OnMouseExited(const ui::MouseEvent& event) {
  is_hovered_ = false;
  popup_collection_->Update();
}

void MessagePopupView::ChildPreferredSizeChanged(views::View* child) {
  popup_collection_->NotifyPopupResized();
}

void MessagePopupView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  message_view_->GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kAlertDialog;
}

const char* MessagePopupView::GetClassName() const {
  return "MessagePopupView";
}

void MessagePopupView::OnDisplayChanged() {
  OnWorkAreaChanged();
}

void MessagePopupView::OnWorkAreaChanged() {
  if (!IsWidgetValid())
    return;

  gfx::NativeView native_view = GetWidget()->GetNativeView();
  if (!native_view)
    return;

  if (popup_collection_->RecomputeAlignment(
          display::Screen::GetScreen()->GetDisplayNearestView(native_view))) {
    popup_collection_->ResetBounds();
  }
}

void MessagePopupView::OnFocus() {
  // This view is just a container, so advance focus to the underlying
  // MessageView.
  GetFocusManager()->SetFocusedView(message_view_);
}

void MessagePopupView::OnWidgetActivationChanged(views::Widget* widget,
                                                 bool active) {
  is_active_ = active;
  popup_collection_->Update();
}

void MessagePopupView::OnWidgetDestroyed(views::Widget* widget) {
  observer_.Remove(widget);
}

bool MessagePopupView::IsWidgetValid() const {
  return GetWidget() && !GetWidget()->IsClosed();
}

}  // namespace message_center
