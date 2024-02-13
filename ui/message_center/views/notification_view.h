// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/notification_view_base.h"

namespace views {
class LabelButton;
class RadioButton;
}  // namespace views

namespace message_center {

// Customized NotificationViewBase for notification on all platforms other
// than ChromeOS. This view is used to displays all current types of
// notification (web, basic, image, and list) except custom notification.
class MESSAGE_CENTER_EXPORT NotificationView : public NotificationViewBase {
  METADATA_HEADER(NotificationView, NotificationViewBase)

 public:
  // TODO(crbug/1241983): Add metadata and builder support to this view.
  explicit NotificationView(const message_center::Notification& notification);
  NotificationView(const NotificationView&) = delete;
  NotificationView& operator=(const NotificationView&) = delete;
  ~NotificationView() override;

  SkColor GetActionButtonColorForTesting(views::LabelButton* action_button);

 private:
  friend class NotificationViewTest;

  class NotificationViewPathGenerator;

  // NotificationViewBase:
  void CreateOrUpdateHeaderView(const Notification& notification) override;
  void CreateOrUpdateTitleView(const Notification& notification) override;
  void CreateOrUpdateSmallIconView(const Notification& notification) override;
  void CreateOrUpdateInlineSettingsViews(
      const Notification& notification) override;
  void CreateOrUpdateSnoozeSettingsViews(
      const Notification& notification) override;
  std::unique_ptr<views::LabelButton> GenerateNotificationLabelButton(
      views::Button::PressedCallback callback,
      const std::u16string& label) override;
  void UpdateViewForExpandedState(bool expanded) override;
  gfx::Size GetIconViewSize() const override;
  int GetLargeImageViewMaxWidth() const override;
  void OnThemeChanged() override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
  void ToggleInlineSettings(const ui::Event& event) override;
  void ToggleSnoozeSettings(const ui::Event& event) override;
  bool IsExpandable() const override;
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;
  void PreferredSizeChanged() override;
  void Layout(PassKey) override;

  void UpdateHeaderViewBackgroundColor();
  SkColor GetNotificationHeaderViewBackgroundColor() const;

  // Update the background that shows behind the `actions_row_`.
  void UpdateActionButtonsRowBackground();

  // Background animations for toggling inline settings.
  void AddBackgroundAnimation(const ui::Event& event);
  void RemoveBackgroundAnimation();

  // Returns the list of children which need to have their layers created or
  // destroyed when the ink drop is visible.
  std::vector<views::View*> GetChildrenForLayerAdjustment();

  void HeaderRowPressed();

  // Notification title, which is dynamically created inside view hierarchy.
  raw_ptr<views::Label, DanglingUntriaged> title_view_ = nullptr;

  // Views for inline settings.
  raw_ptr<views::RadioButton> block_all_button_ = nullptr;
  raw_ptr<views::RadioButton> dont_block_button_ = nullptr;
  raw_ptr<views::LabelButton> settings_done_button_ = nullptr;

  // Ink drop container used in background animations.
  const raw_ptr<views::InkDropContainerView> ink_drop_container_;

  // Owned by views properties. Guaranteed to be not null for the lifetime of
  // |this| because views properties are the last thing cleaned up.
  raw_ptr<NotificationViewPathGenerator> highlight_path_generator_ = nullptr;

  base::WeakPtrFactory<NotificationView> weak_ptr_factory_{this};
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
