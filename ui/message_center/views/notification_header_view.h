// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_HEADER_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_HEADER_VIEW_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "ui/gfx/text_constants.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace message_center {

class MESSAGE_CENTER_EXPORT NotificationHeaderView : public views::Button {
 public:
  explicit NotificationHeaderView(views::ButtonListener* listener);
  ~NotificationHeaderView() override;
  void SetAppIcon(const gfx::ImageSkia& img);
  void SetAppName(const base::string16& name);
  void SetAppNameElideBehavior(gfx::ElideBehavior elide_behavior);

  // Only show AppIcon and AppName in settings mode.
  void SetDetailViewsVisible(bool visible);

  // Progress, summary and overflow indicator are all the same UI element so are
  // mutually exclusive.
  void SetProgress(int progress);
  void SetSummaryText(const base::string16& text);
  void SetOverflowIndicator(int count);

  void SetTimestamp(base::Time timestamp);
  void SetExpandButtonEnabled(bool enabled);
  void SetExpanded(bool expanded);

  // Set the unified theme color used among the app icon, app name, and expand
  // button.
  void SetAccentColor(SkColor color);

  // Sets the background color of the notification. This is used to ensure that
  // the accent color has enough contrast against the background.
  void SetBackgroundColor(SkColor color);

  void ClearAppIcon();
  void ClearProgress();
  void SetSubpixelRenderingEnabled(bool enabled);

  // Completely hides the app icon.
  void HideAppIcon();

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  views::ImageView* expand_button() { return expand_button_; }

  SkColor accent_color_for_testing() { return accent_color_; }

  const views::Label* summary_text_for_testing() const {
    return summary_text_view_;
  }

  const views::ImageView* app_icon_view_for_testing() const {
    return app_icon_view_;
  }

  const base::string16& app_name_for_testing() const;

  const gfx::ImageSkia& app_icon_for_testing() const;

  const base::string16& timestamp_for_testing() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationHeaderViewTest, SettingsMode);

  // Update visibility for both |summary_text_view_| and |timestamp_view_|.
  void UpdateSummaryTextVisibility();

  SkColor accent_color_ = kNotificationDefaultAccentColor;

  // Timer that updates the timestamp over time.
  base::OneShotTimer timestamp_update_timer_;
  base::Optional<base::Time> timestamp_;

  views::ImageView* app_icon_view_ = nullptr;
  views::Label* app_name_view_ = nullptr;
  views::View* detail_views_ = nullptr;
  views::Label* summary_text_divider_ = nullptr;
  views::Label* summary_text_view_ = nullptr;
  views::Label* timestamp_divider_ = nullptr;
  views::Label* timestamp_view_ = nullptr;
  views::ImageView* expand_button_ = nullptr;

  bool has_progress_ = false;
  bool is_expanded_ = false;
  bool using_default_app_icon_ = false;

  DISALLOW_COPY_AND_ASSIGN(NotificationHeaderView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_HEADER_VIEW_H_
