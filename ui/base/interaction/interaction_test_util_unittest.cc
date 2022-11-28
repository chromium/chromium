// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_test_util.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/events/event_constants.h"

#if !BUILDFLAG(IS_IOS)
#include "ui/base/accelerators/accelerator.h"
#endif

namespace ui::test {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementIdentifier);
const ElementContext kTestElementContext(1);

class MockInteractionSimulator : public InteractionTestUtil::Simulator {
 public:
  MockInteractionSimulator() = default;
  ~MockInteractionSimulator() override = default;

  MOCK_METHOD2(PressButton,
               bool(TrackedElement* element, InputType input_type));
  MOCK_METHOD2(SelectMenuItem,
               bool(TrackedElement* element, InputType input_type));
  MOCK_METHOD2(DoDefaultAction,
               bool(TrackedElement* element, InputType input_type));
  MOCK_METHOD3(SelectTab,
               bool(TrackedElement* tab_collection,
                    size_t index,
                    InputType input_type));
  MOCK_METHOD3(SelectDropdownItem,
               bool(TrackedElement* dropdown,
                    size_t index,
                    InputType input_type));
  MOCK_METHOD3(EnterText,
               bool(TrackedElement* element,
                    const std::u16string& text,
                    TextEntryMode mode));
  MOCK_METHOD1(ActivateSurface, bool(TrackedElement* element));
#if !BUILDFLAG(IS_IOS)
  MOCK_METHOD2(SendAccelerator,
               bool(TrackedElement* element, const Accelerator& accelerator));
#endif
  MOCK_METHOD1(Confirm, bool(TrackedElement* element));
};

}  // namespace

TEST(InteractionTestUtilTest, PressButton) {
  TestElement element(kTestElementIdentifier, kTestElementContext);
  InteractionTestUtil util;
  auto* const mock = util.AddSimulator(
      std::make_unique<testing::StrictMock<MockInteractionSimulator>>());
  EXPECT_CALL(*mock,
              PressButton(&element, InteractionTestUtil::InputType::kDontCare))
      .WillOnce(testing::Return(true));
  util.PressButton(&element);
}

TEST(InteractionTestUtilTest, SelectMenuItem) {
  TestElement element(kTestElementIdentifier, kTestElementContext);
  InteractionTestUtil util;
  auto* const mock = util.AddSimulator(
      std::make_unique<testing::StrictMock<MockInteractionSimulator>>());
  EXPECT_CALL(*mock, SelectMenuItem(&element,
                                    InteractionTestUtil::InputType::kDontCare))
      .WillOnce(testing::Return(true));
  util.SelectMenuItem(&element);
}

TEST(InteractionTestUtilTest, DoDefaultAction) {
  TestElement element(kTestElementIdentifier, kTestElementContext);
  InteractionTestUtil util;
  auto* const mock = util.AddSimulator(
      std::make_unique<testing::StrictMock<MockInteractionSimulator>>());
  EXPECT_CALL(*mock, DoDefaultAction(&element,
                                     InteractionTestUtil::InputType::kDontCare))
      .WillOnce(testing::Return(true));
  util.DoDefaultAction(&element);
}

TEST(InteractionTestUtilTest, SelectTab) {
  TestElement element(kTestElementIdentifier, kTestElementContext);
  InteractionTestUtil util;
  auto* const mock = util.AddSimulator(
      std::make_unique<testing::StrictMock<MockInteractionSimulator>>());
  EXPECT_CALL(
      *mock, SelectTab(&element, 1U, InteractionTestUtil::InputType::kDontCare))
      .WillOnce(testing::Return(true));
  util.SelectTab(&element, 1U);
}

TEST(InteractionTestUtilTest, SelectDropdownItem) {
  TestElement element(kTestElementIdentifier, kTestElementContext);
  InteractionTestUtil util;
  auto* const mock = util.AddSimulator(
      std::make_unique<testing::StrictMock<MockInteractionSimulator>>());
  EXPECT_CALL(*mock,
              SelectDropdownItem(&element, 1U,
                                 InteractionTestUtil::InputType::kDontCare))
      .WillOnce(testing::Return(true));
  util.SelectDropdownItem(&element, 1U);
}

TEST(InteractionTestUtilTest, EnterText) {
  constexpr char16_t kText[] = u"Some text.";
  TestElement element(kTestElementIdentifier, kTestElementContext);
  InteractionTestUtil util;
  auto* const mock = util.AddSimulator(
      std::make_unique<testing::StrictMock<MockInteractionSimulator>>());

  EXPECT_CALL(*mock, EnterText(&element, std::u16string(kText),
                               InteractionTestUtil::TextEntryMode::kAppend))
      .WillOnce(testing::Return(true));
  util.EnterText(&element, kText, InteractionTestUtil::TextEntryMode::kAppend);

  EXPECT_CALL(*mock, EnterText(&element, std::u16string(kText),
                               InteractionTestUtil::TextEntryMode::kReplaceAll))
      .WillOnce(testing::Return(true));
  util.EnterText(&element, kText,
                 InteractionTestUtil::TextEntryMode::kReplaceAll);
}

TEST(InteractionTestUtilTest, ActivateSurface) {
  TestElement element(kTestElementIdentifier, kTestElementContext);
  InteractionTestUtil util;
  auto* const mock = util.AddSimulator(
      std::make_unique<testing::StrictMock<MockInteractionSimulator>>());

  EXPECT_CALL(*mock, ActivateSurface(&element)).WillOnce(testing::Return(true));
  util.ActivateSurface(&element);
}

#if !BUILDFLAG(IS_IOS)
TEST(InteractionTestUtilTest, SendAccelerator) {
  TestElement element(kTestElementIdentifier, kTestElementContext);
  InteractionTestUtil util;
  auto* const mock = util.AddSimulator(
      std::make_unique<testing::StrictMock<MockInteractionSimulator>>());

  Accelerator accel(KeyboardCode::VKEY_F5, EF_SHIFT_DOWN);
  EXPECT_CALL(*mock, SendAccelerator(&element, testing::Eq(accel)))
      .WillOnce(testing::Return(true));
  util.SendAccelerator(&element, accel);
}
#endif  // !BUILDFLAG(IS_IOS)

TEST(InteractionTestUtilTest, Confirm) {
  TestElement element(kTestElementIdentifier, kTestElementContext);
  InteractionTestUtil util;
  auto* const mock = util.AddSimulator(
      std::make_unique<testing::StrictMock<MockInteractionSimulator>>());

  EXPECT_CALL(*mock, Confirm(&element)).WillOnce(testing::Return(true));
  util.Confirm(&element);
}

}  // namespace ui::test
