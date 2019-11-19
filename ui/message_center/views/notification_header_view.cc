// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_header_view.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/relative_time_formatter.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"

namespace message_center {

namespace {

constexpr int kHeaderHeight = 32;

// The padding between controls in the header.
constexpr gfx::Insets kHeaderSpacing(0, 2, 0, 2);

// The padding outer the header and the control buttons.
constexpr gfx::Insets kHeaderOuterPadding(2, 2, 0, 2);

constexpr int kInnerHeaderHeight = kHeaderHeight - kHeaderOuterPadding.height();

// Default paddings of the views of texts. Adjusted on Windows.
// Top: 9px = 11px (from the mock) - 2px (outer padding).
// Buttom: 6px from the mock.
constexpr gfx::Insets kTextViewPaddingDefault(9, 0, 6, 0);

// Paddings of the app icon (small image).
// Top: 8px = 10px (from the mock) - 2px (outer padding).
// Bottom: 4px from the mock.
// Right: 4px = 6px (from the mock) - kHeaderHorizontalSpacing.
constexpr gfx::Insets kAppIconPadding(8, 14, 4, 4);

// Size of the expand icon. 8px = 32px - 15px - 9px (values from the mock).
constexpr int kExpandIconSize = 8;
// Paddings of the expand buttons.
// Top: 13px = 15px (from the mock) - 2px (outer padding).
// Bottom: 9px from the mock.
constexpr gfx::Insets kExpandIconViewPadding(13, 2, 9, 0);

// Bullet character. The divider symbol between different parts of the header.
constexpr wchar_t kNotificationHeaderDivider[] = L" \u2022 ";

// "Roboto-Regular, 12sp" is specified in the mock.
constexpr int kHeaderTextFontSize = 12;

// Minimum spacing before the control buttons.
constexpr int kControlButtonSpacing = 16;

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
  node_data->SetName(GetTooltipText(gfx::Point()));
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

NotificationHeaderView::NotificationHeaderView(views::ButtonListener* listener)
    : views::Button(listener) {
  const views::FlexSpecification kAppNameFlex =
      views::FlexSpecification::ForSizeRule(
          views::MinimumFlexSizeRule::kScaleToZero,
          views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1);

  const views::FlexSpecification kSpacerFlex =
      views::FlexSpecification::ForSizeRule(
          views::MinimumFlexSizeRule::kScaleToMinimum,
          views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2);

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetDefault(views::kMarginsKey, kHeaderSpacing);
  layout->SetInteriorMargin(kHeaderOuterPadding);
  layout->SetCollapseMargins(true);

  // App icon view
  app_icon_view_ = new views::ImageView();
  app_icon_view_->SetImageSize(gfx::Size(kSmallImageSizeMD, kSmallImageSizeMD));
  app_icon_view_->SetBorder(views::CreateEmptyBorder(kAppIconPadding));
  app_icon_view_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  app_icon_view_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  DCHECK_EQ(kInnerHeaderHeight, app_icon_view_->GetPreferredSize().height());
  AddChildView(app_icon_view_);

  // Font list for text views.
  gfx::FontList font_list = GetHeaderTextFontList();
  const int font_list_height = font_list.GetHeight();
  gfx::Insets text_view_padding(CalculateTopPadding(font_list_height));

  auto create_label = [&font_list, font_list_height, text_view_padding]() {
    auto* label = new views::Label();
    label->SetFontList(font_list);
    label->SetLineHeight(font_list_height);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetBorder(views::CreateEmptyBorder(text_view_padding));
    DCHECK_EQ(kInnerHeaderHeight, label->GetPreferredSize().height());
    return label;
  };

  // App name view
  app_name_view_ = create_label();
  // Explicitly disable multiline to support proper text elision for URLs.
  app_name_view_->SetMultiLine(false);
  app_name_view_->SetProperty(views::kFlexBehaviorKey, kAppNameFlex);
  AddChildView(app_name_view_);

  // Detail views which will be hidden in settings mode.
  detail_views_ = new views::View();
  auto* detail_layout =
      detail_views_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  detail_layout->SetCollapseMargins(true);
  detail_layout->SetDefault(views::kMarginsKey, kHeaderSpacing);
  AddChildView(detail_views_);

  // Summary text divider
  summary_text_divider_ = create_label();
  summary_text_divider_->SetText(base::WideToUTF16(kNotificationHeaderDivider));
  summary_text_divider_->SetVisible(false);
  detail_views_->AddChildView(summary_text_divider_);

  // Summary text view
  summary_text_view_ = create_label();
  summary_text_view_->SetVisible(false);
  detail_views_->AddChildView(summary_text_view_);

  // Timestamp divider
  timestamp_divider_ = create_label();
  timestamp_divider_->SetText(base::WideToUTF16(kNotificationHeaderDivider));
  timestamp_divider_->SetVisible(false);
  detail_views_->AddChildView(timestamp_divider_);

  // Timestamp view
  timestamp_view_ = create_label();
  timestamp_view_->SetVisible(false);
  detail_views_->AddChildView(timestamp_view_);

  // Expand button view
  expand_button_ = new ExpandButton();
  expand_button_->SetBorder(views::CreateEmptyBorder(kExpandIconViewPadding));
  expand_button_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  expand_button_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  expand_button_->SetImageSize(gfx::Size(kExpandIconSize, kExpandIconSize));
  DCHECK_EQ(kInnerHeaderHeight, expand_button_->GetPreferredSize().height());
  detail_views_->AddChildView(expand_button_);

  // Spacer between left-aligned views and right-aligned views
  views::View* spacer = new views::View;
  spacer->SetPreferredSize(
      gfx::Size(kControlButtonSpacing, kInnerHeaderHeight));
  spacer->SetProperty(views::kFlexBehaviorKey, kSpacerFlex);
  AddChildView(spacer);

  SetAccentColor(accent_color_);
  SetPreferredSize(gfx::Size(kNotificationWidth, kHeaderHeight));
}

NotificationHeaderView::~NotificationHeaderView() = default;

void NotificationHeaderView::SetAppIcon(const gfx::ImageSkia& img) {
  app_icon_view_->SetImage(img);
  using_default_app_icon_ = false;
}

void NotificationHeaderView::ClearAppIcon() {
  app_icon_view_->SetImage(
      gfx::CreateVectorIcon(kProductIcon, kSmallImageSizeMD, accent_color_));
  using_default_app_icon_ = true;
}

void NotificationHeaderView::SetAppName(const base::string16& name) {
  app_name_view_->SetText(name);
}

void NotificationHeaderView::SetAppNameElideBehavior(
    gfx::ElideBehavior elide_behavior) {
  app_name_view_->SetElideBehavior(elide_behavior);
}

void NotificationHeaderView::SetProgress(int progress) {
  summary_text_view_->SetText(l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_NOTIFICATION_PROGRESS_PERCENTAGE, progress));
  has_progress_ = true;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::SetSummaryText(const base::string16& text) {
  DCHECK(!has_progress_);
  summary_text_view_->SetText(text);
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::ClearProgress() {
  summary_text_view_->SetText(base::string16());
  has_progress_ = false;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::SetOverflowIndicator(int count) {
  if (count > 0) {
    summary_text_view_->SetText(l10n_util::GetStringFUTF16Int(
        IDS_MESSAGE_CENTER_LIST_NOTIFICATION_HEADER_OVERFLOW_INDICATOR, count));
  } else {
    summary_text_view_->SetText(base::string16());
  }

  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);

  node_data->SetName(app_name_view_->GetText());
  node_data->SetDescription(summary_text_view_->GetText() +
                            base::ASCIIToUTF16(" ") +
                            timestamp_view_->GetText());

  if (is_expanded_)
    node_data->AddState(ax::mojom::State::kExpanded);
}

void NotificationHeaderView::SetTimestamp(base::Time timestamp) {
  base::string16 relative_time;
  base::TimeDelta next_update;
  GetRelativeTimeStringAndNextUpdateTime(timestamp - base::Time::Now(),
                                         &relative_time, &next_update);

  timestamp_view_->SetText(relative_time);
  timestamp_ = timestamp;
  UpdateSummaryTextVisibility();

  // Unretained is safe as the timer cancels the task on destruction.
  timestamp_update_timer_.Start(
      FROM_HERE, next_update,
      base::BindOnce(&NotificationHeaderView::SetTimestamp,
                     base::Unretained(this), timestamp));
}

void NotificationHeaderView::SetDetailViewsVisible(bool visible) {
  detail_views_->SetVisible(visible);

  if (visible && timestamp_)
    SetTimestamp(timestamp_.value());
  else
    timestamp_update_timer_.Stop();

  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::SetExpandButtonEnabled(bool enabled) {
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
  summary_text_view_->SetEnabledColor(accent_color_);
  summary_text_divider_->SetEnabledColor(accent_color_);
  SetExpanded(is_expanded_);

  // If we are using the default app icon we should clear it so we refresh it
  // with the new accent color.
  if (using_default_app_icon_)
    ClearAppIcon();
}

void NotificationHeaderView::SetBackgroundColor(SkColor color) {
  app_name_view_->SetBackgroundColor(color);
  summary_text_divider_->SetBackgroundColor(color);
  summary_text_view_->SetBackgroundColor(color);
  timestamp_divider_->SetBackgroundColor(color);
  timestamp_view_->SetBackgroundColor(color);
}

void NotificationHeaderView::SetSubpixelRenderingEnabled(bool enabled) {
  app_name_view_->SetSubpixelRenderingEnabled(enabled);
  summary_text_divider_->SetSubpixelRenderingEnabled(enabled);
  summary_text_view_->SetSubpixelRenderingEnabled(enabled);
  timestamp_divider_->SetSubpixelRenderingEnabled(enabled);
  timestamp_view_->SetSubpixelRenderingEnabled(enabled);
}

void NotificationHeaderView::HideAppIcon() {
  app_icon_view_->SetVisible(false);
}

const base::string16& NotificationHeaderView::app_name_for_testing() const {
  return app_name_view_->GetText();
}

const gfx::ImageSkia& NotificationHeaderView::app_icon_for_testing() const {
  return app_icon_view_->GetImage();
}

const base::string16& NotificationHeaderView::timestamp_for_testing() const {
  return timestamp_view_->GetText();
}

void NotificationHeaderView::UpdateSummaryTextVisibility() {
  const bool summary_visible = !summary_text_view_->GetText().empty();
  summary_text_divider_->SetVisible(summary_visible);
  summary_text_view_->SetVisible(summary_visible);

  const bool timestamp_visible = !has_progress_ && timestamp_;
  timestamp_divider_->SetVisible(timestamp_visible);
  timestamp_view_->SetVisible(timestamp_visible);

  // TODO(crbug.com/991492): this should not be necessary.
  detail_views_->InvalidateLayout();
}

}  // namespace message_center
