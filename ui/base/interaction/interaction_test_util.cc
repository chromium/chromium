// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/interaction/interaction_test_util.h"
#include <functional>

namespace ui::test {

namespace {

template <typename... Args>
ActionResult Simulate(
    const std::vector<std::unique_ptr<InteractionTestUtil::Simulator>>&
        simulators,
    ActionResult (InteractionTestUtil::Simulator::*method)(Args...),
    Args... args) {
  for (const auto& simulator : simulators) {
    const auto result = std::invoke(method, simulator.get(), args...);
    if (result != ActionResult::kNotAttempted) {
      return result;
    }
  }
  return ActionResult::kNotAttempted;
}

}  // namespace

ActionResult InteractionTestUtil::Simulator::PressButton(TrackedElement*,
                                                         InputType) {
  return ActionResult::kNotAttempted;
}

ActionResult InteractionTestUtil::Simulator::SelectMenuItem(TrackedElement*,
                                                            InputType) {
  return ActionResult::kNotAttempted;
}

ActionResult InteractionTestUtil::Simulator::DoDefaultAction(TrackedElement*,
                                                             InputType) {
  return ActionResult::kNotAttempted;
}

ActionResult InteractionTestUtil::Simulator::SelectTab(TrackedElement*,
                                                       size_t,
                                                       InputType) {
  return ActionResult::kNotAttempted;
}

ActionResult InteractionTestUtil::Simulator::SelectDropdownItem(TrackedElement*,
                                                                size_t,
                                                                InputType) {
  return ActionResult::kNotAttempted;
}

ActionResult InteractionTestUtil::Simulator::EnterText(TrackedElement* element,
                                                       std::u16string text,
                                                       TextEntryMode mode) {
  return ActionResult::kNotAttempted;
}

ActionResult InteractionTestUtil::Simulator::ActivateSurface(
    TrackedElement* element) {
  return ActionResult::kNotAttempted;
}

#if !BUILDFLAG(IS_IOS)
ActionResult InteractionTestUtil::Simulator::SendAccelerator(
    TrackedElement* element,
    Accelerator accelerator) {
  return ActionResult::kNotAttempted;
}
#endif

ActionResult InteractionTestUtil::Simulator::Confirm(TrackedElement* element) {
  return ActionResult::kNotAttempted;
}

InteractionTestUtil::InteractionTestUtil() = default;
InteractionTestUtil::~InteractionTestUtil() = default;

ActionResult InteractionTestUtil::PressButton(TrackedElement* element,
                                              InputType input_type) {
  return Simulate(simulators_, &Simulator::PressButton, element, input_type);
}

ActionResult InteractionTestUtil::SelectMenuItem(TrackedElement* element,
                                                 InputType input_type) {
  return Simulate(simulators_, &Simulator::SelectMenuItem, element, input_type);
}

ActionResult InteractionTestUtil::DoDefaultAction(TrackedElement* element,
                                                  InputType input_type) {
  return Simulate(simulators_, &Simulator::DoDefaultAction, element,
                  input_type);
}

ActionResult InteractionTestUtil::SelectTab(TrackedElement* tab_collection,
                                            size_t index,
                                            InputType input_type) {
  return Simulate(simulators_, &Simulator::SelectTab, tab_collection, index,
                  input_type);
}

ActionResult InteractionTestUtil::SelectDropdownItem(TrackedElement* dropdown,
                                                     size_t index,
                                                     InputType input_type) {
  return Simulate(simulators_, &Simulator::SelectDropdownItem, dropdown, index,
                  input_type);
}

ActionResult InteractionTestUtil::EnterText(TrackedElement* element,
                                            std::u16string text,
                                            TextEntryMode mode) {
  return Simulate(simulators_, &Simulator::EnterText, element, text, mode);
}

ActionResult InteractionTestUtil::ActivateSurface(TrackedElement* element) {
  return Simulate(simulators_, &Simulator::ActivateSurface, element);
}

#if !BUILDFLAG(IS_IOS)
ActionResult InteractionTestUtil::SendAccelerator(TrackedElement* element,
                                                  Accelerator accelerator) {
  return Simulate(simulators_, &Simulator::SendAccelerator, element,
                  accelerator);
}
#endif

ActionResult InteractionTestUtil::Confirm(TrackedElement* element) {
  return Simulate(simulators_, &Simulator::Confirm, element);
}

void PrintTo(InteractionTestUtil::InputType input_type, std::ostream* os) {
  const char* const kInputTypeNames[] = {
      "InputType::kDontCare", "InputType::kMouse", "InputType::kKeyboard",
      "InputType::kMouse"};
  constexpr int kCount = sizeof(kInputTypeNames) / sizeof(kInputTypeNames[0]);
  static_assert(kCount ==
                static_cast<int>(InteractionTestUtil::InputType::kMaxValue) +
                    1);
  const int value = static_cast<int>(input_type);
  *os << ((value < 0 || value >= kCount) ? "[invalid InputType]"
                                         : kInputTypeNames[value]);
}

std::ostream& operator<<(std::ostream& os,
                         InteractionTestUtil::InputType input_type) {
  PrintTo(input_type, &os);
  return os;
}

}  // namespace ui::test