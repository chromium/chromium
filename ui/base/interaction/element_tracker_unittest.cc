// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_tracker.h"

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/expect_call_in_scope.h"

namespace ui {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier2);
const ElementContext kElementContext1(1);
const ElementContext kElementContext2(2);

}  // namespace

TEST(TrackedElementTest, IsATest) {
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext1);

  EXPECT_TRUE(e1->IsA<TestElement>());
  EXPECT_FALSE(e1->IsA<TestElementOtherFramework>());
  EXPECT_FALSE(e2->IsA<TestElement>());
  EXPECT_TRUE(e2->IsA<TestElementOtherFramework>());
}

TEST(TrackedElementTest, AsATest) {
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier2, kElementContext2);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext2);

  EXPECT_EQ(e1.get(), e1->AsA<TestElement>());
  EXPECT_EQ(nullptr, e1->AsA<TestElementOtherFramework>());
  EXPECT_EQ(nullptr, e2->AsA<TestElement>());
  EXPECT_EQ(e2.get(), e2->AsA<TestElementOtherFramework>());
}

TEST(ElementTrackerTest, GetUniqueElement) {
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
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
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
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
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
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
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
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
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  TestElementPtr e3 = std::make_unique<TestElementOtherFramework>(
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
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementShownCallback(
          kElementIdentifier1, kElementContext1, callback.Get());
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  TestElementPtr e3 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  TestElementPtr e4 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Show());
  e2->Show();
  e3->Show();
  e1->Activate();
  e1->Hide();
  EXPECT_CALL_IN_SCOPE(callback, Run(e4.get()), e4->Show());
}

TEST(ElementTrackerTest, AddElementActivatedCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          kElementIdentifier1, kElementContext1, callback.Get());
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  TestElementPtr e3 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  TestElementPtr e4 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  e1->Show();
  e2->Show();
  e3->Show();
  e4->Show();
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Activate());
  EXPECT_CALL_IN_SCOPE(callback, Run(e4.get()), e4->Activate());
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Activate());
  e2->Activate();
  e3->Activate();
}

TEST(ElementTrackerTest, AddElementHiddenCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          kElementIdentifier1, kElementContext1, callback.Get());
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  TestElementPtr e2 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  TestElementPtr e3 = std::make_unique<TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  TestElementPtr e4 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  e1->Show();
  e2->Show();
  e3->Show();
  e4->Show();
  e1->Activate();
  e4->Activate();
  e2->Hide();
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Hide());
  e3->Hide();
  EXPECT_CALL_IN_SCOPE(callback, Run(e4.get()), e4->Hide());
  e1->Show();
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Hide());
}

TEST(ElementTrackerTest, CleanupAfterElementHidden) {
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  e1->Show();
  EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  {
    UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
    auto subscription =
        ElementTracker::GetElementTracker()->AddElementShownCallback(
            kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  e1->Hide();
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());
}

TEST(ElementTrackerTest, CleanupAfterCallbacksRemoved) {
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);

  // Add element shown callback. An element will be shown transiently during the
  // subscription.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto subscription =
        ElementTracker::GetElementTracker()->AddElementShownCallback(
            kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
    e1->Show();
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
    e1->Hide();
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());

  // Add element activated callback.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto subscription =
        ElementTracker::GetElementTracker()->AddElementActivatedCallback(
            kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());

  // Add element hidden callback.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto subscription =
        ElementTracker::GetElementTracker()->AddElementHiddenCallback(
            kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());

  // Add and remove multiple callbacks.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto sub1 = ElementTracker::GetElementTracker()->AddElementShownCallback(
        kElementIdentifier1, kElementContext1, callback.Get());
    auto sub2 =
        ElementTracker::GetElementTracker()->AddElementActivatedCallback(
            kElementIdentifier1, kElementContext1, callback.Get());
    auto sub3 = ElementTracker::GetElementTracker()->AddElementHiddenCallback(
        kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, ElementTracker::GetElementTracker()->element_data_.size());
  }
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());
}

// The following test specific conditions that could trigger a UAF or cause
// similar instability due to changing callback lists during callbacks. These
// tests may fail all or some builds (specifically asan/msan) if the logic is
// implemented incorrectly.

TEST(ElementTrackerTest, RemoveCallbackDuringRemove) {
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  ElementTracker::Subscription subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          e1->identifier(), e1->context(), callback.Get());

  ON_CALL(callback, Run).WillByDefault([&](TrackedElement*) {
    subscription = ElementTracker::Subscription();
  });

  e1->Show();
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Hide());
  e1->Show();
  e1->Hide();
}

TEST(ElementTrackerTest, RemoveAndThenAddCallbackDuringRemove) {
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  ElementTracker::Subscription subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          e1->identifier(), e1->context(), callback.Get());

  ON_CALL(callback, Run).WillByDefault([&](TrackedElement*) {
    subscription = ElementTracker::Subscription();
    subscription =
        ElementTracker::GetElementTracker()->AddElementHiddenCallback(
            e1->identifier(), e1->context(), callback.Get());
  });
  e1->Show();
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Hide());
  e1->Show();
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Hide());
}

TEST(ElementTrackerTest, RemoveAndThenAddDifferentCallbackDuringRemove) {
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  ElementTracker::Subscription subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          e1->identifier(), e1->context(), callback.Get());

  ON_CALL(callback, Run).WillByDefault([&](TrackedElement*) {
    subscription = ElementTracker::Subscription();
    subscription = ElementTracker::GetElementTracker()->AddElementShownCallback(
        e1->identifier(), e1->context(), callback.Get());
  });

  e1->Show();
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Hide());
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Show());
  e1->Hide();
}

TEST(ElementTrackerTest, MultipleCallbacksForSameEvent) {
  TestElementPtr e1 =
      std::make_unique<TestElement>(kElementIdentifier1, kElementContext1);
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback2);
  ElementTracker::Subscription subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          e1->identifier(), e1->context(), callback.Get());
  ElementTracker::Subscription subscription2 =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          e1->identifier(), e1->context(), callback2.Get());

  e1->Show();

  // Note: these calls are not ordered.
  EXPECT_CALL(callback, Run(e1.get())).Times(1);
  EXPECT_CALL(callback2, Run(e1.get())).Times(1);
  e1->Hide();
}

TEST(ElementTrackerTest, HideDuringShowCallback) {
  TestElement e1(kElementIdentifier1, kElementContext1);
  ElementTracker::Subscription subscription;
  auto callback = base::BindLambdaForTesting([&](TrackedElement* element) {
    subscription = ElementTracker::Subscription();
    e1.Hide();
  });
  subscription = ElementTracker::GetElementTracker()->AddElementShownCallback(
      e1.identifier(), e1.context(), callback);
  e1.Show();

  // Verify that cleanup still happens after all callbacks return.
  EXPECT_TRUE(ElementTracker::GetElementTracker()->element_data_.empty());
}

TEST(SafeElementReferenceTest, ElementRemainsVisible) {
  TestElement e1(kElementIdentifier1, kElementContext1);
  e1.Show();
  SafeElementReference ref(&e1);
  EXPECT_TRUE(ref);
  EXPECT_FALSE(!ref);
  EXPECT_EQ(&e1, ref.get());
  e1.Activate();
  EXPECT_TRUE(ref);
  EXPECT_FALSE(!ref);
  EXPECT_EQ(&e1, ref.get());
}

TEST(SafeElementReferenceTest, ElementHidden) {
  TestElement e1(kElementIdentifier1, kElementContext1);
  e1.Show();
  SafeElementReference ref(&e1);
  EXPECT_TRUE(ref);
  EXPECT_FALSE(!ref);
  EXPECT_EQ(&e1, ref.get());
  e1.Hide();
  EXPECT_FALSE(ref);
  EXPECT_TRUE(!ref);
  EXPECT_EQ(nullptr, ref.get());
}

TEST(SafeElementReferenceTest, MoveConstructor) {
  TestElement e1(kElementIdentifier1, kElementContext1);
  e1.Show();
  std::unique_ptr<SafeElementReference> ref;
  {
    SafeElementReference ref2(&e1);
    ref = std::make_unique<SafeElementReference>(std::move(ref2));
  }
  EXPECT_EQ(&e1, ref->get());
  e1.Hide();
  EXPECT_EQ(nullptr, ref->get());
}

TEST(SafeElementReferenceTest, MoveOperator) {
  TestElement e1(kElementIdentifier1, kElementContext1);
  e1.Show();
  SafeElementReference ref;
  {
    SafeElementReference ref2(&e1);
    ref = std::move(ref2);
  }
  EXPECT_EQ(&e1, ref.get());
  e1.Hide();
  EXPECT_EQ(nullptr, ref.get());
}

TEST(SafeElementReferenceTest, CopyConstructor) {
  TestElement e1(kElementIdentifier1, kElementContext1);
  e1.Show();
  std::unique_ptr<SafeElementReference> ref;
  SafeElementReference ref2(&e1);
  ref = std::make_unique<SafeElementReference>(ref2);
  EXPECT_EQ(&e1, ref2.get());
  EXPECT_EQ(&e1, ref->get());
  e1.Hide();
  EXPECT_EQ(nullptr, ref->get());
  EXPECT_EQ(nullptr, ref2.get());
}

TEST(SafeElementReferenceTest, CopyOperator) {
  TestElement e1(kElementIdentifier1, kElementContext1);
  e1.Show();
  SafeElementReference ref;
  SafeElementReference ref2(&e1);
  ref = ref2;
  EXPECT_EQ(&e1, ref2.get());
  EXPECT_EQ(&e1, ref.get());
  e1.Hide();
  EXPECT_EQ(nullptr, ref.get());
  EXPECT_EQ(nullptr, ref2.get());
}

}  // namespace ui
