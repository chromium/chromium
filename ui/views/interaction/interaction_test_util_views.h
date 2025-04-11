// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_VIEWS_H_
#define UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_VIEWS_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"

namespace ui {
class TrackedElement;
}

namespace views {
class Button;
class View;
class Widget;
}  // namespace views

namespace views::test {

// Views implementation of InteractionTestUtil::Simulator.
// Add one to your InteractionTestUtil instance to get Views support.
class InteractionTestUtilSimulatorViews
    : public ui::test::InteractionTestUtil::Simulator {
 public:
  InteractionTestUtilSimulatorViews();
  ~InteractionTestUtilSimulatorViews() override;

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
  ui::test::ActionResult ActivateSurface(ui::TrackedElement* element) override;
  ui::test::ActionResult FocusElement(ui::TrackedElement* element) override;
  ui::test::ActionResult SendAccelerator(ui::TrackedElement* element,
                                         ui::Accelerator accelerator) override;
  ui::test::ActionResult SendKeyPress(ui::TrackedElement* element,
                                      ui::KeyboardCode key,
                                      int flags) override;
  ui::test::ActionResult Confirm(ui::TrackedElement* element) override;

  // Common functionality for activating a widget.
  static ui::test::ActionResult ActivateWidget(Widget* widget);

  // Convenience method for tests that need to simulate a button press and have
  // direct access to the button.
  static void PressButton(Button* button,
                          InputType input_type = InputType::kDontCare);

  // As above, but for non-button Views.
  static bool DoDefaultAction(View* view,
                              InputType input_type = InputType::kDontCare);

  // Returns whether the current machine is running Wayland.
  static bool IsWayland();
};

// Provides a simple way to wait for focus to change to a specific view.
class ViewFocusedWaiter : public FocusChangeListener {
 public:
  explicit ViewFocusedWaiter(View& target_view);
  ~ViewFocusedWaiter() override;

  void Wait();

  // FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

 private:
  raw_ref<FocusManager> manager_;
  raw_ref<View> target_view_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_VIEWS_H_
