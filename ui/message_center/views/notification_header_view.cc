// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_header_view.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop_stub.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace message_center {

namespace {

constexpr int kHeaderHeight = 32;
constexpr int kHeaderHorizontalSpacing = 2;

// The padding outer the header and the control buttons.
constexpr gfx::Insets kHeaderOuterPadding(2, 2, 0, 2);

// Default paddings of the views of texts. Adjusted on Windows.
// Top: 9px = 11px (from the mock) - 2px (outer padding).
// Buttom: 6px from the mock.
constexpr gfx::Insets kTextViewPaddingDefault(9, 0, 6, 0);

// Paddings of the entire header.
// Left: 14px = 16px (from the mock) - 2px (outer padding).
// Right: 2px = minimum padding between the control buttons and the header.
constexpr gfx::Insets kHeaderPadding(0, 14, 0, 2);

// Paddings of the app icon (small image).
// Top: 8px = 10px (from the mock) - 2px (outer padding).
// Bottom: 4px from the mock.
// Right: 4px = 6px (from the mock) - kHeaderHorizontalSpacing.
constexpr gfx::Insets kAppIconPadding(8, 0, 4, 4);

// Size of the expand icon. 8px = 32px - 15px - 9px (values from the mock).
constexpr int kExpandIconSize = 8;
// Paddings of the expand buttons.
// Top: 13px = 15px (from the mock) - 2px (outer padding).
// Bottom: 9px from the mock.
constexpr gfx::Insets kExpandIconViewPadding(13, 2, 9, 0);

// Bullet character. The divider symbol between different parts of the header.
constexpr wchar_t kNotificationHeaderDivider[] = L" \u2022 ";

// base::TimeBase has similar constants, but some of them are missing.
constexpr int64_t kMinuteInMillis = 60LL * 1000LL;
constexpr int64_t kHourInMillis = 60LL * kMinuteInMillis;
constexpr int64_t kDayInMillis = 24LL * kHourInMillis;
// In Android, DateUtils.YEAR_IN_MILLIS is 364 days.
constexpr int64_t kYearInMillis = 364LL * kDayInMillis;

// "Roboto-Regular, 12sp" is specified in the mock.
constexpr int kHeaderTextFontSize = 12;

// ExpandButtton forwards all mouse and key events to NotificationHeaderView,
// but takes tab focus for accessibility purpose.
class ExpandButton : public views::ImageView {
 public:
  ExpandButton();
  ~ExpandButton() override;

  // Overridden from views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  std::unique_ptr<views::Painter> focus_painter_;
};

ExpandButton::ExpandButton() {
  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(0, 0, 1, 1));
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

ExpandButton::~ExpandButton() = default;

void ExpandButton::OnPaint(gfx::Canvas* canvas) {
  views::ImageView::OnPaint(canvas);
  if (HasFocus())
    views::Painter::PaintPainterAt(canvas, focus_painter_.get(),
                                   GetContentsBounds());
}

void ExpandButton::OnFocus() {
  views::ImageView::OnFocus();
  SchedulePaint();
}

void ExpandButton::OnBlur() {
  views::ImageView::OnBlur();
  SchedulePaint();
}

void ExpandButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetName(tooltip_text());
}

// Do relative time string formatting that is similar to
// com.java.android.widget.DateTimeView.updateRelativeTime.
// Chromium has its own base::TimeFormat::Simple(), but none of the formats
// supported by the function is similar to Android's one.
base::string16 FormatToRelativeTime(base::Time past) {
  base::Time now = base::Time::Now();
  int64_t duration = (now - past).InMilliseconds();
  if (duration < kMinuteInMillis) {
    return l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST);
  } else if (duration < kHourInMillis) {
    int count = static_cast<int>(duration / kMinuteInMillis);
    return l10n_util::GetPluralStringFUTF16(
        IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST, count);
  } else if (duration < kDayInMillis) {
    int count = static_cast<int>(duration / kHourInMillis);
    return l10n_util::GetPluralStringFUTF16(
        IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST, count);
  } else if (duration < kYearInMillis) {
    int count = static_cast<int>(duration / kDayInMillis);
    return l10n_util::GetPluralStringFUTF16(
        IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, count);
  } else {
    int count = static_cast<int>(duration / kYearInMillis);
    return l10n_util::GetPluralStringFUTF16(
        IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST, count);
  }
}

gfx::FontList GetHeaderTextFontList() {
  gfx::Font default_font;
  int font_size_delta = kHeaderTextFontSize - default_font.GetFontSize();
  gfx::Font font = default_font.Derive(font_size_delta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::NORMAL);
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

  DCHECK_EQ(15, font_list_height);
  return kTextViewPaddingDefault;
}

}  // namespace

NotificationHeaderView::NotificationHeaderView(
    NotificationControlButtonsView* control_buttons_view,
    views::ButtonListener* listener)
    : views::Button(listener) {
  const int kInnerHeaderHeight = kHeaderHeight - kHeaderOuterPadding.height();

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, kHeaderOuterPadding,
      kHeaderHorizontalSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);

  views::View* app_info_container = new views::View();
  auto app_info_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, kHeaderPadding, kHeaderHorizontalSpacing);
  app_info_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  app_info_container->SetLayoutManager(std::move(app_info_layout));
  AddChildView(app_info_container);

  // App icon view
  app_icon_view_ = new views::ImageView();
  app_icon_view_->SetImageSize(gfx::Size(kSmallImageSizeMD, kSmallImageSizeMD));
  app_icon_view_->SetBorder(views::CreateEmptyBorder(kAppIconPadding));
  app_icon_view_->SetVerticalAlignment(views::ImageView::LEADING);
  app_icon_view_->SetHorizontalAlignment(views::ImageView::LEADING);
  DCHECK_EQ(kInnerHeaderHeight, app_icon_view_->GetPreferredSize().height());
  app_info_container->AddChildView(app_icon_view_);

  // Font list for text views.
  gfx::FontList font_list = GetHeaderTextFontList();
  const int font_list_height = font_list.GetHeight();
  gfx::Insets text_view_padding(CalculateTopPadding(font_list_height));

  // App name view
  app_name_view_ = new views::Label(base::string16());
  app_name_view_->SetFontList(font_list);
  app_name_view_->SetLineHeight(font_list_height);
  app_name_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  app_name_view_->SetEnabledColor(accent_color_);
  app_name_view_->SetBorder(views::CreateEmptyBorder(text_view_padding));
  DCHECK_EQ(kInnerHeaderHeight, app_name_view_->GetPreferredSize().height());
  app_info_container->AddChildView(app_name_view_);

  // Summary text divider
  summary_text_divider_ =
      new views::Label(base::WideToUTF16(kNotificationHeaderDivider));
  summary_text_divider_->SetFontList(font_list);
  summary_text_divider_->SetLineHeight(font_list_height);
  summary_text_divider_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  summary_text_divider_->SetBorder(views::CreateEmptyBorder(text_view_padding));
  summary_text_divider_->SetVisible(false);
  DCHECK_EQ(kInnerHeaderHeight,
            summary_text_divider_->GetPreferredSize().height());
  app_info_container->AddChildView(summary_text_divider_);

  // Summary text view
  summary_text_view_ = new views::Label(base::string16());
  summary_text_view_->SetFontList(font_list);
  summary_text_view_->SetLineHeight(font_list_height);
  summary_text_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  summary_text_view_->SetBorder(views::CreateEmptyBorder(text_view_padding));
  summary_text_view_->SetVisible(false);
  DCHECK_EQ(kInnerHeaderHeight,
            summary_text_view_->GetPreferredSize().height());
  app_info_container->AddChildView(summary_text_view_);

  // Timestamp divider
  timestamp_divider_ =
      new views::Label(base::WideToUTF16(kNotificationHeaderDivider));
  timestamp_divider_->SetFontList(font_list);
  timestamp_divider_->SetLineHeight(font_list_height);
  timestamp_divider_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  timestamp_divider_->SetBorder(views::CreateEmptyBorder(text_view_padding));
  timestamp_divider_->SetVisible(false);
  DCHECK_EQ(kInnerHeaderHeight,
            timestamp_divider_->GetPreferredSize().height());
  app_info_container->AddChildView(timestamp_divider_);

  // Timestamp view
  timestamp_view_ = new views::Label(base::string16());
  timestamp_view_->SetFontList(font_list);
  timestamp_view_->SetLineHeight(font_list_height);
  timestamp_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  timestamp_view_->SetBorder(views::CreateEmptyBorder(text_view_padding));
  timestamp_view_->SetVisible(false);
  DCHECK_EQ(kInnerHeaderHeight, timestamp_view_->GetPreferredSize().height());
  app_info_container->AddChildView(timestamp_view_);

  // Expand button view
  expand_button_ = new ExpandButton();
  SetExpanded(is_expanded_);
  expand_button_->SetBorder(views::CreateEmptyBorder(kExpandIconViewPadding));
  expand_button_->SetVerticalAlignment(views::ImageView::LEADING);
  expand_button_->SetHorizontalAlignment(views::ImageView::LEADING);
  expand_button_->SetImageSize(gfx::Size(kExpandIconSize, kExpandIconSize));
  DCHECK_EQ(kInnerHeaderHeight, expand_button_->GetPreferredSize().height());
  app_info_container->AddChildView(expand_button_);

  // Spacer between left-aligned views and right-aligned views
  views::View* spacer = new views::View;
  spacer->SetPreferredSize(gfx::Size(1, kInnerHeaderHeight));
  AddChildView(spacer);
  layout->SetFlexForView(spacer, 1);

  // Settings and close buttons view
  AddChildView(control_buttons_view);

  SetPreferredSize(gfx::Size(kNotificationWidth, kHeaderHeight));
}

void NotificationHeaderView::SetAppIcon(const gfx::ImageSkia& img) {
  app_icon_view_->SetImage(img);
}

void NotificationHeaderView::ClearAppIcon() {
  app_icon_view_->SetImage(
      gfx::CreateVectorIcon(kProductIcon, kSmallImageSizeMD, accent_color_));
}

void NotificationHeaderView::SetAppName(const base::string16& name) {
  app_name_view_->SetText(name);
}

void NotificationHeaderView::SetProgress(int progress) {
  summary_text_view_->SetText(l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_NOTIFICATION_PROGRESS_PERCENTAGE, progress));
  has_progress_ = true;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::ClearProgress() {
  has_progress_ = false;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::SetOverflowIndicator(int count) {
  if (count > 0) {
    summary_text_view_->SetText(l10n_util::GetStringFUTF16Int(
        IDS_MESSAGE_CENTER_LIST_NOTIFICATION_HEADER_OVERFLOW_INDICATOR, count));
    has_overflow_indicator_ = true;
  } else {
    has_overflow_indicator_ = false;
  }
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::ClearOverflowIndicator() {
  has_overflow_indicator_ = false;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);

  node_data->SetName(app_name_view_->text());
  node_data->SetDescription(summary_text_view_->text() +
                            base::ASCIIToUTF16(" ") + timestamp_view_->text());

  if (is_expanded_)
    node_data->AddState(ax::mojom::State::kExpanded);
}

void NotificationHeaderView::SetTimestamp(base::Time past) {
  timestamp_view_->SetText(FormatToRelativeTime(past));
  has_timestamp_ = true;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::ClearTimestamp() {
  has_timestamp_ = false;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::SetExpandButtonEnabled(bool enabled) {
  // SetInkDropMode iff. the visibility changed.
  // Otherwise, the ink drop animation cannot finish.
  if (expand_button_->visible() != enabled)
    SetInkDropMode(enabled ? InkDropMode::ON : InkDropMode::OFF);

  expand_button_->SetVisible(enabled);
}

void NotificationHeaderView::SetExpanded(bool expanded) {
  is_expanded_ = expanded;
  expand_button_->SetImage(gfx::CreateVectorIcon(
      expanded ? kNotificationExpandLessIcon : kNotificationExpandMoreIcon,
      kExpandIconSize, accent_color_));
  expand_button_->set_tooltip_text(l10n_util::GetStringUTF16(
      expanded ? IDS_MESSAGE_CENTER_COLLAPSE_NOTIFICATION
               : IDS_MESSAGE_CENTER_EXPAND_NOTIFICATION));
  NotifyAccessibilityEvent(ax::mojom::Event::kStateChanged, true);
}

void NotificationHeaderView::SetAccentColor(SkColor color) {
  accent_color_ = color;
  app_name_view_->SetEnabledColor(accent_color_);
  SetExpanded(is_expanded_);
}

bool NotificationHeaderView::IsExpandButtonEnabled() {
  return expand_button_->visible();
}

void NotificationHeaderView::SetSubpixelRenderingEnabled(bool enabled) {
  app_name_view_->SetSubpixelRenderingEnabled(enabled);
  summary_text_divider_->SetSubpixelRenderingEnabled(enabled);
  summary_text_view_->SetSubpixelRenderingEnabled(enabled);
  timestamp_divider_->SetSubpixelRenderingEnabled(enabled);
  timestamp_view_->SetSubpixelRenderingEnabled(enabled);
}

std::unique_ptr<views::InkDrop> NotificationHeaderView::CreateInkDrop() {
  return std::make_unique<views::InkDropStub>();
}

void NotificationHeaderView::UpdateSummaryTextVisibility() {
  const bool visible = has_progress_ || has_overflow_indicator_;
  summary_text_divider_->SetVisible(visible);
  summary_text_view_->SetVisible(visible);
  timestamp_divider_->SetVisible(!has_progress_ && has_timestamp_);
  timestamp_view_->SetVisible(!has_progress_ && has_timestamp_);
  Layout();
}

}  // namespace message_center
