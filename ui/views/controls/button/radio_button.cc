// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/radio_button.h"

#include "base/logging.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace views {

RadioButton::RadioButton(const base::string16& label, int group_id)
    : Checkbox(label, nullptr) {
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
  const auto i =
      std::find_if(views.cbegin(), views.cend(), [](const auto* view) {
        // Why don't we check the runtime type like is done in SetChecked()?
        return static_cast<const RadioButton*>(view)->GetChecked();
      });
  return (i == views.cend()) ? nullptr : *i;
}

bool RadioButton::IsGroupFocusTraversable() const {
  // When focusing a radio button with tab/shift+tab, only the selected button
  // from the group should be focused.
  return false;
}

void RadioButton::OnFocus() {
  Checkbox::OnFocus();
  SetChecked(true);
}

void RadioButton::RequestFocusFromEvent() {
  Checkbox::RequestFocusFromEvent();
  // Take focus only if another radio button in the group has focus.
  Views views;
  GetViewsInGroupFromParent(GetGroup(), &views);
  if (std::any_of(views.begin(), views.end(),
                  [](View* v) { return v->HasFocus(); }))
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
  path.addOval(gfx::RectToSkRect(image()->GetMirroredBounds()));
  return path;
}

void RadioButton::GetViewsInGroupFromParent(int group, Views* views) {
  if (parent())
    parent()->GetViewsInGroup(group, views);
}

BEGIN_METADATA(RadioButton)
METADATA_PARENT_CLASS(Checkbox)
END_METADATA()

}  // namespace views
