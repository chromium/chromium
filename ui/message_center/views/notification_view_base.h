// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_BASE_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_BASE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_input_container.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"

namespace views {
class BoxLayoutView;
class Label;
class LabelButton;
class LayoutManager;
class ProgressBar;
}  // namespace views

namespace message_center {

class NotificationHeaderView;
class ProportionalImageView;

// TODO(crbug/1241983): Add metadata and builder support to these views.

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
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> message_ = nullptr;
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
    kMainRightView,
    kHeaderLeftContent,
    kCollapsedSummaryView,
    kAppIconViewContainer,
    kLargeImageView,
  };

  NotificationViewBase(const NotificationViewBase&) = delete;
  NotificationViewBase& operator=(const NotificationViewBase&) = delete;
  ~NotificationViewBase() override;

  void Activate();

  // MessageView:
  void Layout() override;
  void OnFocus() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void UpdateWithNotification(const Notification& notification) override;
  NotificationControlButtonsView* GetControlButtonsView() const override;
  bool IsExpanded() const override;
  void SetExpanded(bool expanded) override;
  bool IsManuallyExpandedOrCollapsed() const override;
  void SetManuallyExpandedOrCollapsed(ExpandState state) override;
  void OnSettingsButtonPressed(const ui::Event& event) override;

  // views::InkDropObserver:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState ink_drop_state) override;

  // Overridden from NotificationInputDelegate:
  void OnNotificationInputSubmit(size_t index,
                                 const std::u16string& text) override;

  // Whether the notification view is showing `icon_view_`.
  virtual bool IsIconViewShown() const;

 protected:
  explicit NotificationViewBase(const Notification& notification);

  // Control buttons view contains settings button, close button, etc.
  views::Builder<NotificationControlButtonsView> CreateControlButtonsBuilder();

  // Header row contains app_icon, app_name, control buttons, etc.
  views::Builder<NotificationHeaderView> CreateHeaderRowBuilder();

  // Left content view contains most of the contents, including title, message,
  // compacted title and message, progress bar and status for progress
  // notification, etc.
  views::Builder<views::BoxLayoutView> CreateLeftContentBuilder();

  // Right content contains notification icon and small image.
  views::Builder<views::View> CreateRightContentBuilder();

  // Content row contains all the main content in a notification. This view will
  // be hidden when settings are shown.
  views::Builder<views::View> CreateContentRowBuilder();

  // Image container view contains the expanded notification image.
  views::Builder<views::View> CreateImageContainerBuilder();

  // Inline settings view contains inline settings.
  views::Builder<views::BoxLayoutView> CreateInlineSettingsBuilder();

  // Actions row contains inline action buttons and inline textfield. Use the
  // given layout manager for the actions row.
  std::unique_ptr<views::View> CreateActionsRow(
      std::unique_ptr<views::LayoutManager> layout_manager =
          std::make_unique<views::FillLayout>());

  // Generate a view to show notification title and other supporting views.
  static std::unique_ptr<views::Label> GenerateTitleView(
      const std::u16string& title);

  // Generate a view that shows the inline reply textfield and send message
  // button.
  virtual std::unique_ptr<NotificationInputContainer>
  GenerateNotificationInputContainer();

  // Generate a view that is a views::LabelButton, used in the `actions_row_`.
  // The base class uses an MdTextButton, but ash does not.
  virtual std::unique_ptr<views::LabelButton> GenerateNotificationLabelButton(
      views::Button::PressedCallback callback,
      const std::u16string& label) = 0;

  void CreateOrUpdateViews(const Notification& notification);

  virtual void UpdateViewForExpandedState(bool expanded);

  virtual void CreateOrUpdateHeaderView(const Notification& notification);

  virtual void CreateOrUpdateCompactTitleMessageView(
      const Notification& notification);

  void CreateOrUpdateProgressBarView(const Notification& notification);

  void CreateOrUpdateProgressStatusView(const Notification& notification);

  virtual void CreateOrUpdateTitleView(const Notification& notification) = 0;

  virtual void CreateOrUpdateSmallIconView(
      const Notification& notification) = 0;

  virtual void CreateOrUpdateInlineSettingsViews(
      const Notification& notification) = 0;

  // Add view to `left_content_` in its appropriate position according to
  // `left_content_count_`. Return a pointer to added view.
  template <typename T>
  T* AddViewToLeftContent(std::unique_ptr<T> view) {
    return left_content_->AddChildViewAt(std::move(view),
                                         left_content_count_++);
  }

  // Reorder the view in `left_content_` according to `left_content_count_`.
  void ReorderViewInLeftContent(views::View* view);

  // Thic function is called when the UI changes from notification view to
  // inline settings or vice versa.
  virtual void ToggleInlineSettings(const ui::Event& event);

  // Called when a user clicks on a notification action button, identified by
  // `index`.
  virtual void ActionButtonPressed(size_t index, const ui::Event& event);

  // Called after `inline_reply_` is updated for custom handling.
  virtual void OnInlineReplyUpdated();

  // Whether `notification` is configured to have an inline reply field.
  bool HasInlineReply(const Notification& notification) const;

  NotificationControlButtonsView* control_buttons_view() {
    return control_buttons_view_;
  }
  NotificationHeaderView* header_row() { return header_row_; }

  views::View* content_row() { return content_row_; }
  const views::View* content_row() const { return content_row_; }

  views::View* left_content() { return left_content_; }
  views::View* right_content() { return right_content_; }

  views::Label* message_label() { return message_label_; }
  const views::Label* message_label() const { return message_label_; }

  ProportionalImageView* icon_view() const { return icon_view_; }

  views::View* inline_settings_row() { return settings_row_; }
  const views::View* inline_settings_row() const { return settings_row_; }

  views::View* image_container_view() { return image_container_view_; }
  const views::View* image_container_view() const {
    return image_container_view_;
  }

  views::View* actions_row() { return actions_row_; }

  views::View* action_buttons_row() { return action_buttons_row_; }
  const views::View* action_buttons_row() const { return action_buttons_row_; }

  std::vector<views::LabelButton*> action_buttons() { return action_buttons_; }

  views::ProgressBar* progress_bar_view() const { return progress_bar_view_; }

  NotificationInputContainer* inline_reply() { return inline_reply_; }

  views::Label* status_view() { return status_view_; }
  const views::Label* status_view() const { return status_view_; }
  const std::vector<views::View*> item_views() const { return item_views_; }

  bool inline_settings_enabled() const { return inline_settings_enabled_; }
  void set_inline_settings_enabled(bool inline_settings_enabled) {
    inline_settings_enabled_ = inline_settings_enabled;
  }

  bool hide_icon_on_expanded() const { return hide_icon_on_expanded_; }

  virtual bool IsExpandable() const = 0;

  virtual void SetExpandButtonEnabled(bool enabled);

  // Returns the size of `icon_view_`.
  virtual gfx::Size GetIconViewSize() const = 0;

  // Returns the max width of the large image inside `image_container_view_`.
  virtual int GetLargeImageViewMaxWidth() const = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, AppNameExtension);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, AppNameSystemNotification);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, AppNameWebNotification);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, AppNameWebAppNotification);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, CreateOrUpdateTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, InlineSettings);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest,
                           InlineSettingsInkDropAnimation);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, NotificationWithoutIcon);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, ShowProgress);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, ShowTimestamp);
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
  FRIEND_TEST_ALL_PREFIXES(NotificationViewBaseTest, UseImageAsIcon);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, TestIconSizing);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, LeftContentResizeForIcon);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, ManuallyExpandedOrCollapsed);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, InlineSettingsNotBlock);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, InlineSettingsBlockAll);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, TestAccentColor);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest,
                           TestAccentColorTextFlagAffectsHeaderRow);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, ExpandLongMessage);

  friend class NotificationViewBaseTest;

  void CreateOrUpdateMessageLabel(const Notification& notification);
  virtual void CreateOrUpdateProgressViews(const Notification& notification);
  void CreateOrUpdateListItemViews(const Notification& notification);
  void CreateOrUpdateIconView(const Notification& notification);
  void CreateOrUpdateImageView(const Notification& notification);
  void CreateOrUpdateActionButtonViews(const Notification& notification);

  // View containing close and settings buttons
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION NotificationControlButtonsView* control_buttons_view_ =
      nullptr;

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

  // Describes whether this view is for an ash/ChromeOS notification (ash
  // notification UI uses AshNotificationView, which has customized layout,
  // header view, etc.).
  const bool for_ash_notification_;

  // Describes whether the view can display inline settings or not.
  bool inline_settings_enabled_ = false;

  // Container views directly attached to this view.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION NotificationHeaderView* header_row_ = nullptr;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::View* content_row_ = nullptr;
  raw_ptr<views::View> actions_row_ = nullptr;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::View* settings_row_ = nullptr;

  // Containers for left and right side on |content_row_|
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::View* left_content_ = nullptr;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::View* right_content_ = nullptr;

  // Views which are dynamically created inside view hierarchy.
  raw_ptr<views::Label, DanglingUntriaged> message_label_ = nullptr;
  raw_ptr<views::Label, DanglingUntriaged> status_view_ = nullptr;
  raw_ptr<ProportionalImageView, DanglingUntriaged> icon_view_ = nullptr;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::View* image_container_view_ = nullptr;
  std::vector<views::LabelButton*> action_buttons_;
  std::vector<views::View*> item_views_;
  raw_ptr<views::ProgressBar, DanglingUntriaged> progress_bar_view_ = nullptr;
  raw_ptr<CompactTitleMessageView, DanglingUntriaged>
      compact_title_message_view_ = nullptr;
  raw_ptr<views::View> action_buttons_row_ = nullptr;
  raw_ptr<NotificationInputContainer> inline_reply_ = nullptr;

  // A map from views::LabelButton's in `action_buttons_` to their associated
  // placeholder strings.
  std::map<views::LabelButton*, absl::optional<std::u16string>>
      action_button_to_placeholder_map_;

  // Counter for view layouting, which is used during the CreateOrUpdate*
  // phases to keep track of the view ordering. See crbug.com/901045
  size_t left_content_count_;

  std::unique_ptr<ui::EventHandler> click_activator_;

  base::TimeTicks last_mouse_pressed_timestamp_;

  base::WeakPtrFactory<NotificationViewBase> weak_ptr_factory_{this};
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_BASE_H_
