// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/checkbox.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"

namespace views {

namespace {
constexpr gfx::Size kCheckboxInkDropSize = gfx::Size(24, 24);
constexpr float kCheckboxIconDipSize = 16;
constexpr int kCheckboxIconCornerRadius = 2;
}

class Checkbox::FocusRingHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  SkPath GetHighlightPath(const views::View* view) override {
    SkPath path;
    auto* checkbox = static_cast<const views::Checkbox*>(view);
    if (checkbox->image_container_view()->bounds().IsEmpty()) {
      return path;
    }
    return checkbox->GetFocusRingPath();
  }
};

Checkbox::Checkbox(const std::u16string& label,
                   PressedCallback callback,
                   int button_context)
    : LabelButton(std::move(callback), label, button_context) {
  SetImageCentered(false);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);

  SetRequestFocusOnPress(false);
  InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  InkDrop::UseInkDropWithoutAutoHighlight(InkDrop::Get(this),
                                          /*highlight_on_hover=*/false);
  InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](Checkbox* host) {
        // The "small" size is 21dp, the large size is 1.33 * 21dp = 28dp.
        return InkDrop::Get(host)->CreateSquareRipple(
            host->image_container_view()
                ->GetMirroredContentsBounds()
                .CenterPoint(),
            gfx::Size(21, 21));
      },
      this));
  InkDrop::Get(this)->SetBaseColorCallback(base::BindRepeating(
      [](Checkbox* host) {
        // Usually ink-drop ripples match the text color. Checkboxes use the
        // color of the unchecked, enabled icon.
        return host->GetIconImageColor(IconState::ENABLED);
      },
      this));

  // Limit the checkbox height to match the legacy appearance.
  const gfx::Size preferred_size(LabelButton::CalculatePreferredSize({}));
  SetMinSize(gfx::Size(0, preferred_size.height() + 4));

  // Checkboxes always have a focus ring, even when the platform otherwise
  // doesn't generally use them for buttons.
  SetInstallFocusRingOnFocus(true);
  FocusRing::Get(this)->SetPathGenerator(
      std::make_unique<FocusRingHighlightPathGenerator>());

  // Avoid the default ink-drop mask to allow the ripple effect to extend beyond
  // the checkbox view (otherwise it gets clipped which looks weird).
  views::InstallEmptyHighlightPathGenerator(this);

  InkDrop::Install(image_container_view(),
                   std::make_unique<InkDropHost>(image_container_view()));
  SetInkDropView(image_container_view());
  InkDrop::Get(image_container_view())->SetMode(InkDropHost::InkDropMode::ON);

  // Allow ImageView to capture mouse events in order for InkDrop effects to
  // trigger.
  image_container_view()->SetCanProcessEventsWithinSubtree(true);

  // Avoid the default ink-drop mask to allow the InkDrop effect to extend
  // beyond the image view (otherwise it gets clipped which looks weird).
  views::InstallEmptyHighlightPathGenerator(image_container_view());

  InkDrop::Get(image_container_view())
      ->SetCreateHighlightCallback(base::BindRepeating(
          [](View* host) {
            int radius =
                InkDropHost::GetLargeSize(kCheckboxInkDropSize).width() / 2;
            return std::make_unique<views::InkDropHighlight>(
                gfx::PointF(host->GetContentsBounds().CenterPoint()),
                std::make_unique<CircleLayerDelegate>(
                    views::InkDrop::Get(host)->GetBaseColor(), radius));
          },
          image_container_view()));

  InkDrop::Get(image_container_view())
      ->SetCreateRippleCallback(base::BindRepeating(
          [](View* host) {
            return InkDrop::Get(host)->CreateSquareRipple(
                host->GetContentsBounds().CenterPoint(), kCheckboxInkDropSize);
          },
          image_container_view()));

  // Usually ink-drop ripples match the text color. Checkboxes use the
  // color of the unchecked, enabled icon.
  InkDrop::Get(image_container_view())
      ->SetBaseColorId(ui::kColorCheckboxForegroundUnchecked);

  GetViewAccessibility().SetRole(ax::mojom::Role::kCheckBox);
  SetAndUpdateAccessibleDefaultActionVerb();
}

Checkbox::~Checkbox() = default;

void Checkbox::SetChecked(bool checked) {
  if (GetChecked() == checked)
    return;
  checked_ = checked;
  NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);
  UpdateImage();
  OnPropertyChanged(&checked_, kPropertyEffectsNone);
  NotifyViewControllerCallback();
  SetAndUpdateAccessibleDefaultActionVerb();
}

bool Checkbox::GetChecked() const {
  return checked_;
}

base::CallbackListSubscription Checkbox::AddCheckedChangedCallback(
    PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(&checked_, callback);
}

void Checkbox::SetMultiLine(bool multi_line) {
  if (GetMultiLine() == multi_line)
    return;
  label()->SetMultiLine(multi_line);
  // TODO(pkasting): Remove this and forward callback subscriptions to the
  // underlying label property when Label is converted to properties.
  OnPropertyChanged(this, kPropertyEffectsNone);
}

bool Checkbox::GetMultiLine() const {
  return label()->GetMultiLine();
}

void Checkbox::SetCheckedIconImageColor(SkColor color) {
  checked_icon_image_color_ = color;
}

void Checkbox::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  LabelButton::GetAccessibleNodeData(node_data);
  const ax::mojom::CheckedState checked_state =
      GetChecked() ? ax::mojom::CheckedState::kTrue
                   : ax::mojom::CheckedState::kFalse;
  node_data->SetCheckedState(checked_state);
}

gfx::ImageSkia Checkbox::GetImage(ButtonState for_state) const {
  const int icon_state = GetIconState(for_state);

  const SkColor container_color = GetIconImageColor(icon_state);
  if (GetChecked()) {
    const gfx::ImageSkia check_icon = gfx::CreateVectorIcon(
        GetVectorIcon(), kCheckboxIconDipSize, GetIconCheckColor(icon_state));

    return gfx::ImageSkiaOperations::CreateImageWithRoundRectBackground(
        gfx::SizeF(kCheckboxIconDipSize, kCheckboxIconDipSize),
        kCheckboxIconCornerRadius, container_color, check_icon);
  }
  return gfx::CreateVectorIcon(GetVectorIcon(), kCheckboxIconDipSize,
                               container_color);
}

std::unique_ptr<LabelButtonBorder> Checkbox::CreateDefaultBorder() const {
  std::unique_ptr<LabelButtonBorder> border =
      LabelButton::CreateDefaultBorder();
  border->set_insets(
      LayoutProvider::Get()->GetInsetsMetric(INSETS_CHECKBOX_RADIO_BUTTON));
  return border;
}

std::unique_ptr<ActionViewInterface> Checkbox::GetActionViewInterface() {
  return std::make_unique<CheckboxActionViewInterface>(this);
}

void Checkbox::OnThemeChanged() {
  LabelButton::OnThemeChanged();
}

SkPath Checkbox::GetFocusRingPath() const {
  SkPath path;
  gfx::Rect bounds = image_container_view()->GetMirroredContentsBounds();
  path.addRect(RectToSkRect(bounds));
  return path;
}

SkColor Checkbox::GetIconImageColor(int icon_state) const {
  if (icon_state & IconState::CHECKED) {
    return GetColorProvider()->GetColor(
        (icon_state & IconState::ENABLED)
            ? ui::kColorCheckboxContainer
            : ui::kColorCheckboxContainerDisabled);
  }
  return GetColorProvider()->GetColor((icon_state & IconState::ENABLED)
                                          ? ui::kColorCheckboxOutline
                                          : ui::kColorCheckboxOutlineDisabled);
}

SkColor Checkbox::GetIconCheckColor(int icon_state) const {
  DCHECK(GetChecked());

  // Use the overridden checked icon image color instead if set.
  if (checked_icon_image_color_.has_value()) {
    return checked_icon_image_color_.value();
  }

  return GetColorProvider()->GetColor((icon_state & IconState::ENABLED)
                                          ? ui::kColorCheckboxCheck
                                          : ui::kColorCheckboxCheckDisabled);
}

const gfx::VectorIcon& Checkbox::GetVectorIcon() const {
  return GetChecked() ? kCheckboxCheckCr2023Icon : kCheckboxNormalCr2023Icon;
}

int Checkbox::GetIconState(ButtonState for_state) const {
  int icon_state = 0;
  if (GetChecked()) {
    icon_state |= IconState::CHECKED;
  }
  if (for_state != STATE_DISABLED) {
    icon_state |= IconState::ENABLED;
  }
  return icon_state;
}

void Checkbox::NotifyClick(const ui::Event& event) {
  SetChecked(!GetChecked());
  LabelButton::NotifyClick(event);
}

ui::NativeTheme::Part Checkbox::GetThemePart() const {
  return ui::NativeTheme::kCheckbox;
}

void Checkbox::GetExtraParams(ui::NativeTheme::ExtraParams* params) const {
  LabelButton::GetExtraParams(params);
  absl::get<ui::NativeTheme::ButtonExtraParams>(*params).checked = GetChecked();
}

void Checkbox::SetAndUpdateAccessibleDefaultActionVerb() {
  SetDefaultActionVerb(checked_ ? ax::mojom::DefaultActionVerb::kUncheck
                                : ax::mojom::DefaultActionVerb::kCheck);
  UpdateAccessibleDefaultActionVerb();
}

CheckboxActionViewInterface::CheckboxActionViewInterface(Checkbox* action_view)
    : LabelButtonActionViewInterface(action_view), action_view_(action_view) {}

void CheckboxActionViewInterface::ActionItemChangedImpl(
    actions::ActionItem* action_item) {
  LabelButtonActionViewInterface::ActionItemChangedImpl(action_item);
  action_view_->SetChecked(action_item->GetChecked());
}

void CheckboxActionViewInterface::OnViewChangedImpl(
    actions::ActionItem* action_item) {
  LabelButtonActionViewInterface::OnViewChangedImpl(action_item);
  // The checked property is tied together for all checkboxes that are linked to
  // the same ActionItem.
  action_item->SetChecked(action_view_->GetChecked());
}

BEGIN_METADATA(Checkbox)
ADD_PROPERTY_METADATA(bool, Checked)
ADD_PROPERTY_METADATA(bool, MultiLine)
END_METADATA

}  // namespace views
