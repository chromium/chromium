// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/checkbox.h"

#include <stddef.h>

#include <utility>

#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
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

Checkbox::Checkbox(const base::string16& label, ButtonListener* listener)
    : LabelButton(listener, label), checked_(false), label_ax_id_(0) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetFocusForPlatform();

  set_request_focus_on_press(false);
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);

  // Limit the checkbox height to match the legacy appearance.
  const gfx::Size preferred_size(LabelButton::CalculatePreferredSize());
  SetMinSize(gfx::Size(0, preferred_size.height() + 4));

  // Checkboxes always have a focus ring, even when the platform otherwise
  // doesn't generally use them for buttons.
  SetInstallFocusRingOnFocus(true);
  focus_ring()->SetPathGenerator(
      std::make_unique<FocusRingHighlightPathGenerator>());
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
  label_ax_id_ = labelling_view->GetViewAccessibility().GetUniqueId().Get();
  ui::AXNodeData node_data;
  labelling_view->GetAccessibleNodeData(&node_data);
  // TODO(aleventhal) automatically handle setting the name from the related
  // label in ViewAccessibility and have it update the name if the text of the
  // associated label changes.
  SetAccessibleName(
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
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
  if (label_ax_id_) {
    node_data->AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                                   {label_ax_id_});
  }
}

void Checkbox::OnThemeChanged() {
  LabelButton::OnThemeChanged();
  UpdateImage();
}

std::unique_ptr<InkDrop> Checkbox::CreateInkDrop() {
  std::unique_ptr<InkDropImpl> ink_drop = CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetAutoHighlightMode(InkDropImpl::AutoHighlightMode::NONE);
  return ink_drop;
}

std::unique_ptr<InkDropRipple> Checkbox::CreateInkDropRipple() const {
  // The "small" size is 21dp, the large size is 1.33 * 21dp = 28dp.
  return CreateSquareInkDropRipple(image()->GetMirroredBounds().CenterPoint(),
                                   gfx::Size(21, 21));
}

std::unique_ptr<InkDropMask> Checkbox::CreateInkDropMask() const {
  // Avoid the default ink-drop mask to allow the ripple effect to extend beyond
  // the checkbox view (otherwise it gets clipped which looks weird).
  return nullptr;
}

SkColor Checkbox::GetInkDropBaseColor() const {
  // Usually ink-drop ripples match the text color. Checkboxes use the color of
  // the unchecked, enabled icon.
  return GetIconImageColor(IconState::ENABLED);
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

SkPath Checkbox::GetFocusRingPath() const {
  SkPath path;
  gfx::Rect bounds = image()->GetMirroredBounds();
  bounds.Inset(1, 1);
  path.addRect(RectToSkRect(bounds));
  return path;
}

const gfx::VectorIcon& Checkbox::GetVectorIcon() const {
  return GetChecked() ? kCheckboxActiveIcon : kCheckboxNormalIcon;
}

SkColor Checkbox::GetIconImageColor(int icon_state) const {
  const SkColor active_color = GetNativeTheme()->GetSystemColor(
      (icon_state & IconState::CHECKED)
          ? ui::NativeTheme::kColorId_ProminentButtonColor
          : ui::NativeTheme::kColorId_ButtonEnabledColor);
  return (icon_state & IconState::ENABLED)
             ? active_color
             : color_utils::BlendTowardMaxContrast(active_color,
                                                   gfx::kDisabledControlAlpha);
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

BEGIN_METADATA(Checkbox)
METADATA_PARENT_CLASS(LabelButton)
ADD_PROPERTY_METADATA(Checkbox, bool, Checked)
ADD_PROPERTY_METADATA(Checkbox, bool, MultiLine)
END_METADATA()

}  // namespace views
