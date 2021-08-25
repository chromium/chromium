// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_tracker_mac.h"

#import <Cocoa/Cocoa.h>

#include <initializer_list>
#include <map>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/interaction/element_tracker.h"

namespace ui {

namespace {

NSMenu* const kFakeMenu1 = reinterpret_cast<NSMenu*>(1);
NSMenu* const kFakeMenu2 = reinterpret_cast<NSMenu*>(2);
NSMenu* const kFakeMenu3 = reinterpret_cast<NSMenu*>(3);

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementIdentifier3);

const ElementContext kElementContext1(1);
const ElementContext kElementContext2(2);

class ExpectedCall {
 public:
  enum Type { kShown, kActivated, kHidden };

  ExpectedCall(ElementIdentifier identifier,
               ElementContext context,
               Type type,
               int expected_count)
      : identifier_(identifier),
        context_(context),
        expected_count_(expected_count) {
    ElementTracker::Callback callback =
        base::BindRepeating(&ExpectedCall::OnCalled, base::Unretained(this));
    switch (type) {
      case kShown:
        subscription_ =
            ElementTracker::GetElementTracker()->AddElementShownCallback(
                identifier, context, callback);
        break;
      case kActivated:
        subscription_ =
            ElementTracker::GetElementTracker()->AddElementActivatedCallback(
                identifier, context, callback);
        break;
      case kHidden:
        subscription_ =
            ElementTracker::GetElementTracker()->AddElementHiddenCallback(
                identifier, context, callback);
        break;
    }
  }

  ExpectedCall(const ExpectedCall& other) = delete;
  void operator=(const ExpectedCall& other) = delete;

  ~ExpectedCall() { EXPECT_EQ(expected_count_, count_); }

  int count() const { return count_; }

 private:
  void OnCalled(TrackedElement* element) {
    ASSERT_TRUE(element->IsA<TrackedElementMac>());
    EXPECT_EQ(identifier_, element->identifier());
    EXPECT_EQ(context_, element->context());
    ++count_;
    EXPECT_LE(count_, expected_count_);
  }

  const ElementIdentifier identifier_;
  const ElementContext context_;
  const int expected_count_;
  int count_ = 0;
  ElementTracker::Subscription subscription_;
};

class TestElementTrackerMac : public ElementTrackerMac {
 public:
  TestElementTrackerMac() = default;

  ~TestElementTrackerMac() override { EXPECT_FALSE(is_tracking_any_menus()); }

  void SetParent(NSMenu* menu, NSMenu* parent) {
    parenting_.emplace(menu, parent);
  }

 protected:
  // ElementTrackerMac:
  NSMenu* GetRootMenu(NSMenu* menu) const override {
    while (true) {
      const auto it = parenting_.find(menu);
      if (it == parenting_.end())
        return menu;
      menu = it->second;
    }
  }

 private:
  std::map<NSMenu*, NSMenu*> parenting_;
};

}  // namespace

class ElementTrackerMacTest : public PlatformTest {
 public:
  void SetUp() override {
    element_tracker_ = std::make_unique<TestElementTrackerMac>();
  }

  void TearDown() override { element_tracker_.reset(); }

 protected:
  std::unique_ptr<TestElementTrackerMac> element_tracker_;
};

TEST_F(ElementTrackerMacTest, OpenCloseNoEvents) {
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

TEST_F(ElementTrackerMacTest, OpenShowOneIdentifier) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 0);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

TEST_F(ElementTrackerMacTest, OpenShowMultipleIdentifiers) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 0);
  ExpectedCall shown2(kElementIdentifier2, kElementContext1,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden2(kElementIdentifier2, kElementContext1,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated2(kElementIdentifier2, kElementContext1,
                          ExpectedCall::kActivated, 0);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier2);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

TEST_F(ElementTrackerMacTest, OpenShowMultipleIdentifiersDifferentMenus) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 0);
  ExpectedCall shown2(kElementIdentifier1, kElementContext2,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden2(kElementIdentifier1, kElementContext2,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated2(kElementIdentifier1, kElementContext2,
                          ExpectedCall::kActivated, 0);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuWillShow(kFakeMenu2, kElementContext2);
  element_tracker_->NotifyMenuItemShown(kFakeMenu2, kElementIdentifier1);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu2, kElementIdentifier1);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu2);
}

TEST_F(ElementTrackerMacTest, OpenShowMultipleIdentifiersActivate) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 1);
  ExpectedCall shown2(kElementIdentifier2, kElementContext1,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden2(kElementIdentifier2, kElementContext1,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated2(kElementIdentifier2, kElementContext1,
                          ExpectedCall::kActivated, 0);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier2);
  element_tracker_->NotifyMenuItemActivated(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

TEST_F(ElementTrackerMacTest,
       OpenShowMultipleIdentifiersDifferentMenusActivate) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 1);
  ExpectedCall shown2(kElementIdentifier1, kElementContext2,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden2(kElementIdentifier1, kElementContext2,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated2(kElementIdentifier1, kElementContext2,
                          ExpectedCall::kActivated, 1);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuWillShow(kFakeMenu2, kElementContext2);
  element_tracker_->NotifyMenuItemShown(kFakeMenu2, kElementIdentifier1);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemActivated(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu2, kElementIdentifier1);
  element_tracker_->NotifyMenuItemActivated(kFakeMenu2, kElementIdentifier1);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu2);
}

TEST_F(ElementTrackerMacTest, ShowHideMultipleTimes) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 2);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 2);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 0);
  ExpectedCall shown2(kElementIdentifier2, kElementContext1,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden2(kElementIdentifier2, kElementContext1,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated2(kElementIdentifier2, kElementContext1,
                          ExpectedCall::kActivated, 0);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier2);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

TEST_F(ElementTrackerMacTest, NestedMenus) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 0);
  ExpectedCall shown2(kElementIdentifier2, kElementContext1,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden2(kElementIdentifier2, kElementContext1,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated2(kElementIdentifier2, kElementContext1,
                          ExpectedCall::kActivated, 0);
  element_tracker_->SetParent(kFakeMenu2, kFakeMenu1);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

TEST_F(ElementTrackerMacTest, NestedMenusActivate) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 0);
  ExpectedCall shown2(kElementIdentifier2, kElementContext1,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden2(kElementIdentifier2, kElementContext1,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated2(kElementIdentifier2, kElementContext1,
                          ExpectedCall::kActivated, 1);
  element_tracker_->SetParent(kFakeMenu2, kFakeMenu1);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemActivated(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

TEST_F(ElementTrackerMacTest, NestMenusSendHideEvents) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 0);
  ExpectedCall shown2(kElementIdentifier2, kElementContext1,
                      ExpectedCall::kShown, 2);
  ExpectedCall hidden2(kElementIdentifier2, kElementContext1,
                       ExpectedCall::kHidden, 2);
  ExpectedCall activated2(kElementIdentifier2, kElementContext1,
                          ExpectedCall::kActivated, 1);
  element_tracker_->SetParent(kFakeMenu2, kFakeMenu1);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemShown(kFakeMenu2, kElementIdentifier2);
  EXPECT_EQ(1, hidden2.count());
  element_tracker_->NotifyMenuItemHidden(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemActivated(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

TEST_F(ElementTrackerMacTest, NestMenusSendHideEvents_SecondSubmenuOpened) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 0);
  ExpectedCall shown2(kElementIdentifier2, kElementContext1,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden2(kElementIdentifier2, kElementContext1,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated2(kElementIdentifier2, kElementContext1,
                          ExpectedCall::kActivated, 0);
  ExpectedCall shown3(kElementIdentifier3, kElementContext1,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden3(kElementIdentifier3, kElementContext1,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated3(kElementIdentifier3, kElementContext1,
                          ExpectedCall::kActivated, 1);
  element_tracker_->SetParent(kFakeMenu2, kFakeMenu1);
  element_tracker_->SetParent(kFakeMenu3, kFakeMenu1);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemShown(kFakeMenu3, kElementIdentifier3);
  EXPECT_EQ(1, hidden2.count());
  element_tracker_->NotifyMenuItemHidden(kFakeMenu3, kElementIdentifier3);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemActivated(kFakeMenu3, kElementIdentifier3);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

TEST_F(ElementTrackerMacTest, NestMenusSendHideEvents_DoubleSubmenu) {
  ExpectedCall shown(kElementIdentifier1, kElementContext1,
                     ExpectedCall::kShown, 1);
  ExpectedCall hidden(kElementIdentifier1, kElementContext1,
                      ExpectedCall::kHidden, 1);
  ExpectedCall activated(kElementIdentifier1, kElementContext1,
                         ExpectedCall::kActivated, 0);
  ExpectedCall shown2(kElementIdentifier2, kElementContext1,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden2(kElementIdentifier2, kElementContext1,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated2(kElementIdentifier2, kElementContext1,
                          ExpectedCall::kActivated, 0);
  ExpectedCall shown3(kElementIdentifier3, kElementContext1,
                      ExpectedCall::kShown, 1);
  ExpectedCall hidden3(kElementIdentifier3, kElementContext1,
                       ExpectedCall::kHidden, 1);
  ExpectedCall activated3(kElementIdentifier3, kElementContext1,
                          ExpectedCall::kActivated, 1);
  element_tracker_->SetParent(kFakeMenu2, kFakeMenu1);
  element_tracker_->SetParent(kFakeMenu3, kFakeMenu2);
  element_tracker_->NotifyMenuWillShow(kFakeMenu1, kElementContext1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemShown(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemShown(kFakeMenu3, kElementIdentifier3);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu3, kElementIdentifier3);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu2, kElementIdentifier2);
  element_tracker_->NotifyMenuItemHidden(kFakeMenu1, kElementIdentifier1);
  element_tracker_->NotifyMenuItemActivated(kFakeMenu3, kElementIdentifier3);
  element_tracker_->NotifyMenuDoneShowing(kFakeMenu1);
}

}  // namespace ui
