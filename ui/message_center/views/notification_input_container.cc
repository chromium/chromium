// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_input_container.h"

#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/notification_view_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_class_properties.h"

namespace message_center {

namespace {
// This key/property allows tagging the textfield with its index.
DEFINE_UI_CLASS_PROPERTY_KEY(int, kTextfieldIndexKey, 0U)

constexpr auto kInputTextfieldPadding = gfx::Insets::TLBR(16, 16, 16, 0);

constexpr auto kInputReplyButtonPadding = gfx::Insets::TLBR(0, 14, 0, 14);

// The icon size of inline reply input field.
constexpr int kInputReplyButtonSize = 20;

}  // namespace

NotificationInputContainer::NotificationInputContainer(
    NotificationInputDelegate* delegate)
    : delegate_(delegate),
      textfield_(new views::Textfield()),
      button_(new views::ImageButton(base::BindRepeating(
          [](NotificationInputContainer* container) {
            container->delegate_->OnNotificationInputSubmit(
                container->textfield_->GetProperty(kTextfieldIndexKey),
                container->textfield_->GetText());
          },
          base::Unretained(this)))) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
}

NotificationInputContainer::~NotificationInputContainer() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  if (views::InkDrop::Get(this))
    views::InkDrop::Remove(this);
}

void NotificationInputContainer::Init() {
  auto* box_layout = InstallLayoutManager();
  ink_drop_container_ = InstallInkDrop();

  textfield_->set_controller(this);
  textfield_->SetBorder(views::CreateEmptyBorder(GetTextfieldPadding()));
  StyleTextfield();
  AddChildView(textfield_.get());
  box_layout->SetFlexForView(textfield_, 1);

  button_->SetBorder(views::CreateEmptyBorder(GetSendButtonPadding()));
  SetSendButtonHighlightPath();
  button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(GetDefaultAccessibleNameStringId()));
  button_->SetTooltipText(
      l10n_util::GetStringUTF16(GetDefaultAccessibleNameStringId()));

  OnAfterUserAction(textfield_);
  AddChildView(button_.get());

  views::InstallRectHighlightPathGenerator(this);
}

void NotificationInputContainer::SetTextfieldIndex(int index) {
  textfield()->SetProperty(kTextfieldIndexKey, index);
}

size_t NotificationInputContainer::GetTextfieldIndex() const {
  return textfield()->GetProperty(kTextfieldIndexKey);
}

void NotificationInputContainer::SetPlaceholderText(
    const std::optional<std::u16string>& placeholder) {
  textfield_->SetPlaceholderText(
      placeholder->empty()
          ? l10n_util::GetStringUTF16(GetDefaultPlaceholderStringId())
          : *placeholder);
}

void NotificationInputContainer::AnimateBackground(const ui::Event& event) {
  if (!ink_drop_container_)
    return;

  std::unique_ptr<ui::Event> located_event =
      notification_view_util::ConvertToBoundedLocatedEvent(event, this);
  auto* ink_drop = views::InkDrop::Get(this);
  DCHECK(ink_drop);
  ink_drop->AnimateToState(views::InkDropState::ACTION_PENDING,
                           ui::LocatedEvent::FromIfValid(located_event.get()));
}

void NotificationInputContainer::AddLayerToRegion(ui::Layer* layer,
                                                  views::LayerRegion region) {
  if (!ink_drop_container_)
    return;

  // When a ink drop layer is added it is stacked between the textfield/button
  // and the parent (|this|). Since the ink drop is opaque, we have to paint the
  // textfield/button on their own layers in otherwise they remain painted on
  // |this|'s layer which would be covered by the ink drop.
  textfield_->SetPaintToLayer();
  textfield_->layer()->SetFillsBoundsOpaquely(false);
  button_->SetPaintToLayer();
  button_->layer()->SetFillsBoundsOpaquely(false);
  ink_drop_container_->AddLayerToRegion(layer, region);
}

void NotificationInputContainer::RemoveLayerFromRegions(ui::Layer* layer) {
  if (!ink_drop_container_)
    return;

  ink_drop_container_->RemoveLayerFromRegions(layer);
  textfield_->DestroyLayer();
  button_->DestroyLayer();
}

void NotificationInputContainer::OnThemeChanged() {
  View::OnThemeChanged();

  const auto* color_provider = GetColorProvider();
  textfield_->SetTextColor(
      color_provider->GetColor(ui::kColorNotificationInputForeground));
  StyleTextfield();
  if (ink_drop_container_)
    textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield_->set_placeholder_text_color(color_provider->GetColor(
      ui::kColorNotificationInputPlaceholderForeground));
  UpdateButtonImage();
}

views::ProposedLayout NotificationInputContainer::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  DCHECK(size_bounds.is_fully_bounded());
  layout.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  if (!ink_drop_container_) {
    return layout;
  }

  // The animation is needed to run inside of the border.
  layout.child_layouts.emplace_back(ink_drop_container_.get(),
                                    ink_drop_container_->GetVisible(),
                                    gfx::Rect(layout.host_size));
  return layout;
}

bool NotificationInputContainer::HandleKeyEvent(views::Textfield* sender,
                                                const ui::KeyEvent& event) {
  if (event.type() == ui::EventType::kKeyPressed &&
      event.key_code() == ui::VKEY_RETURN) {
    delegate_->OnNotificationInputSubmit(
        textfield_->GetProperty(kTextfieldIndexKey), textfield_->GetText());
    textfield_->SetText(std::u16string());
    return true;
  }
  return event.type() == ui::EventType::kKeyReleased;
}

void NotificationInputContainer::OnAfterUserAction(views::Textfield* sender) {
  DCHECK_EQ(sender, textfield_);
  UpdateButtonImage();
}

views::BoxLayout* NotificationInputContainer::InstallLayoutManager() {
  return SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      /*inside_border_insets=*/gfx::Insets(), /*between_child_spacing=*/0));
}

views::InkDropContainerView* NotificationInputContainer::InstallInkDrop() {
  views::InkDrop::Install(this, std::make_unique<views::InkDropHost>(this));
  views::InkDrop::Get(this)->SetMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
  views::InkDrop::Get(this)->SetVisibleOpacity(1);
  views::InkDrop::Get(this)->SetBaseColorId(
      ui::kColorNotificationInputBackground);

  auto* ink_drop_container =
      AddChildView(std::make_unique<views::InkDropContainerView>());
  ink_drop_container->SetProperty(views::kViewIgnoredByLayoutKey, true);
  return ink_drop_container;
}

gfx::Insets NotificationInputContainer::GetTextfieldPadding() const {
  return kInputTextfieldPadding;
}

gfx::Insets NotificationInputContainer::GetSendButtonPadding() const {
  return kInputReplyButtonPadding;
}

void NotificationInputContainer::SetSendButtonHighlightPath() {
  // Use the default highlight path.
}

int NotificationInputContainer::GetDefaultPlaceholderStringId() const {
  return IDS_MESSAGE_CENTER_NOTIFICATION_INLINE_REPLY_PLACEHOLDER;
}

int NotificationInputContainer::GetDefaultAccessibleNameStringId() const {
  return IDS_MESSAGE_CENTER_NOTIFICATION_INLINE_REPLY_ACCESSIBLE_NAME;
}

void NotificationInputContainer::StyleTextfield() {
  // No background.
}

void NotificationInputContainer::UpdateButtonImage() {
  if (!GetWidget())
    return;

  auto icon_color_id = textfield_->GetText().empty()
                           ? ui::kColorNotificationInputPlaceholderForeground
                           : ui::kColorNotificationInputForeground;
  button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          kNotificationInlineReplyIcon,
          GetColorProvider()->GetColor(icon_color_id), kInputReplyButtonSize));
}

BEGIN_METADATA(NotificationInputContainer)
END_METADATA

}  // namespace message_center
