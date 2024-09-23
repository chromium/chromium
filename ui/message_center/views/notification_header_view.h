// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_HEADER_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_HEADER_VIEW_H_

#include <optional>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
class FontList;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace message_center {

class MESSAGE_CENTER_EXPORT NotificationHeaderView : public views::Button {
  METADATA_HEADER(NotificationHeaderView, views::Button)

 public:
  explicit NotificationHeaderView(PressedCallback callback = PressedCallback());
  NotificationHeaderView(const NotificationHeaderView&) = delete;
  NotificationHeaderView& operator=(const NotificationHeaderView&) = delete;
  ~NotificationHeaderView() override;

  // Configure the style of all the labels used in this view.
  void ConfigureLabelsStyle(const gfx::FontList& font_list,
                            const gfx::Insets& text_view_padding,
                            bool auto_color_readability);

  void SetAppIcon(const gfx::ImageSkia& img);
  void SetAppName(const std::u16string& name);
  void SetAppNameElideBehavior(gfx::ElideBehavior elide_behavior);

  // Only show AppIcon and AppName in settings mode.
  void SetDetailViewsVisible(bool visible);

  // Progress, summary and overflow indicator are all the same UI element so are
  // mutually exclusive.
  void SetProgress(int progress);
  void SetSummaryText(const std::u16string& text);
  void SetOverflowIndicator(int count);

  void SetTimestamp(base::Time timestamp);
  void SetExpandButtonEnabled(bool enabled);
  void SetExpanded(bool expanded);

  // Calls UpdateColors() to set the unified theme color used among the app
  // icon, app name, and expand button. If set to std::nullopt it will use the
  // NotificationDefaultAccentColor from the native theme.
  void SetColor(std::optional<SkColor> color);

  // Sets the background color of the notification. This is used to ensure that
  // the accent color has enough contrast against the background.
  void SetBackgroundColor(SkColor color);

  void ClearAppIcon();
  void SetSubpixelRenderingEnabled(bool enabled);

  // Shows or hides the app icon.
  void SetAppIconVisible(bool visible);

  // Shows or hides the timestamp and timestamp divider
  void SetTimestampVisible(bool visible);

  void SetIsInAshNotificationView(bool is_in_ash_notification);

  // The header only shows timestamp if it is in a group child notification.
  void SetIsInGroupChildNotification(bool is_in_group_child_notification);

  // views::View:
  void OnThemeChanged() override;

  views::ImageView* expand_button() { return expand_button_; }

  std::optional<SkColor> color_for_testing() const { return color_; }

  const views::Label* summary_text_for_testing() const {
    return summary_text_view_;
  }

  const views::ImageView* app_icon_view_for_testing() const {
    return app_icon_view_;
  }

  const views::Label* timestamp_view_for_testing() const {
    return timestamp_view_;
  }

  const std::u16string& app_name_for_testing() const;

  gfx::ImageSkia app_icon_for_testing() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationHeaderViewTest, SettingsMode);
  FRIEND_TEST_ALL_PREFIXES(NotificationHeaderViewTest,
                           GroupChildNotificationVisibility);

  // Update visibility for both |summary_text_view_| and |timestamp_view_|.
  void UpdateSummaryTextAndTimestampVisibility();

  void UpdateColors();

  void UpdateExpandedCollapsedAccessibleState() const;

  void OnTextChanged();

  // Color used for labels and buttons in this view.
  std::optional<SkColor> color_;

  // Timer that updates the timestamp over time.
  base::OneShotTimer timestamp_update_timer_;
  std::optional<base::Time> timestamp_;

  raw_ptr<views::ImageView> app_icon_view_ = nullptr;
  raw_ptr<views::Label> app_name_view_ = nullptr;
  raw_ptr<views::View> detail_views_ = nullptr;
  raw_ptr<views::View> spacer_ = nullptr;
  raw_ptr<views::Label> summary_text_divider_ = nullptr;
  raw_ptr<views::Label> summary_text_view_ = nullptr;
  raw_ptr<views::Label> timestamp_divider_ = nullptr;
  raw_ptr<views::Label> timestamp_view_ = nullptr;
  raw_ptr<views::ImageView> expand_button_ = nullptr;

  bool has_progress_ = false;
  bool is_expanded_ = false;
  bool using_default_app_icon_ = false;

  // Whether this view is used for an ash notification view.
  bool is_in_ash_notification_ = false;

  // Whether this view is used for a group child notification.
  bool is_in_group_child_notification_ = false;

  base::CallbackListSubscription summary_text_changed_callback_;
  base::CallbackListSubscription timestamp_changed_callback_;
};

BEGIN_VIEW_BUILDER(MESSAGE_CENTER_EXPORT, NotificationHeaderView, views::Button)
VIEW_BUILDER_PROPERTY(bool, IsInAshNotificationView)
VIEW_BUILDER_PROPERTY(std::optional<SkColor>, Color)
END_VIEW_BUILDER

}  // namespace message_center

DEFINE_VIEW_BUILDER(MESSAGE_CENTER_EXPORT,
                    message_center::NotificationHeaderView)

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_HEADER_VIEW_H_
