// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_header_view.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/relative_time_formatter.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
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
constexpr int kHeaderHeightInAsh = 26;

// The padding between controls in the header.
constexpr auto kHeaderSpacing = gfx::Insets::TLBR(0, 2, 0, 2);

// The padding outer the header and the control buttons.
constexpr auto kHeaderOuterPadding = gfx::Insets::TLBR(2, 2, 0, 2);

constexpr int kInnerHeaderHeight = kHeaderHeight - kHeaderOuterPadding.height();

// Paddings of the app icon (small image).
// Top: 8px = 10px (from the mock) - 2px (outer padding).
// Bottom: 4px from the mock.
// Right: 4px = 6px (from the mock) - kHeaderHorizontalSpacing.
constexpr auto kAppIconPadding = gfx::Insets::TLBR(8, 14, 4, 4);

// Size of the expand icon. 8px = 32px - 15px - 9px (values from the mock).
constexpr int kExpandIconSize = 8;
// Paddings of the expand buttons.
// Top: 13px = 15px (from the mock) - 2px (outer padding).
// Bottom: 9px from the mock.
constexpr auto kExpandIconViewPadding = gfx::Insets::TLBR(13, 2, 9, 0);

// Bullet character. The divider symbol between different parts of the header.
constexpr char16_t kNotificationHeaderDivider[] = u" \u2022 ";

// Minimum spacing before the control buttons.
constexpr int kControlButtonSpacing = 16;

void ConfigureLabel(views::Label* label,
                    const gfx::FontList& font_list,
                    const gfx::Insets& text_view_padding,
                    bool auto_color_readability) {
  if (auto_color_readability)
    label->SetAutoColorReadabilityEnabled(false);
  const int font_list_height = font_list.GetHeight();
  label->SetFontList(font_list);
  label->SetLineHeight(font_list_height);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetBorder(views::CreateEmptyBorder(text_view_padding));
}

// ExpandButton forwards all mouse and key events to NotificationHeaderView, but
// takes tab focus for accessibility purpose.
class ExpandButton : public views::ImageView {
  METADATA_HEADER(ExpandButton, views::ImageView)

 public:
  ExpandButton();
  ~ExpandButton() override;

  // Overridden from views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnThemeChanged() override;
  void SetTooltipText(const std::u16string& tooltip) override;

 private:
  std::unique_ptr<views::Painter> focus_painter_;
};

ExpandButton::ExpandButton() {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  if (GetTooltipText().empty()) {
    GetViewAccessibility().SetName(
        GetTooltipText(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }
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

void ExpandButton::OnThemeChanged() {
  ImageView::OnThemeChanged();
  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      GetColorProvider()->GetColor(ui::kColorFocusableBorderFocused),
      gfx::Insets::TLBR(0, 0, 1, 1));
}

void ExpandButton::SetTooltipText(const std::u16string& tooltip) {
  views::ImageView::SetTooltipText(tooltip);

  if (GetTooltipText().empty()) {
    GetViewAccessibility().SetName(
        GetTooltipText(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }
}

BEGIN_METADATA(ExpandButton)
END_METADATA

}  // namespace

NotificationHeaderView::NotificationHeaderView(PressedCallback callback)
    : views::Button(std::move(callback)) {
  const views::FlexSpecification kAppNameFlex =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1);

  const views::FlexSpecification kSpacerFlex =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2);

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetDefault(views::kMarginsKey, kHeaderSpacing);
  layout->SetInteriorMargin(kHeaderOuterPadding);
  layout->SetCollapseMargins(true);

  // TODO(crbug/1241602): std::unique_ptr can be used in multiple places here.
  // Also, consider using views::Builder<T>.

  // App icon view
  app_icon_view_ = new views::ImageView();
  app_icon_view_->SetImageSize(gfx::Size(kSmallImageSizeMD, kSmallImageSizeMD));
  app_icon_view_->SetBorder(views::CreateEmptyBorder(kAppIconPadding));
  app_icon_view_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  app_icon_view_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  DCHECK_EQ(kInnerHeaderHeight, app_icon_view_->GetPreferredSize({}).height());
  AddChildView(app_icon_view_.get());

  // App name view
  auto app_name_view = std::make_unique<views::Label>();
  // Explicitly disable multiline to support proper text elision for URLs.
  app_name_view->SetMultiLine(false);
  app_name_view->SetProperty(views::kFlexBehaviorKey, kAppNameFlex);
  app_name_view->SetID(NotificationView::kAppNameView);
  app_name_view_ = AddChildView(std::move(app_name_view));

  // Detail views which will be hidden in settings mode.
  detail_views_ = new views::View();
  auto* detail_layout =
      detail_views_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  detail_layout->SetCollapseMargins(true);
  detail_layout->SetDefault(views::kMarginsKey, kHeaderSpacing);
  detail_views_->SetID(NotificationView::kHeaderDetailViews);
  AddChildView(detail_views_.get());

  // Summary text divider
  auto summary_text_divider = std::make_unique<views::Label>();
  summary_text_divider->SetText(kNotificationHeaderDivider);
  summary_text_divider->SetVisible(false);
  summary_text_divider_ =
      detail_views_->AddChildView(std::move(summary_text_divider));

  // Summary text view
  auto summary_text_view = std::make_unique<views::Label>();
  summary_text_view->SetVisible(false);
  summary_text_view->SetID(NotificationView::kSummaryTextView);
  summary_text_view_ =
      detail_views_->AddChildView(std::move(summary_text_view));

  // Timestamp divider
  auto timestamp_divider = std::make_unique<views::Label>();
  timestamp_divider->SetText(kNotificationHeaderDivider);
  timestamp_divider->SetVisible(false);
  timestamp_divider_ =
      detail_views_->AddChildView(std::move(timestamp_divider));

  // Timestamp view
  auto timestamp_view = std::make_unique<views::Label>();
  timestamp_view->SetVisible(false);
  timestamp_view_ = detail_views_->AddChildView(std::move(timestamp_view));

  expand_button_ = new ExpandButton();
  expand_button_->SetBorder(views::CreateEmptyBorder(kExpandIconViewPadding));
  expand_button_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  expand_button_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  expand_button_->SetImageSize(gfx::Size(kExpandIconSize, kExpandIconSize));
  DCHECK_EQ(kInnerHeaderHeight, expand_button_->GetPreferredSize({}).height());
  detail_views_->AddChildView(expand_button_.get());

  // Spacer between left-aligned views and right-aligned views
  auto spacer = std::make_unique<views::View>();
  spacer->SetPreferredSize(
      gfx::Size(kControlButtonSpacing, kInnerHeaderHeight));
  spacer->SetProperty(views::kFlexBehaviorKey, kSpacerFlex);
  spacer_ = AddChildView(std::move(spacer));

  SetPreferredSize(gfx::Size(GetNotificationWidth(), kHeaderHeight));

  // Not focusable by default, only for accessibility.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  UpdateExpandedCollapsedAccessibleState();

  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);

  if (app_name_view_->GetText().empty()) {
    GetViewAccessibility().SetName(
        app_name_view_->GetText(),
        ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  } else {
    GetViewAccessibility().SetName(app_name_view_->GetText());
  }

  OnTextChanged();
  summary_text_changed_callback_ =
      summary_text_view_->AddTextChangedCallback(base::BindRepeating(
          &NotificationHeaderView::OnTextChanged, base::Unretained(this)));
  timestamp_changed_callback_ =
      timestamp_view_->AddTextChangedCallback(base::BindRepeating(
          &NotificationHeaderView::OnTextChanged, base::Unretained(this)));
}

NotificationHeaderView::~NotificationHeaderView() = default;

void NotificationHeaderView::ConfigureLabelsStyle(
    const gfx::FontList& font_list,
    const gfx::Insets& text_view_padding,
    bool auto_color_readability) {
  ConfigureLabel(app_name_view_, font_list, text_view_padding,
                 auto_color_readability);
  ConfigureLabel(summary_text_view_, font_list, text_view_padding,
                 auto_color_readability);
  ConfigureLabel(summary_text_divider_, font_list, text_view_padding,
                 auto_color_readability);
  ConfigureLabel(timestamp_divider_, font_list, text_view_padding,
                 auto_color_readability);
  ConfigureLabel(timestamp_view_, font_list, text_view_padding,
                 auto_color_readability);
}

void NotificationHeaderView::SetAppIcon(const gfx::ImageSkia& img) {
  app_icon_view_->SetImage(ui::ImageModel::FromImageSkia(img));
  using_default_app_icon_ = false;
}

void NotificationHeaderView::ClearAppIcon() {
  using_default_app_icon_ = true;
  UpdateColors();
}

void NotificationHeaderView::SetAppName(const std::u16string& name) {
  app_name_view_->SetText(name);
  if (name.empty()) {
    GetViewAccessibility().SetName(
        name, ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  } else {
    GetViewAccessibility().SetName(name);
  }
}

void NotificationHeaderView::SetAppNameElideBehavior(
    gfx::ElideBehavior elide_behavior) {
  app_name_view_->SetElideBehavior(elide_behavior);
}

void NotificationHeaderView::SetProgress(int progress) {
  summary_text_view_->SetText(l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_NOTIFICATION_PROGRESS_PERCENTAGE, progress));
  has_progress_ = true;
  UpdateSummaryTextAndTimestampVisibility();
}

void NotificationHeaderView::SetSummaryText(const std::u16string& text) {
  summary_text_view_->SetText(text);
  has_progress_ = false;
  UpdateSummaryTextAndTimestampVisibility();
}

void NotificationHeaderView::SetOverflowIndicator(int count) {
  summary_text_view_->SetText(l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_LIST_NOTIFICATION_HEADER_OVERFLOW_INDICATOR, count));
  has_progress_ = false;
  UpdateSummaryTextAndTimestampVisibility();
}

void NotificationHeaderView::OnThemeChanged() {
  Button::OnThemeChanged();
  UpdateColors();
}

void NotificationHeaderView::SetTimestamp(base::Time timestamp) {
  std::u16string relative_time;
  base::TimeDelta next_update;
  GetRelativeTimeStringAndNextUpdateTime(timestamp - base::Time::Now(),
                                         &relative_time, &next_update);

  timestamp_view_->SetText(relative_time);
  timestamp_ = timestamp;
  UpdateSummaryTextAndTimestampVisibility();

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

  UpdateSummaryTextAndTimestampVisibility();
}

void NotificationHeaderView::SetExpandButtonEnabled(bool enabled) {
  // We shouldn't execute this method if the expand button is not here.
  DCHECK(expand_button_);
  expand_button_->SetVisible(enabled);
  UpdateExpandedCollapsedAccessibleState();
}

void NotificationHeaderView::SetExpanded(bool expanded) {
  // We shouldn't execute this method if the expand button is not here.
  DCHECK(expand_button_);
  is_expanded_ = expanded;
  UpdateColors();
  expand_button_->SetTooltipText(l10n_util::GetStringUTF16(
      expanded ? IDS_MESSAGE_CENTER_COLLAPSE_NOTIFICATION
               : IDS_MESSAGE_CENTER_EXPAND_NOTIFICATION));

  UpdateExpandedCollapsedAccessibleState();
}

void NotificationHeaderView::SetColor(std::optional<SkColor> color) {
  color_ = std::move(color);
  UpdateColors();
}

void NotificationHeaderView::SetBackgroundColor(SkColor color) {
  app_name_view_->SetBackgroundColor(color);
  summary_text_divider_->SetBackgroundColor(color);
  summary_text_view_->SetBackgroundColor(color);
  timestamp_divider_->SetBackgroundColor(color);
  timestamp_view_->SetBackgroundColor(color);
  UpdateColors();
}

void NotificationHeaderView::SetSubpixelRenderingEnabled(bool enabled) {
  app_name_view_->SetSubpixelRenderingEnabled(enabled);
  summary_text_divider_->SetSubpixelRenderingEnabled(enabled);
  summary_text_view_->SetSubpixelRenderingEnabled(enabled);
  timestamp_divider_->SetSubpixelRenderingEnabled(enabled);
  timestamp_view_->SetSubpixelRenderingEnabled(enabled);
}

void NotificationHeaderView::SetAppIconVisible(bool visible) {
  app_icon_view_->SetVisible(visible);
}

void NotificationHeaderView::SetTimestampVisible(bool visible) {
  timestamp_divider_->SetVisible(!is_in_group_child_notification_ && visible);
  timestamp_view_->SetVisible(visible);
}

void NotificationHeaderView::SetIsInAshNotificationView(
    bool is_in_ash_notification) {
  is_in_ash_notification_ = is_in_ash_notification;
  app_icon_view_->SetVisible(!is_in_ash_notification_);
  expand_button_->SetVisible(!is_in_ash_notification_);

  // HeaderView size is different for ash notifications.
  spacer_->SetPreferredSize(
      gfx::Size(kControlButtonSpacing,
                kHeaderHeightInAsh - kHeaderOuterPadding.height()));
  SetPreferredSize(gfx::Size(GetNotificationWidth(), kHeaderHeightInAsh));
}

void NotificationHeaderView::SetIsInGroupChildNotification(
    bool is_in_group_child_notification) {
  if (is_in_group_child_notification_ == is_in_group_child_notification)
    return;
  is_in_group_child_notification_ = is_in_group_child_notification;

  app_name_view_->SetVisible(!is_in_group_child_notification_);
  app_icon_view_->SetVisible(!is_in_ash_notification_ &&
                             !is_in_group_child_notification_);
  expand_button_->SetVisible(!is_in_ash_notification_ &&
                             !is_in_group_child_notification_);
  UpdateSummaryTextAndTimestampVisibility();
}

const std::u16string& NotificationHeaderView::app_name_for_testing() const {
  return app_name_view_->GetText();
}

gfx::ImageSkia NotificationHeaderView::app_icon_for_testing() const {
  return app_icon_view_->GetImage();
}

void NotificationHeaderView::UpdateSummaryTextAndTimestampVisibility() {
  const bool summary_visible = !is_in_group_child_notification_ &&
                               !summary_text_view_->GetText().empty();
  summary_text_divider_->SetVisible(summary_visible);
  summary_text_view_->SetVisible(summary_visible);

  const bool timestamp_visible = !has_progress_ && timestamp_;
  SetTimestampVisible(timestamp_visible);

  // TODO(crbug.com/40639286): this should not be necessary.
  detail_views_->InvalidateLayout();
}

void NotificationHeaderView::UpdateColors() {
  if (!GetWidget()) {
    // Return early here since GetColorProvider() depends on the widget.
    return;
  }

  SkColor color = color_.value_or(
      GetColorProvider()->GetColor(ui::kColorNotificationHeaderForeground));

  app_name_view_->SetEnabledColor(color);
  summary_text_view_->SetEnabledColor(color);
  summary_text_divider_->SetEnabledColor(color);
  if (is_in_ash_notification_) {
    timestamp_divider_->SetEnabledColor(color);
    timestamp_view_->SetEnabledColor(color);
  }

  // Get actual color based on readablility requirements.
  SkColor actual_color = app_name_view_->GetEnabledColor();

  if (expand_button_) {
    expand_button_->SetImage(ui::ImageModel::FromVectorIcon(
        is_expanded_ ? kNotificationExpandLessIcon
                     : kNotificationExpandMoreIcon,
        actual_color, kExpandIconSize));
  }

  if (using_default_app_icon_ && app_icon_view_) {
    app_icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        kProductIcon, actual_color, kSmallImageSizeMD));
  }
}

void NotificationHeaderView::UpdateExpandedCollapsedAccessibleState() const {
  if (!expand_button_ || !expand_button_->GetVisible()) {
    GetViewAccessibility().RemoveExpandCollapseState();
    return;
  }

  if (is_expanded_) {
    GetViewAccessibility().SetIsExpanded();
  } else {
    GetViewAccessibility().SetIsCollapsed();
  }
}

void NotificationHeaderView::OnTextChanged() {
  GetViewAccessibility().SetDescription(summary_text_view_->GetText() + u" " +
                                        timestamp_view_->GetText());
}

BEGIN_METADATA(NotificationHeaderView)
END_METADATA

}  // namespace message_center
