// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view_md.h"

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/class_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/native_cursor.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace message_center {

namespace {

// Dimensions.
constexpr gfx::Insets kContentRowPadding(0, 12, 16, 12);
constexpr gfx::Insets kActionsRowPadding(8, 8, 8, 8);
constexpr int kActionsRowHorizontalSpacing = 8;
constexpr gfx::Insets kStatusTextPadding(4, 0, 0, 0);
constexpr gfx::Size kActionButtonMinSize(0, 32);
// TODO(tetsui): Move |kIconViewSize| to public/cpp/message_center_constants.h
// and merge with contradicting |kNotificationIconSize|.
constexpr gfx::Size kIconViewSize(36, 36);
constexpr gfx::Insets kLargeImageContainerPadding(0, 16, 16, 16);
constexpr int kLargeImageMaxHeight = 218;
constexpr gfx::Insets kLeftContentPadding(2, 4, 0, 4);
constexpr gfx::Insets kLeftContentPaddingWithIcon(2, 4, 0, 12);
constexpr gfx::Insets kInputTextfieldPadding(16, 16, 16, 0);
constexpr gfx::Insets kInputReplyButtonPadding(0, 14, 0, 14);
constexpr gfx::Insets kSettingsRowPadding(8, 0, 0, 0);
constexpr gfx::Insets kSettingsRadioButtonPadding(14, 18, 14, 18);
constexpr gfx::Insets kSettingsButtonRowPadding(8);

// The icon size of inline reply input field.
constexpr int kInputReplyButtonSize = 20;

// Max number of lines for title_view_.
constexpr int kMaxLinesForTitleView = 1;
// Max number of lines for message_view_.
constexpr int kMaxLinesForMessageView = 1;
constexpr int kMaxLinesForExpandedMessageView = 4;

constexpr int kCompactTitleMessageViewSpacing = 12;

constexpr int kProgressBarHeight = 4;

constexpr int kMessageViewWidthWithIcon =
    kNotificationWidth - kIconViewSize.width() -
    kLeftContentPaddingWithIcon.left() - kLeftContentPaddingWithIcon.right() -
    kContentRowPadding.left() - kContentRowPadding.right();

constexpr int kMessageViewWidth =
    kNotificationWidth - kLeftContentPadding.left() -
    kLeftContentPadding.right() - kContentRowPadding.left() -
    kContentRowPadding.right();

const int kMinPixelsPerTitleCharacterMD = 4;

// Character limit = pixels per line * line limit / min. pixels per character.
constexpr size_t kMessageCharacterLimitMD =
    kNotificationWidth * kMessageExpandedLineLimit / 3;

// The default is 12, so this normally come out to 13.
constexpr int kTextFontSizeDelta = 1;

// In progress notification, if both the title and the message are long, the
// message would be prioritized and the title would be elided.
// However, it is not preferable that we completely omit the title, so
// the ratio of the message width is limited to this value.
constexpr double kProgressNotificationMessageRatio = 0.7;

// Line height of title and message views.
constexpr int kLineHeightMD = 17;

// This key/property allows tagging the textfield with its index.
DEFINE_UI_CLASS_PROPERTY_KEY(int, kTextfieldIndexKey, 0U)

// FontList for the texts except for the header.
gfx::FontList GetTextFontList() {
  gfx::Font default_font;
  gfx::Font font = default_font.Derive(kTextFontSizeDelta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::NORMAL);
  return gfx::FontList(font);
}

class ClickActivator : public ui::EventHandler {
 public:
  explicit ClickActivator(NotificationViewMD* owner) : owner_(owner) {}
  ~ClickActivator() override = default;

 private:
  // ui::EventHandler
  void OnEvent(ui::Event* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_GESTURE_TAP) {
      owner_->Activate();
    }
  }

  NotificationViewMD* const owner_;

  DISALLOW_COPY_AND_ASSIGN(ClickActivator);
};

// Creates a view responsible for drawing each list notification item's title
// and message next to each other within a single column.
std::unique_ptr<views::View> CreateItemView(const NotificationItem& item) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));

  const gfx::FontList font_list = GetTextFontList();

  auto* title = new views::Label(item.title);
  title->SetFontList(font_list);
  title->SetCollapseWhenHidden(true);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  view->AddChildView(title);

  views::Label* message = view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          IDS_MESSAGE_CENTER_LIST_NOTIFICATION_MESSAGE_WITH_DIVIDER,
          item.message),
      views::style::CONTEXT_LABEL, views::style::STYLE_DISABLED));
  message->SetFontList(font_list);
  message->SetCollapseWhenHidden(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return view;
}

std::unique_ptr<ui::Event> ConvertToBoundedLocatedEvent(const ui::Event& event,
                                                        views::View* target) {
  // In case the animation is triggered from keyboard operation.
  if (!event.IsLocatedEvent())
    return nullptr;

  // Convert the point of |event| from the coordinate system of its target to
  // that of the passed in |target| and create a new LocatedEvent.
  std::unique_ptr<ui::Event> cloned_event = ui::Event::Clone(event);
  ui::LocatedEvent* located_event = cloned_event->AsLocatedEvent();
  event.target()->ConvertEventToTarget(target, located_event);

  // Use default animation if location is out of bounds.
  if (!target->HitTestPoint(located_event->location()))
    return nullptr;

  return cloned_event;
}

}  // anonymous namespace

// CompactTitleMessageView /////////////////////////////////////////////////////

CompactTitleMessageView::~CompactTitleMessageView() = default;

const char* CompactTitleMessageView::GetClassName() const {
  return "CompactTitleMessageView";
}

CompactTitleMessageView::CompactTitleMessageView() {
  const gfx::FontList& font_list = GetTextFontList();

  title_ = new views::Label();
  title_->SetFontList(font_list);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(title_);

  message_ = AddChildView(std::make_unique<views::Label>(
      base::string16(), views::style::CONTEXT_LABEL,
      views::style::STYLE_DISABLED));
  message_->SetFontList(font_list);
  message_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
}

gfx::Size CompactTitleMessageView::CalculatePreferredSize() const {
  gfx::Size title_size = title_->GetPreferredSize();
  gfx::Size message_size = message_->GetPreferredSize();
  return gfx::Size(title_size.width() + message_size.width() +
                       kCompactTitleMessageViewSpacing,
                   std::max(title_size.height(), message_size.height()));
}

void CompactTitleMessageView::Layout() {
  // Elides title and message.
  // * If the message is too long, the message occupies at most
  //   kProgressNotificationMessageRatio of the width.
  // * If the title is too long, the full content of the message is shown,
  //   kCompactTitleMessageViewSpacing is added between them, and the elided
  //   title is shown.
  // * If they are short enough, the title is left-aligned and the message is
  //   right-aligned.
  const int message_width = std::min(
      message_->GetPreferredSize().width(),
      title_->GetPreferredSize().width() > 0
          ? static_cast<int>(kProgressNotificationMessageRatio * width())
          : width());
  const int title_width =
      std::max(0, width() - message_width - kCompactTitleMessageViewSpacing);

  title_->SetBounds(0, 0, title_width, height());
  message_->SetBounds(width() - message_width, 0, message_width, height());
}

void CompactTitleMessageView::set_title(const base::string16& title) {
  title_->SetText(title);
}

void CompactTitleMessageView::set_message(const base::string16& message) {
  message_->SetText(message);
}

// LargeImageView //////////////////////////////////////////////////////////////

LargeImageView::LargeImageView(const gfx::Size& max_size)
    : max_size_(max_size), min_size_(max_size_.width(), /*height=*/0) {}

LargeImageView::~LargeImageView() = default;

void LargeImageView::SetImage(const gfx::ImageSkia& image) {
  image_ = image;
  gfx::Size preferred_size = GetResizedImageSize();
  preferred_size.SetToMax(min_size_);
  preferred_size.SetToMin(max_size_);
  SetPreferredSize(preferred_size);
  SchedulePaint();
  Layout();
}

void LargeImageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Size resized_size = GetResizedImageSize();
  gfx::Size drawn_size = resized_size;
  drawn_size.SetToMin(max_size_);
  gfx::Rect drawn_bounds = GetContentsBounds();
  drawn_bounds.ClampToCenteredSize(drawn_size);

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      image_, skia::ImageOperations::RESIZE_BEST, resized_size);

  // Cut off the overflown part.
  gfx::ImageSkia drawn_image = gfx::ImageSkiaOperations::ExtractSubset(
      resized_image, gfx::Rect(drawn_size));

  canvas->DrawImageInt(drawn_image, drawn_bounds.x(), drawn_bounds.y());
}

const char* LargeImageView::GetClassName() const {
  return "LargeImageView";
}

void LargeImageView::OnThemeChanged() {
  View::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationLargeImageBackground)));
}

// Returns expected size of the image right after resizing.
// The GetResizedImageSize().width() <= max_size_.width() holds, but
// GetResizedImageSize().height() may be larger than max_size_.height().
// In this case, the overflown part will be just cutted off from the view.
gfx::Size LargeImageView::GetResizedImageSize() {
  gfx::Size original_size = image_.size();
  if (original_size.width() <= max_size_.width())
    return image_.size();

  const double proportion =
      original_size.height() / static_cast<double>(original_size.width());
  gfx::Size resized_size;
  resized_size.SetSize(max_size_.width(), max_size_.width() * proportion);
  return resized_size;
}

// NotificationMDTextButton ////////////////////////////////////////////////

NotificationMdTextButton::NotificationMdTextButton(
    views::ButtonListener* listener,
    const base::string16& label,
    const base::Optional<base::string16>& placeholder)
    : views::MdTextButton(listener, label), placeholder_(placeholder) {
  SetMinSize(kActionButtonMinSize);
  views::InstallRectHighlightPathGenerator(this);
  SetTextSubpixelRenderingEnabled(false);
}

NotificationMdTextButton::~NotificationMdTextButton() = default;

void NotificationMdTextButton::UpdateBackgroundColor() {
  // Overridden as no-op so we don't draw any background or border.
}

void NotificationMdTextButton::OnThemeChanged() {
  views::MdTextButton::OnThemeChanged();
  SetEnabledTextColors(text_color_);
  label()->SetAutoColorReadabilityEnabled(true);
  label()->SetBackgroundColor(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationActionsRowBackground));
}

void NotificationMdTextButton::OverrideTextColor(
    base::Optional<SkColor> text_color) {
  text_color_ = std::move(text_color);
  SetEnabledTextColors(text_color_);
  label()->SetAutoColorReadabilityEnabled(true);
}

BEGIN_METADATA(NotificationMdTextButton, views::MdTextButton)
END_METADATA

// NotificationInputContainerMD ////////////////////////////////////////////////

NotificationInputContainerMD::NotificationInputContainerMD(
    NotificationInputDelegate* delegate)
    : delegate_(delegate),
      ink_drop_container_(new views::InkDropContainerView()),
      textfield_(new views::Textfield()),
      button_(new views::ImageButton(this)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));

  SetInkDropMode(InkDropMode::ON);
  SetInkDropVisibleOpacity(1);

  AddChildView(ink_drop_container_);

  textfield_->set_controller(this);
  textfield_->SetBorder(views::CreateEmptyBorder(kInputTextfieldPadding));
  AddChildView(textfield_);
  layout->SetFlexForView(textfield_, 1);

  button_->SetBorder(views::CreateEmptyBorder(kInputReplyButtonPadding));
  button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  OnAfterUserAction(textfield_);
  AddChildView(button_);

  views::InstallRectHighlightPathGenerator(this);
}

NotificationInputContainerMD::~NotificationInputContainerMD() = default;

void NotificationInputContainerMD::AnimateBackground(const ui::Event& event) {
  std::unique_ptr<ui::Event> located_event =
      ConvertToBoundedLocatedEvent(event, this);
  AnimateInkDrop(views::InkDropState::ACTION_PENDING,
                 ui::LocatedEvent::FromIfValid(located_event.get()));
}

void NotificationInputContainerMD::AddLayerBeneathView(ui::Layer* layer) {
  // When a ink drop layer is added it is stacked between the textfield/button
  // and the parent (|this|). Since the ink drop is opaque, we have to paint the
  // textfield/button on their own layers in otherwise they remain painted on
  // |this|'s layer which would be covered by the ink drop.
  textfield_->SetPaintToLayer();
  textfield_->layer()->SetFillsBoundsOpaquely(false);
  button_->SetPaintToLayer();
  button_->layer()->SetFillsBoundsOpaquely(false);
  ink_drop_container_->AddLayerBeneathView(layer);
}

void NotificationInputContainerMD::RemoveLayerBeneathView(ui::Layer* layer) {
  ink_drop_container_->RemoveLayerBeneathView(layer);
  textfield_->DestroyLayer();
  button_->DestroyLayer();
}

std::unique_ptr<views::InkDropRipple>
NotificationInputContainerMD::CreateInkDropRipple() const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      GetInkDropVisibleOpacity());
}

SkColor NotificationInputContainerMD::GetInkDropBaseColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationInkDropBase);
}

void NotificationInputContainerMD::OnThemeChanged() {
  InkDropHostView::OnThemeChanged();
  auto* theme = GetNativeTheme();
  SetBackground(views::CreateSolidBackground(theme->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationActionsRowBackground)));
  textfield_->SetTextColor(SK_ColorWHITE);
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield_->set_placeholder_text_color(theme->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationEmptyPlaceholderTextColor));
  SetButtonImage();
}

void NotificationInputContainerMD::Layout() {
  views::InkDropHostView::Layout();
  // The animation is needed to run inside of the border.
  ink_drop_container_->SetBoundsRect(GetLocalBounds());
}

bool NotificationInputContainerMD::HandleKeyEvent(views::Textfield* sender,
                                                  const ui::KeyEvent& event) {
  if (event.type() == ui::ET_KEY_PRESSED &&
      event.key_code() == ui::VKEY_RETURN) {
    delegate_->OnNotificationInputSubmit(
        textfield_->GetProperty(kTextfieldIndexKey), textfield_->GetText());
    textfield_->SetText(base::string16());
    return true;
  }
  return event.type() == ui::ET_KEY_RELEASED;
}

void NotificationInputContainerMD::OnAfterUserAction(views::Textfield* sender) {
  DCHECK_EQ(sender, textfield_);
  SetButtonImage();
}

void NotificationInputContainerMD::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender == button_) {
    delegate_->OnNotificationInputSubmit(
        textfield_->GetProperty(kTextfieldIndexKey), textfield_->GetText());
  }
}

void NotificationInputContainerMD::SetButtonImage() {
  auto placeholder_icon_color_id =
      textfield_->GetText().empty()
          ? ui::NativeTheme::kColorId_NotificationEmptyPlaceholderIconColor
          : ui::NativeTheme::kColorId_NotificationPlaceholderIconColor;
  button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(
          kNotificationInlineReplyIcon, kInputReplyButtonSize,
          GetNativeTheme()->GetSystemColor(placeholder_icon_color_id)));
}

// InlineSettingsRadioButton ///////////////////////////////////////////////////

class InlineSettingsRadioButton : public views::RadioButton {
 public:
  explicit InlineSettingsRadioButton(const base::string16& label_text)
      : views::RadioButton(label_text, 1 /* group */) {
    label()->SetFontList(GetTextFontList());
    label()->SetSubpixelRenderingEnabled(false);
  }

  void OnThemeChanged() override {
    RadioButton::OnThemeChanged();
    SetEnabledTextColors(GetTextColor());
    label()->SetAutoColorReadabilityEnabled(true);
    label()->SetBackgroundColor(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_NotificationInlineSettingsBackground));
  }

 private:
  SkColor GetTextColor() const {
    return GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_LabelEnabledColor);
  }
};

// NotificationInkDropImpl /////////////////////////////////////////////////////

class NotificationInkDropImpl : public views::InkDropImpl {
 public:
  NotificationInkDropImpl(views::InkDropHostView* ink_drop_host,
                          const gfx::Size& host_size)
      : views::InkDropImpl(ink_drop_host, host_size) {
    SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  }

  void HostSizeChanged(const gfx::Size& new_size) override {
    // Prevent a call to InkDropImpl::HostSizeChanged which recreates the ripple
    // and stops the currently active animation: http://crbug.com/915222.
  }
};

// ////////////////////////////////////////////////////////////
// NotificationViewMD
// ////////////////////////////////////////////////////////////

class NotificationViewMD::NotificationViewMDPathGenerator
    : public views::HighlightPathGenerator {
 public:
  explicit NotificationViewMDPathGenerator(gfx::Insets insets)
      : insets_(std::move(insets)) {}
  NotificationViewMDPathGenerator(const NotificationViewMDPathGenerator&) =
      delete;
  NotificationViewMDPathGenerator& operator=(
      const NotificationViewMDPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  base::Optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::RectF bounds = rect;
    if (!preferred_size_.IsEmpty())
      bounds.set_size(gfx::SizeF(preferred_size_));
    bounds.Inset(insets_);
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
  // required before a Layout() on the view is run. See
  // http://crbug.com/915222.
  gfx::Size preferred_size_;
};

void NotificationViewMD::CreateOrUpdateViews(const Notification& notification) {
  left_content_count_ = 0;

  CreateOrUpdateContextTitleView(notification);
  CreateOrUpdateTitleView(notification);
  CreateOrUpdateMessageView(notification);
  CreateOrUpdateCompactTitleMessageView(notification);
  CreateOrUpdateProgressBarView(notification);
  CreateOrUpdateProgressStatusView(notification);
  CreateOrUpdateListItemViews(notification);
  CreateOrUpdateIconView(notification);
  CreateOrUpdateSmallIconView(notification);
  CreateOrUpdateImageView(notification);
  CreateOrUpdateInlineSettingsViews(notification);
  UpdateViewForExpandedState(expanded_);
  // Should be called at the last because SynthesizeMouseMoveEvent() requires
  // everything is in the right location when called.
  CreateOrUpdateActionButtonViews(notification);
}

NotificationViewMD::NotificationViewMD(const Notification& notification)
    : MessageView(notification),
      ink_drop_container_(new views::InkDropContainerView()) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  SetInkDropVisibleOpacity(1);

  AddChildView(ink_drop_container_);

  // |header_row_| contains app_icon, app_name, control buttons, etc...
  header_row_ = new NotificationHeaderView(this);
  header_row_->SetPreferredSize(header_row_->GetPreferredSize() -
                                gfx::Size(GetInsets().width(), 0));
  control_buttons_view_ = header_row_->AddChildView(
      std::make_unique<NotificationControlButtonsView>(this));
  AddChildView(header_row_);

  // |content_row_| contains title, message, image, progressbar, etc...
  content_row_ = new views::View();
  auto* content_row_layout =
      content_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kContentRowPadding, 0));
  content_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  AddChildView(content_row_);

  // |left_content_| contains most contents like title, message, etc...
  left_content_ = new views::View();
  left_content_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
  left_content_->SetBorder(views::CreateEmptyBorder(kLeftContentPadding));
  content_row_->AddChildView(left_content_);
  content_row_layout->SetFlexForView(left_content_, 1);

  // |right_content_| contains notification icon and small image.
  right_content_ = new views::View();
  right_content_->SetLayoutManager(std::make_unique<views::FillLayout>());
  content_row_->AddChildView(right_content_);

  // |action_row_| contains inline action buttons and inline textfield.
  actions_row_ = new views::View();
  actions_row_->SetVisible(false);
  actions_row_->SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(actions_row_);

  // |action_buttons_row_| contains inline action buttons.
  action_buttons_row_ = new views::View();
  action_buttons_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kActionsRowPadding,
      kActionsRowHorizontalSpacing));
  action_buttons_row_->SetVisible(false);
  actions_row_->AddChildView(action_buttons_row_);

  // |inline_reply_| is a container for an inline textfield.
  inline_reply_ = new NotificationInputContainerMD(this);
  inline_reply_->SetVisible(false);
  actions_row_->AddChildView(inline_reply_);

  CreateOrUpdateViews(notification);
  UpdateControlButtonsVisibilityWithNotification(notification);

  SetNotifyEnterExitOnChild(true);

  click_activator_ = std::make_unique<ClickActivator>(this);
  // Reasons to use pretarget handler instead of OnMousePressed:
  // - NotificationViewMD::OnMousePresssed would not fire on the inline reply
  //   textfield click in native notification.
  // - To make it look similar to ArcNotificationContentView::EventForwarder.
  AddPreTargetHandler(click_activator_.get());

  auto highlight_path_generator =
      std::make_unique<NotificationViewMDPathGenerator>(GetInsets());
  highlight_path_generator_ = highlight_path_generator.get();
  views::HighlightPathGenerator::Install(this,
                                         std::move(highlight_path_generator));

  DCHECK(focus_ring());
  focus_ring()->SetPathGenerator(
      std::make_unique<MessageView::HighlightPathGenerator>());

  UpdateCornerRadius(kNotificationCornerRadius, kNotificationCornerRadius);
}

NotificationViewMD::~NotificationViewMD() {
  RemovePreTargetHandler(click_activator_.get());
}

void NotificationViewMD::AddLayerBeneathView(ui::Layer* layer) {
  GetInkDrop()->AddObserver(this);
  for (auto* child : GetChildrenForLayerAdjustment()) {
    child->SetPaintToLayer();
    child->layer()->SetFillsBoundsOpaquely(false);
  }
  ink_drop_container_->AddLayerBeneathView(layer);
}

void NotificationViewMD::RemoveLayerBeneathView(ui::Layer* layer) {
  ink_drop_container_->RemoveLayerBeneathView(layer);
  for (auto* child : GetChildrenForLayerAdjustment())
    child->DestroyLayer();
  GetInkDrop()->RemoveObserver(this);
}

void NotificationViewMD::Layout() {
  MessageView::Layout();

  // We need to call IsExpandable() at the end of Layout() call, since whether
  // we should show expand button or not depends on the current view layout.
  // (e.g. Show expand button when |message_view_| exceeds one line.)
  header_row_->SetExpandButtonEnabled(IsExpandable());
  header_row_->Layout();

  // The notification background is rounded in MessageView::Layout(),
  // but we also have to round the actions row background here.
  if (actions_row_->GetVisible()) {
    constexpr SkScalar kCornerRadius = SkIntToScalar(kNotificationCornerRadius);

    // Use vertically larger clip path, so that actions row's top corners will
    // not be rounded.
    SkPath path;
    gfx::Rect bounds = actions_row_->GetLocalBounds();
    bounds.set_y(bounds.y() - bounds.height());
    bounds.set_height(bounds.height() * 2);
    path.addRoundRect(gfx::RectToSkRect(bounds), kCornerRadius, kCornerRadius);

    action_buttons_row_->SetClipPath(path);
    inline_reply_->SetClipPath(path);
  }

  // The animation is needed to run inside of the border.
  ink_drop_container_->SetBoundsRect(GetLocalBounds());
}

void NotificationViewMD::OnFocus() {
  MessageView::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

bool NotificationViewMD::OnMousePressed(const ui::MouseEvent& event) {
  last_mouse_pressed_timestamp_ = base::TimeTicks(event.time_stamp());
  return true;
}

bool NotificationViewMD::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

void NotificationViewMD::OnMouseReleased(const ui::MouseEvent& event) {
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
  if (settings_row_ && settings_row_->GetVisible())
    return;

  MessageView::OnMouseReleased(event);
}

void NotificationViewMD::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
      UpdateControlButtonsVisibility();
      break;
    case ui::ET_MOUSE_EXITED:
      UpdateControlButtonsVisibility();
      break;
    default:
      break;
  }
  View::OnMouseEvent(event);
}

void NotificationViewMD::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_LONG_TAP) {
    ToggleInlineSettings(*event);
    return;
  }
  MessageView::OnGestureEvent(event);
}

void NotificationViewMD::PreferredSizeChanged() {
  highlight_path_generator_->set_preferred_size(GetPreferredSize());
  MessageView::PreferredSizeChanged();
}

void NotificationViewMD::UpdateWithNotification(
    const Notification& notification) {
  MessageView::UpdateWithNotification(notification);
  UpdateControlButtonsVisibilityWithNotification(notification);

  CreateOrUpdateViews(notification);
  Layout();
  SchedulePaint();
}

// TODO(yoshiki): Move this to the parent class (MessageView).
void NotificationViewMD::UpdateControlButtonsVisibilityWithNotification(
    const Notification& notification) {
  control_buttons_view_->ShowSettingsButton(
      notification.should_show_settings_button());
  control_buttons_view_->ShowSnoozeButton(
      notification.should_show_snooze_button());
  control_buttons_view_->ShowCloseButton(GetMode() != Mode::PINNED);
  UpdateControlButtonsVisibility();
}

void NotificationViewMD::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  // Tapping anywhere on |header_row_| can expand the notification, though only
  // |expand_button| can be focused by TAB.
  if (sender == header_row_) {
    if (IsExpandable() && content_row_->GetVisible()) {
      SetManuallyExpandedOrCollapsed(true);
      auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
      ToggleExpanded();
      // Check |this| is valid before continuing, because ToggleExpanded() might
      // cause |this| to be deleted.
      if (!weak_ptr)
        return;
      Layout();
      SchedulePaint();
    }
    return;
  }

  // See if the button pressed was an action button.
  for (size_t i = 0; i < action_buttons_.size(); ++i) {
    if (sender != action_buttons_[i])
      continue;

    const base::Optional<base::string16>& placeholder =
        action_buttons_[i]->placeholder();
    if (placeholder) {
      inline_reply_->textfield()->SetProperty(kTextfieldIndexKey,
                                              static_cast<int>(i));
      inline_reply_->textfield()->SetPlaceholderText(
          placeholder->empty()
              ? l10n_util::GetStringUTF16(
                    IDS_MESSAGE_CENTER_NOTIFICATION_INLINE_REPLY_PLACEHOLDER)
              : *placeholder);
      inline_reply_->AnimateBackground(event);
      inline_reply_->SetVisible(true);
      action_buttons_row_->SetVisible(false);
      // RequestFocus() should be called after SetVisible().
      inline_reply_->textfield()->RequestFocus();
      Layout();
      SchedulePaint();
    } else {
      MessageCenter::Get()->ClickOnNotificationButton(notification_id(), i);
    }
    return;
  }

  if (sender == settings_done_button_) {
    ToggleInlineSettings(event);
    return;
  }
}

void NotificationViewMD::OnNotificationInputSubmit(size_t index,
                                                   const base::string16& text) {
  MessageCenter::Get()->ClickOnNotificationButtonWithReply(notification_id(),
                                                           index, text);
}

void NotificationViewMD::CreateOrUpdateContextTitleView(
    const Notification& notification) {
  header_row_->SetAccentColor(notification.accent_color());
  header_row_->SetTimestamp(notification.timestamp());
  header_row_->SetAppNameElideBehavior(gfx::ELIDE_TAIL);
  header_row_->SetSummaryText(base::string16());

  base::string16 app_name;
  if (notification.UseOriginAsContextMessage()) {
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

void NotificationViewMD::CreateOrUpdateTitleView(
    const Notification& notification) {
  if (notification.title().empty() ||
      notification.type() == NOTIFICATION_TYPE_PROGRESS) {
    DCHECK(!title_view_ || left_content_->Contains(title_view_));
    delete title_view_;
    title_view_ = nullptr;
    return;
  }

  int title_character_limit =
      kNotificationWidth * kMaxTitleLines / kMinPixelsPerTitleCharacterMD;

  base::string16 title = gfx::TruncateString(
      notification.title(), title_character_limit, gfx::WORD_BREAK);
  if (!title_view_) {
    const gfx::FontList& font_list = GetTextFontList();

    title_view_ = new views::Label(title);
    title_view_->SetFontList(font_list);
    title_view_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    title_view_->SetLineHeight(kLineHeightMD);
    // TODO(knollr): multiline should not be required, but we need to set the
    // width of |title_view_| (because of crbug.com/682266), which only works in
    // multiline mode.
    title_view_->SetMultiLine(true);
    title_view_->SetMaxLines(kMaxLinesForTitleView);
    title_view_->SetAllowCharacterBreak(true);
    left_content_->AddChildViewAt(title_view_, left_content_count_);
  } else {
    title_view_->SetText(title);
  }

  left_content_count_++;
}

void NotificationViewMD::CreateOrUpdateMessageView(
    const Notification& notification) {
  if (notification.type() == NOTIFICATION_TYPE_PROGRESS ||
      notification.message().empty()) {
    // Deletion will also remove |message_view_| from its parent.
    delete message_view_;
    message_view_ = nullptr;
    return;
  }

  base::string16 text = gfx::TruncateString(
      notification.message(), kMessageCharacterLimitMD, gfx::WORD_BREAK);

  if (!message_view_) {
    const gfx::FontList& font_list = GetTextFontList();

    message_view_ = left_content_->AddChildViewAt(
        std::make_unique<views::Label>(text, views::style::CONTEXT_LABEL,
                                       views::style::STYLE_DISABLED),
        left_content_count_);
    message_view_->SetFontList(font_list);
    message_view_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    message_view_->SetLineHeight(kLineHeightMD);
    message_view_->SetMultiLine(true);
    message_view_->SetMaxLines(kMaxLinesForMessageView);
    message_view_->SetAllowCharacterBreak(true);
  } else {
    message_view_->SetText(text);
  }

  message_view_->SetVisible(notification.items().empty());
  left_content_count_++;
}

void NotificationViewMD::CreateOrUpdateCompactTitleMessageView(
    const Notification& notification) {
  if (notification.type() != NOTIFICATION_TYPE_PROGRESS) {
    DCHECK(!compact_title_message_view_ ||
           left_content_->Contains(compact_title_message_view_));
    delete compact_title_message_view_;
    compact_title_message_view_ = nullptr;
    return;
  }
  if (!compact_title_message_view_) {
    compact_title_message_view_ = new CompactTitleMessageView();
    left_content_->AddChildViewAt(compact_title_message_view_,
                                  left_content_count_);
  }

  compact_title_message_view_->set_title(notification.title());
  compact_title_message_view_->set_message(notification.message());
  left_content_->InvalidateLayout();
  left_content_count_++;
}

void NotificationViewMD::CreateOrUpdateProgressBarView(
    const Notification& notification) {
  if (notification.type() != NOTIFICATION_TYPE_PROGRESS) {
    DCHECK(!progress_bar_view_ || left_content_->Contains(progress_bar_view_));
    delete progress_bar_view_;
    progress_bar_view_ = nullptr;
    return;
  }

  DCHECK(left_content_);

  if (!progress_bar_view_) {
    progress_bar_view_ = new views::ProgressBar(kProgressBarHeight,
                                                /* allow_round_corner */ false);
    progress_bar_view_->SetBorder(
        views::CreateEmptyBorder(kProgressBarTopPadding, 0, 0, 0));
    left_content_->AddChildViewAt(progress_bar_view_, left_content_count_);
  }

  progress_bar_view_->SetValue(notification.progress() / 100.0);
  progress_bar_view_->SetVisible(notification.items().empty());

  if (0 <= notification.progress() && notification.progress() <= 100)
    header_row_->SetProgress(notification.progress());

  left_content_count_++;
}

void NotificationViewMD::CreateOrUpdateProgressStatusView(
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
    const gfx::FontList& font_list = GetTextFontList();
    status_view_ = left_content_->AddChildViewAt(
        std::make_unique<views::Label>(base::string16(),
                                       views::style::CONTEXT_LABEL,
                                       views::style::STYLE_DISABLED),
        left_content_count_);
    status_view_->SetFontList(font_list);
    status_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    status_view_->SetBorder(views::CreateEmptyBorder(kStatusTextPadding));
  }

  status_view_->SetText(notification.progress_status());
  left_content_count_++;
}

void NotificationViewMD::CreateOrUpdateListItemViews(
    const Notification& notification) {
  for (auto* item_view : item_views_)
    delete item_view;
  item_views_.clear();

  const std::vector<NotificationItem>& items = notification.items();

  for (size_t i = 0; i < items.size() && i < kMaxLinesForExpandedMessageView;
       ++i) {
    std::unique_ptr<views::View> item_view = CreateItemView(items[i]);
    item_views_.push_back(item_view.get());
    left_content_->AddChildViewAt(item_view.release(), left_content_count_++);
  }

  list_items_count_ = items.size();

  // Needed when CreateOrUpdateViews is called for update.
  if (!item_views_.empty())
    left_content_->InvalidateLayout();
}

void NotificationViewMD::CreateOrUpdateIconView(
    const Notification& notification) {
  const bool use_image_for_icon = notification.icon().IsEmpty();

  gfx::ImageSkia icon = use_image_for_icon ? notification.image().AsImageSkia()
                                           : notification.icon().AsImageSkia();

  if (notification.type() == NOTIFICATION_TYPE_PROGRESS ||
      notification.type() == NOTIFICATION_TYPE_MULTIPLE || icon.isNull()) {
    DCHECK(!icon_view_ || right_content_->Contains(icon_view_));
    delete icon_view_;
    icon_view_ = nullptr;
    return;
  }

  if (!icon_view_) {
    icon_view_ = new ProportionalImageView(kIconViewSize);
    right_content_->AddChildView(icon_view_);
  }

  icon_view_->SetImage(icon, icon.size());

  // Hide the icon on the right side when the notification is expanded.
  hide_icon_on_expanded_ = use_image_for_icon;
}

void NotificationViewMD::CreateOrUpdateSmallIconView(
    const Notification& notification) {
  SkColor accent_color =
      notification.accent_color().value_or(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_NotificationDefaultAccentColor));
  SkColor icon_color =
      color_utils::BlendForMinContrast(
          accent_color, GetNotificationHeaderViewBackgroundColor())
          .color;

  // TODO(knollr): figure out if this has a performance impact and
  // cache images if so. (crbug.com/768748)
  gfx::Image masked_small_icon =
      notification.GenerateMaskedSmallIcon(kSmallImageSizeMD, icon_color);

  if (masked_small_icon.IsEmpty()) {
    header_row_->ClearAppIcon();
  } else {
    header_row_->SetAppIcon(masked_small_icon.AsImageSkia());
  }
}

void NotificationViewMD::CreateOrUpdateImageView(
    const Notification& notification) {
  if (notification.image().IsEmpty()) {
    if (image_container_view_) {
      DCHECK(Contains(image_container_view_));
      delete image_container_view_;
      image_container_view_ = nullptr;
    }
    return;
  }

  if (!image_container_view_) {
    image_container_view_ = new views::View();
    image_container_view_->SetLayoutManager(
        std::make_unique<views::FillLayout>());
    image_container_view_->SetBorder(
        views::CreateEmptyBorder(kLargeImageContainerPadding));
    int max_width = kNotificationWidth - kLargeImageContainerPadding.width() -
                    GetInsets().width();
    image_container_view_->AddChildView(std::make_unique<LargeImageView>(
        gfx::Size(max_width, kLargeImageMaxHeight)));

    // Insert the created image container just after the |content_row_|.
    AddChildViewAt(image_container_view_, GetIndexOf(content_row_) + 1);
  }

  static_cast<LargeImageView*>(image_container_view_->children().front())
      ->SetImage(notification.image().AsImageSkia());
}

void NotificationViewMD::CreateOrUpdateActionButtonViews(
    const Notification& notification) {
  const std::vector<ButtonInfo>& buttons = notification.buttons();
  bool new_buttons = action_buttons_.size() != buttons.size();

  if (new_buttons || buttons.empty()) {
    for (auto* item : action_buttons_)
      delete item;
    action_buttons_.clear();
    actions_row_->SetVisible(expanded_ && !buttons.empty());
  }

  DCHECK_EQ(this, actions_row_->parent());

  // Hide inline reply field if it doesn't exist anymore.
  if (inline_reply_->GetVisible()) {
    const size_t index =
        inline_reply_->textfield()->GetProperty(kTextfieldIndexKey);
    if (index >= buttons.size() || !buttons[index].placeholder.has_value()) {
      action_buttons_row_->SetVisible(true);
      inline_reply_->SetVisible(false);
    }
  }

  for (size_t i = 0; i < buttons.size(); ++i) {
    ButtonInfo button_info = buttons[i];
    base::string16 label = base::i18n::ToUpper(button_info.title);
    if (new_buttons) {
      action_buttons_.push_back(action_buttons_row_->AddChildView(
          std::make_unique<NotificationMdTextButton>(this, label,
                                                     button_info.placeholder)));
      // TODO(pkasting): BoxLayout should invalidate automatically when a child
      // is added, at which point we can remove this call.
      action_buttons_row_->InvalidateLayout();
    } else {
      action_buttons_[i]->SetText(label);
      action_buttons_[i]->set_placeholder(button_info.placeholder);
    }

    // Change action button color to the accent color.
    action_buttons_[i]->OverrideTextColor(notification.accent_color());
  }

  // Inherit mouse hover state when action button views reset.
  // If the view is not expanded, there should be no hover state.
  if (new_buttons && expanded_) {
    views::Widget* widget = GetWidget();
    if (widget) {
      // This Layout() is needed because button should be in the right location
      // in the view hierarchy when SynthesizeMouseMoveEvent() is called.
      Layout();
      widget->SetSize(widget->GetContentsView()->GetPreferredSize());
      GetWidget()->SynthesizeMouseMoveEvent();
    }
  }
}

void NotificationViewMD::CreateOrUpdateInlineSettingsViews(
    const Notification& notification) {
  if (settings_row_) {
    DCHECK_EQ(SettingsButtonHandler::INLINE,
              notification.rich_notification_data().settings_button_handler);
    return;
  }

  if (notification.rich_notification_data().settings_button_handler !=
      SettingsButtonHandler::INLINE) {
    return;
  }

  // |settings_row_| contains inline settings.
  settings_row_ = new views::View();
  settings_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kSettingsRowPadding, 0));

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
      NOTREACHED();
      break;
  }
  DCHECK_NE(block_notifications_message_id, 0);

  block_all_button_ = new InlineSettingsRadioButton(
      l10n_util::GetStringUTF16(block_notifications_message_id));
  block_all_button_->SetBorder(
      views::CreateEmptyBorder(kSettingsRadioButtonPadding));
  settings_row_->AddChildView(block_all_button_);

  dont_block_button_ = new InlineSettingsRadioButton(
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_DONT_BLOCK_NOTIFICATIONS));
  dont_block_button_->SetBorder(
      views::CreateEmptyBorder(kSettingsRadioButtonPadding));
  settings_row_->AddChildView(dont_block_button_);
  settings_row_->SetVisible(false);

  settings_done_button_ = new NotificationMdTextButton(
      this, l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS_DONE),
      base::nullopt);

  auto* settings_button_row = new views::View;
  auto settings_button_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kSettingsButtonRowPadding, 0);
  settings_button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  settings_button_row->SetLayoutManager(std::move(settings_button_layout));
  settings_button_row->AddChildView(settings_done_button_);
  settings_row_->AddChildView(settings_button_row);

  AddChildViewAt(settings_row_, GetIndexOf(actions_row_));
}

bool NotificationViewMD::IsExpandable() {
  // Inline settings can not be expanded.
  if (GetMode() == Mode::SETTING)
    return false;

  // Expandable if the message exceeds one line.
  if (message_view_ && message_view_->GetVisible() &&
      message_view_->GetRequiredLines() > 1) {
    return true;
  }
  // Expandable if there is at least one inline action.
  if (!action_buttons_row_->children().empty())
    return true;

  // Expandable if the notification has image.
  if (image_container_view_)
    return true;

  // Expandable if there are multiple list items.
  if (item_views_.size() > 1)
    return true;

  // Expandable if both progress bar and status message exist.
  if (status_view_)
    return true;

  return false;
}

void NotificationViewMD::ToggleExpanded() {
  SetExpanded(!expanded_);
}

void NotificationViewMD::UpdateViewForExpandedState(bool expanded) {
  header_row_->SetExpanded(expanded);
  if (message_view_) {
    message_view_->SetMaxLines(expanded ? kMaxLinesForExpandedMessageView
                                        : kMaxLinesForMessageView);
  }
  if (image_container_view_)
    image_container_view_->SetVisible(expanded);

  actions_row_->SetVisible(expanded &&
                           !action_buttons_row_->children().empty());
  if (!expanded) {
    action_buttons_row_->SetVisible(true);
    inline_reply_->SetVisible(false);
  }

  for (size_t i = kMaxLinesForMessageView; i < item_views_.size(); ++i) {
    item_views_[i]->SetVisible(expanded);
  }
  if (status_view_)
    status_view_->SetVisible(expanded);

  int max_items = expanded ? item_views_.size() : kMaxLinesForMessageView;
  if (list_items_count_ > max_items)
    header_row_->SetOverflowIndicator(list_items_count_ - max_items);
  else if (!item_views_.empty())
    header_row_->SetSummaryText(base::string16());

  bool has_icon = icon_view_ && (!hide_icon_on_expanded_ || !expanded);
  right_content_->SetVisible(has_icon);
  left_content_->SetBorder(views::CreateEmptyBorder(
      has_icon ? kLeftContentPaddingWithIcon : kLeftContentPadding));

  // TODO(tetsui): Workaround https://crbug.com/682266 by explicitly setting
  // the width.
  // Ideally, we should fix the original bug, but it seems there's no obvious
  // solution for the bug according to https://crbug.com/678337#c7, we should
  // ensure that the change won't break any of the users of BoxLayout class.
  const int message_view_width =
      (has_icon ? kMessageViewWidthWithIcon : kMessageViewWidth) -
      GetInsets().width();
  if (title_view_)
    title_view_->SizeToFit(message_view_width);
  if (message_view_)
    message_view_->SizeToFit(message_view_width);

  content_row_->InvalidateLayout();
}

void NotificationViewMD::ToggleInlineSettings(const ui::Event& event) {
  if (!settings_row_)
    return;

  bool inline_settings_visible = !settings_row_->GetVisible();
  bool disable_notification =
      settings_row_->GetVisible() && block_all_button_->GetChecked();

  settings_row_->SetVisible(inline_settings_visible);
  content_row_->SetVisible(!inline_settings_visible);
  header_row_->SetDetailViewsVisible(!inline_settings_visible);

  // Always check "Don't block" when inline settings is shown.
  // If it's already blocked, users should not see inline settings.
  // Toggling should reset the state.
  dont_block_button_->SetChecked(true);

  SetSettingMode(inline_settings_visible);

  // Grab a weak pointer before calling SetExpanded() as it might cause |this|
  // to be deleted.
  {
    auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
    SetExpanded(!inline_settings_visible);
    if (!weak_ptr)
      return;
  }

  PreferredSizeChanged();

  if (inline_settings_visible)
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

void NotificationViewMD::UpdateHeaderViewBackgroundColor() {
  SkColor header_background_color = GetNotificationHeaderViewBackgroundColor();
  header_row_->SetBackgroundColor(header_background_color);
  control_buttons_view_->SetBackgroundColor(header_background_color);

  auto* notification =
      MessageCenter::Get()->FindVisibleNotificationById(notification_id());
  if (notification)
    CreateOrUpdateSmallIconView(*notification);
}

SkColor NotificationViewMD::GetNotificationHeaderViewBackgroundColor() const {
  bool inline_settings_visible = settings_row_ && settings_row_->GetVisible();
  return GetNativeTheme()->GetSystemColor(
      inline_settings_visible
          ? ui::NativeTheme::kColorId_NotificationInlineSettingsBackground
          : ui::NativeTheme::kColorId_NotificationDefaultBackground);
}

void NotificationViewMD::UpdateActionButtonsRowBackground() {
  action_buttons_row_->SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<NotificationBackgroundPainter>(
          /*top_radius=*/0, bottom_radius(),
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_NotificationActionsRowBackground))));
}

void NotificationViewMD::UpdateCornerRadius(int top_radius, int bottom_radius) {
  MessageView::UpdateCornerRadius(top_radius, bottom_radius);
  UpdateActionButtonsRowBackground();
  highlight_path_generator_->set_top_radius(top_radius);
  highlight_path_generator_->set_bottom_radius(bottom_radius);
}

NotificationControlButtonsView* NotificationViewMD::GetControlButtonsView()
    const {
  return control_buttons_view_;
}

bool NotificationViewMD::IsExpanded() const {
  return expanded_;
}

void NotificationViewMD::SetExpanded(bool expanded) {
  if (expanded_ == expanded)
    return;
  expanded_ = expanded;

  UpdateViewForExpandedState(expanded_);
  PreferredSizeChanged();
}

bool NotificationViewMD::IsManuallyExpandedOrCollapsed() const {
  return manually_expanded_or_collapsed_;
}

void NotificationViewMD::SetManuallyExpandedOrCollapsed(bool value) {
  manually_expanded_or_collapsed_ = value;
}

void NotificationViewMD::OnSettingsButtonPressed(const ui::Event& event) {
  for (auto& observer : *observers())
    observer.OnSettingsButtonPressed(notification_id());

  if (settings_row_)
    ToggleInlineSettings(event);
  else
    MessageView::OnSettingsButtonPressed(event);
}

void NotificationViewMD::OnThemeChanged() {
  MessageView::OnThemeChanged();
  UpdateHeaderViewBackgroundColor();
  UpdateActionButtonsRowBackground();
}

void NotificationViewMD::Activate() {
  GetWidget()->widget_delegate()->SetCanActivate(true);
  GetWidget()->Activate();
}

void NotificationViewMD::AddBackgroundAnimation(const ui::Event& event) {
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  std::unique_ptr<ui::Event> located_event =
      ConvertToBoundedLocatedEvent(event, this);
  AnimateInkDrop(views::InkDropState::ACTION_PENDING,
                 ui::LocatedEvent::FromIfValid(located_event.get()));
}

void NotificationViewMD::RemoveBackgroundAnimation() {
  SetInkDropMode(InkDropMode::OFF);
  AnimateInkDrop(views::InkDropState::HIDDEN, nullptr);
}

std::unique_ptr<views::InkDrop> NotificationViewMD::CreateInkDrop() {
  return std::make_unique<NotificationInkDropImpl>(this, size());
}

std::unique_ptr<views::InkDropRipple> NotificationViewMD::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      GetPreferredSize(), GetInkDropCenterBasedOnLastEvent(),
      GetInkDropBaseColor(), GetInkDropVisibleOpacity());
}

std::vector<views::View*> NotificationViewMD::GetChildrenForLayerAdjustment()
    const {
  return {header_row_, block_all_button_, dont_block_button_,
          settings_done_button_};
}

SkColor NotificationViewMD::GetInkDropBaseColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationInlineSettingsBackground);
}

void NotificationViewMD::InkDropAnimationStarted() {
  header_row_->SetSubpixelRenderingEnabled(false);
}

void NotificationViewMD::InkDropRippleAnimationEnded(
    views::InkDropState ink_drop_state) {
  if (ink_drop_state == views::InkDropState::HIDDEN)
    header_row_->SetSubpixelRenderingEnabled(true);
}

}  // namespace message_center
