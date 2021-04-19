// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

class AXInspectScenarioTest : public testing::Test {
 public:
  AXInspectScenarioTest() = default;
  ~AXInspectScenarioTest() override = default;

  AXInspectScenarioTest(const AXInspectScenarioTest&) = delete;
  AXInspectScenarioTest& operator=(const AXInspectScenarioTest&) = delete;
};

TEST_F(AXInspectScenarioTest, NoLoadExpected) {
  AXInspectScenario scenario =
      AXInspectScenario::From("@MAC", {"@NO-LOAD-EXPECTED:broken.jpg"});
  EXPECT_THAT(scenario.no_load_expected, testing::ElementsAre("broken.jpg"));
}

TEST_F(AXInspectScenarioTest, WaitFor) {
  AXInspectScenario scenario =
      AXInspectScenario::From("@MAC", {"@WAIT-FOR:(0, 50"});
  EXPECT_THAT(scenario.wait_for, testing::ElementsAre("(0, 50"));
}

TEST_F(AXInspectScenarioTest, DefaultActionOn) {
  AXInspectScenario scenario =
      AXInspectScenario::From("@MAC", {"@DEFAULT-ACTION-ON:Show time picker"});
  EXPECT_THAT(scenario.default_action_on,
              testing::ElementsAre("Show time picker"));
}

TEST_F(AXInspectScenarioTest, ExecuteAndWaitFor) {
  AXInspectScenario scenario =
      AXInspectScenario::From("@MAC", {"@EXECUTE-AND-WAIT-FOR:open_modal()"});
  EXPECT_THAT(scenario.execute, testing::ElementsAre("open_modal()"));
}

TEST_F(AXInspectScenarioTest, RunUntil) {
  AXInspectScenario scenario =
      AXInspectScenario::From("@MAC", {"@MAC-RUN-UNTIL-EVENT:AXRowCollapsed"});
  EXPECT_THAT(scenario.run_until, testing::ElementsAre("AXRowCollapsed"));
}

TEST_F(AXInspectScenarioTest, PropertyFilters) {
  AXInspectScenario scenario = AXInspectScenario::From(
      "@MAC", {"@MAC-ALLOW:AXRoleDescription", "@MAC-ALLOW-EMPTY:AXARIALive",
               "@MAC-DENY:AXTitle"});
  EXPECT_EQ(scenario.property_filters.size(), 3U);
  EXPECT_EQ(scenario.property_filters[0].type, AXPropertyFilter::ALLOW);
  EXPECT_EQ(scenario.property_filters[0].match_str, "AXRoleDescription");
  EXPECT_EQ(scenario.property_filters[1].type, AXPropertyFilter::ALLOW_EMPTY);
  EXPECT_EQ(scenario.property_filters[1].match_str, "AXARIALive");
  EXPECT_EQ(scenario.property_filters[2].type, AXPropertyFilter::DENY);
  EXPECT_EQ(scenario.property_filters[2].match_str, "AXTitle");
}

TEST_F(AXInspectScenarioTest, PropertyFilters_Multiline) {
  AXInspectScenario scenario = AXInspectScenario::From(
      "@MAC", {"@MAC-ALLOW:", "  AXRoleDescription", "  AXARIALive"});
  EXPECT_EQ(scenario.property_filters.size(), 2U);
  EXPECT_EQ(scenario.property_filters[0].type, AXPropertyFilter::ALLOW);
  EXPECT_EQ(scenario.property_filters[0].match_str, "AXRoleDescription");
  EXPECT_EQ(scenario.property_filters[1].type, AXPropertyFilter::ALLOW);
  EXPECT_EQ(scenario.property_filters[1].match_str, "AXARIALive");
}

TEST_F(AXInspectScenarioTest, NodeFilters) {
  AXInspectScenario scenario =
      AXInspectScenario::From("@MAC", {"@MAC-DENY-NODE:AXRole=AXCheckBox"});
  EXPECT_EQ(scenario.node_filters.size(), 1U);
  EXPECT_EQ(scenario.node_filters[0].property, "AXRole");
  EXPECT_EQ(scenario.node_filters[0].pattern, "AXCheckBox");
}

}  // namespace ui
