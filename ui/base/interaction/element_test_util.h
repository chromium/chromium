// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_TEST_UTIL_H_
#define UI_BASE_INTERACTION_ELEMENT_TEST_UTIL_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect.h"

namespace ui::test {

// Provides a platform-less pseudoelement for use in ElementTracker and
// InteractionSequence tests.
class TestElementBase : public TrackedElement {
 public:
  TestElementBase(ElementIdentifier id, ElementContext context);
  ~TestElementBase() override;

  // Simulate the element shown event.
  void Show();

  // Simulate the element activated event.
  void Activate();

  // Simulate the element hidden event.
  void Hide();

  bool IsVisible() const;

  // Simuate a custom event on this element.
  void SendCustomEvent(CustomElementEventType event_type);

  void SetScreenBounds(const gfx::Rect& screen_bounds);
  gfx::Rect GetScreenBounds() const override;

 private:
  bool visible_ = false;
  gfx::Rect screen_bounds_;
};

// Provides a platform-less test element in a fictional UI framework.
class TestElement : public TestElementBase {
 public:
  TestElement(ElementIdentifier id, ElementContext context);
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

// Provides a platform-less test element in a fictional UI framework distinct
// from `TestElement`.
class TestElementOtherFramework : public TestElementBase {
 public:
  TestElementOtherFramework(ElementIdentifier id, ElementContext context);
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

// Convenience typedef for unique pointers to test elements.
using TestElementPtr = std::unique_ptr<TestElementBase>;

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_ELEMENT_TEST_UTIL_H_
