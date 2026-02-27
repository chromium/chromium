// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_highlighter.h"

#include <memory>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/expect_call_in_scope.h"

namespace ui {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButton1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButton2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOther1);
// DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOther2);

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAddHighlight);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kRemoveHighlight);

constexpr ElementContext kElementContext =
    ElementContext::CreateFakeContextForTesting(1);

template <typename ElementType>
class TestElementHighlight : public ui::ElementHighlighter::Highlight {
 public:
  explicit TestElementHighlight(ui::TrackedElement& element)
      : element_(&element) {
    element_.get_as<ElementType>()->SendCustomEvent(kAddHighlight);
  }

  ~TestElementHighlight() override {
    if (element_) {
      element_.get_as<ElementType>()->SendCustomEvent(kRemoveHighlight);
    }
  }

 private:
  SafeElementReference element_;
};

// A test highlighter implementation that can "highlight" test elements with
// identifiers button1 and button2 by sending them custom events.
template <typename ElementType>
class TestElementHighlighterBase : public ui::ElementHighlighter::Backend {
 public:
  TestElementHighlighterBase() = default;
  ~TestElementHighlighterBase() override = default;

  // ui::ElementHighlighter::Backend implementation:
  bool CanBeHighlighted(ui::TrackedElement& element) const override {
    if (!element.IsA<ElementType>()) {
      return false;
    }

    return element.identifier() == kButton1 || element.identifier() == kButton2;
  }

  std::unique_ptr<ui::ElementHighlighter::Highlight> AddHighlight(
      ui::TrackedElement& element) override {
    if (!element.IsA<ElementType>()) {
      return nullptr;
    }

    if (element.identifier() == kButton1 || element.identifier() == kButton2) {
      return std::make_unique<TestElementHighlight<ElementType>>(element);
    }

    return nullptr;
  }
};

class TestElementHighlighter
    : public TestElementHighlighterBase<test::TestElement> {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

class TestOtherElementHighlighter
    : public TestElementHighlighterBase<test::TestElementOtherFramework> {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestElementHighlighter)
DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestOtherElementHighlighter)

}  // namespace

TEST(ElementHighlighterTest, CanBeHiglighted) {
  auto highlighter = ElementHighlighter::MakeInstanceForTesting();
  highlighter->MaybeRegisterBackend<TestElementHighlighter>();

  test::TestElementPtr button =
      std::make_unique<test::TestElement>(kButton1, kElementContext);
  test::TestElementPtr other =
      std::make_unique<test::TestElement>(kOther1, kElementContext);
  test::TestElementPtr foreign_button =
      std::make_unique<test::TestElementOtherFramework>(kButton1,
                                                        kElementContext);

  EXPECT_TRUE(highlighter->CanBeHighlighted(button.get()));
  EXPECT_FALSE(highlighter->CanBeHighlighted(other.get()));
  EXPECT_FALSE(highlighter->CanBeHighlighted(foreign_button.get()));
}

// Test with multiple frameworks supported.
TEST(ElementHighlighterTest, CanBeHighlightedMultiFramework) {
  auto highlighter = ElementHighlighter::MakeInstanceForTesting();
  highlighter->MaybeRegisterBackend<TestElementHighlighter>();
  highlighter->MaybeRegisterBackend<TestOtherElementHighlighter>();

  test::TestElementPtr button =
      std::make_unique<test::TestElement>(kButton1, kElementContext);
  test::TestElementPtr other =
      std::make_unique<test::TestElement>(kOther1, kElementContext);
  test::TestElementPtr foreign_button =
      std::make_unique<test::TestElementOtherFramework>(kButton1,
                                                        kElementContext);

  EXPECT_TRUE(highlighter->CanBeHighlighted(button.get()));
  EXPECT_FALSE(highlighter->CanBeHighlighted(other.get()));
  EXPECT_TRUE(highlighter->CanBeHighlighted(foreign_button.get()));
}

TEST(ElementHighlighterTest, AddHighlight) {
  // There shouldn't be add/remove highlight calls done on elements not
  // handled by the test fixture, which we try with first.
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, add_highlight);
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, remove_highlight);

  auto subscribe_add =
      ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kAddHighlight, kElementContext, add_highlight.Get());
  auto subscribe_rm =
      ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kRemoveHighlight, kElementContext, remove_highlight.Get());

  auto highlighter = ElementHighlighter::MakeInstanceForTesting();
  highlighter->MaybeRegisterBackend<TestElementHighlighter>();

  test::TestElementPtr button1 =
      std::make_unique<test::TestElement>(kButton1, kElementContext);
  button1->Show();
  test::TestElementPtr button2 =
      std::make_unique<test::TestElement>(kButton1, kElementContext);
  button2->Show();
  test::TestElementPtr other =
      std::make_unique<test::TestElement>(kOther1, kElementContext);
  other->Show();
  test::TestElementPtr foreign_button =
      std::make_unique<test::TestElementOtherFramework>(kButton1,
                                                        kElementContext);
  foreign_button->Show();

  auto other_hl = highlighter->AddHighlight(other.get());
  auto foreign_button_hl = highlighter->AddHighlight(foreign_button.get());
  EXPECT_FALSE(other_hl);
  EXPECT_FALSE(foreign_button_hl);

  std::unique_ptr<ui::ElementHighlighter::Highlight> button1_hl, button2_hl;

  EXPECT_CALL_IN_SCOPE(add_highlight, Run(button1.get()),
                       button1_hl = highlighter->AddHighlight(button1.get()));
  EXPECT_CALL_IN_SCOPE(add_highlight, Run(button2.get()),
                       button2_hl = highlighter->AddHighlight(button2.get()));
  EXPECT_TRUE(button1_hl);
  EXPECT_TRUE(button2_hl);

  EXPECT_CALL_IN_SCOPE(remove_highlight, Run(button1.get()),
                       button1_hl.reset());
  EXPECT_CALL_IN_SCOPE(remove_highlight, Run(button2.get()),
                       button2_hl.reset());
}

// Test with multiple frameworks supported.
TEST(ElementHighlighterTest, AddHighlightMultiFramework) {
  // There shouldn't be add/remove highlight calls done on elements not
  // handled by the test fixture, which we try with first.
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, add_highlight);
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, remove_highlight);

  auto subscribe_add =
      ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kAddHighlight, kElementContext, add_highlight.Get());
  auto subscribe_rm =
      ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kRemoveHighlight, kElementContext, remove_highlight.Get());

  auto highlighter = ElementHighlighter::MakeInstanceForTesting();
  highlighter->MaybeRegisterBackend<TestElementHighlighter>();
  highlighter->MaybeRegisterBackend<TestOtherElementHighlighter>();

  test::TestElementPtr button1 =
      std::make_unique<test::TestElement>(kButton1, kElementContext);
  button1->Show();
  test::TestElementPtr button2 =
      std::make_unique<test::TestElement>(kButton1, kElementContext);
  button2->Show();
  test::TestElementPtr other =
      std::make_unique<test::TestElement>(kOther1, kElementContext);
  other->Show();
  test::TestElementPtr foreign_button =
      std::make_unique<test::TestElementOtherFramework>(kButton1,
                                                        kElementContext);
  foreign_button->Show();

  auto other_hl = highlighter->AddHighlight(other.get());
  EXPECT_FALSE(other_hl);

  std::unique_ptr<ui::ElementHighlighter::Highlight> button1_hl, button2_hl,
      foreign_button_hl;

  EXPECT_CALL_IN_SCOPE(add_highlight, Run(button1.get()),
                       button1_hl = highlighter->AddHighlight(button1.get()));
  EXPECT_CALL_IN_SCOPE(add_highlight, Run(button2.get()),
                       button2_hl = highlighter->AddHighlight(button2.get()));
  EXPECT_CALL_IN_SCOPE(
      add_highlight, Run(foreign_button.get()),
      foreign_button_hl = highlighter->AddHighlight(foreign_button.get()));
  EXPECT_TRUE(button1_hl);
  EXPECT_TRUE(button2_hl);
  EXPECT_TRUE(foreign_button_hl);

  EXPECT_CALL_IN_SCOPE(remove_highlight, Run(foreign_button.get()),
                       foreign_button_hl.reset());
  EXPECT_CALL_IN_SCOPE(remove_highlight, Run(button1.get()),
                       button1_hl.reset());
  EXPECT_CALL_IN_SCOPE(remove_highlight, Run(button2.get()),
                       button2_hl.reset());
}

}  // namespace ui
