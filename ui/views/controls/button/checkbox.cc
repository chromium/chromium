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
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"

namespace views {

// static
const char Checkbox::kViewClassName[] = "Checkbox";

Checkbox::Checkbox(const base::string16& label, ButtonListener* listener)
    : LabelButton(listener, label), checked_(false), label_ax_id_(0) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetFocusForPlatform();
  SetFocusPainter(nullptr);

  set_request_focus_on_press(false);
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);

  // Limit the checkbox height to match the legacy appearance.
  const gfx::Size preferred_size(LabelButton::CalculatePreferredSize());
  SetMinSize(gfx::Size(0, preferred_size.height() + 4));

  // Checkboxes always have a focus ring, even when the platform otherwise
  // doesn't generally use them for buttons.
  SetInstallFocusRingOnFocus(true);
}

Checkbox::~Checkbox() {
}

void Checkbox::SetChecked(bool checked) {
  if (checked_ != checked) {
    checked_ = checked;
    NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);
  }
  UpdateImage();
}

void Checkbox::SetMultiLine(bool multi_line) {
  label()->SetMultiLine(multi_line);
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

const char* Checkbox::GetClassName() const {
  return kViewClassName;
}

void Checkbox::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  LabelButton::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kCheckBox;
  const ax::mojom::CheckedState checked_state =
      checked() ? ax::mojom::CheckedState::kTrue
                : ax::mojom::CheckedState::kFalse;
  node_data->SetCheckedState(checked_state);
  if (enabled()) {
    if (checked()) {
      node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kUncheck);
    } else {
      node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kCheck);
    }
  }
  if (label_ax_id_) {
    node_data->AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                                   {label_ax_id_});
  }
}

void Checkbox::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  LabelButton::OnNativeThemeChanged(theme);
  UpdateImage();
}

std::unique_ptr<InkDrop> Checkbox::CreateInkDrop() {
  // Completely removes the highlight.
  std::unique_ptr<InkDropImpl> ink_drop = CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetAutoHighlightMode(InkDropImpl::AutoHighlightMode::NONE);
  return ink_drop;
}

std::unique_ptr<InkDropRipple> Checkbox::CreateInkDropRipple() const {
  // The "small" size is 21dp, the large size is 1.33 * 21dp = 28dp.
  return CreateDefaultInkDropRipple(image()->GetMirroredBounds().CenterPoint(),
                                    gfx::Size(21, 21));
}

SkColor Checkbox::GetInkDropBaseColor() const {
  // Usually ink drop ripples match the text color. Checkboxes use the color of
  // the unchecked, enabled icon.
  return GetIconImageColor(IconState::ENABLED);
}

gfx::ImageSkia Checkbox::GetImage(ButtonState for_state) const {
  const int checked = checked_ ? IconState::CHECKED : 0;
  const int enabled = for_state != STATE_DISABLED ? IconState::ENABLED : 0;
  return gfx::CreateVectorIcon(GetVectorIcon(), 16,
                               GetIconImageColor(checked | enabled));
}

std::unique_ptr<LabelButtonBorder> Checkbox::CreateDefaultBorder() const {
  std::unique_ptr<LabelButtonBorder> border =
      LabelButton::CreateDefaultBorder();
  border->set_insets(
      LayoutProvider::Get()->GetInsetsMetric(INSETS_CHECKBOX_RADIO_BUTTON));
  return border;
}

void Checkbox::Layout() {
  LabelButton::Layout();
  if (focus_ring() && !image()->bounds().IsEmpty())
    focus_ring()->SetPath(GetFocusRingPath());
}

SkPath Checkbox::GetFocusRingPath() const {
  SkPath path;
  gfx::Rect bounds = image()->GetMirroredBounds();
  bounds.Inset(1, 1);
  path.addRect(RectToSkRect(bounds));
  return path;
}

const gfx::VectorIcon& Checkbox::GetVectorIcon() const {
  return checked() ? kCheckboxActiveIcon : kCheckboxNormalIcon;
}

SkColor Checkbox::GetIconImageColor(int icon_state) const {
  const SkColor active_color =
      (icon_state & IconState::CHECKED)
          ? GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_FocusedBorderColor)
          // When unchecked, the icon color matches push button text color.
          : style::GetColor(*this, style::CONTEXT_BUTTON_MD,
                            style::STYLE_PRIMARY);
  return (icon_state & IconState::ENABLED)
             ? active_color
             : color_utils::BlendTowardOppositeLuma(active_color,
                                                    gfx::kDisabledControlAlpha);
}

void Checkbox::NotifyClick(const ui::Event& event) {
  SetChecked(!checked());
  LabelButton::NotifyClick(event);
}

ui::NativeTheme::Part Checkbox::GetThemePart() const {
  return ui::NativeTheme::kCheckbox;
}

void Checkbox::GetExtraParams(ui::NativeTheme::ExtraParams* params) const {
  LabelButton::GetExtraParams(params);
  params->button.checked = checked_;
}

}  // namespace views
