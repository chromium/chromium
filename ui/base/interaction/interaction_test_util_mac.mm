// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_test_util_mac.h"

#include "base/mac/foundation_util.h"
#include "ui/base/cocoa/menu_controller.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/models/menu_model.h"

namespace ui::test {

InteractionTestUtilSimulatorMac::InteractionTestUtilSimulatorMac() = default;
InteractionTestUtilSimulatorMac::~InteractionTestUtilSimulatorMac() = default;

bool InteractionTestUtilSimulatorMac::SelectMenuItem(
    ui::TrackedElement* element,
    InputType input_type) {
  auto* const mac_element = element->AsA<TrackedElementMac>();
  if (!mac_element)
    return false;

  // We can't inject specific inputs; so only "don't care" is supported through
  // direct programmatic simulation of menu engagement.
  if (input_type != InputType::kDontCare)
    return false;

  NSMenu* menu = ElementTrackerMac::GetInstance()->GetRootMenuForContext(
      mac_element->context());
  if (!menu)
    return false;

  MenuControllerCocoa* controller =
      base::mac::ObjCCastStrict<MenuControllerCocoa>([menu delegate]);
  DCHECK(controller);
  ui::MenuModel* const model = [controller model];
  if (!model)
    return false;

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (model->GetElementIdentifierAt(i) == element->identifier()) {
      NSMenuItem* item = [menu itemWithTag:i];
      if (item) {
        DCHECK([item action]);
        [controller performSelector:[item action] withObject:item];
        [controller cancel];
        return true;
      }
    }
  }

  return false;
}

}  // namespace ui::test
