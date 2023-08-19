// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_tracker.h"

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
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

// Use DECLARE/DEFINE_ELEMENT instead of DEFINE_LOCAL_ELEMENT so that this
// ElementIdentifier's name is predictable.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier1);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier1);
const char* const kElementIdentifier1Name = "kElementIdentifier1";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier2);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType1);
const ElementContext kElementContext1(1);
const ElementContext kElementContext2(2);

struct EventTestStruct {
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEventType2);
};
DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(EventTestStruct, kCustomEventType2);

}  // namespace

TEST(TrackedElementTest, IsATest) {
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier1, kElementContext1);

  EXPECT_TRUE(e1->IsA<test::TestElement>());
  EXPECT_FALSE(e1->IsA<test::TestElementOtherFramework>());
  EXPECT_FALSE(e2->IsA<test::TestElement>());
  EXPECT_TRUE(e2->IsA<test::TestElementOtherFramework>());
}

TEST(TrackedElementTest, AsATest) {
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier2, kElementContext2);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier2, kElementContext2);

  EXPECT_EQ(e1.get(), e1->AsA<test::TestElement>());
  EXPECT_EQ(nullptr, e1->AsA<test::TestElementOtherFramework>());
  EXPECT_EQ(nullptr, e2->AsA<test::TestElement>());
  EXPECT_EQ(e2.get(), e2->AsA<test::TestElementOtherFramework>());
}

TEST(ElementTrackerTest, GetUniqueElement) {
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
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
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
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
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
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
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
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

TEST(ElementTrackerTest, GetAllMatchingElementsInAnyContext) {
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e3 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext2);
  test::TestElementPtr e4 = std::make_unique<test::TestElement>(
      kElementIdentifier2, kElementContext1);

  EXPECT_THAT(
      ElementTracker::GetElementTracker()->GetAllMatchingElementsInAnyContext(
          kElementIdentifier1),
      testing::IsEmpty());

  e1->Show();
  EXPECT_THAT(
      ElementTracker::GetElementTracker()->GetAllMatchingElementsInAnyContext(
          kElementIdentifier1),
      testing::UnorderedElementsAre(e1.get()));

  e2->Show();
  e3->Show();
  e4->Show();
  EXPECT_THAT(
      ElementTracker::GetElementTracker()->GetAllMatchingElementsInAnyContext(
          kElementIdentifier1),
      testing::UnorderedElementsAre(e1.get(), e2.get(), e3.get()));

  e1->Hide();
  EXPECT_THAT(
      ElementTracker::GetElementTracker()->GetAllMatchingElementsInAnyContext(
          kElementIdentifier1),
      testing::UnorderedElementsAre(e2.get(), e3.get()));

  EXPECT_THAT(
      ElementTracker::GetElementTracker()->GetAllMatchingElementsInAnyContext(
          kElementIdentifier2),
      testing::UnorderedElementsAre(e4.get()));

  e4->Hide();
  EXPECT_THAT(
      ElementTracker::GetElementTracker()->GetAllMatchingElementsInAnyContext(
          kElementIdentifier2),
      testing::IsEmpty());
}

TEST(ElementTrackerTest, GetElementInAnyContext) {
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e3 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext2);
  test::TestElementPtr e4 = std::make_unique<test::TestElement>(
      kElementIdentifier2, kElementContext1);

  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetElementInAnyContext(
                kElementIdentifier1));

  e1->Show();
  EXPECT_EQ(e1.get(),
            ElementTracker::GetElementTracker()->GetElementInAnyContext(
                kElementIdentifier1));

  e2->Show();
  e3->Show();
  e4->Show();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetElementInAnyContext(
                  kElementIdentifier1),
              testing::AnyOf(testing::Eq(e1.get()), testing::Eq(e2.get()),
                             testing::Eq(e3.get())));

  e1->Hide();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetElementInAnyContext(
                  kElementIdentifier1),
              testing::AnyOf(testing::Eq(e2.get()), testing::Eq(e3.get())));

  EXPECT_EQ(e4.get(),
            ElementTracker::GetElementTracker()->GetElementInAnyContext(
                kElementIdentifier2));

  e4->Hide();
  EXPECT_EQ(nullptr,
            ElementTracker::GetElementTracker()->GetElementInAnyContext(
                kElementIdentifier2));
}

TEST(ElementTrackerTest, IsElementVisible) {
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  test::TestElementPtr e3 = std::make_unique<test::TestElementOtherFramework>(
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
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  test::TestElementPtr e3 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  test::TestElementPtr e4 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Show());
  e2->Show();
  e3->Show();
  e1->Activate();
  e1->Hide();
  EXPECT_CALL_IN_SCOPE(callback, Run(e4.get()), e4->Show());
}

TEST(ElementTrackerTest, AddElementShownInAnyContextCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementShownInAnyContextCallback(
          kElementIdentifier1, callback.Get());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  test::TestElementPtr e3 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  test::TestElementPtr e4 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Show());
  e2->Show();
  EXPECT_CALL_IN_SCOPE(callback, Run(e3.get()), e3->Show());
  e1->Activate();
  e1->Hide();
  EXPECT_CALL_IN_SCOPE(callback, Run(e4.get()), e4->Show());
}

TEST(ElementTrackerTest, AddElementActivatedCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          kElementIdentifier1, kElementContext1, callback.Get());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  test::TestElementPtr e3 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  test::TestElementPtr e4 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
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

TEST(ElementTrackerTest, AddElementActivatedInAnyContextCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription = ElementTracker::GetElementTracker()
                          ->AddElementActivatedInAnyContextCallback(
                              kElementIdentifier1, callback.Get());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  test::TestElementPtr e3 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  e1->Show();
  e2->Show();
  e3->Show();

  // Two of these elements have the specified identifier, but in different
  // contexts, while the other element does not share the identifier and should
  // not trigger the callback.
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Activate());
  e2->Activate();
  EXPECT_CALL_IN_SCOPE(callback, Run(e3.get()), e3->Activate());
}

TEST(ElementTrackerTest, AddElementHiddenCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          kElementIdentifier1, kElementContext1, callback.Get());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  test::TestElementPtr e3 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  test::TestElementPtr e4 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
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

TEST(ElementTrackerTest, AddElementHiddenInAnyContextCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementHiddenInAnyContextCallback(
          kElementIdentifier1, callback.Get());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier2, kElementContext1);
  test::TestElementPtr e3 = std::make_unique<test::TestElementOtherFramework>(
      kElementIdentifier1, kElementContext2);
  e1->Show();
  e2->Show();
  e3->Show();

  // Two of these elements have the specified identifier, but in different
  // contexts, while the other element does not share the identifier and should
  // not trigger the callback.
  EXPECT_CALL_IN_SCOPE(callback, Run(e1.get()), e1->Hide());
  e2->Hide();
  EXPECT_CALL_IN_SCOPE(callback, Run(e3.get()), e3->Hide());
}

TEST(ElementTrackerTest, AddCustomEventCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kCustomEventType1, kElementContext1, callback.Get());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext2);
  e1->Show();
  e2->Show();
  EXPECT_CALL_IN_SCOPE(
      callback, Run(e1.get()),
      ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          e1.get(), kCustomEventType1));
  ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(e2.get(),
                                                            kCustomEventType1);
}

TEST(ElementTrackerTest, AddCustomEventInAnyContextCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddCustomEventInAnyContextCallback(
          kCustomEventType1, callback.Get());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  test::TestElementPtr e2 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext2);
  e1->Show();
  e2->Show();

  // Two of these elements have the specified identifier, but in different
  // contexts, while the other element does not share the identifier and should
  // not trigger the callback.
  EXPECT_CALL_IN_SCOPE(
      callback, Run(e1.get()),
      ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          e1.get(), kCustomEventType1));
  EXPECT_CALL_IN_SCOPE(
      callback, Run(e2.get()),
      ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          e2.get(), kCustomEventType1));
}

TEST(ElementTrackerTest, AddClassCustomEventCallback) {
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  auto subscription =
      ElementTracker::GetElementTracker()->AddCustomEventCallback(
          EventTestStruct::kCustomEventType2, kElementContext1, callback.Get());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  e1->Show();
  EXPECT_CALL_IN_SCOPE(
      callback, Run(e1.get()),
      ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          e1.get(), EventTestStruct::kCustomEventType2));
}

TEST(ElementTrackerTest, MultipleCustomEventCallbacks) {
  // We will test that custom events work with multiple event types, including
  // in the edge case that the event type is the same as an element identifier
  // (this should never happen, but should also never break).
  const CustomElementEventType kCustomEventType2 = kElementIdentifier1;
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
  UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback2);
  auto subscription =
      ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kCustomEventType1, kElementContext1, callback.Get());
  auto subscription2 =
      ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kCustomEventType2, kElementContext1, callback2.Get());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  e1->Show();
  EXPECT_CALL_IN_SCOPE(
      callback, Run(e1.get()),
      ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          e1.get(), kCustomEventType1));
  EXPECT_CALL_IN_SCOPE(
      callback2, Run(e1.get()),
      ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          e1.get(), kCustomEventType2));
}

TEST(ElementTrackerTest, CleanupAfterElementHidden) {
  // Because tests can run concurrently, we have to create our own tracker to
  // avoid other tests messing with the data here.
  ElementTracker element_tracker;
  EXPECT_TRUE(element_tracker.element_data_.empty());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
  element_tracker.NotifyElementShown(e1.get());
  EXPECT_EQ(1U, element_tracker.element_data_.size());
  {
    UNCALLED_MOCK_CALLBACK(ElementTracker::Callback, callback);
    auto subscription = element_tracker.AddElementShownCallback(
        kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, element_tracker.element_data_.size());
  }
  element_tracker.NotifyElementHidden(e1.get());
  EXPECT_TRUE(element_tracker.element_data_.empty());
}

TEST(ElementTrackerTest, CleanupAfterCallbacksRemoved) {
  // Because tests can run concurrently, we have to create our own tracker to
  // avoid other tests messing with the data here.
  ElementTracker element_tracker;
  EXPECT_TRUE(element_tracker.element_data_.empty());
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);

  // Add element shown callback. An element will be shown transiently during the
  // subscription.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto subscription = element_tracker.AddElementShownCallback(
        kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, element_tracker.element_data_.size());
    element_tracker.NotifyElementShown(e1.get());
    EXPECT_EQ(1U, element_tracker.element_data_.size());
    element_tracker.NotifyElementHidden(e1.get());
    EXPECT_EQ(1U, element_tracker.element_data_.size());
  }
  EXPECT_TRUE(element_tracker.element_data_.empty());

  // Add element shown in any context callback.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto subscription = element_tracker.AddElementShownInAnyContextCallback(
        kElementIdentifier1, callback.Get());
    EXPECT_EQ(1U, element_tracker.element_data_.size());
  }
  EXPECT_TRUE(element_tracker.element_data_.empty());

  // Add element activated callback.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto subscription = element_tracker.AddElementActivatedCallback(
        kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, element_tracker.element_data_.size());
  }
  EXPECT_TRUE(element_tracker.element_data_.empty());

  // Add element hidden callback.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto subscription = element_tracker.AddElementHiddenCallback(
        kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, element_tracker.element_data_.size());
  }
  EXPECT_TRUE(element_tracker.element_data_.empty());

  // Add custom event callback.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto subscription = element_tracker.AddCustomEventCallback(
        kCustomEventType1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, element_tracker.element_data_.size());
  }
  EXPECT_TRUE(element_tracker.element_data_.empty());

  // Add and remove multiple callbacks.
  {
    base::MockCallback<ElementTracker::Callback> callback;
    EXPECT_CALL(callback, Run).Times(testing::AnyNumber());
    auto sub1 = element_tracker.AddElementShownCallback(
        kElementIdentifier1, kElementContext1, callback.Get());
    auto sub2 = element_tracker.AddElementActivatedCallback(
        kElementIdentifier1, kElementContext1, callback.Get());
    auto sub3 = element_tracker.AddElementHiddenCallback(
        kElementIdentifier1, kElementContext1, callback.Get());
    EXPECT_EQ(1U, element_tracker.element_data_.size());
  }
  EXPECT_TRUE(element_tracker.element_data_.empty());
}

// The following test specific conditions that could trigger a UAF or cause
// similar instability due to changing callback lists during callbacks. These
// tests may fail all or some builds (specifically asan/msan) if the logic is
// implemented incorrectly.

TEST(ElementTrackerTest, RemoveCallbackDuringRemove) {
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
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
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
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
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
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
  test::TestElementPtr e1 = std::make_unique<test::TestElement>(
      kElementIdentifier1, kElementContext1);
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
  // Because we have to test for correct cleanup, we have to use an isolated
  // ElementTracker.
  ElementTracker element_tracker;
  test::TestElement e1(kElementIdentifier1, kElementContext1);
  ElementTracker::Subscription subscription;
  auto callback = base::BindLambdaForTesting([&](TrackedElement* element) {
    subscription = ElementTracker::Subscription();
    element_tracker.NotifyElementHidden(&e1);
  });
  subscription = element_tracker.AddElementShownCallback(
      e1.identifier(), e1.context(), callback);
  element_tracker.NotifyElementShown(&e1);

  // Verify that cleanup still happens after all callbacks return.
  EXPECT_TRUE(element_tracker.element_data_.empty());
}

// Regression test for a case where an element disappears in the middle of shown
// callbacks.
TEST(ElementTrackerTest, HideDuringShowCallbackMultipleListeners) {
  test::TestElement e1(kElementIdentifier1, kElementContext1);
  bool called1 = false;
  bool called2 = false;

  // The first callback should be called normally.
  ElementTracker::Subscription subscription1 =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          e1.identifier(), e1.context(),
          base::BindLambdaForTesting([&](TrackedElement* element) {
            EXPECT_EQ(&e1, element);
            called1 = true;
            e1.Hide();
          }));

  // The second callback will be called because of limitations with CallbackList
  // but will receive a null argument.
  ElementTracker::Subscription subscription2 =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          e1.identifier(), e1.context(),
          base::BindLambdaForTesting([&](TrackedElement* element) {
            EXPECT_EQ(nullptr, element);
            called2 = true;
          }));

  // The any context callbacks happen after context-specific callbacks, so we
  // don't bother calling them at all if the element is gone.
  ElementTracker::Subscription subscription3 =
      ui::ElementTracker::GetElementTracker()
          ->AddElementShownInAnyContextCallback(
              e1.identifier(),
              base::BindLambdaForTesting(
                  [&](TrackedElement* element) { NOTREACHED(); }));
  e1.Show();
  EXPECT_TRUE(called1);
  EXPECT_TRUE(called2);
}

TEST(ElementTrackerTest, GetAllContextsForTesting) {
  test::TestElement e1(kElementIdentifier1, kElementContext1);
  test::TestElement e2(kElementIdentifier1, kElementContext2);
  test::TestElement e3(kElementIdentifier2, kElementContext2);

  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllContextsForTesting(),
              testing::IsEmpty());

  e1.Show();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllContextsForTesting(),
              testing::UnorderedElementsAre(kElementContext1));

  e2.Show();
  e3.Show();
  EXPECT_THAT(
      ElementTracker::GetElementTracker()->GetAllContextsForTesting(),
      testing::UnorderedElementsAre(kElementContext1, kElementContext2));

  e1.Hide();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllContextsForTesting(),
              testing::UnorderedElementsAre(kElementContext2));
}

TEST(ElementTrackerTest, GetAllElementsForTestingInAnyContext) {
  test::TestElement e1(kElementIdentifier1, kElementContext1);
  test::TestElement e2(kElementIdentifier1, kElementContext2);
  test::TestElement e3(kElementIdentifier2, kElementContext2);

  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(),
              testing::IsEmpty());

  e1.Show();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(),
              testing::UnorderedElementsAre(&e1));

  e2.Show();
  e3.Show();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(),
              testing::UnorderedElementsAre(&e1, &e2, &e3));

  e1.Hide();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(),
              testing::UnorderedElementsAre(&e2, &e3));
}

TEST(ElementTrackerTest, GetAllElementsForTestingInSpecificContexts) {
  test::TestElement e1(kElementIdentifier1, kElementContext1);
  test::TestElement e2(kElementIdentifier1, kElementContext2);
  test::TestElement e3(kElementIdentifier2, kElementContext2);

  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(
                  kElementContext1),
              testing::IsEmpty());
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(
                  kElementContext2),
              testing::IsEmpty());

  e1.Show();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(
                  kElementContext1),
              testing::UnorderedElementsAre(&e1));
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(
                  kElementContext2),
              testing::IsEmpty());

  e2.Show();
  e3.Show();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(
                  kElementContext1),
              testing::UnorderedElementsAre(&e1));
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(
                  kElementContext2),
              testing::UnorderedElementsAre(&e2, &e3));

  e1.Hide();
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(
                  kElementContext1),
              testing::IsEmpty());
  EXPECT_THAT(ElementTracker::GetElementTracker()->GetAllElementsForTesting(
                  kElementContext2),
              testing::UnorderedElementsAre(&e2, &e3));
}

TEST(SafeElementReferenceTest, ElementRemainsVisible) {
  test::TestElement e1(kElementIdentifier1, kElementContext1);
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
  test::TestElement e1(kElementIdentifier1, kElementContext1);
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
  test::TestElement e1(kElementIdentifier1, kElementContext1);
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
  test::TestElement e1(kElementIdentifier1, kElementContext1);
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
  test::TestElement e1(kElementIdentifier1, kElementContext1);
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
  test::TestElement e1(kElementIdentifier1, kElementContext1);
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

class ElementTrackerIdentifierTest : public testing::Test {
 public:
  void SetUp() override { ElementIdentifier::GetKnownIdentifiers().clear(); }
};

TEST_F(ElementTrackerIdentifierTest, ShowElementRegistersIdentifier) {
  test::TestElement e1(kElementIdentifier1, kElementContext1);
  EXPECT_FALSE(ElementIdentifier::FromName(kElementIdentifier1Name));
  e1.Show();
  EXPECT_EQ(kElementIdentifier1,
            ElementIdentifier::FromName(kElementIdentifier1Name));
  e1.Hide();
  EXPECT_EQ(kElementIdentifier1,
            ElementIdentifier::FromName(kElementIdentifier1Name));
}

TEST_F(ElementTrackerIdentifierTest, AddListenerRegistersIdentifier) {
  EXPECT_FALSE(ElementIdentifier::FromName(kElementIdentifier1Name));
  auto subscription =
      ElementTracker::GetElementTracker()->AddElementShownCallback(
          kElementIdentifier1, kElementContext1, base::DoNothing());
  EXPECT_EQ(kElementIdentifier1,
            ElementIdentifier::FromName(kElementIdentifier1Name));
  subscription = ElementTracker::Subscription();
  EXPECT_EQ(kElementIdentifier1,
            ElementIdentifier::FromName(kElementIdentifier1Name));
}

}  // namespace ui
