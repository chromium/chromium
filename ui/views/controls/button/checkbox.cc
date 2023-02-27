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
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
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

class Checkbox::FocusRingHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  SkPath GetHighlightPath(const views::View* view) override {
    SkPath path;
    auto* checkbox = static_cast<const views::Checkbox*>(view);
    if (checkbox->image()->bounds().IsEmpty())
      return path;
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
            host->image()->GetMirroredContentsBounds().CenterPoint(),
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
  const gfx::Size preferred_size(LabelButton::CalculatePreferredSize());
  SetMinSize(gfx::Size(0, preferred_size.height() + 4));

  // Checkboxes always have a focus ring, even when the platform otherwise
  // doesn't generally use them for buttons.
  SetInstallFocusRingOnFocus(true);
  FocusRing::Get(this)->SetPathGenerator(
      std::make_unique<FocusRingHighlightPathGenerator>());

  // Avoid the default ink-drop mask to allow the ripple effect to extend beyond
  // the checkbox view (otherwise it gets clipped which looks weird).
  views::InstallEmptyHighlightPathGenerator(this);
}

Checkbox::~Checkbox() = default;

void Checkbox::SetChecked(bool checked) {
  if (GetChecked() == checked)
    return;
  checked_ = checked;
  NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);
  UpdateImage();
  OnPropertyChanged(&checked_, kPropertyEffectsNone);
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

void Checkbox::SetAssociatedLabel(View* labelling_view) {
  DCHECK(labelling_view);
  GetViewAccessibility().OverrideLabelledBy(labelling_view);
  ui::AXNodeData node_data;
  labelling_view->GetAccessibleNodeData(&node_data);
  // Labelled-by relations are not common practice in native UI, so we also
  // set the checkbox accessible name for ATs which don't support that.
  // TODO(aleventhal) automatically handle setting the name from the related
  // label in ViewAccessibility and have it update the name if the text of the
  // associated label changes.
  SetAccessibleName(
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

void Checkbox::SetCheckedIconImageColor(SkColor color) {
  checked_icon_image_color_ = color;
}

void Checkbox::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  LabelButton::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kCheckBox;
  const ax::mojom::CheckedState checked_state =
      GetChecked() ? ax::mojom::CheckedState::kTrue
                   : ax::mojom::CheckedState::kFalse;
  node_data->SetCheckedState(checked_state);
  if (GetEnabled()) {
    node_data->SetDefaultActionVerb(GetChecked()
                                        ? ax::mojom::DefaultActionVerb::kUncheck
                                        : ax::mojom::DefaultActionVerb::kCheck);
  }
}

gfx::ImageSkia Checkbox::GetImage(ButtonState for_state) const {
  int icon_state = 0;
  if (GetChecked())
    icon_state |= IconState::CHECKED;
  if (for_state != STATE_DISABLED)
    icon_state |= IconState::ENABLED;
  return gfx::CreateVectorIcon(GetVectorIcon(), 16,
                               GetIconImageColor(icon_state));
}

std::unique_ptr<LabelButtonBorder> Checkbox::CreateDefaultBorder() const {
  std::unique_ptr<LabelButtonBorder> border =
      LabelButton::CreateDefaultBorder();
  border->set_insets(
      LayoutProvider::Get()->GetInsetsMetric(INSETS_CHECKBOX_RADIO_BUTTON));
  return border;
}

void Checkbox::OnThemeChanged() {
  LabelButton::OnThemeChanged();
}

SkPath Checkbox::GetFocusRingPath() const {
  SkPath path;
  gfx::Rect bounds = image()->GetMirroredContentsBounds();
  // Don't add extra insets in the ChromeRefresh case so that the focus ring can
  // be drawn in the ChromeRefresh style.
  if (!features::IsChromeRefresh2023()) {
    bounds.Inset(1);
  }
  path.addRect(RectToSkRect(bounds));
  return path;
}

SkColor Checkbox::GetIconImageColor(int icon_state) const {
  SkColor active_color =
      GetColorProvider()->GetColor((icon_state & IconState::CHECKED)
                                       ? ui::kColorCheckboxForegroundChecked
                                       : ui::kColorCheckboxForegroundUnchecked);

  // TODO(crbug.com/1394575): Remove block and update the above ColorIds
  if (features::IsChromeRefresh2023()) {
    active_color = GetColorProvider()->GetColor(
        (icon_state & IconState::CHECKED) ? ui::kColorAlertHighSeverity
                                          : ui::kColorAlertMediumSeverityIcon);
  }

  // Use the overridden checked icon image color instead if set.
  if (icon_state & IconState::CHECKED && checked_icon_image_color_.has_value())
    active_color = checked_icon_image_color_.value();

  return (icon_state & IconState::ENABLED)
             ? active_color
             : color_utils::BlendTowardMaxContrast(active_color,
                                                   gfx::kDisabledControlAlpha);
}

const gfx::VectorIcon& Checkbox::GetVectorIcon() const {
  if (features::IsChromeRefresh2023()) {
    return GetChecked() ? kCheckboxActiveCr2023Icon : kCheckboxNormalIcon;
  }

  return GetChecked() ? kCheckboxActiveIcon : kCheckboxNormalIcon;
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
  params->button.checked = GetChecked();
}

BEGIN_METADATA(Checkbox, LabelButton)
ADD_PROPERTY_METADATA(bool, Checked)
ADD_PROPERTY_METADATA(bool, MultiLine)
END_METADATA

}  // namespace views
