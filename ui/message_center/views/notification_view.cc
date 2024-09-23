// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include <memory>

#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_button_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/message_center/views/notification_view_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace message_center {

namespace {

// TODO(crbug.com/40787532): Move the padding and spacing definition from
// NotificationViewBase to this class.

constexpr auto kContentRowPadding = gfx::Insets::TLBR(0, 12, 16, 12);
// TODO(tetsui): Move |kIconViewSize| to public/cpp/message_center_constants.h
// and merge with contradicting |kNotificationIconSize|.
constexpr gfx::Size kIconViewSize(36, 36);
constexpr auto kLeftContentPadding = gfx::Insets::TLBR(2, 4, 0, 4);
constexpr auto kLeftContentPaddingWithIcon = gfx::Insets::TLBR(2, 4, 0, 12);

// Minimum size of a button in the actions row.
constexpr gfx::Size kActionButtonMinSize(0, 32);

constexpr int kMessageLabelWidthWithIcon =
    kNotificationWidth - kIconViewSize.width() -
    kLeftContentPaddingWithIcon.left() - kLeftContentPaddingWithIcon.right() -
    kContentRowPadding.left() - kContentRowPadding.right();

constexpr int kMessageLabelWidth =
    kNotificationWidth - kLeftContentPadding.left() -
    kLeftContentPadding.right() - kContentRowPadding.left() -
    kContentRowPadding.right();

constexpr auto kLargeImageContainerPadding = gfx::Insets::TLBR(0, 16, 16, 16);

// Max number of lines for title_view_.
constexpr int kMaxLinesForTitleView = 1;

constexpr int kTitleCharacterLimit =
    kNotificationWidth * kMaxTitleLines / kMinPixelsPerTitleCharacter;

// "Roboto-Regular, 12sp" is specified in the mock.
constexpr int kHeaderTextFontSize = 12;

// Default paddings of the views of texts. Adjusted on Windows.
// Top: 9px = 11px (from the mock) - 2px (outer padding).
// Bottom: 6px from the mock.
constexpr auto kTextViewPaddingDefault = gfx::Insets::TLBR(9, 0, 6, 0);

constexpr auto kSettingsRowPadding = gfx::Insets::TLBR(8, 0, 0, 0);
constexpr auto kSettingsRadioButtonPadding = gfx::Insets::VH(14, 18);
constexpr gfx::Insets kSettingsButtonRowPadding(8);

gfx::FontList GetHeaderTextFontList() {
  gfx::Font default_font;
  int font_size_delta = kHeaderTextFontSize - default_font.GetFontSize();
  const gfx::Font& font = default_font.Derive(
      font_size_delta, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  DCHECK_EQ(kHeaderTextFontSize, font.GetFontSize());
  return gfx::FontList(font);
}

gfx::Insets CalculateTopPadding(int font_list_height) {
#if BUILDFLAG(IS_WIN)
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
    return kTextViewPaddingDefault - gfx::Insets::TLBR(1, 0, 0, 0);
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
  METADATA_HEADER(NotificationTextButton, views::MdTextButton)

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

  void SetEnabledTextColors(std::optional<SkColor> color) override {
    color_ = std::move(color);
    views::MdTextButton::SetEnabledTextColors(color_);
    label()->SetAutoColorReadabilityEnabled(true);
  }

  std::optional<SkColor> color() const { return color_; }

 private:
  std::optional<SkColor> color_;
};

BEGIN_METADATA(NotificationTextButton)
END_METADATA

// InlineSettingsRadioButton ///////////////////////////////////////////////////

class InlineSettingsRadioButton : public views::RadioButton {
  METADATA_HEADER(InlineSettingsRadioButton, views::RadioButton)

 public:
  explicit InlineSettingsRadioButton(const std::u16string& label_text)
      : views::RadioButton(label_text, 1 /* group */) {
    label()->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
    label()->SetSubpixelRenderingEnabled(false);
  }

  void OnThemeChanged() override {
    RadioButton::OnThemeChanged();
    SetEnabledTextColors(GetTextColor());
    label()->SetAutoColorReadabilityEnabled(true);
    label()->SetBackgroundColor(
        GetColorProvider()->GetColor(ui::kColorNotificationBackgroundActive));
  }

 private:
  SkColor GetTextColor() const {
    return GetColorProvider()->GetColor(ui::kColorLabelForeground);
  }
};

BEGIN_METADATA(InlineSettingsRadioButton)
END_METADATA

// NotificationInkDropImpl /////////////////////////////////////////////////////

class NotificationInkDropImpl : public views::InkDropImpl {
 public:
  NotificationInkDropImpl(views::InkDropHost* ink_drop_host,
                          const gfx::Size& host_size)
      : views::InkDropImpl(
            ink_drop_host,
            host_size,
            views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE) {}

  void HostSizeChanged(const gfx::Size& new_size) override {
    // Prevent a call to InkDropImpl::HostSizeChanged which recreates the ripple
    // and stops the currently active animation: http://crbug.com/915222.
  }
};

}  // namespace

class NotificationView::NotificationViewPathGenerator
    : public views::HighlightPathGenerator {
 public:
  explicit NotificationViewPathGenerator(gfx::Insets insets)
      : insets_(std::move(insets)) {}
  NotificationViewPathGenerator(const NotificationViewPathGenerator&) = delete;
  NotificationViewPathGenerator& operator=(
      const NotificationViewPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::RectF bounds = rect;
    if (!preferred_size_.IsEmpty())
      bounds.set_size(gfx::SizeF(preferred_size_));
    bounds.Inset(gfx::InsetsF(insets_));
    gfx::RoundedCornersF corner_radius(top_radius_, top_radius_, bottom_radius_,
                                       bottom_radius_);
    return gfx::RRectF(bounds, corner_radius);
  }

  void set_top_radius(int val) { top_radius_ = val; }
  void set_bottom_radius(int val) { bottom_radius_ = val; }
  void set_preferred_size(const gfx::Size& val) { preferred_size_ = val; }

 private:
  int top_radius_ = 0;
  int bottom_radius_ = 0;
  gfx::Insets insets_;

  // This custom PathGenerator is used for the ink drop clipping bounds. By
  // setting |preferred_size_| we set the correct clip bounds in
  // GetRoundRect(). This is needed as the correct bounds for the ink drop are
  // required before the view does layout. See http://crbug.com/915222.
  gfx::Size preferred_size_;
};

NotificationView::NotificationView(
    const message_center::Notification& notification)
    : NotificationViewBase(notification),
      ink_drop_container_(
          AddChildView(std::make_unique<views::InkDropContainerView>())) {
  // Instantiate view instances and define layout and view hierarchy.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  views::InkDrop::Install(this, std::make_unique<views::InkDropHost>(this));
  views::InkDrop::Get(this)->SetVisibleOpacity(1.0f);
  views::InkDrop::Get(this)->SetCreateInkDropCallback(base::BindRepeating(
      [](NotificationViewBase* host) -> std::unique_ptr<views::InkDrop> {
        auto ink_drop = std::make_unique<NotificationInkDropImpl>(
            views::InkDrop::Get(host), host->size());
        // This code assumes that `ink_drop`'s observer list is unchecked, and
        // that `host` outlives `ink_drop`.
        ink_drop->AddObserver(host);
        return ink_drop;
      },
      this));
  views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](NotificationViewBase* host) -> std::unique_ptr<views::InkDropRipple> {
        return std::make_unique<views::FloodFillInkDropRipple>(
            views::InkDrop::Get(host), host->GetPreferredSize({}),
            views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
            views::InkDrop::Get(host)->GetBaseColor(),
            views::InkDrop::Get(host)->GetVisibleOpacity());
      },
      this));
  views::InkDrop::Get(this)->SetBaseColorId(
      ui::kColorNotificationBackgroundActive);

  auto header_row = CreateHeaderRowBuilder().Build();
  // Font list for text views.
  const gfx::FontList& font_list = GetHeaderTextFontList();
  const int font_list_height = font_list.GetHeight();
  const gfx::Insets& text_view_padding(CalculateTopPadding(font_list_height));
  header_row->ConfigureLabelsStyle(font_list, text_view_padding, false);
  header_row->SetPreferredSize(header_row->GetPreferredSize({}) -
                               gfx::Size(GetInsets().width(), 0));
  header_row->SetCallback(base::BindRepeating(
      &NotificationView::HeaderRowPressed, base::Unretained(this)));
  header_row->AddChildView(
      CreateControlButtonsBuilder()
          .SetNotificationControlButtonFactory(
              std::make_unique<NotificationControlButtonFactory>())
          .Build());

  auto content_row = CreateContentRowBuilder()
                         .SetLayoutManager(std::make_unique<views::BoxLayout>(
                             views::BoxLayout::Orientation::kHorizontal))
                         .Build();
  auto* content_row_layout =
      static_cast<views::BoxLayout*>(content_row->GetLayoutManager());
  content_row_layout->set_inside_border_insets(kContentRowPadding);
  content_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  auto* left_content_ptr =
      content_row->AddChildView(CreateLeftContentBuilder().Build());
  static_cast<views::BoxLayout*>(content_row->GetLayoutManager())
      ->SetFlexForView(left_content_ptr, 1);
  content_row->AddChildView(CreateRightContentBuilder().Build());

  AddChildView(std::move(header_row));
  AddChildView(std::move(content_row));
  AddChildView(
      CreateImageContainerBuilder()
          .SetBorder(views::CreateEmptyBorder(kLargeImageContainerPadding))
          .Build());
  AddChildView(CreateInlineSettingsBuilder().Build());
  AddChildView(CreateSnoozeSettingsBuilder().Build());
  AddChildView(CreateActionsRow());

  CreateOrUpdateViews(notification);
  UpdateControlButtonsVisibilityWithNotification(notification);

  auto highlight_path_generator =
      std::make_unique<NotificationViewPathGenerator>(GetInsets());
  highlight_path_generator_ = highlight_path_generator.get();
  views::HighlightPathGenerator::Install(this,
                                         std::move(highlight_path_generator));
}

NotificationView::~NotificationView() {
  // InkDrop is explicitly removed as it can have `this` as an observer
  // installed. This is currently also required because
  // RemoveLayerFromRegions() gets called in the destructor of InkDrop which
  // would've called the wrong override if it destroys in a parent destructor.
  views::InkDrop::Remove(this);
}

SkColor NotificationView::GetActionButtonColorForTesting(
    views::LabelButton* action_button) {
  NotificationTextButton* button =
      static_cast<NotificationTextButton*>(action_button);
  return button->color().value_or(SkColor());
}

void NotificationView::CreateOrUpdateHeaderView(
    const Notification& notification) {
  if (!notification.rich_notification_data().ignore_accent_color_for_text) {
    header_row()->SetColor(notification.accent_color());
  }
  header_row()->SetSummaryText(std::u16string());
  NotificationViewBase::CreateOrUpdateHeaderView(notification);
}

void NotificationView::CreateOrUpdateTitleView(
    const Notification& notification) {
  if (notification.title().empty() ||
      notification.type() == NOTIFICATION_TYPE_PROGRESS) {
    if (title_view_) {
      DCHECK(left_content()->Contains(title_view_));
      left_content()->RemoveChildViewT(title_view_.get());
      title_view_ = nullptr;
    }
    return;
  }

  const std::u16string& title = gfx::TruncateString(
      notification.title(), kTitleCharacterLimit, gfx::WORD_BREAK);
  if (!title_view_) {
    auto title_view = GenerateTitleView(title);
    // TODO(crbug.com/41295639): multiline should not be required, but we need
    // to set the width of |title_view_|, which only works in multiline mode.
    title_view->SetMultiLine(true);
    title_view->SetMaxLines(kMaxLinesForTitleView);
    title_view_ = AddViewToLeftContent(std::move(title_view));
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

  // TODO(crbug.com/40541732): figure out if this has a performance impact and
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

void NotificationView::CreateOrUpdateInlineSettingsViews(
    const Notification& notification) {
  if (inline_settings_enabled()) {
    DCHECK_EQ(message_center::SettingsButtonHandler::INLINE,
              notification.rich_notification_data().settings_button_handler);
    return;
  }

  set_inline_settings_enabled(
      notification.rich_notification_data().settings_button_handler ==
      message_center::SettingsButtonHandler::INLINE);

  if (!inline_settings_enabled()) {
    return;
  }

  int block_notifications_message_id = 0;
  switch (notification.notifier_id().type) {
    case NotifierType::APPLICATION:
    case NotifierType::ARC_APPLICATION:
      block_notifications_message_id =
          IDS_MESSAGE_CENTER_BLOCK_ALL_NOTIFICATIONS_APP;
      break;
    case NotifierType::WEB_PAGE:
      block_notifications_message_id =
          IDS_MESSAGE_CENTER_BLOCK_ALL_NOTIFICATIONS_SITE;
      break;
    case NotifierType::SYSTEM_COMPONENT:
      block_notifications_message_id =
          IDS_MESSAGE_CENTER_BLOCK_ALL_NOTIFICATIONS;
      break;
    case NotifierType::CROSTINI_APPLICATION:
      [[fallthrough]];
    // PhoneHub notifications do not have inline settings.
    case NotifierType::PHONE_HUB:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  DCHECK_NE(block_notifications_message_id, 0);

  inline_settings_row()->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  inline_settings_row()->SetInsideBorderInsets(kSettingsRowPadding);

  auto block_all_button = std::make_unique<InlineSettingsRadioButton>(
      l10n_util::GetStringUTF16(block_notifications_message_id));
  block_all_button->SetBorder(
      views::CreateEmptyBorder(kSettingsRadioButtonPadding));
  block_all_button_ =
      inline_settings_row()->AddChildView(std::move(block_all_button));

  auto dont_block_button = std::make_unique<InlineSettingsRadioButton>(
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_DONT_BLOCK_NOTIFICATIONS));
  dont_block_button->SetBorder(
      views::CreateEmptyBorder(kSettingsRadioButtonPadding));
  dont_block_button_ =
      inline_settings_row()->AddChildView(std::move(dont_block_button));

  inline_settings_row()->SetVisible(false);

  auto settings_done_button = GenerateNotificationLabelButton(
      base::BindRepeating(&NotificationView::ToggleInlineSettings,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS_DONE));

  auto settings_button_row = std::make_unique<views::View>();
  auto settings_button_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kSettingsButtonRowPadding, 0);
  settings_button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  settings_button_row->SetLayoutManager(std::move(settings_button_layout));
  settings_done_button_ =
      settings_button_row->AddChildView(std::move(settings_done_button));
  inline_settings_row()->AddChildView(std::move(settings_button_row));
}

void NotificationView::CreateOrUpdateSnoozeSettingsViews(
    const Notification& notification) {
  // Not implemented by default.
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
  const int message_label_width =
      (IsIconViewShown() ? kMessageLabelWidthWithIcon : kMessageLabelWidth) -
      GetInsets().width();
  if (title_view_)
    title_view_->SizeToFit(message_label_width);
  if (message_label()) {
    message_label()->SetMultiLine(true);
    message_label()->SetMaxLines(expanded ? kMaxLinesForExpandedMessageLabel
                                          : kMaxLinesForMessageLabel);
    message_label()->SizeToFit(message_label_width);
  }
  NotificationViewBase::UpdateViewForExpandedState(expanded);
}

gfx::Size NotificationView::GetIconViewSize() const {
  return kIconViewSize;
}

int NotificationView::GetLargeImageViewMaxWidth() const {
  return kNotificationWidth - kLargeImageContainerPadding.width() -
         GetInsets().width();
}

void NotificationView::OnThemeChanged() {
  MessageView::OnThemeChanged();
  UpdateHeaderViewBackgroundColor();
  UpdateActionButtonsRowBackground();
}

void NotificationView::UpdateCornerRadius(int top_radius, int bottom_radius) {
  NotificationViewBase::UpdateCornerRadius(top_radius, bottom_radius);
  UpdateActionButtonsRowBackground();
  highlight_path_generator_->set_top_radius(top_radius);
  highlight_path_generator_->set_bottom_radius(bottom_radius);
}

void NotificationView::ToggleInlineSettings(const ui::Event& event) {
  if (!inline_settings_enabled())
    return;

  bool inline_settings_visible = !inline_settings_row()->GetVisible();

  // TODO(crbug.com/40781007): In later refactor, `block_all_button_` and
  // `dont_block_button_` should be moved from NotificationViewBase to this
  // class, since AshNotificationView will use a different UI for inline
  // settings.
  bool disable_notification =
      !inline_settings_visible && block_all_button_->GetChecked();

  content_row()->SetVisible(!inline_settings_visible);

  // Always check "Don't block" when inline settings is shown.
  // If it's already blocked, users should not see inline settings.
  // Toggling should reset the state.
  dont_block_button_->SetChecked(true);

  NotificationViewBase::ToggleInlineSettings(event);
  PreferredSizeChanged();

  if (inline_settings_row()->GetVisible())
    AddBackgroundAnimation(event);
  else
    RemoveBackgroundAnimation();

  UpdateHeaderViewBackgroundColor();
  DeprecatedLayoutImmediately();
  SchedulePaint();

  // Call DisableNotification() at the end, because |this| can be deleted at any
  // point after it's called.
  if (disable_notification)
    MessageCenter::Get()->DisableNotification(notification_id());
}

void NotificationView::ToggleSnoozeSettings(const ui::Event& event) {
  // Not implemented by default.
}

bool NotificationView::IsExpandable() const {
  // Inline settings can not be expanded.
  if (GetMode() == Mode::SETTING)
    return false;

  // Expandable if the message exceeds one line.
  if (message_label() && message_label()->GetVisible() &&
      message_label()->GetRequiredLines() > 1) {
    return true;
  }
  // Expandable if there is at least one inline action.
  if (!action_buttons_row()->children().empty())
    return true;

  // Expandable if the notification has image.
  if (!image_container_view()->children().empty())
    return true;

  // Expandable if there are multiple list items.
  if (item_views().size() > 1)
    return true;

  // Expandable if both progress bar and status message exist.
  if (status_view())
    return true;

  return false;
}

void NotificationView::AddLayerToRegion(ui::Layer* layer,
                                        views::LayerRegion region) {
  for (auto* child : GetChildrenForLayerAdjustment()) {
    child->SetPaintToLayer();
    child->layer()->SetFillsBoundsOpaquely(false);
  }
  ink_drop_container_->AddLayerToRegion(layer, region);
}

void NotificationView::RemoveLayerFromRegions(ui::Layer* layer) {
  ink_drop_container_->RemoveLayerFromRegions(layer);
  for (auto* child : GetChildrenForLayerAdjustment())
    child->DestroyLayer();
}

void NotificationView::Layout(PassKey) {
  LayoutSuperclass<NotificationViewBase>(this);

  // We need to call IsExpandable() after doing superclass layout, since whether
  // we should show expand button or not depends on the current view layout.
  // (e.g. Show expand button when `message_label_` exceeds one line.)
  SetExpandButtonVisibility(IsExpandable());
  header_row()->DeprecatedLayoutImmediately();

  // The notification background is rounded in MessageView layout, but we also
  // have to round the actions row background here.
  if (actions_row()->GetVisible()) {
    constexpr SkScalar kCornerRadius = SkIntToScalar(kNotificationCornerRadius);

    // Use vertically larger clip path, so that actions row's top corners will
    // not be rounded.
    SkPath path;
    gfx::Rect bounds = actions_row()->GetLocalBounds();
    bounds.set_y(bounds.y() - bounds.height());
    bounds.set_height(bounds.height() * 2);
    path.addRoundRect(gfx::RectToSkRect(bounds), kCornerRadius, kCornerRadius);

    action_buttons_row()->SetClipPath(path);

    if (inline_reply()) {
      inline_reply()->SetClipPath(path);
    }
  }

  // The animation is needed to run inside of the border.
  ink_drop_container_->SetBoundsRect(GetLocalBounds());
}

void NotificationView::PreferredSizeChanged() {
  highlight_path_generator_->set_preferred_size(GetPreferredSize({}));
  MessageView::PreferredSizeChanged();
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

std::vector<views::View*> NotificationView::GetChildrenForLayerAdjustment() {
  return {header_row(), block_all_button_, dont_block_button_,
          settings_done_button_};
}

void NotificationView::HeaderRowPressed() {
  if (!IsExpandable() || !content_row()->GetVisible())
    return;

  const bool target_expanded_state = !IsExpanded();

  // Tapping anywhere on |header_row_| can expand the notification, though only
  // |expand_button| can be focused by TAB.
  SetManuallyExpandedOrCollapsed(
      target_expanded_state ? message_center::ExpandState::USER_EXPANDED
                            : message_center::ExpandState::USER_COLLAPSED);
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  SetExpanded(target_expanded_state);
  // Check |this| is valid before continuing, because ToggleExpanded() might
  // cause |this| to be deleted.
  if (!weak_ptr)
    return;
  DeprecatedLayoutImmediately();
  SchedulePaint();
}

BEGIN_METADATA(NotificationView)
END_METADATA

}  // namespace message_center
