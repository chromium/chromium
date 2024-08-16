// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace message_center {

class MessagePopupCollection;
class MessageView;
class Notification;

// The widget delegate of a notification popup. The view is owned by the widget.
class MESSAGE_CENTER_EXPORT MessagePopupView
    : public views::FocusChangeListener,
      public views::WidgetDelegateView {
  METADATA_HEADER(MessagePopupView, views::WidgetDelegateView)

 public:
  MessagePopupView(MessageView* message_view,
                   MessagePopupCollection* popup_collection,
                   bool a11y_feedback_on_init);
  MessagePopupView(const MessagePopupView&) = delete;
  MessagePopupView& operator=(const MessagePopupView&) = delete;
  ~MessagePopupView() override;

  // Update notification contents to |notification|. Virtual for unit testing.
  virtual void UpdateContents(const Notification& notification);

  // Updates the content of the child notification view inside this popup.
  // Virtual for unit testing.
  virtual void UpdateContentsForChildNotification(
      const std::string& notification_id,
      const Notification& notification);

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
  std::unique_ptr<views::Widget> Show();

  // Closes popup. It should be callable even if Show() is not called, and
  // in such case MessagePopupView should be deleted. Virtual for unit testing.
  virtual void Close();

  void OnWillChangeFocus(views::View* before, views::View* now) override {}
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  // views::WidgetDelegateView:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnDisplayChanged() override;
  void OnWorkAreaChanged() override;
  void OnFocus() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  bool is_hovered() const { return is_hovered_; }
  bool is_focused() const { return is_focused_; }

  MessageView* message_view() { return message_view_; }

  bool view_added_to_widget() { return view_added_to_widget_; }

 protected:
  // For unit testing.
  explicit MessagePopupView(MessagePopupCollection* popup_collection);

 private:
  // True if the view has a widget and the widget is not closed.
  bool IsWidgetValid() const;

  // Owned by views hierarchy.
  raw_ptr<MessageView> message_view_;

  // Unowned.
  const raw_ptr<MessagePopupCollection> popup_collection_;

  const bool a11y_feedback_on_init_;
  bool is_hovered_ = false;
  bool is_focused_ = false;
  // Was this view ever hosted in a Widget? If so, the Widget will "own" this
  // view and delete it accordingly. Otherwise, the MessagePopupCollection is
  // responsible for its destruction.
  bool view_added_to_widget_ = false;

  // Owned by the widget associated with this view.
  raw_ptr<views::FocusManager> focus_manager_ = nullptr;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_VIEW_H_
