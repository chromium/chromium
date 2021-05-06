// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_tracker.h"

#include <memory>

#include "base/callback_forward.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestFrameworkIdentifier);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherFrameworkIdentifier);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier2);
const ElementContext kElementContext1(1);
const ElementContext kElementContext2(2);

class TestElementBase : public ElementTrackerElement {
 public:
  TestElementBase(ElementIdentifier id, ElementContext context)
      : ElementTrackerElement(id, context) {}
  ~TestElementBase() override { Hide(); }

  void Show() {
    if (visible_)
      return;
    visible_ = true;
    ElementTracker::GetFrameworkDelegate()->NotifyElementShown(this);
  }

  void Activate() {
    DCHECK(visible_);
    ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(this);
  }

  void Hide() {
    if (!visible_)
      return;
    visible_ = false;
    ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(this);
  }

 private:
  bool visible_ = false;
};

using ElementPtr = std::unique_ptr<TestElementBase>;

class TestElement : public TestElementBase {
 public:
  using TestElementBase::TestElementBase;

  static FrameworkIdentifier GetFrameworkIdentifier() {
    return kTestFrameworkIdentifier;
  }

  FrameworkIdentifier GetInstanceFrameworkIdentifier() const override {
    return kTestFrameworkIdentifier;
  }
};

class TestElementOtherFramework : public TestElementBase {
 public:
  using TestElementBase::TestElementBase;

  static FrameworkIdentifier GetFrameworkIdentifier() {
    return kOtherFrameworkIdentifier;
  }

  FrameworkIdentifier GetInstanceFrameworkIdentifier() const override {
    return kOtherFrameworkIdentifier;
  }
};

class TestCallback {
 public:
  ElementTracker::Callback GetCallback() {
    return base::BindLambdaForTesting([&](ElementTrackerElement* element) {
      ++count_;
      last_element_ = element;
      if (on_callback_)
        on_callback_.Run();
    });
  }

  void SetCallback(base::RepeatingClosure on_callback) {
    on_callback_ = on_callback;
  }

  size_t count() const { return count_; }
  const ElementTrackerElement* last_element() const { return last_element_; }

 private:
  size_t count_ = 0;
  const ElementTrackerElement* last_element_ = nullptr;
  base::RepeatingClosure on_callback_;
};

}  // namespace

TEST(ElementTrackerElementTest, IsATest) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext1);

  EXPECT_TRUE(e1->IsA<TestElement>());
  EXPECT_FALSE(e1->IsA<TestElementOtherFramework>());
  EXPECT_FALSE(e2->IsA<TestElement>());
  EXPECT_TRUE(e2->IsA<TestElementOtherFramework>());
}

TEST(ElementTrackerElementTest, AsATest) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier2, kElementContext2);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext2);

  EXPECT_EQ(e1.get(), e1->AsA<TestElement>());
  EXPECT_EQ(nullptr, e1->AsA<TestElementOtherFramework>());
  EXPECT_EQ(nullptr, e2->AsA<TestElement>());
  EXPECT_EQ(e2.get(), e2->AsA<TestElementOtherFramework>());
}

TEST(ElementTrackerTest, GetUniqueElement) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  EXPECT_EQ(nullptr, ElementTracker::GetElementTracker()->GetUniqueElement(
                         kElementIdentifier1, kElementContext1));
  EXPECT_EQ(nullptr, ElementTracker::GetElementTracker()->GetUniqueElement(
                         kElementIdentifier2, kElementContext1));
  e1->Show();
  EXPECT_EQ(e1.get(), ElementTracker::GetElementTracker()->GetUniqueElement(
                          kElementIdentifier1, kElementContext1));
  EXPECT_EQ(nullptr, ElementTracker::GetElementTracker()->GetUniqueElement(
                         kElementIdentifier2, kElementContext1));
  e2->Show();
  EXPECT_EQ(e1.get(), ElementTracker::GetElementTracker()->GetUniqueElement(
                          kElementIdentifier1, kElementContext1));
  EXPECT_EQ(e2.get(), ElementTracker::GetElementTracker()->GetUniqueElement(
                          kElementIdentifier2, kElementContext1));
  e1->Hide();
  EXPECT_EQ(nullptr, ElementTracker::GetElementTracker()->GetUniqueElement(
                         kElementIdentifier1, kElementContext1));
  EXPECT_EQ(e2.get(), ElementTracker::GetElementTracker()->GetUniqueElement(
                          kElementIdentifier2, kElementContext1));
  e2->Hide();
  EXPECT_EQ(nullptr, ElementTracker::GetElementTracker()->GetUniqueElement(
                         kElementIdentifier1, kElementContext1));
  EXPECT_EQ(nullptr, ElementTracker::GetElementTracker()->GetUniqueElement(
                         kElementIdentifier2, kElementContext1));
}

TEST(ElementTrackerTest, GetFirstMatchingElement) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier2, kElementContext1));
  e1->Show();
  EXPECT_EQ(e1.get(),
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier2, kElementContext1));
  e2->Show();
  EXPECT_EQ(e1.get(),
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  EXPECT_EQ(e2.get(),
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier2, kElementContext1));
  e1->Hide();
  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  EXPECT_EQ(e2.get(),
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier2, kElementContext1));
  e2->Hide();
  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier2, kElementContext1));
}

TEST(ElementTrackerTest, GetFirstMatchingElementWithMultipleElements) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext1);
  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  e1->Show();
  EXPECT_EQ(e1.get(),
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  e2->Show();
  EXPECT_EQ(e1.get(),
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  e1->Hide();
  EXPECT_EQ(e2.get(),
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  e1->Show();
  EXPECT_EQ(e2.get(),
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  e2->Hide();
  EXPECT_EQ(e1.get(),
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
  e1->Hide();
  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kElementIdentifier1, kElementContext1));
}

TEST(ElementTrackerTest, GetAllMatchingElements) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext1);
  ElementTracker::ElementList expected;
  EXPECT_EQ(expected,
            ElementTracker::GetElementTracker()->GetAllMatchingElements(
                kElementIdentifier1, kElementContext1));
  e1->Show();
  expected = ElementTracker::ElementList{e1.get()};
  EXPECT_EQ(expected,
            ElementTracker::GetElementTracker()->GetAllMatchingElements(
                kElementIdentifier1, kElementContext1));
  e2->Show();
  expected = ElementTracker::ElementList{e1.get(), e2.get()};
  EXPECT_EQ(expected,
            ElementTracker::GetElementTracker()->GetAllMatchingElements(
                kElementIdentifier1, kElementContext1));
  e1->Hide();
  expected = ElementTracker::ElementList{e2.get()};
  EXPECT_EQ(expected,
            ElementTracker::GetElementTracker()->GetAllMatchingElements(
                kElementIdentifier1, kElementContext1));
  e1->Show();
  expected = ElementTracker::ElementList{e2.get(), e1.get()};
  EXPECT_EQ(expected,
            ElementTracker::GetElementTracker()->GetAllMatchingElements(
                kElementIdentifier1, kElementContext1));
  e2->Hide();
  expected = ElementTracker::ElementList{e1.get()};
  EXPECT_EQ(expected,
            ElementTracker::GetElementTracker()->GetAllMatchingElements(
                kElementIdentifier1, kElementContext1));
  e1->Hide();
  expected = ElementTracker::ElementList{};
  EXPECT_EQ(expected,
            ElementTracker::GetElementTracker()->GetAllMatchingElements(
                kElementIdentifier1, kElementContext1));
}

TEST(ElementTrackerTest, IsElementVisible) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  ElementPtr e3 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext1));
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier2, kElementContext1));
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext2));
  e1->Show();
  EXPECT_TRUE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext1));
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier2, kElementContext1));
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext2));
  e2->Show();
  EXPECT_TRUE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext1));
  EXPECT_TRUE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier2, kElementContext1));
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext2));
  e3->Show();
  EXPECT_TRUE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext1));
  EXPECT_TRUE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier2, kElementContext1));
  EXPECT_TRUE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext2));
  e2->Hide();
  EXPECT_TRUE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext1));
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier2, kElementContext1));
  EXPECT_TRUE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext2));
  e1->Hide();
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext1));
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier2, kElementContext1));
  EXPECT_TRUE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext2));
  e3->Hide();
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext1));
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier2, kElementContext1));
  EXPECT_FALSE(ElementTracker::GetElementTracker()->IsElementVisible(
      kElementIdentifier1, kElementContext2));
}

TEST(ElementTrackerTest, AddElementShownCallback) {
  TestCallback callback;
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementShownCallback(
          kElementIdentifier1, kElementContext1, callback.GetCallback());
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  ElementPtr e3 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  ElementPtr e4 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  EXPECT_EQ(0U, callback.count());
  e1->Show();
  EXPECT_EQ(1U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
  e2->Show();
  EXPECT_EQ(1U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
  e3->Show();
  EXPECT_EQ(1U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
  e1->Activate();
  e1->Hide();
  EXPECT_EQ(1U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
  e4->Show();
  EXPECT_EQ(2U, callback.count());
  EXPECT_EQ(e4.get(), callback.last_element());
}

TEST(ElementTrackerTest, AddElementActivatedCallback) {
  TestCallback callback;
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          kElementIdentifier1, kElementContext1, callback.GetCallback());
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  ElementPtr e3 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  ElementPtr e4 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  EXPECT_EQ(0U, callback.count());
  e1->Show();
  EXPECT_EQ(0U, callback.count());
  e2->Show();
  EXPECT_EQ(0U, callback.count());
  e3->Show();
  EXPECT_EQ(0U, callback.count());
  e4->Show();
  EXPECT_EQ(0U, callback.count());
  e1->Activate();
  EXPECT_EQ(1U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
  e4->Activate();
  EXPECT_EQ(2U, callback.count());
  EXPECT_EQ(e4.get(), callback.last_element());
  e1->Activate();
  EXPECT_EQ(3U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
  e2->Activate();
  EXPECT_EQ(3U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
  e3->Activate();
  EXPECT_EQ(3U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
}

TEST(ElementTrackerTest, AddElementHiddenCallback) {
  TestCallback callback;
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          kElementIdentifier1, kElementContext1, callback.GetCallback());
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  ElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  ElementPtr e3 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  ElementPtr e4 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  EXPECT_EQ(0U, callback.count());
  e1->Show();
  EXPECT_EQ(0U, callback.count());
  e2->Show();
  EXPECT_EQ(0U, callback.count());
  e3->Show();
  EXPECT_EQ(0U, callback.count());
  e4->Show();
  EXPECT_EQ(0U, callback.count());
  e1->Activate();
  EXPECT_EQ(0U, callback.count());
  e4->Activate();
  EXPECT_EQ(0U, callback.count());
  e2->Hide();
  EXPECT_EQ(0U, callback.count());
  e1->Hide();
  EXPECT_EQ(1U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
  e3->Hide();
  EXPECT_EQ(1U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
  e4->Hide();
  EXPECT_EQ(2U, callback.count());
  EXPECT_EQ(e4.get(), callback.last_element());
  e1->Show();
  EXPECT_EQ(2U, callback.count());
  EXPECT_EQ(e4.get(), callback.last_element());
  e1->Hide();
  EXPECT_EQ(3U, callback.count());
  EXPECT_EQ(e1.get(), callback.last_element());
}

TEST(ElementTrackerTest, CleanupAfterElementHidden) {
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  e1->Show();
  EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  {
    TestCallback callback;
    auto subscription =
        ElementTracker::GetElementTracker()->AddElementShownCallback(
            kElementIdentifier1, kElementContext1, callback.GetCallback());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  e1->Hide();
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());
}

TEST(ElementTrackerTest, CleanupAfterCallbacksRemoved) {
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);

  // Add element shown callback. An element will be shown transiently during the
  // subscription.
  {
    TestCallback callback;
    auto subscription =
        ElementTracker::GetElementTracker()->AddElementShownCallback(
            kElementIdentifier1, kElementContext1, callback.GetCallback());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
    e1->Show();
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
    e1->Hide();
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());

  // Add element activated callback.
  {
    TestCallback callback;
    auto subscription =
        ElementTracker::GetElementTracker()->AddElementActivatedCallback(
            kElementIdentifier1, kElementContext1, callback.GetCallback());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());

  // Add element hidden callback.
  {
    TestCallback callback;
    auto subscription =
        ElementTracker::GetElementTracker()->AddElementHiddenCallback(
            kElementIdentifier1, kElementContext1, callback.GetCallback());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());

  // Add and remove multiple callbacks.
  {
    TestCallback callback;
    auto sub1 = ElementTracker::GetElementTracker()->AddElementShownCallback(
        kElementIdentifier1, kElementContext1, callback.GetCallback());
    auto sub2 =
        ElementTracker::GetElementTracker()->AddElementActivatedCallback(
            kElementIdentifier1, kElementContext1, callback.GetCallback());
    auto sub3 = ElementTracker::GetElementTracker()->AddElementHiddenCallback(
        kElementIdentifier1, kElementContext1, callback.GetCallback());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());
}

// The following test specific conditions that could trigger a UAF or cause
// similar instability due to changing callback lists during callbacks. These
// tests may fail all or some builds (specifically asan/msan) if the logic is
// implemented incorrectly.

TEST(ElementTrackerTest, RemoveCallbackDuringRemove) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestCallback callback;
  ElementTracker::Subscription subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          e1->identifier(), e1->context(), callback.GetCallback());
  callback.SetCallback(base::BindLambdaForTesting(
      [&]() { subscription = ElementTracker::Subscription(); }));
  e1->Show();
  EXPECT_EQ(0U, callback.count());
  e1->Hide();
  EXPECT_EQ(1U, callback.count());
}

TEST(ElementTrackerTest, RemoveAndThenAddCallbackDuringRemove) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestCallback callback;
  ElementTracker::Subscription subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          e1->identifier(), e1->context(), callback.GetCallback());
  callback.SetCallback(base::BindLambdaForTesting([&]() {
    subscription = ElementTracker::Subscription();
    subscription =
        ElementTracker::GetElementTracker()->AddElementHiddenCallback(
            e1->identifier(), e1->context(), callback.GetCallback());
  }));
  e1->Show();
  EXPECT_EQ(0U, callback.count());
  e1->Hide();
}

TEST(ElementTrackerTest, RemoveAndThenAddDifferentCallbackDuringRemove) {
  ElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestCallback callback;
  ElementTracker::Subscription subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          e1->identifier(), e1->context(), callback.GetCallback());
  callback.SetCallback(base::BindLambdaForTesting([&]() {
    subscription = ElementTracker::Subscription();
    subscription = ElementTracker::GetElementTracker()->AddElementShownCallback(
        e1->identifier(), e1->context(), callback.GetCallback());
  }));
  e1->Show();
  EXPECT_EQ(0U, callback.count());
  e1->Hide();
}

}  // namespace ui
