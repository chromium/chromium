// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_test_util_mac.h"

#include "base/apple/foundation_util.h"
#include "ui/base/cocoa/menu_controller.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/models/menu_model.h"

namespace ui::test {

InteractionTestUtilSimulatorMac::InteractionTestUtilSimulatorMac() = default;
InteractionTestUtilSimulatorMac::~InteractionTestUtilSimulatorMac() = default;

ActionResult InteractionTestUtilSimulatorMac::SelectMenuItem(
    ui::TrackedElement* element,
    InputType input_type) {
  auto* const mac_element = element->AsA<TrackedElementMac>();
  if (!mac_element)
    return ActionResult::kNotAttempted;

  NSMenu* menu = ElementTrackerMac::GetInstance()->GetRootMenuForContext(
      mac_element->context());
  if (!menu)
    return ActionResult::kNotAttempted;

  if (input_type != InputType::kDontCare) {
    LOG(WARNING) << "SelectMenuItem on Mac does not support specific input "
                    "types; use InputType::kDontCare";
    return ActionResult::kKnownIncompatible;
  }

  MenuControllerCocoa* controller =
      base::apple::ObjCCastStrict<MenuControllerCocoa>([menu delegate]);
  if (!controller) {
    LOG(ERROR) << "Cannot retrieve MenuControllerCocoa from menu.";
    return ActionResult::kFailed;
  }
  ui::MenuModel* const model = [controller model];
  if (!model) {
    LOG(ERROR) << "Cannot retrieve MenuModel from controller.";
    return ActionResult::kFailed;
  }

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (model->GetElementIdentifierAt(i) == element->identifier()) {
      NSMenuItem* item = [menu itemWithTag:i];
      if (item) {
        DCHECK([item action]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
        [controller performSelector:[item action] withObject:item];
#pragma clang diagnostic pop
        [controller cancel];
        return ActionResult::kSucceeded;
      }
    }
  }

  LOG(ERROR) << "Item with id " << element->identifier()
             << " not found in menu.";
  return ActionResult::kFailed;
}

}  // namespace ui::test
