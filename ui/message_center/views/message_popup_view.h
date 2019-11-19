// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_VIEW_H_

#include "ui/message_center/message_center_export.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace message_center {

class MessagePopupCollection;
class MessageView;
class Notification;

// The widget delegate of a notification popup. The view is owned by the widget.
class MESSAGE_CENTER_EXPORT MessagePopupView : public views::WidgetDelegateView,
                                               public views::WidgetObserver {
 public:
  MessagePopupView(const Notification& notification,
                   MessagePopupCollection* popup_collection);
  ~MessagePopupView() override;

  // Update notification contents to |notification|. Virtual for unit testing.
  virtual void UpdateContents(const Notification& notification);

  // Return opacity of the widget.
  float GetOpacity() const;

  // Sets widget bounds.
  void SetPopupBounds(const gfx::Rect& bounds);

  // Set widget opacity.
  void SetOpacity(float opacity);

  // Collapses the notification unless the user is interacting with it. The
  // request can be ignored. Virtual for unit testing.
  virtual void AutoCollapse();

  // Shows popup. After this call, MessagePopupView should be owned by the
  // widget.
  void Show();

  // Closes popup. It should be callable even if Show() is not called, and
  // in such case MessagePopupView should be deleted. Virtual for unit testing.
  virtual void Close();

  // views::WidgetDelegateView:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  const char* GetClassName() const override;
  void OnDisplayChanged() override;
  void OnWorkAreaChanged() override;
  void OnFocus() override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  bool is_hovered() const { return is_hovered_; }
  bool is_active() const { return is_active_; }

 protected:
  // For unit testing.
  MessagePopupView(MessagePopupCollection* popup_collection);

 private:
  // True if the view has a widget and the widget is not closed.
  bool IsWidgetValid() const;

  // Owned by views hierarchy.
  MessageView* message_view_;

  // Unowned.
  MessagePopupCollection* const popup_collection_;

  const bool a11y_feedback_on_init_;
  bool is_hovered_ = false;
  bool is_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(MessagePopupView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_VIEW_H_
