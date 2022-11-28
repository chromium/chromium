// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_test_util.h"

namespace ui::test {

bool InteractionTestUtil::Simulator::PressButton(TrackedElement*, InputType) {
  return false;
}

bool InteractionTestUtil::Simulator::SelectMenuItem(TrackedElement*,
                                                    InputType) {
  return false;
}

bool InteractionTestUtil::Simulator::DoDefaultAction(TrackedElement*,
                                                     InputType) {
  return false;
}

bool InteractionTestUtil::Simulator::SelectTab(TrackedElement*,
                                               size_t,
                                               InputType) {
  return false;
}

bool InteractionTestUtil::Simulator::SelectDropdownItem(TrackedElement*,
                                                        size_t,
                                                        InputType) {
  return false;
}

bool InteractionTestUtil::Simulator::EnterText(TrackedElement* element,
                                               const std::u16string& text,
                                               TextEntryMode mode) {
  return false;
}

bool InteractionTestUtil::Simulator::ActivateSurface(TrackedElement* element) {
  return false;
}

#if !BUILDFLAG(IS_IOS)
bool InteractionTestUtil::Simulator::SendAccelerator(
    TrackedElement* element,
    const Accelerator& accelerator) {
  return false;
}
#endif

bool InteractionTestUtil::Simulator::Confirm(TrackedElement* element) {
  return false;
}

InteractionTestUtil::InteractionTestUtil() = default;
InteractionTestUtil::~InteractionTestUtil() = default;

void InteractionTestUtil::PressButton(TrackedElement* element,
                                      InputType input_type) {
  for (const auto& simulator : simulators_) {
    if (simulator->PressButton(element, input_type))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

void InteractionTestUtil::SelectMenuItem(TrackedElement* element,
                                         InputType input_type) {
  for (const auto& simulator : simulators_) {
    if (simulator->SelectMenuItem(element, input_type))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

void InteractionTestUtil::DoDefaultAction(TrackedElement* element,
                                          InputType input_type) {
  for (const auto& simulator : simulators_) {
    if (simulator->DoDefaultAction(element, input_type))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

void InteractionTestUtil::SelectTab(TrackedElement* tab_collection,
                                    size_t index,
                                    InputType input_type) {
  for (const auto& simulator : simulators_) {
    if (simulator->SelectTab(tab_collection, index, input_type))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

void InteractionTestUtil::SelectDropdownItem(TrackedElement* dropdown,
                                             size_t index,
                                             InputType input_type) {
  for (const auto& simulator : simulators_) {
    if (simulator->SelectDropdownItem(dropdown, index, input_type))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

void InteractionTestUtil::EnterText(TrackedElement* element,
                                    std::u16string text,
                                    TextEntryMode mode) {
  for (const auto& simulator : simulators_) {
    if (simulator->EnterText(element, text, mode))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

void InteractionTestUtil::ActivateSurface(TrackedElement* element) {
  for (const auto& simulator : simulators_) {
    if (simulator->ActivateSurface(element))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

#if !BUILDFLAG(IS_IOS)
void InteractionTestUtil::SendAccelerator(TrackedElement* element,
                                          Accelerator accelerator) {
  for (const auto& simulator : simulators_) {
    if (simulator->SendAccelerator(element, accelerator))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}
#endif

void InteractionTestUtil::Confirm(TrackedElement* element) {
  for (const auto& simulator : simulators_) {
    if (simulator->Confirm(element))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

}  // namespace ui::test