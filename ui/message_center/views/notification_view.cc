// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/layout/box_layout.h"

namespace message_center {

namespace {

// TODO(crbug/1243889): Move the padding and spacing definition from
// NotificationViewBase to this class.

constexpr gfx::Insets kContentRowPadding(0, 12, 16, 12);
// TODO(tetsui): Move |kIconViewSize| to public/cpp/message_center_constants.h
// and merge with contradicting |kNotificationIconSize|.
constexpr gfx::Size kIconViewSize(36, 36);
constexpr gfx::Insets kLeftContentPadding(2, 4, 0, 4);
constexpr gfx::Insets kLeftContentPaddingWithIcon(2, 4, 0, 12);

// Minimum size of a button in the actions row.
constexpr gfx::Size kActionButtonMinSize(0, 32);

constexpr int kMessageViewWidthWithIcon =
    kNotificationWidth - kIconViewSize.width() -
    kLeftContentPaddingWithIcon.left() - kLeftContentPaddingWithIcon.right() -
    kContentRowPadding.left() - kContentRowPadding.right();

constexpr int kMessageViewWidth =
    kNotificationWidth - kLeftContentPadding.left() -
    kLeftContentPadding.right() - kContentRowPadding.left() -
    kContentRowPadding.right();

constexpr int kTitleCharacterLimit =
    kNotificationWidth * kMaxTitleLines / kMinPixelsPerTitleCharacter;

// "Roboto-Regular, 12sp" is specified in the mock.
constexpr int kHeaderTextFontSize = 12;

// Default paddings of the views of texts. Adjusted on Windows.
// Top: 9px = 11px (from the mock) - 2px (outer padding).
// Bottom: 6px from the mock.
constexpr gfx::Insets kTextViewPaddingDefault(9, 0, 6, 0);

gfx::FontList GetHeaderTextFontList() {
  gfx::Font default_font;
  int font_size_delta = kHeaderTextFontSize - default_font.GetFontSize();
  const gfx::Font& font = default_font.Derive(
      font_size_delta, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  DCHECK_EQ(kHeaderTextFontSize, font.GetFontSize());
  return gfx::FontList(font);
}

gfx::Insets CalculateTopPadding(int font_list_height) {
#if defined(OS_WIN)
  // On Windows, the fonts can have slightly different metrics reported,
  // depending on where the code runs. In Chrome, DirectWrite is on, which means
  // font metrics are reported from Skia, which rounds from float using ceil.
  // In unit tests, however, GDI is used to report metrics, and the height
  // reported there is consistent with other platforms. This means there is a
  // difference of 1px in height between Chrome on Windows and everything else
  // (where everything else includes unit tests on Windows). This 1px causes the
  // text and everything else to stop aligning correctly, so we account for it
  // by shrinking the top padding by 1.
  if (font_list_height != 15) {
    DCHECK_EQ(16, font_list_height);
    return kTextViewPaddingDefault - gfx::Insets(1 /* top */, 0, 0, 0);
  }
#endif

  return kTextViewPaddingDefault;
}

// NotificationTextButton //////////////////////////////////////////////////////

// TODO(crbug/1241983): Add metadata and builder support to this view.

// NotificationTextButton extends MdTextButton to allow for placeholder text
// as well as capitalizing the given label string. Used by chrome notifications.
// Ash notifications create their own.
class NotificationTextButton : public views::MdTextButton {
 public:
  NotificationTextButton(PressedCallback callback, const std::u16string& label)
      : views::MdTextButton(std::move(callback), label) {
    SetMinSize(kActionButtonMinSize);
    views::InstallRectHighlightPathGenerator(this);
    SetTextSubpixelRenderingEnabled(false);
  }
  NotificationTextButton(const NotificationTextButton&) = delete;
  NotificationTextButton& operator=(const NotificationTextButton&) = delete;
  ~NotificationTextButton() override = default;

  // views::MdTextButton:
  void UpdateBackgroundColor() override {
    // Overridden as no-op so we don't draw any background or border.
  }

  void OnThemeChanged() override {
    views::MdTextButton::OnThemeChanged();
    SetEnabledTextColors(color_);
    label()->SetBackgroundColor(
        GetColorProvider()->GetColor(ui::kColorNotificationActionsBackground));
  }

  void SetEnabledTextColors(absl::optional<SkColor> color) override {
    color_ = std::move(color);
    views::MdTextButton::SetEnabledTextColors(color_);
    label()->SetAutoColorReadabilityEnabled(true);
  }

 private:
  absl::optional<SkColor> color_;
};

}  // namespace

NotificationView::NotificationView(
    const message_center::Notification& notification)
    : NotificationViewBase(notification) {
  // Instantiate view instances and define layout and view hierarchy.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  auto header_row = CreateHeaderRow();

  // Font list for text views.
  const gfx::FontList& font_list = GetHeaderTextFontList();
  const int font_list_height = font_list.GetHeight();
  const gfx::Insets& text_view_padding(CalculateTopPadding(font_list_height));
  header_row->ConfigureLabelsStyle(font_list, text_view_padding, false);

  header_row->AddChildView(CreateControlButtonsView());

  auto content_row = CreateContentRow();
  auto* content_row_layout =
      static_cast<views::BoxLayout*>(content_row->GetLayoutManager());
  content_row_layout->set_inside_border_insets(kContentRowPadding);

  auto left_content = CreateLeftContentView();
  left_content->SetBorder(views::CreateEmptyBorder(kLeftContentPadding));
  auto* left_content_ptr_ = content_row->AddChildView(std::move(left_content));
  static_cast<views::BoxLayout*>(content_row->GetLayoutManager())
      ->SetFlexForView(left_content_ptr_, 1);
  content_row->AddChildView(CreateRightContentView());

  AddChildView(std::move(header_row));
  AddChildView(std::move(content_row));
  AddChildView(CreateImageContainerView());
  AddChildView(CreateInlineSettingsView());
  AddChildView(CreateActionsRow());

  CreateOrUpdateViews(notification);
  UpdateControlButtonsVisibilityWithNotification(notification);
}

NotificationView::~NotificationView() = default;

void NotificationView::CreateOrUpdateTitleView(
    const Notification& notification) {
  if (notification.title().empty() ||
      notification.type() == NOTIFICATION_TYPE_PROGRESS) {
    if (title_view_) {
      DCHECK(left_content()->Contains(title_view_));
      left_content()->RemoveChildViewT(title_view_);
      title_view_ = nullptr;
    }
    return;
  }

  const std::u16string& title = gfx::TruncateString(
      notification.title(), kTitleCharacterLimit, gfx::WORD_BREAK);
  if (!title_view_) {
    title_view_ = AddViewToLeftContent(GenerateTitleView(title));
  } else {
    title_view_->SetText(title);
    ReorderViewInLeftContent(title_view_);
  }
}

void NotificationView::CreateOrUpdateSmallIconView(
    const Notification& notification) {
  // This is called when the notification view is inserted into a Widget
  // hierarchy and when the Widget's theme has changed. If not currently in a
  // Widget hierarchy, defer updating the small icon view since
  // GetColorProvider() depends on the widget.
  if (!GetWidget())
    return;
  const auto* color_provider = GetColorProvider();
  SkColor accent_color = notification.accent_color().value_or(
      color_provider->GetColor(ui::kColorNotificationHeaderForeground));
  SkColor icon_color =
      color_utils::BlendForMinContrast(
          accent_color, GetNotificationHeaderViewBackgroundColor())
          .color;

  // TODO(crbug.com/768748): figure out if this has a performance impact and
  // cache images if so.
  gfx::Image masked_small_icon = notification.GenerateMaskedSmallIcon(
      kSmallImageSizeMD, icon_color,
      color_provider->GetColor(ui::kColorNotificationIconBackground),
      color_provider->GetColor(ui::kColorNotificationIconForeground));

  if (masked_small_icon.IsEmpty()) {
    header_row()->ClearAppIcon();
  } else {
    header_row()->SetAppIcon(masked_small_icon.AsImageSkia());
  }
}

std::unique_ptr<views::LabelButton>
NotificationView::GenerateNotificationLabelButton(
    views::Button::PressedCallback callback,
    const std::u16string& label) {
  return std::make_unique<NotificationTextButton>(std::move(callback), label);
}

void NotificationView::UpdateViewForExpandedState(bool expanded) {
  left_content()->SetBorder(views::CreateEmptyBorder(
      IsIconViewShown() ? kLeftContentPaddingWithIcon : kLeftContentPadding));

  // TODO(tetsui): Workaround https://crbug.com/682266 by explicitly setting
  // the width.
  // Ideally, we should fix the original bug, but it seems there's no obvious
  // solution for the bug according to https://crbug.com/678337#c7, we should
  // ensure that the change won't break any of the users of BoxLayout class.
  const int message_view_width =
      (IsIconViewShown() ? kMessageViewWidthWithIcon : kMessageViewWidth) -
      GetInsets().width();
  if (title_view_)
    title_view_->SizeToFit(message_view_width);
  if (message_view())
    message_view()->SizeToFit(message_view_width);
  NotificationViewBase::UpdateViewForExpandedState(expanded);
}

gfx::Size NotificationView::GetIconViewSize() const {
  return kIconViewSize;
}

void NotificationView::OnThemeChanged() {
  MessageView::OnThemeChanged();
  UpdateHeaderViewBackgroundColor();
  UpdateActionButtonsRowBackground();
}

void NotificationView::UpdateCornerRadius(int top_radius, int bottom_radius) {
  UpdateActionButtonsRowBackground();
  NotificationViewBase::UpdateCornerRadius(top_radius, bottom_radius);
}

void NotificationView::ToggleInlineSettings(const ui::Event& event) {
  if (!inline_settings_enabled())
    return;

  // TODO(crbug/1233670): In later refactor, `block_all_button_` and
  // `dont_block_button_` should be moved from NotificationViewBase to this
  // class, since AshNotificationView will use a different UI for inline
  // settings.
  bool disable_notification =
      inline_settings_row()->GetVisible() && block_all_button()->GetChecked();

  NotificationViewBase::ToggleInlineSettings(event);

  if (inline_settings_row()->GetVisible())
    AddBackgroundAnimation(event);
  else
    RemoveBackgroundAnimation();

  UpdateHeaderViewBackgroundColor();
  Layout();
  SchedulePaint();

  // Call DisableNotification() at the end, because |this| can be deleted at any
  // point after it's called.
  if (disable_notification)
    MessageCenter::Get()->DisableNotification(notification_id());
}

void NotificationView::UpdateHeaderViewBackgroundColor() {
  SkColor header_background_color = GetNotificationHeaderViewBackgroundColor();
  header_row()->SetBackgroundColor(header_background_color);
  control_buttons_view()->SetBackgroundColor(header_background_color);

  auto* notification =
      MessageCenter::Get()->FindVisibleNotificationById(notification_id());
  if (notification)
    CreateOrUpdateSmallIconView(*notification);
}

SkColor NotificationView::GetNotificationHeaderViewBackgroundColor() const {
  bool inline_settings_visible = inline_settings_row()->GetVisible();
  return GetColorProvider()->GetColor(
      inline_settings_visible ? ui::kColorNotificationBackgroundActive
                              : ui::kColorNotificationBackgroundInactive);
}

void NotificationView::UpdateActionButtonsRowBackground() {
  if (!GetWidget())
    return;

  action_buttons_row()->SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<NotificationBackgroundPainter>(
          /*top_radius=*/0, bottom_radius(),
          GetColorProvider()->GetColor(
              ui::kColorNotificationActionsBackground))));
}

void NotificationView::AddBackgroundAnimation(const ui::Event& event) {
  views::InkDrop::Get(this)->SetMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
  std::unique_ptr<ui::Event> located_event =
      notification_view_util::ConvertToBoundedLocatedEvent(event, this);
  views::InkDrop::Get(this)->AnimateToState(
      views::InkDropState::ACTION_PENDING,
      ui::LocatedEvent::FromIfValid(located_event.get()));
}

void NotificationView::RemoveBackgroundAnimation() {
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
  views::InkDrop::Get(this)->AnimateToState(views::InkDropState::HIDDEN,
                                            nullptr);
}

}  // namespace message_center
