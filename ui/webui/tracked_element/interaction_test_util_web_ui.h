// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_TRACKED_ELEMENT_INTERACTION_TEST_UTIL_WEB_UI_H_
#define UI_WEBUI_TRACKED_ELEMENT_INTERACTION_TEST_UTIL_WEB_UI_H_

#include "ui/base/interaction/interaction_test_util.h"

namespace ui {

// WebUI implementation of InteractionTestUtil::Simulator.
// Add one to your InteractionTestUtil instance to get WebUI support for
// TrackedElementWebUI.
class InteractionTestUtilSimulatorWebUI
    : public ui::test::InteractionTestUtil::Simulator {
 public:
  InteractionTestUtilSimulatorWebUI();
  ~InteractionTestUtilSimulatorWebUI() override;

  // ui::test::InteractionTestUtil::Simulator:
  ui::test::ActionResult PressButton(ui::TrackedElement* element,
                                     InputType input_type) override;
  ui::test::ActionResult SelectMenuItem(ui::TrackedElement* element,
                                        InputType input_type) override;
  ui::test::ActionResult DoDefaultAction(ui::TrackedElement* element,
                                         InputType input_type) override;
  ui::test::ActionResult SelectTab(
      ui::TrackedElement* tab_collection,
      size_t index,
      InputType input_type,
      std::optional<size_t> expected_index_after_selection) override;
  ui::test::ActionResult SelectDropdownItem(ui::TrackedElement* dropdown,
                                            size_t index,
                                            InputType input_type) override;
  ui::test::ActionResult EnterText(ui::TrackedElement* element,
                                   std::u16string text,
                                   TextEntryMode mode) override;
  ui::test::ActionResult FocusElement(ui::TrackedElement* element) override;
  ui::test::ActionResult Confirm(ui::TrackedElement* element) override;
};

}  // namespace ui

#endif  // UI_WEBUI_TRACKED_ELEMENT_INTERACTION_TEST_UTIL_WEB_UI_H_
