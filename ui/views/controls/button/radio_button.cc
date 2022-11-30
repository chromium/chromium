// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/radio_button.h"

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

constexpr int kFocusRingRadius = 16;

}  // namespace

RadioButton::RadioButton(const std::u16string& label, int group_id)
    : Checkbox(label) {
  SetGroup(group_id);
}

RadioButton::~RadioButton() = default;

void RadioButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Checkbox::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kRadioButton;
}

View* RadioButton::GetSelectedViewForGroup(int group) {
  Views views;
  GetViewsInGroupFromParent(group, &views);
  const auto i = base::ranges::find_if(views, [](const auto* view) {
    // Why don't we check the runtime type like is done in SetChecked()?
    return static_cast<const RadioButton*>(view)->GetChecked();
  });
  return (i == views.cend()) ? nullptr : *i;
}

bool RadioButton::HandleAccessibleAction(const ui::AXActionData& action_data) {
  if (action_data.action == ax::mojom::Action::kFocus) {
    if (IsAccessibilityFocusable()) {
      base::AutoReset<bool> reset(&select_on_focus_, false);
      RequestFocus();
      return true;
    }
  }
  return Checkbox::HandleAccessibleAction(action_data);
}

bool RadioButton::IsGroupFocusTraversable() const {
  // When focusing a radio button with tab/shift+tab, only the selected button
  // from the group should be focused.
  return false;
}

void RadioButton::OnFocus() {
  Checkbox::OnFocus();
  if (select_on_focus_)
    SetChecked(true);
}

void RadioButton::OnThemeChanged() {
  Checkbox::OnThemeChanged();
  SchedulePaint();
}

void RadioButton::RequestFocusFromEvent() {
  Checkbox::RequestFocusFromEvent();
  // Take focus only if another radio button in the group has focus.
  Views views;
  GetViewsInGroupFromParent(GetGroup(), &views);
  if (base::ranges::any_of(views, [](View* v) { return v->HasFocus(); }))
    RequestFocus();
}

void RadioButton::NotifyClick(const ui::Event& event) {
  // Set the checked state to true only if we are unchecked, since we can't
  // be toggled on and off like a checkbox.
  if (!GetChecked())
    SetChecked(true);
  LabelButton::NotifyClick(event);
}

ui::NativeTheme::Part RadioButton::GetThemePart() const {
  return ui::NativeTheme::kRadio;
}

void RadioButton::SetChecked(bool checked) {
  if (checked == RadioButton::GetChecked())
    return;
  if (checked) {
    // We can't start from the root view because other parts of the UI might use
    // radio buttons with the same group. This can happen when re-using the same
    // component or even if views want to use the group for a different purpose.
    Views other;
    GetViewsInGroupFromParent(GetGroup(), &other);
    for (auto* peer : other) {
      if (peer != this) {
        DCHECK(!strcmp(peer->GetClassName(), kViewClassName))
            << "radio-button-nt has same group as non radio-button-nt views.";
        static_cast<RadioButton*>(peer)->SetChecked(false);
      }
    }
  }
  Checkbox::SetChecked(checked);
}

const gfx::VectorIcon& RadioButton::GetVectorIcon() const {
  return GetChecked() ? kRadioButtonActiveIcon : kRadioButtonNormalIcon;
}

SkPath RadioButton::GetFocusRingPath() const {
  SkPath path;
  const gfx::Point center = image()->GetMirroredBounds().CenterPoint();
  path.addCircle(center.x(), center.y(), kFocusRingRadius);
  return path;
}

void RadioButton::GetViewsInGroupFromParent(int group, Views* views) {
  if (parent())
    parent()->GetViewsInGroup(group, views);
}

BEGIN_METADATA(RadioButton, Checkbox)
END_METADATA

}  // namespace views
