// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_BASE_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_BASE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace views {
class ImageButton;
class Label;
class ProgressBar;
class RadioButton;
class Textfield;
}  // namespace views

namespace message_center {

class NotificationHeaderView;
class ProportionalImageView;

// TODO(crbug/1241983): Add metadata and builder support to these views.

// NotificationTextButton extends MdText button to allow for placeholder text
// as well as capitalizing the given label string.
class MESSAGE_CENTER_EXPORT NotificationTextButton
    : public views::MdTextButton {
 public:
  METADATA_HEADER(NotificationTextButton);

  NotificationTextButton(PressedCallback callback,
                         const std::u16string& label,
                         const absl::optional<std::u16string>& placeholder);
  ~NotificationTextButton() override;

  // views::MdTextButton:
  void UpdateBackgroundColor() override;
  void OnThemeChanged() override;

  const absl::optional<std::u16string>& placeholder() const {
    return placeholder_;
  }
  void set_placeholder(absl::optional<std::u16string> placeholder) {
    placeholder_ = std::move(placeholder);
  }
  SkColor enabled_color_for_testing() const {
    return label()->GetEnabledColor();
  }

  void OverrideTextColor(absl::optional<SkColor> text_color);

 private:
  absl::optional<std::u16string> placeholder_;
  absl::optional<SkColor> text_color_;
};

// CompactTitleMessageView shows notification title and message in a single
// line. This view is used for NOTIFICATION_TYPE_PROGRESS.
class CompactTitleMessageView : public views::View {
 public:
  CompactTitleMessageView();
  CompactTitleMessageView(const CompactTitleMessageView&) = delete;
  CompactTitleMessageView& operator=(const CompactTitleMessageView&) = delete;
  ~CompactTitleMessageView() override;

  const char* GetClassName() const override;

  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  void set_title(const std::u16string& title);
  void set_message(const std::u16string& message);

 private:
  views::Label* title_ = nullptr;
  views::Label* message_ = nullptr;
};

class LargeImageView : public views::View {
 public:
  explicit LargeImageView(const gfx::Size& max_size);
  LargeImageView(const LargeImageView&) = delete;
  LargeImageView& operator=(const LargeImageView&) = delete;
  ~LargeImageView() override;

  void SetImage(const gfx::ImageSkia& image);

  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 private:
  gfx::Size GetResizedImageSize();

  gfx::Size max_size_;
  gfx::Size min_size_;
  gfx::ImageSkia image_;
};

class NotificationInputDelegate {
 public:
  virtual void OnNotificationInputSubmit(size_t index,
                                         const std::u16string& text) = 0;
  virtual ~NotificationInputDelegate() = default;
};

class NotificationInputContainer : public views::View,
                                   public views::TextfieldController {
 public:
  explicit NotificationInputContainer(NotificationInputDelegate* delegate);
  NotificationInputContainer(const NotificationInputContainer&) = delete;
  NotificationInputContainer& operator=(const NotificationInputContainer&) =
      delete;
  ~NotificationInputContainer() override;

  void AnimateBackground(const ui::Event& event);

  // views::View:
  void AddLayerBeneathView(ui::Layer* layer) override;
  void RemoveLayerBeneathView(ui::Layer* layer) override;
  void OnThemeChanged() override;
  void Layout() override;

  // Overridden from views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnAfterUserAction(views::Textfield* sender) override;

  views::Textfield* textfield() const { return textfield_; }
  views::ImageButton* button() const { return button_; }

 private:
  void UpdateButtonImage();

  NotificationInputDelegate* const delegate_;

  views::InkDropContainerView* const ink_drop_container_;

  views::Textfield* const textfield_;
  views::ImageButton* const button_;
};

// View that displays all current types of notification (web, basic, image, and
// list) except the custom notification. Future notification types may be
// handled by other classes, in which case instances of those classes would be
// returned by the Create() factory method below.
class MESSAGE_CENTER_EXPORT NotificationViewBase
    : public MessageView,
      public views::InkDropObserver,
      public NotificationInputDelegate {
 public:
  // This defines an enumeration of IDs that can uniquely identify a view within
  // the scope of NotificationViewBase.
  enum ViewId {
    // We start from 1 because 0 is the default view ID.
    kHeaderRow = 1,
    kAppNameView,
    kHeaderDetailViews,
    kSummaryTextView,

    kContentRow,
    kActionButtonsRow,
    kInlineReply,
  };

  NotificationViewBase(const NotificationViewBase&) = delete;
  NotificationViewBase& operator=(const NotificationViewBase&) = delete;
  ~NotificationViewBase() override;

  void Activate();

  void AddBackgroundAnimation(const ui::Event& event);
  void RemoveBackgroundAnimation();

  // MessageView:
  void AddGroupNotification(const Notification& notification,
                            bool newest_first) override;
  void RemoveGroupNotification(const std::string& notification_id) override;
  void AddLayerBeneathView(ui::Layer* layer) override;
  void RemoveLayerBeneathView(ui::Layer* layer) override;
  void Layout() override;
  void OnFocus() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void PreferredSizeChanged() override;
  void UpdateWithNotification(const Notification& notification) override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
  NotificationControlButtonsView* GetControlButtonsView() const override;
  bool IsExpanded() const override;
  void SetExpanded(bool expanded) override;
  bool IsManuallyExpandedOrCollapsed() const override;
  void SetManuallyExpandedOrCollapsed(bool value) override;
  void OnSettingsButtonPressed(const ui::Event& event) override;
  void OnThemeChanged() override;

  // views::InkDropObserver:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState ink_drop_state) override;

  // Overridden from NotificationInputDelegate:
  void OnNotificationInputSubmit(size_t index,
                                 const std::u16string& text) override;

 protected:
  explicit NotificationViewBase(const Notification& notification);

  // Control buttons view contains settings button, close button, etc.
  std::unique_ptr<NotificationControlButtonsView> CreateControlButtonsView();

  // Header row contains app_icon, app_name, control buttons, etc.
  std::unique_ptr<NotificationHeaderView> CreateHeaderRow();

  // Left content view contains most of the contents, including title, message,
  // compacted title and message, progress bar and status for progress
  // notification, etc.
  std::unique_ptr<views::View> CreateLeftContentView();

  // Right content contains notification icon and small image.
  std::unique_ptr<views::View> CreateRightContentView();

  // Content row contains all the main content in a notification. This view will
  // be hidden when settings are shown.
  std::unique_ptr<views::View> CreateContentRow();

  // Image container view contains the expanded notification image.
  std::unique_ptr<views::View> CreateImageContainerView();

  // Inline settings view contains inline settings.
  std::unique_ptr<views::View> CreateInlineSettingsView();

  // Actions row contains inline action buttons and inline textfield.
  std::unique_ptr<views::View> CreateActionsRow();

  void CreateOrUpdateViews(const Notification& notification);

  views::View* image_container_view() { return image_container_view_; }
  bool IsExpandable() const;

  virtual void SetExpandButtonEnabled(bool enabled);

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, AppNameExtension);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, AppNameSystemNotification);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, AppNameWebNotification);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, AppNameWebAppNotification);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, CreateOrUpdateTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, ExpandLongMessage);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, InkDropClipRect);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, InlineSettings);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest,
                           InlineSettingsInkDropAnimation);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, NotificationWithoutIcon);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, ShowProgress);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, ShowTimestamp);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, TestAccentColor);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, TestActionButtonClick);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, TestClick);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, TestClickExpanded);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest,
                           TestDeleteOnDisableNotification);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest,
                           TestDeleteOnToggleExpanded);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, TestInlineReply);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest,
                           TestInlineReplyActivateWithKeyPress);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest,
                           TestInlineReplyRemovedByUpdate);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, TestLongTitleAndMessage);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, UpdateAddingIcon);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, UpdateButtonCountTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, UpdateButtonsStateTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, UpdateInSettings);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, UpdateType);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, UpdateViewsOrderingTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, UseImageAsIcon);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, TestIconSizing);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, LeftContentResizeForIcon);

  friend class NotificationViewBaseTest;

  class NotificationViewPathGenerator;

  void CreateOrUpdateContextTitleView(const Notification& notification);
  void CreateOrUpdateTitleView(const Notification& notification);
  void CreateOrUpdateMessageView(const Notification& notification);
  void CreateOrUpdateCompactTitleMessageView(const Notification& notification);
  void CreateOrUpdateProgressBarView(const Notification& notification);
  void CreateOrUpdateProgressStatusView(const Notification& notification);
  void CreateOrUpdateListItemViews(const Notification& notification);
  void CreateOrUpdateIconView(const Notification& notification);
  void CreateOrUpdateSmallIconView(const Notification& notification);
  void CreateOrUpdateImageView(const Notification& notification);
  void CreateOrUpdateActionButtonViews(const Notification& notification);
  void CreateOrUpdateInlineSettingsViews(const Notification& notification);

  void HeaderRowPressed();
  void ActionButtonPressed(size_t index, const ui::Event& event);

  void ToggleExpanded();
  void UpdateViewForExpandedState(bool expanded);
  void ToggleInlineSettings(const ui::Event& event);
  void UpdateHeaderViewBackgroundColor();
  SkColor GetNotificationHeaderViewBackgroundColor() const;
  void UpdateActionButtonsRowBackground();

  // Returns the list of children which need to have their layers created or
  // destroyed when the ink drop is visible.
  std::vector<views::View*> GetChildrenForLayerAdjustment() const;

  views::InkDropContainerView* const ink_drop_container_;

  // View containing close and settings buttons
  NotificationControlButtonsView* control_buttons_view_ = nullptr;

  // Whether this notification is expanded or not.
  bool expanded_ = false;

  // True if the notification is expanded/collapsed by user interaction.
  // If true, MessagePopupCollection will not auto-collapse the notification.
  bool manually_expanded_or_collapsed_ = false;

  // Whether hiding icon on the right side when expanded.
  bool hide_icon_on_expanded_ = false;

  // Number of total list items in the given Notification class.
  int list_items_count_ = 0;

  // Describes whether the view should display a hand pointer or not.
  bool clickable_;

  // Describes if the view should display an expand button in the header view.
  bool has_expand_button_in_header_view_ = true;

  // Describes whether the view can display inline settings or not.
  bool inline_settings_enabled_ = false;

  // Container views directly attached to this view.
  NotificationHeaderView* header_row_ = nullptr;
  views::View* content_row_ = nullptr;
  views::View* actions_row_ = nullptr;
  views::View* settings_row_ = nullptr;

  // Containers for left and right side on |content_row_|
  views::View* left_content_ = nullptr;
  views::View* right_content_ = nullptr;

  // Views which are dynamically created inside view hierarchy.
  views::Label* title_view_ = nullptr;
  views::Label* message_view_ = nullptr;
  views::Label* status_view_ = nullptr;
  ProportionalImageView* icon_view_ = nullptr;
  views::View* image_container_view_ = nullptr;
  std::vector<NotificationTextButton*> action_buttons_;
  std::vector<views::View*> item_views_;
  views::ProgressBar* progress_bar_view_ = nullptr;
  CompactTitleMessageView* compact_title_message_view_ = nullptr;
  views::View* action_buttons_row_ = nullptr;
  NotificationInputContainer* inline_reply_ = nullptr;

  // Counter for view layouting, which is used during the CreateOrUpdate*
  // phases to keep track of the view ordering. See crbug.com/901045
  int left_content_count_;

  // Views for inline settings.
  views::RadioButton* block_all_button_ = nullptr;
  views::RadioButton* dont_block_button_ = nullptr;
  NotificationTextButton* settings_done_button_ = nullptr;

  // Owned by views properties. Guaranteed to be not null for the lifetime of
  // |this| because views properties are the last thing cleaned up.
  NotificationViewPathGenerator* highlight_path_generator_ = nullptr;

  std::unique_ptr<ui::EventHandler> click_activator_;

  base::TimeTicks last_mouse_pressed_timestamp_;

  base::WeakPtrFactory<NotificationViewBase> weak_ptr_factory_{this};
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_BASE_H_