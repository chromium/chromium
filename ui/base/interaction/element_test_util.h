// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_TEST_UTIL_H_
#define UI_BASE_INTERACTION_ELEMENT_TEST_UTIL_H_

#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace ui {

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

 private:
  bool visible_ = false;
};

// Provides a platform-less test element in a fictional UI framework.
class TestElement : public TestElementBase {
 public:
  TestElement(ElementIdentifier id, ElementContext context);
  static FrameworkIdentifier GetFrameworkIdentifier();
  FrameworkIdentifier GetInstanceFrameworkIdentifier() const override;
};

// Provides a platform-less test element in a fictional UI framework distinct
// from `TestElement`.
class TestElementOtherFramework : public TestElementBase {
 public:
  TestElementOtherFramework(ElementIdentifier id, ElementContext context);
  static FrameworkIdentifier GetFrameworkIdentifier();
  FrameworkIdentifier GetInstanceFrameworkIdentifier() const override;
};

// Convenience typedef for unique pointers to test elements.
using TestElementPtr = std::unique_ptr<TestElementBase>;

// Require that a specific base::MockCallback (or callbacks) is called in a
// specific order, inside a specific block of code.

#define EXPECT_CALL_IN_SCOPE(Name, Call, Block) \
  EXPECT_CALL(Name, Call).Times(1);             \
  Block;                                        \
  EXPECT_CALL(Name, Run).Times(0)

#define EXPECT_CALLS_IN_SCOPE_2(Name1, Call1, Name2, Call2, Block) \
  {                                                                \
    testing::InSequence in_sequence;                               \
    EXPECT_CALL(Name1, Call1).Times(1);                            \
    EXPECT_CALL(Name2, Call2).Times(1);                            \
  }                                                                \
  Block;                                                           \
  EXPECT_CALL(Name1, Run).Times(0);                                \
  EXPECT_CALL(Name2, Run).Times(0)

#define EXPECT_CALLS_IN_SCOPE_3(Name1, Call1, Name2, Call2, Name3, Call3, \
                                Block)                                    \
  {                                                                       \
    testing::InSequence in_sequence;                                      \
    EXPECT_CALL(Name1, Call1).Times(1);                                   \
    EXPECT_CALL(Name2, Call2).Times(1);                                   \
    EXPECT_CALL(Name3, Call3).Times(1);                                   \
  }                                                                       \
  Block;                                                                  \
  EXPECT_CALL(Name1, Run).Times(0);                                       \
  EXPECT_CALL(Name2, Run).Times(0);                                       \
  EXPECT_CALL(Name3, Run).Times(0)

}  // namespace ui

#endif  // UI_BASE_INTERACTION_ELEMENT_TEST_UTIL_H_
