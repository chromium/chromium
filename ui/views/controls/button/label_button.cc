// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

namespace {
constexpr Button::ButtonState kEnabledStates[] = {
    Button::STATE_NORMAL, Button::STATE_HOVERED, Button::STATE_PRESSED};
}  // namespace

LabelButton::LabelButton(
    PressedCallback callback,
    const std::u16string& text,
    int button_context,
    std::unique_ptr<LabelButtonImageContainer> image_container)
    : Button(std::move(callback)),
      image_container_(std::move(image_container)),
      cached_normal_font_list_(
          TypographyProvider::Get().GetFont(button_context,
                                            style::STYLE_PRIMARY)),
      cached_default_button_font_list_(TypographyProvider::Get().GetFont(
          button_context,
          style::STYLE_DIALOG_BUTTON_DEFAULT)),
      appear_disabled_in_inactive_widget_(
          PlatformStyle::kInactiveWidgetControlsAppearDisabled) {
  ink_drop_container_ = AddChildView(
      Builder<InkDropContainerView>().SetAutoMatchParentBounds(true).Build());
  ink_drop_container_->SetVisible(false);
  ink_drop_container_->SetProperty(kViewIgnoredByLayoutKey, true);

  AddChildView(image_container_->CreateView());

  label_ = AddChildView(
      std::make_unique<internal::LabelButtonLabel>(text, button_context));
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);

  SetAnimationDuration(base::Milliseconds(170));
  SetTextInternal(text);
  SetLayoutManager(std::make_unique<DelegatingLayoutManager>(this));
  GetViewAccessibility().SetIsDefault(is_default_);
}

LabelButton::~LabelButton() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerFromRegions(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

gfx::ImageSkia LabelButton::GetImage(ButtonState state) const {
  state = ImageStateForState(state);
  auto image_model = GetImageModel(state).value_or(ui::ImageModel());
  return image_model.Rasterize(GetColorProvider());
}

const std::optional<ui::ImageModel>& LabelButton::GetImageModel(
    ButtonState state) const {
  return button_state_image_models_[state];
}

void LabelButton::SetImageModel(
    ButtonState state,
    const std::optional<ui::ImageModel>& image_model) {
  if (button_state_image_models_[state] == image_model) {
    return;
  }

  const auto old_image_state = ImageStateForState(GetVisualState());

  button_state_image_models_[state] = image_model;

  if (state == old_image_state ||
      state == ImageStateForState(GetVisualState())) {
    UpdateImage();
  }
}

bool LabelButton::HasImage(ButtonState state) const {
  return button_state_image_models_[state].has_value() &&
         !button_state_image_models_[state]->IsEmpty();
}

const std::u16string& LabelButton::GetText() const {
  return label_->GetText();
}

void LabelButton::SetText(const std::u16string& text) {
  SetTextInternal(text);
}

void LabelButton::SetLabelStyle(views::style::TextStyle text_style) {
  label_->SetTextStyle(text_style);
}

void LabelButton::ShrinkDownThenClearText() {
  if (GetText().empty())
    return;
  // First, we recalculate preferred size for the new mode (without the label).
  shrinking_down_label_ = true;
  PreferredSizeChanged();
  // Second, we clear the label right away if the button is already small.
  ClearTextIfShrunkDown();
}

void LabelButton::SetTextColor(ButtonState for_state, SkColor color) {
  button_state_colors_[for_state] = color;
  if (for_state == STATE_DISABLED)
    label_->SetDisabledColor(color);
  else if (for_state == GetState())
    label_->SetEnabledColor(color);
  explicitly_set_colors_[for_state] = true;
}

void LabelButton::SetTextColorId(ButtonState for_state, ui::ColorId color_id) {
  button_state_colors_[for_state] = color_id;
  if (for_state == STATE_DISABLED) {
    label_->SetDisabledColorId(color_id);
  } else if (for_state == GetState()) {
    label_->SetEnabledColorId(color_id);
  }
  explicitly_set_colors_[for_state] = true;
}

float LabelButton::GetFocusRingCornerRadius() const {
  return focus_ring_corner_radius_;
}

void LabelButton::SetFocusRingCornerRadius(float radius) {
  if (focus_ring_corner_radius_ == radius)
    return;
  focus_ring_corner_radius_ = radius;
  InkDrop::Get(this)->SetSmallCornerRadius(focus_ring_corner_radius_);
  InkDrop::Get(this)->SetLargeCornerRadius(focus_ring_corner_radius_);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                focus_ring_corner_radius_);
  OnPropertyChanged(&focus_ring_corner_radius_, kPropertyEffectsPaint);
}

void LabelButton::SetEnabledTextColors(std::optional<SkColor> color) {
  if (color.has_value()) {
    for (auto state : kEnabledStates) {
      SetTextColor(state, color.value());
    }
    return;
  }
  for (auto state : kEnabledStates) {
    explicitly_set_colors_[state] = false;
  }
  ResetColorsFromNativeTheme();
}

void LabelButton::SetEnabledTextColorIds(ui::ColorId color_id) {
  for (auto state : kEnabledStates) {
    SetTextColorId(state, color_id);
  }
}

SkColor LabelButton::GetCurrentTextColor() const {
  return label_->GetEnabledColor();
}

void LabelButton::SetTextShadows(const gfx::ShadowValues& shadows) {
  label_->SetShadows(shadows);
}

void LabelButton::SetTextSubpixelRenderingEnabled(bool enabled) {
  label_->SetSubpixelRenderingEnabled(enabled);
}

void LabelButton::SetElideBehavior(gfx::ElideBehavior elide_behavior) {
  label_->SetElideBehavior(elide_behavior);
}

void LabelButton::SetHorizontalAlignment(gfx::HorizontalAlignment alignment) {
  DCHECK_NE(gfx::ALIGN_TO_HEAD, alignment);
  if (GetHorizontalAlignment() == alignment)
    return;
  horizontal_alignment_ = alignment;
  OnPropertyChanged(&min_size_, kPropertyEffectsLayout);
}

gfx::HorizontalAlignment LabelButton::GetHorizontalAlignment() const {
  return horizontal_alignment_;
}

gfx::Size LabelButton::GetMinSize() const {
  return min_size_;
}

void LabelButton::SetMinSize(const gfx::Size& min_size) {
  if (GetMinSize() == min_size)
    return;
  min_size_ = min_size;
  OnPropertyChanged(&min_size_, kPropertyEffectsPreferredSizeChanged);
}

gfx::Size LabelButton::GetMaxSize() const {
  return max_size_;
}

void LabelButton::SetMaxSize(const gfx::Size& max_size) {
  if (GetMaxSize() == max_size)
    return;
  max_size_ = max_size;
  OnPropertyChanged(&max_size_, kPropertyEffectsPreferredSizeChanged);
}

bool LabelButton::GetIsDefault() const {
  return is_default_;
}

void LabelButton::SetIsDefault(bool is_default) {
  // TODO(estade): move this to MdTextButton once |style_| is removed.
  if (GetIsDefault() == is_default)
    return;
  is_default_ = is_default;

  // The default button has an accelerator for VKEY_RETURN, which clicks it.
  // Note that if PlatformStyle::kReturnClicksFocusedControl is true and another
  // button is focused, that button's VKEY_RETURN handler will take precedence.
  // See Button::GetKeyClickActionForEvent().
  ui::Accelerator accel(ui::VKEY_RETURN, ui::EF_NONE);
  if (is_default)
    AddAccelerator(accel);
  else
    RemoveAccelerator(accel);
  GetViewAccessibility().SetIsDefault(is_default);
  OnPropertyChanged(&is_default_, UpdateStyleToIndicateDefaultStatus());
}

int LabelButton::GetImageLabelSpacing() const {
  return image_label_spacing_;
}

void LabelButton::SetImageLabelSpacing(int spacing) {
  if (GetImageLabelSpacing() == spacing)
    return;
  image_label_spacing_ = spacing;
  OnPropertyChanged(&image_label_spacing_,
                    kPropertyEffectsPreferredSizeChanged);
}

bool LabelButton::GetImageCentered() const {
  return image_centered_;
}

void LabelButton::SetImageCentered(bool image_centered) {
  if (GetImageCentered() == image_centered)
    return;
  image_centered_ = image_centered;
  OnPropertyChanged(&image_centered_, kPropertyEffectsLayout);
}

std::unique_ptr<LabelButtonBorder> LabelButton::CreateDefaultBorder() const {
  auto border = std::make_unique<LabelButtonBorder>();
  border->set_insets(views::LabelButtonAssetBorder::GetDefaultInsets());
  return border;
}

void LabelButton::SetAppearDisabledInInactiveWidget(bool appear_disabled) {
  appear_disabled_in_inactive_widget_ = appear_disabled;
  if (GetWidget()) {
    AddedToWidget();
  }
}

void LabelButton::SetBorder(std::unique_ptr<Border> border) {
  explicitly_set_border_ = true;
  View::SetBorder(std::move(border));
}

void LabelButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ClearTextIfShrunkDown();
  Button::OnBoundsChanged(previous_bounds);
}

gfx::Size LabelButton::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  gfx::Size size = GetUnclampedSizeWithoutLabel();

  // Account for the label only when the button is not shrinking down to hide
  // the label entirely.
  if (!shrinking_down_label_) {
    SizeBound available_width =
        max_size_.width() > 0 ? available_size.width().min_of(max_size_.width())
                              : available_size.width();
    SizeBound label_available_width =
        std::max<SizeBound>(0, available_width - size.width());

    const gfx::Size preferred_label_size =
        label_->GetPreferredSize(SizeBounds(label_available_width, {}));
    size.Enlarge(preferred_label_size.width(), 0);
    size.SetToMax(
        gfx::Size(0, preferred_label_size.height() + GetInsets().height()));
  }

  size.SetToMax(GetMinSize());

  // Clamp size to max size (if valid).
  const gfx::Size max_size = GetMaxSize();
  if (max_size.width() > 0)
    size.set_width(std::min(max_size.width(), size.width()));
  if (max_size.height() > 0)
    size.set_height(std::min(max_size.height(), size.height()));

  return size;
}

gfx::Size LabelButton::GetMinimumSize() const {
  if (label_->GetElideBehavior() == gfx::ElideBehavior::NO_ELIDE)
    return GetPreferredSize({0, 0});

  gfx::Size size = image_container_view()->GetPreferredSize({});
  const gfx::Insets insets(GetInsets());
  size.Enlarge(insets.width(), insets.height());

  size.SetToMax(GetMinSize());
  const gfx::Size max_size = GetMaxSize();
  if (max_size.width() > 0)
    size.set_width(std::min(max_size.width(), size.width()));
  if (max_size.height() > 0)
    size.set_height(std::min(max_size.height(), size.height()));

  return size;
}

ProposedLayout LabelButton::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    layouts.host_size = gfx::Size();
    return layouts;
  }

  gfx::Rect image_area = GetLocalBounds();
  gfx::Insets insets = GetInsets();
  // If the button have a limited space to fit in, the image and the label
  // may overlap with the border, which often times contains a lot of empty
  // padding.
  image_area.Inset(gfx::Insets::TLBR(0, insets.left(), 0, insets.right()));
  // The space that the label can use. Labels truncate horizontally, so there
  // is no need to allow the label to take up the complete horizontal space.
  gfx::Rect label_area = image_area;

  gfx::Size image_size =
      image_container_view()->GetPreferredSize(SizeBounds(image_area.size()));
  image_size.SetToMin(image_area.size());

  const auto horizontal_alignment = GetHorizontalAlignment();
  if (!image_size.IsEmpty()) {
    int image_space = image_size.width() + GetImageLabelSpacing();
    if (horizontal_alignment == gfx::ALIGN_RIGHT)
      label_area.Inset(gfx::Insets::TLBR(0, 0, 0, image_space));
    else
      label_area.Inset(gfx::Insets::TLBR(0, image_space, 0, 0));
  }

  gfx::Size label_size(
      std::min(label_area.width(),
               label_->GetPreferredSize(SizeBounds(label_area.size())).width()),
      label_area.height());

  gfx::Point image_origin = image_area.origin();
  if (label_->GetMultiLine() && !image_centered_) {
    // This code assumes the text is vertically centered.
    DCHECK_EQ(gfx::ALIGN_MIDDLE, label_->GetVerticalAlignment());
    int label_height = label_->GetHeightForWidth(label_size.width());
    int first_line_y =
        label_area.y() + (label_area.height() - label_height) / 2;
    int image_origin_y =
        first_line_y +
        (label_->font_list().GetHeight() - image_size.height()) / 2;
    image_origin.Offset(0, std::max(0, image_origin_y));
  } else {
    image_origin.Offset(0, (image_area.height() - image_size.height()) / 2);
  }
  if (horizontal_alignment == gfx::ALIGN_CENTER) {
    const int spacing = (image_size.width() > 0 && label_size.width() > 0)
                            ? GetImageLabelSpacing()
                            : 0;
    const int total_width = image_size.width() + label_size.width() + spacing;
    image_origin.Offset((image_area.width() - total_width) / 2, 0);
  } else if (horizontal_alignment == gfx::ALIGN_RIGHT) {
    image_origin.Offset(image_area.width() - image_size.width(), 0);
  }
  layouts.child_layouts.emplace_back(
      const_cast<LabelButton*>(this)->image_container_view(),
      static_cast<DelegatingLayoutManager*>(GetLayoutManager())
          ->CanBeVisible(image_container_view()),
      gfx::Rect(image_origin, image_size), SizeBounds());

  gfx::Rect label_bounds = label_area;
  if (label_area.width() == label_size.width()) {
    // Label takes up the whole area.
  } else if (horizontal_alignment == gfx::ALIGN_CENTER) {
    label_bounds.ClampToCenteredSize(label_size);
  } else {
    label_bounds.set_size(label_size);
    if (horizontal_alignment == gfx::ALIGN_RIGHT) {
      label_bounds.Offset(label_area.width() - label_size.width(), 0);
    }
  }

  layouts.child_layouts.emplace_back(
      label_.get(),
      static_cast<DelegatingLayoutManager*>(GetLayoutManager())
          ->CanBeVisible(label_.get()),
      label_bounds, SizeBounds());
  layouts.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());

  return layouts;
}

ui::NativeTheme::Part LabelButton::GetThemePart() const {
  return ui::NativeTheme::kPushButton;
}

gfx::Rect LabelButton::GetThemePaintRect() const {
  return GetLocalBounds();
}

ui::NativeTheme::State LabelButton::GetThemeState(
    ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  switch (GetState()) {
    case STATE_NORMAL:
      return ui::NativeTheme::kNormal;
    case STATE_HOVERED:
      return ui::NativeTheme::kHovered;
    case STATE_PRESSED:
      return ui::NativeTheme::kPressed;
    case STATE_DISABLED:
      return ui::NativeTheme::kDisabled;
    case STATE_COUNT:
      NOTREACHED();
  }
  return ui::NativeTheme::kNormal;
}

const gfx::Animation* LabelButton::GetThemeAnimation() const {
  return &hover_animation();
}

ui::NativeTheme::State LabelButton::GetBackgroundThemeState(
    ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return ui::NativeTheme::kNormal;
}

ui::NativeTheme::State LabelButton::GetForegroundThemeState(
    ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return ui::NativeTheme::kHovered;
}

void LabelButton::UpdateImage() {
  if (GetWidget())
    image_container()->UpdateImage(this);
}

void LabelButton::AddLayerToRegion(ui::Layer* new_layer,
                                   views::LayerRegion region) {
  image_container_view()->SetPaintToLayer();
  image_container_view()->layer()->SetFillsBoundsOpaquely(false);
  ink_drop_container()->SetVisible(true);
  ink_drop_container()->AddLayerToRegion(new_layer, region);
}

void LabelButton::RemoveLayerFromRegions(ui::Layer* old_layer) {
  ink_drop_container()->RemoveLayerFromRegions(old_layer);
  ink_drop_container()->SetVisible(false);
  image_container_view()->DestroyLayer();
}

std::unique_ptr<ActionViewInterface> LabelButton::GetActionViewInterface() {
  return std::make_unique<LabelButtonActionViewInterface>(this);
}

void LabelButton::GetExtraParams(ui::NativeTheme::ExtraParams* params) const {
  auto& button = absl::get<ui::NativeTheme::ButtonExtraParams>(*params);
  button.checked = false;
  button.indeterminate = false;
  button.is_default = GetIsDefault();
  button.is_focused =
      HasFocus() && GetViewAccessibility().IsAccessibilityFocusable();
  button.has_border = false;
  button.classic_state = 0;
  button.background_color = label_->GetBackgroundColor();
}

PropertyEffects LabelButton::UpdateStyleToIndicateDefaultStatus() {
  // Check that a subclass hasn't replaced the Label font. These buttons may
  // never be given default status.
  DCHECK_EQ(cached_normal_font_list_.GetFontSize(),
            label()->font_list().GetFontSize());
  // TODO(tapted): This should use TypographyProvider::Get().GetFont(), but this
  // part can just be deleted when default buttons no longer go bold. Colors
  // will need updating still.
  label_->SetFontList(GetIsDefault() ? cached_default_button_font_list_
                                     : cached_normal_font_list_);
  ResetLabelEnabledColor();
  return kPropertyEffectsPreferredSizeChanged;
}

void LabelButton::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

void LabelButton::AddedToWidget() {
  if (appear_disabled_in_inactive_widget_) {
    paint_as_active_subscription_ =
        GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
            &LabelButton::VisualStateChanged, base::Unretained(this)));
    // Set the initial state correctly.
    VisualStateChanged();
  }
}

void LabelButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

void LabelButton::OnFocus() {
  Button::OnFocus();
  // Typically the border renders differently when focused.
  SchedulePaint();
}

void LabelButton::OnBlur() {
  Button::OnBlur();
  // Typically the border renders differently when focused.
  SchedulePaint();
}

void LabelButton::OnThemeChanged() {
  Button::OnThemeChanged();
  ResetColorsFromNativeTheme();
  UpdateImage();
  if (!explicitly_set_border_)
    View::SetBorder(CreateDefaultBorder());
  ResetLabelEnabledColor();
  // The entire button has to be repainted here, since the native theme can
  // define the tint for the entire background/border/focus ring.
  SchedulePaint();
}

void LabelButton::StateChanged(ButtonState old_state) {
  Button::StateChanged(old_state);
  ResetLabelEnabledColor();
  VisualStateChanged();
}

void LabelButton::SetTextInternal(const std::u16string& text) {
  GetViewAccessibility().SetName(text);
  label_->SetText(text);

  // Setting text cancels ShrinkDownThenClearText().
  const auto effects = shrinking_down_label_
                           ? kPropertyEffectsPreferredSizeChanged
                           : kPropertyEffectsNone;
  shrinking_down_label_ = false;

  // TODO(pkasting): Remove this and forward callback subscriptions to the
  // underlying label property when Label is converted to properties.
  OnPropertyChanged(label_, effects);
}

void LabelButton::ClearTextIfShrunkDown() {
  const gfx::Size preferred_size = GetPreferredSize({});
  if (shrinking_down_label_ && width() <= preferred_size.width() &&
      height() <= preferred_size.height()) {
    // Once the button shrinks down to its preferred size (that disregards the
    // current text), we finish the operation by clearing the text.
    SetText(std::u16string());
  }
}

gfx::Size LabelButton::GetUnclampedSizeWithoutLabel() const {
  const gfx::Size image_size = image_container_view()->GetPreferredSize({});
  gfx::Size size = image_size;
  const gfx::Insets insets(GetInsets());
  size.Enlarge(insets.width(), insets.height());

  // Accommodate for spacing between image and text if both are present.
  if (image_size.width() > 0 && !GetText().empty() && !shrinking_down_label_)
    size.Enlarge(GetImageLabelSpacing(), 0);

  // Make the size at least as large as the minimum size needed by the border.
  if (GetBorder())
    size.SetToMax(GetBorder()->GetMinimumSize());

  return size;
}

Button::ButtonState LabelButton::GetVisualState() const {
  const auto* widget = GetWidget();
  if (!widget || !widget->ShouldViewsStyleFollowWidgetActivation() ||
      !appear_disabled_in_inactive_widget_) {
    return GetState();
  }

  // Paint as inactive if neither this widget nor its parent should paint as
  // active.
  if (!widget->ShouldPaintAsActive() &&
      !(widget->parent() && widget->parent()->ShouldPaintAsActive()))
    return STATE_DISABLED;

  return GetState();
}

void LabelButton::VisualStateChanged() {
  if (GetWidget()) {
    UpdateImage();
    UpdateBackgroundColor();
  }
  label_->SetEnabled(GetVisualState() != STATE_DISABLED);
}

void LabelButton::ResetColorsFromNativeTheme() {
  // Since this is a LabelButton, use the label colors.
  constexpr std::array<ui::ColorId, STATE_COUNT> color_ids{
      {ui::kColorLabelForeground, ui::kColorLabelForeground,
       ui::kColorLabelForeground, ui::kColorLabelForegroundDisabled}};

  label_->SetBackground(nullptr);
  label_->SetAutoColorReadabilityEnabled(false);

  for (size_t state = STATE_NORMAL; state < STATE_COUNT; ++state) {
    if (!explicitly_set_colors_[state]) {
      SetTextColorId(static_cast<ButtonState>(state), color_ids[state]);
      explicitly_set_colors_[state] = false;
    }
  }
}

void LabelButton::ResetLabelEnabledColor() {
  if (GetState() == STATE_DISABLED) {
    return;
  }
  const absl::variant<SkColor, ui::ColorId>& color =
      button_state_colors_[GetState()];
  if (absl::holds_alternative<SkColor>(color) &&
      label_->GetEnabledColor() != absl::get<SkColor>(color)) {
    label_->SetEnabledColor(absl::get<SkColor>(color));
  } else if (absl::holds_alternative<ui::ColorId>(color)) {
    // Omitting the check that the new color id differs from the existing color
    // id, because the setter already does that check.
    label_->SetEnabledColorId(absl::get<ui::ColorId>(color));
  }
}

Button::ButtonState LabelButton::ImageStateForState(
    ButtonState for_state) const {
  return button_state_image_models_[for_state].has_value() ? for_state
                                                           : STATE_NORMAL;
}

void LabelButton::FlipCanvasOnPaintForRTLUIChanged() {
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(
      GetFlipCanvasOnPaintForRTLUI());
}

LabelButtonActionViewInterface::LabelButtonActionViewInterface(
    LabelButton* action_view)
    : ButtonActionViewInterface(action_view), action_view_(action_view) {}

void LabelButtonActionViewInterface::ActionItemChangedImpl(
    actions::ActionItem* action_item) {
  ButtonActionViewInterface::ActionItemChangedImpl(action_item);
  action_view_->SetText(action_item->GetText());
  action_view_->SetImageModel(action_view_->GetState(),
                              action_item->GetImage());
}

BEGIN_METADATA(LabelButton)
ADD_PROPERTY_METADATA(std::u16string, Text)
ADD_PROPERTY_METADATA(gfx::HorizontalAlignment, HorizontalAlignment)
ADD_PROPERTY_METADATA(gfx::Size, MinSize)
ADD_PROPERTY_METADATA(gfx::Size, MaxSize)
ADD_PROPERTY_METADATA(bool, IsDefault)
ADD_PROPERTY_METADATA(int, ImageLabelSpacing)
ADD_PROPERTY_METADATA(bool, ImageCentered)
ADD_PROPERTY_METADATA(float, FocusRingCornerRadius)
END_METADATA

}  // namespace views
