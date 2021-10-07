// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_

#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/notification_view_base.h"

namespace views {
class LabelButton;
}  // namespace views

namespace message_center {

// Customized NotificationViewBase for notification on all platforms other
// than ChromeOS. This view is used to displays all current types of
// notification (web, basic, image, and list) except custom notification.
class MESSAGE_CENTER_EXPORT NotificationView : public NotificationViewBase {
 public:
  // TODO(crbug/1241983): Add metadata and builder support to this view.
  explicit NotificationView(const message_center::Notification& notification);
  NotificationView(const NotificationView&) = delete;
  NotificationView& operator=(const NotificationView&) = delete;
  ~NotificationView() override;

 private:
  friend class NotificationViewTest;

  // NotificationViewBase:
  void CreateOrUpdateTitleView(const Notification& notification) override;
  void CreateOrUpdateSmallIconView(const Notification& notification) override;
  std::unique_ptr<views::LabelButton> GenerateNotificationLabelButton(
      views::Button::PressedCallback callback,
      const std::u16string& label) override;
  void UpdateViewForExpandedState(bool expanded) override;
  gfx::Size GetIconViewSize() const override;
  void OnThemeChanged() override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
  void ToggleInlineSettings(const ui::Event& event) override;

  void UpdateHeaderViewBackgroundColor();
  SkColor GetNotificationHeaderViewBackgroundColor() const;

  // Update the background that shows behind the `actions_row_`.
  void UpdateActionButtonsRowBackground();

  // Background animations for toggling inline settings.
  void AddBackgroundAnimation(const ui::Event& event);
  void RemoveBackgroundAnimation();

  // Notification title, which is dynamically created inside view hierarchy.
  views::Label* title_view_ = nullptr;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_