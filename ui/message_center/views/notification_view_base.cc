// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view_base.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/class_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/large_image_view.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace message_center {

namespace {

// Dimensions.
constexpr int kActionsRowHorizontalSpacing = 8;
constexpr auto kStatusTextPadding = gfx::Insets::TLBR(4, 0, 0, 0);
constexpr gfx::Insets kActionsRowPadding(8);
constexpr int kLargeImageMaxHeight = 218;

constexpr int kCompactTitleMessageViewSpacing = 12;

constexpr int kProgressBarHeight = 4;

// In progress notification, if both the title and the message are long, the
// message would be prioritized and the title would be elided.
// However, it is not preferable that we completely omit the title, so
// the ratio of the message width is limited to this value.
constexpr double kProgressNotificationMessageRatio = 0.7;

// Creates a view responsible for drawing each list notification item's title
// and message next to each other within a single column.
std::unique_ptr<views::View> CreateItemView(const NotificationItem& item) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));

  auto* title = view->AddChildView(std::make_unique<views::Label>(
      item.title(), views::style::CONTEXT_DIALOG_BODY_TEXT));
  title->SetCollapseWhenHidden(true);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* message = view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          IDS_MESSAGE_CENTER_LIST_NOTIFICATION_MESSAGE_WITH_DIVIDER,
          item.message()),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  message->SetCollapseWhenHidden(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return view;
}

bool IsForAshNotification() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // anonymous namespace

// CompactTitleMessageView /////////////////////////////////////////////////////

CompactTitleMessageView::~CompactTitleMessageView() = default;

CompactTitleMessageView::CompactTitleMessageView() {
  title_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  message_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY));
  message_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
}

gfx::Size CompactTitleMessageView::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  gfx::Size title_size = title_->GetPreferredSize();
  gfx::Size message_size = message_->GetPreferredSize();
  return gfx::Size(title_size.width() + message_size.width() +
                       kCompactTitleMessageViewSpacing,
                   std::max(title_size.height(), message_size.height()));
}

views::ProposedLayout CompactTitleMessageView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  DCHECK(size_bounds.is_fully_bounded());
  layout.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  // Elides title and message.
  // * If the message is too long, the message occupies at most
  //   kProgressNotificationMessageRatio of the width.
  // * If the title is too long, the full content of the message is shown,
  //   kCompactTitleMessageViewSpacing is added between them, and the elided
  //   title is shown.
  // * If they are short enough, the title is left-aligned and the message is
  //   right-aligned.
  const int message_width =
      std::min(message_->GetPreferredSize().width(),
               title_->GetPreferredSize().width() > 0
                   ? static_cast<int>(kProgressNotificationMessageRatio *
                                      layout.host_size.width())
                   : layout.host_size.width());
  const int title_width = std::max(0, layout.host_size.width() - message_width -
                                          kCompactTitleMessageViewSpacing);
  layout.child_layouts.emplace_back(
      title_.get(), title_->GetVisible(),
      gfx::Rect(0, 0, title_width, layout.host_size.height()));
  layout.child_layouts.emplace_back(
      message_.get(), message_->GetVisible(),
      gfx::Rect(layout.host_size.width() - message_width, 0, message_width,
                layout.host_size.height()));
  return layout;
}

void CompactTitleMessageView::set_title(const std::u16string& title) {
  title_->SetText(title);
}

void CompactTitleMessageView::set_message(const std::u16string& message) {
  message_->SetText(message);
}

BEGIN_METADATA(CompactTitleMessageView)
END_METADATA

// ////////////////////////////////////////////////////////////
// NotificationViewBase
// ////////////////////////////////////////////////////////////

void NotificationViewBase::CreateOrUpdateViews(
    const Notification& notification) {
  left_content_count_ = 0;

  CreateOrUpdateHeaderView(notification);
  CreateOrUpdateTitleView(notification);
  CreateOrUpdateMessageLabel(notification);
  CreateOrUpdateCompactTitleMessageView(notification);
  CreateOrUpdateProgressViews(notification);
  CreateOrUpdateListItemViews(notification);
  CreateOrUpdateIconView(notification);
  CreateOrUpdateSmallIconView(notification);
  CreateOrUpdateImageView(notification);
  CreateOrUpdateInlineSettingsViews(notification);
  CreateOrUpdateSnoozeSettingsViews(notification);
  UpdateViewForExpandedState(expanded_);
  // Should be called at the last because SynthesizeMouseMoveEvent() requires
  // everything is in the right location when called.
  CreateOrUpdateActionButtonViews(notification);
}

NotificationViewBase::NotificationViewBase(const Notification& notification)
    : MessageView(notification), for_ash_notification_(IsForAshNotification()) {
  UpdateCornerRadius(kNotificationCornerRadius, kNotificationCornerRadius);
  SetProperty(views::kElementIdentifierKey,
              notification.host_view_element_id());
}

NotificationViewBase::~NotificationViewBase() = default;

void NotificationViewBase::OnFocus() {
  MessageView::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

bool NotificationViewBase::OnMousePressed(const ui::MouseEvent& event) {
  last_mouse_pressed_timestamp_ = base::TimeTicks(event.time_stamp());
  return true;
}

bool NotificationViewBase::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

void NotificationViewBase::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return;

  // The mouse has been clicked for a long time.
  if (ui::EventTimeStampToSeconds(event.time_stamp()) -
          ui::EventTimeStampToSeconds(last_mouse_pressed_timestamp_) >
      ui::GetGestureProviderConfig(
          ui::GestureProviderConfigType::CURRENT_PLATFORM)
          .gesture_detector_config.longpress_timeout.InSecondsF()) {
    ToggleInlineSettings(event);
    return;
  }

  // Ignore click of actions row outside action buttons.
  if (expanded_) {
    DCHECK(actions_row_);
    gfx::Point point_in_child = event.location();
    ConvertPointToTarget(this, actions_row_, &point_in_child);
    if (actions_row_->HitTestPoint(point_in_child))
      return;
  }

  // Ignore clicks of outside region when inline settings is shown.
  if (settings_row_->GetVisible())
    return;

  MessageView::OnMouseReleased(event);
}

void NotificationViewBase::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureLongTap) {
    ToggleInlineSettings(*event);
    return;
  }
  MessageView::OnGestureEvent(event);
}

void NotificationViewBase::UpdateWithNotification(
    const Notification& notification) {
  MessageView::UpdateWithNotification(notification);
  UpdateControlButtonsVisibilityWithNotification(notification);

  CreateOrUpdateViews(notification);
  DeprecatedLayoutImmediately();
  SchedulePaint();
}

void NotificationViewBase::OnNotificationInputSubmit(
    size_t index,
    const std::u16string& text) {
  MessageCenter::Get()->ClickOnNotificationButtonWithReply(notification_id(),
                                                           index, text);
}

bool NotificationViewBase::IsIconViewShown() const {
  return icon_view_ && (!hide_icon_on_expanded_ || !expanded_);
}

views::Builder<NotificationControlButtonsView>
NotificationViewBase::CreateControlButtonsBuilder() {
  DCHECK(!control_buttons_view_);
  return views::Builder<NotificationControlButtonsView>()
      .CopyAddressTo(&control_buttons_view_)
      .SetMessageView(this);
}

views::Builder<NotificationHeaderView>
NotificationViewBase::CreateHeaderRowBuilder() {
  DCHECK(!header_row_);
  auto header_row_builder = views::Builder<NotificationHeaderView>()
                                .SetID(kHeaderRow)
                                .CopyAddressTo(&header_row_);
  return header_row_builder;
}

views::Builder<views::BoxLayoutView>
NotificationViewBase::CreateLeftContentBuilder() {
  DCHECK(!left_content_);
  return views::Builder<views::BoxLayoutView>()
      .CopyAddressTo(&left_content_)
      .SetOrientation(views::BoxLayout::Orientation::kVertical);
}

views::Builder<views::View> NotificationViewBase::CreateRightContentBuilder() {
  DCHECK(!right_content_);
  return views::Builder<views::View>()
      .CopyAddressTo(&right_content_)
      .SetUseDefaultFillLayout(true);
}

views::Builder<views::View> NotificationViewBase::CreateContentRowBuilder() {
  DCHECK(!content_row_);
  return views::Builder<views::View>()
      .SetID(kContentRow)
      .CopyAddressTo(&content_row_);
}

views::Builder<views::BoxLayoutView>
NotificationViewBase::CreateInlineSettingsBuilder() {
  DCHECK(!settings_row_);
  return views::Builder<views::BoxLayoutView>()
      .CopyAddressTo(&settings_row_)
      .SetVisible(false);
}

views::Builder<views::BoxLayoutView>
NotificationViewBase::CreateSnoozeSettingsBuilder() {
  CHECK(!snooze_row_);
  return views::Builder<views::BoxLayoutView>()
      .CopyAddressTo(&snooze_row_)
      .SetVisible(false);
}

views::Builder<views::View>
NotificationViewBase::CreateImageContainerBuilder() {
  DCHECK(!image_container_view_);
  return views::Builder<views::View>()
      .CopyAddressTo(&image_container_view_)
      .SetUseDefaultFillLayout(true);
}

std::unique_ptr<views::View> NotificationViewBase::CreateActionsRow(
    std::unique_ptr<views::LayoutManager> layout_manager) {
  DCHECK(!actions_row_);
  auto actions_row = std::make_unique<views::View>();
  actions_row->SetVisible(false);
  actions_row->SetLayoutManager(std::move(layout_manager));

  // |action_buttons_row_| contains inline action buttons.
  DCHECK(!action_buttons_row_);
  auto action_buttons_row = std::make_unique<views::View>();
  action_buttons_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kActionsRowPadding,
      kActionsRowHorizontalSpacing));
  action_buttons_row->SetVisible(false);
  action_buttons_row->SetID(kActionButtonsRow);
  action_buttons_row_ =
      actions_row->AddChildView(std::move(action_buttons_row));

  // |inline_reply_| is a container for an inline textfield.
  DCHECK(!inline_reply_);
  auto inline_reply = GenerateNotificationInputContainer();
  inline_reply->Init();
  inline_reply->SetVisible(false);
  inline_reply->SetID(kInlineReply);
  inline_reply_ = actions_row->AddChildView(std::move(inline_reply));

  actions_row_ = actions_row.get();
  return actions_row;
}

// static
std::unique_ptr<views::Label> NotificationViewBase::GenerateTitleView(
    const std::u16string& title) {
  auto title_view = std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_BODY_TEXT);
  title_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_view->SetAllowCharacterBreak(true);
  return title_view;
}

std::unique_ptr<NotificationInputContainer>
NotificationViewBase::GenerateNotificationInputContainer() {
  return std::make_unique<NotificationInputContainer>(this);
}

void NotificationViewBase::CreateOrUpdateHeaderView(
    const Notification& notification) {
  header_row_->SetTimestamp(notification.timestamp());
  header_row_->SetAppNameElideBehavior(gfx::ELIDE_TAIL);

  std::u16string app_name;
  if (notification.notifier_id().title.has_value()) {
    app_name = notification.notifier_id().title.value();
  } else if (notification.UseOriginAsContextMessage()) {
    app_name = url_formatter::FormatUrlForSecurityDisplay(
        notification.origin_url(),
        url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    header_row_->SetAppNameElideBehavior(gfx::ELIDE_HEAD);
  } else if (notification.display_source().empty() &&
             notification.notifier_id().type ==
                 NotifierType::SYSTEM_COMPONENT) {
    app_name = MessageCenter::Get()->GetSystemNotificationAppName();
  } else if (!notification.context_message().empty()) {
    app_name = notification.context_message();
  } else {
    app_name = notification.display_source();
  }
  header_row_->SetAppName(app_name);
}

void NotificationViewBase::CreateOrUpdateCompactTitleMessageView(
    const Notification& notification) {
  if (notification.type() != NOTIFICATION_TYPE_PROGRESS) {
    DCHECK(!compact_title_message_view_ ||
           left_content_->Contains(compact_title_message_view_));
    delete compact_title_message_view_;
    compact_title_message_view_ = nullptr;
    return;
  }

  if (!compact_title_message_view_) {
    auto compact_title_message_view =
        std::make_unique<CompactTitleMessageView>();
    compact_title_message_view_ =
        AddViewToLeftContent(std::move(compact_title_message_view));
  } else {
    ReorderViewInLeftContent(compact_title_message_view_);
  }

  compact_title_message_view_->set_title(notification.title());
  compact_title_message_view_->set_message(notification.message());
  left_content_->InvalidateLayout();
}

void NotificationViewBase::CreateOrUpdateProgressBarView(
    const Notification& notification) {
  if (notification.type() != NOTIFICATION_TYPE_PROGRESS) {
    DCHECK(!progress_bar_view_ || left_content_->Contains(progress_bar_view_));
    delete progress_bar_view_;
    progress_bar_view_ = nullptr;
    return;
  }

  DCHECK(left_content_);

  if (!progress_bar_view_) {
    auto progress_bar_view = std::make_unique<views::ProgressBar>();
    progress_bar_view->SetPreferredHeight(kProgressBarHeight);
    progress_bar_view->SetPreferredCornerRadii(std::nullopt);
    progress_bar_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kProgressBarTopPadding, 0, 0, 0)));
    progress_bar_view_ = AddViewToLeftContent(std::move(progress_bar_view));
  } else {
    ReorderViewInLeftContent(progress_bar_view_);
  }

  progress_bar_view_->SetValue(notification.progress() / 100.0);
  progress_bar_view_->SetVisible(notification.items().empty());

  if (0 <= notification.progress() && notification.progress() <= 100)
    header_row_->SetProgress(notification.progress());
}

void NotificationViewBase::CreateOrUpdateProgressStatusView(
    const Notification& notification) {
  if (notification.type() != NOTIFICATION_TYPE_PROGRESS ||
      notification.progress_status().empty()) {
    if (!status_view_)
      return;
    DCHECK(left_content_->Contains(status_view_));
    delete status_view_;
    status_view_ = nullptr;
    return;
  }

  if (!status_view_) {
    auto status_view = std::make_unique<views::Label>(
        std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_SECONDARY);
    status_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    status_view->SetBorder(views::CreateEmptyBorder(kStatusTextPadding));
    status_view_ = AddViewToLeftContent(std::move(status_view));
  } else {
    ReorderViewInLeftContent(status_view_);
  }

  status_view_->SetText(notification.progress_status());
}

void NotificationViewBase::CreateOrUpdateMessageLabel(
    const Notification& notification) {
  if (notification.type() == NOTIFICATION_TYPE_PROGRESS ||
      notification.message().empty()) {
    // Deletion will also remove |message_label_| from its parent.
    delete message_label_;
    message_label_ = nullptr;
    return;
  }

  const std::u16string text = gfx::TruncateString(
      notification.message(), GetMessageCharacterLimit(), gfx::WORD_BREAK);

  if (!message_label_) {
    auto message_label = std::make_unique<views::Label>(
        text, views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_SECONDARY);
    message_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    message_label->SetAllowCharacterBreak(true);
    message_label_ = AddViewToLeftContent(std::move(message_label));
  } else {
    message_label_->SetText(text);
    ReorderViewInLeftContent(message_label_);
  }

  message_label_->SetVisible(notification.items().empty());
}

void NotificationViewBase::CreateOrUpdateProgressViews(
    const Notification& notification) {
  // Ordering should be Progress Bar, then the Progress Status for chrome. Ash
  // reverses the ordering.
  CreateOrUpdateProgressBarView(notification);
  CreateOrUpdateProgressStatusView(notification);
}

void NotificationViewBase::CreateOrUpdateListItemViews(
    const Notification& notification) {
  for (views::View* item_view : item_views_) {
    delete item_view;
  }
  item_views_.clear();

  const std::vector<NotificationItem>& items = notification.items();

  for (size_t i = 0; i < items.size() && i < kMaxLinesForExpandedMessageLabel;
       ++i) {
    std::unique_ptr<views::View> item_view = CreateItemView(items[i]);
    item_views_.push_back(AddViewToLeftContent(std::move(item_view)));
  }

  list_items_count_ = items.size();

  // Needed when CreateOrUpdateViews is called for update.
  if (!item_views_.empty())
    left_content_->InvalidateLayout();
}

void NotificationViewBase::CreateOrUpdateIconView(
    const Notification& notification) {
  const bool use_image_for_icon = notification.icon().IsEmpty();

  ui::ImageModel icon = use_image_for_icon
                            ? ui::ImageModel::FromImage(notification.image())
                            : notification.icon();

  if (notification.type() == NOTIFICATION_TYPE_PROGRESS ||
      notification.type() == NOTIFICATION_TYPE_MULTIPLE || icon.IsEmpty()) {
    DCHECK(!icon_view_ || right_content_->Contains(icon_view_));
    delete icon_view_;
    icon_view_ = nullptr;
    return;
  }

  if (!icon_view_) {
    icon_view_ = right_content_->AddChildView(
        std::make_unique<ProportionalImageView>(GetIconViewSize()));
  }

  bool apply_rounded_corners = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  apply_rounded_corners = for_ash_notification_;
#endif  // IS_CHROMEOS_ASH
  icon_view_->SetImage(icon, icon.Size(), apply_rounded_corners);

  // Hide the icon on the right side when the notification is expanded.
  hide_icon_on_expanded_ = use_image_for_icon;
}

void NotificationViewBase::CreateOrUpdateImageView(
    const Notification& notification) {
  if (notification.image().IsEmpty()) {
    image_container_view_->RemoveAllChildViews();
    image_container_view_->SetVisible(false);
    return;
  }

  if (image_container_view_->children().empty()) {
    image_container_view_->AddChildView(std::make_unique<LargeImageView>(
        gfx::Size(GetLargeImageViewMaxWidth(), kLargeImageMaxHeight)));
    image_container_view_->SetVisible(true);
  }

  static_cast<LargeImageView*>(image_container_view_->children().front())
      ->SetImage(notification.image().AsImageSkia());
}

void NotificationViewBase::CreateOrUpdateActionButtonViews(
    const Notification& notification) {
  const std::vector<ButtonInfo>& buttons = notification.buttons();
  bool new_buttons = action_buttons_.size() != buttons.size();

  if (new_buttons || buttons.empty()) {
    for (views::LabelButton* item : action_buttons_) {
      delete item;
    }
    action_buttons_.clear();

    // The `actions_row_` also contains the snooze button in ash.
    actions_row_->SetVisible(
        expanded_ &&
        (!buttons.empty() ||
         (for_ash_notification_ && notification.should_show_snooze_button())));
  }

  // Hide inline reply field if it doesn't exist anymore.
  if (inline_reply_ && inline_reply_->GetVisible() &&
      !HasInlineReply(notification)) {
    action_buttons_row_->SetVisible(true);
    inline_reply_->SetVisible(false);
  }

  for (size_t i = 0; i < buttons.size(); ++i) {
    ButtonInfo button_info = buttons[i];
    std::u16string label = for_ash_notification_
                               ? button_info.title
                               : base::i18n::ToUpper(button_info.title);
    if (new_buttons) {
      action_buttons_.push_back(
          action_buttons_row_->AddChildView(GenerateNotificationLabelButton(
              base::BindRepeating(&NotificationViewBase::ActionButtonPressed,
                                  base::Unretained(this), i),
              label)));
      action_button_to_placeholder_map_[action_buttons_.back()] =
          button_info.placeholder;
      // TODO(pkasting): BoxLayout should invalidate automatically when a child
      // is added, at which point we can remove this call.
      action_buttons_row_->InvalidateLayout();
    } else {
      action_buttons_[i]->SetText(label);
      action_button_to_placeholder_map_[action_buttons_[i]] =
          button_info.placeholder;
    }

    bool use_accent_color =
        !for_ash_notification_ &&
        !notification.rich_notification_data().ignore_accent_color_for_text;
    if (use_accent_color) {
      // Change action button color to the accent color.
      action_buttons_[i]->SetEnabledTextColors(notification.accent_color());
    }
  }

  // Inherit mouse hover state when action button views reset.
  // If the view is not expanded, there should be no hover state.
  if (new_buttons && expanded_) {
    views::Widget* widget = GetWidget();
    if (widget && !widget->IsClosed()) {
      // This DeprecatedLayoutImmediately() is needed because button should be
      // in the right location in the view hierarchy when
      // SynthesizeMouseMoveEvent() is called.
      DeprecatedLayoutImmediately();
      widget->SetSize(widget->GetContentsView()->GetPreferredSize({}));
      widget->SynthesizeMouseMoveEvent();
    }
  }
}

void NotificationViewBase::ReorderViewInLeftContent(views::View* view) {
  left_content_->ReorderChildView(view, left_content_count_++);
}

void NotificationViewBase::ActionButtonPressed(size_t index,
                                               const ui::Event& event) {
  const std::optional<std::u16string>& placeholder =
      action_button_to_placeholder_map_[action_buttons_[index]];
  if (placeholder && inline_reply_) {
    inline_reply_->SetTextfieldIndex(static_cast<int>(index));
    inline_reply_->SetPlaceholderText(placeholder);
    inline_reply_->AnimateBackground(event);
    inline_reply_->SetVisible(true);

    action_buttons_row_->SetVisible(false);

    // RequestFocus() should be called after SetVisible().
    inline_reply_->textfield()->RequestFocus();
    DeprecatedLayoutImmediately();
    SchedulePaint();

    OnInlineReplyUpdated();
    return;
  }

    MessageCenter::Get()->ClickOnNotificationButton(notification_id(),
                                                    static_cast<int>(index));
    // `this` may be deleted after handling the click. See crbug/1316656.
}

void NotificationViewBase::OnInlineReplyUpdated() {
  // Not implemented by default.
}

bool NotificationViewBase::HasInlineReply(
    const Notification& notification) const {
  if (!inline_reply_)
    return false;
  auto buttons = notification.buttons();
  const size_t index = inline_reply_->GetTextfieldIndex();
  return index < buttons.size() && buttons[index].placeholder.has_value();
}

void NotificationViewBase::SetExpandButtonVisibility(bool enabled) {
  if (!for_ash_notification_)
    header_row_->SetExpandButtonEnabled(enabled);
}

void NotificationViewBase::UpdateViewForExpandedState(bool expanded) {
  if (!for_ash_notification_)
    header_row_->SetExpanded(expanded);

  if (!image_container_view_->children().empty())
    image_container_view_->SetVisible(expanded);

  actions_row_->SetVisible(expanded &&
                           !action_buttons_row_->children().empty());
  if (!expanded) {
    action_buttons_row_->SetVisible(true);
    if (inline_reply_)
      inline_reply_->SetVisible(false);
  }

  for (size_t i = kMaxLinesForMessageLabel; i < item_views_.size(); ++i) {
    item_views_[i]->SetVisible(expanded);
  }
  if (status_view_)
    status_view_->SetVisible(expanded);

  int max_items = expanded ? item_views_.size() : kMaxLinesForMessageLabel;
  if (!for_ash_notification_ && list_items_count_ > max_items)
    header_row_->SetOverflowIndicator(list_items_count_ - max_items);
  else if (!item_views_.empty())
    header_row_->SetSummaryText(std::u16string());

  bool has_icon = IsIconViewShown();
  right_content_->SetVisible(has_icon);

  content_row_->InvalidateLayout();
}

NotificationControlButtonsView* NotificationViewBase::GetControlButtonsView()
    const {
  return control_buttons_view_;
}

bool NotificationViewBase::IsExpanded() const {
  return expanded_;
}

void NotificationViewBase::SetExpanded(bool expanded) {
  MessageView::SetExpanded(expanded);
  if (expanded_ == expanded)
    return;
  expanded_ = expanded;

  UpdateViewForExpandedState(expanded_);
  PreferredSizeChanged();
}

bool NotificationViewBase::IsManuallyExpandedOrCollapsed() const {
  return MessageCenter::Get()->GetNotificationExpandState(notification_id()) !=
         ExpandState::DEFAULT;
}

void NotificationViewBase::SetManuallyExpandedOrCollapsed(ExpandState state) {
  MessageCenter::Get()->SetNotificationExpandState(notification_id(), state);
}

void NotificationViewBase::ToggleInlineSettings(const ui::Event& event) {
  bool inline_settings_visible = !settings_row_->GetVisible();

  settings_row_->SetVisible(inline_settings_visible);
  header_row_->SetDetailViewsVisible(!inline_settings_visible);

  SetSettingMode(inline_settings_visible);

  // Grab a weak pointer before calling SetExpanded() as it might cause |this|
  // to be deleted.
  {
    auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
    SetExpanded(!inline_settings_visible);
    if (!weak_ptr) {
      return;
    }
  }
}

void NotificationViewBase::ToggleSnoozeSettings(const ui::Event& event) {
  bool snooze_settings_visible = !snooze_row_->GetVisible();

  snooze_row_->SetVisible(snooze_settings_visible);

  SetSettingMode(snooze_settings_visible);

  // Grab a weak pointer before calling SetExpanded() as it might cause |this|
  // to be deleted.
  {
    auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
    SetExpanded(!snooze_settings_visible);
    if (!weak_ptr) {
      return;
    }
  }
}

void NotificationViewBase::InkDropAnimationStarted() {
  header_row_->SetSubpixelRenderingEnabled(false);
}

void NotificationViewBase::InkDropRippleAnimationEnded(
    views::InkDropState ink_drop_state) {
  if (ink_drop_state == views::InkDropState::HIDDEN)
    header_row_->SetSubpixelRenderingEnabled(true);
}

BEGIN_METADATA(NotificationViewBase)
END_METADATA

}  // namespace message_center
