// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_view.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#endif

namespace message_center {

MessagePopupView::MessagePopupView(MessageView* message_view,
                                   MessagePopupCollection* popup_collection,
                                   bool a11y_feedback_on_init)
    : message_view_(message_view),
      popup_collection_(popup_collection),
      a11y_feedback_on_init_(a11y_feedback_on_init) {
  set_suppress_default_focus_handling();
  SetLayoutManager(std::make_unique<views::FillLayout>());

  CHECK(message_view_) << "MessagePopupView requires a message_view";
  if (!message_view_->IsManuallyExpandedOrCollapsed()) {
    message_view_->SetExpanded(message_view_->IsAutoExpandingAllowed());
  }
  AddChildView(message_view_.get());

  SetNotifyEnterExitOnChild(true);

  GetViewAccessibility().SetRole(ax::mojom::Role::kAlertDialog);
}

MessagePopupView::MessagePopupView(MessagePopupCollection* popup_collection)
    : message_view_(nullptr),
      popup_collection_(popup_collection),
      a11y_feedback_on_init_(false) {
  set_suppress_default_focus_handling();
  SetLayoutManager(std::make_unique<views::FillLayout>());

  GetViewAccessibility().SetRole(ax::mojom::Role::kAlertDialog);
}

MessagePopupView::~MessagePopupView() {
  popup_collection_->NotifyPopupClosed(this);
  if (focus_manager_)
    focus_manager_->RemoveFocusChangeListener(this);
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

void MessagePopupView::UpdateContentsForChildNotification(
    const std::string& notification_id,
    const Notification& notification) {
  if (!IsWidgetValid()) {
    return;
  }

  auto* child_notification_view = static_cast<MessageView*>(
      message_view_->FindGroupNotificationView(notification_id));
  if (!child_notification_view) {
    return;
  }

  child_notification_view->UpdateWithNotification(notification);
  popup_collection_->NotifyPopupResized();
}

#if !BUILDFLAG(IS_APPLE)
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

std::unique_ptr<views::Widget> MessagePopupView::Show() {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Make the widget explicitly activatable as TYPE_POPUP is not activatable by
  // default but we need focus for the inline reply textarea.
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
#else
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
#endif
  params.delegate = this;
  auto widget = std::make_unique<views::Widget>();
  popup_collection_->ConfigureWidgetInitParamsForContainer(widget.get(),
                                                           &params);
  widget->set_focus_on_creation(false);

#if BUILDFLAG(IS_WIN)
  // We want to ensure that this toast always goes to the native desktop,
  // not the Ash desktop (since there is already another toast contents view
  // there.
  if (!params.parent)
    params.native_widget = new views::DesktopNativeWidgetAura(widget.get());
#endif

  widget->Init(std::move(params));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, notification pop-ups are shown in the
  // `SettingBubbleContainer`, together with other shelf pod bubbles. This
  // widget would inherit the parent's window targeter by default. But it is not
  // good for popup. So we override it with the normal WindowTargeter.
  gfx::NativeWindow native_window = widget->GetNativeWindow();
  native_window->SetEventTargeter(std::make_unique<aura::WindowTargeter>());

  // Newly shown popups are stacked at the bottom so they do not cast shadows
  // on previously shown popups.
  native_window->parent()->StackChildAtBottom(native_window);
#endif

  widget->SetOpacity(0.0);
  widget->ShowInactive();

  if (a11y_feedback_on_init_)
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);

  return widget;
}

void MessagePopupView::Close() {
  if (GetWidget() && !GetWidget()->IsClosed()) {
    GetWidget()->CloseNow();
  }
}

void MessagePopupView::OnDidChangeFocus(views::View* before, views::View* now) {
  is_focused_ = Contains(now);
  popup_collection_->Update();
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
  // TODO(pbos): Consider removing the test-only constructor that has
  // `message_view_` as nullptr.
  if (message_view_)
    message_view_->GetAccessibleNodeData(node_data);
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

void MessagePopupView::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  }
  view_added_to_widget_ = true;
}

void MessagePopupView::RemovedFromWidget() {
  if (focus_manager_)
    focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
}

bool MessagePopupView::IsWidgetValid() const {
  return GetWidget() && !GetWidget()->IsClosed();
}

BEGIN_METADATA(MessagePopupView)
END_METADATA

}  // namespace message_center
