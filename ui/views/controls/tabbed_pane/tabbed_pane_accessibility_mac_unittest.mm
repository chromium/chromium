// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/point.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#import "testing/gtest_mac.h"

namespace views {
namespace test {

class TabbedPaneAccessibilityMacTest : public WidgetTest {
 public:
  TabbedPaneAccessibilityMacTest() {}

  // WidgetTest:
  void SetUp() override {
    WidgetTest::SetUp();
    widget_ = CreateTopLevelPlatformWidget();
    widget_->SetBounds(gfx::Rect(50, 50, 100, 100));
    tabbed_pane_ = new TabbedPane();
    tabbed_pane_->SetSize(gfx::Size(100, 100));

    // Create two tabs and position/size them.
    tabbed_pane_->AddTab(base::ASCIIToUTF16("Tab 1"), new View());
    tabbed_pane_->AddTab(base::ASCIIToUTF16("Tab 2"), new View());
    tabbed_pane_->Layout();

    widget_->GetContentsView()->AddChildView(tabbed_pane_);
    widget_->Show();
  }

  void TearDown() override {
    widget_->CloseNow();
    WidgetTest::TearDown();
  }

  Tab* GetTabAt(int index) {
    return static_cast<Tab*>(tabbed_pane_->tab_strip_->child_at(index));
  }

  id AttributeValueAtPoint(NSString* attribute, const gfx::Point& point) {
    id value =
        [A11yElementAtPoint(point) accessibilityAttributeValue:attribute];
    EXPECT_NE(nil, value);
    return value;
  }

  id A11yElementAtPoint(const gfx::Point& point) {
    // Accessibility hit tests come in Cocoa screen coordinates.
    NSPoint ns_point = gfx::ScreenPointToNSPoint(point);
    return [widget_->GetNativeWindow().GetNativeNSWindow()
        accessibilityHitTest:ns_point];
  }

  gfx::Point TabCenterPoint(int index) {
    return GetTabAt(index)->GetBoundsInScreen().CenterPoint();
  }

 protected:
  Widget* widget_ = nullptr;
  TabbedPane* tabbed_pane_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabbedPaneAccessibilityMacTest);
};

// Test the Tab's a11y information compared to a Cocoa NSTabViewItem.
TEST_F(TabbedPaneAccessibilityMacTest, AttributesMatchAppKit) {
  // Create a Cocoa NSTabView to test against and select the first tab.
  base::scoped_nsobject<NSTabView> cocoa_tab_group(
      [[NSTabView alloc] initWithFrame:NSMakeRect(50, 50, 100, 100)]);
  NSArray* cocoa_tabs = @[
    [[[NSTabViewItem alloc] init] autorelease],
    [[[NSTabViewItem alloc] init] autorelease],
  ];
  for (size_t i = 0; i < [cocoa_tabs count]; ++i) {
    [cocoa_tabs[i] setLabel:[NSString stringWithFormat:@"Tab %zu", i + 1]];
    [cocoa_tab_group addTabViewItem:cocoa_tabs[i]];
  }

  // General a11y information.
  EXPECT_NSEQ(
      [cocoa_tabs[0] accessibilityAttributeValue:NSAccessibilityRoleAttribute],
      AttributeValueAtPoint(NSAccessibilityRoleAttribute, TabCenterPoint(0)));
  EXPECT_NSEQ(
      [cocoa_tabs[0]
          accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute],
      AttributeValueAtPoint(NSAccessibilityRoleDescriptionAttribute,
                            TabCenterPoint(0)));
  EXPECT_NSEQ(
      [cocoa_tabs[0] accessibilityAttributeValue:NSAccessibilityTitleAttribute],
      AttributeValueAtPoint(NSAccessibilityTitleAttribute, TabCenterPoint(0)));

  // Compare the value attribute against native Cocoa and check it matches up
  // with whether tabs are actually selected.
  for (int i : {0, 1}) {
    NSNumber* cocoa_value = [cocoa_tabs[i]
        accessibilityAttributeValue:NSAccessibilityValueAttribute];
    // Verify that only the second tab is selected.
    EXPECT_EQ(i ? 0 : 1, [cocoa_value intValue]);
    EXPECT_NSEQ(cocoa_value,
                AttributeValueAtPoint(NSAccessibilityValueAttribute,
                                      TabCenterPoint(i)));
  }

  // NSTabViewItem doesn't support NSAccessibilitySelectedAttribute, so don't
  // compare against Cocoa here.
  EXPECT_TRUE([AttributeValueAtPoint(NSAccessibilitySelectedAttribute,
                                     TabCenterPoint(0)) boolValue]);
  EXPECT_FALSE([AttributeValueAtPoint(NSAccessibilitySelectedAttribute,
                                      TabCenterPoint(1)) boolValue]);
}

// Make sure tabs can be selected by writing the value attribute.
TEST_F(TabbedPaneAccessibilityMacTest, WritableValue) {
  id tab1_a11y = A11yElementAtPoint(TabCenterPoint(0));
  id tab2_a11y = A11yElementAtPoint(TabCenterPoint(1));

  // Only unselected tabs should be writable.
  EXPECT_FALSE([tab1_a11y
      accessibilityIsAttributeSettable:NSAccessibilityValueAttribute]);
  EXPECT_TRUE([tab2_a11y
      accessibilityIsAttributeSettable:NSAccessibilityValueAttribute]);

  // Select the second tab. AXValue actually accepts any type, but for tabs,
  // Cocoa uses an integer. Despite this, the Accessibility Inspector provides a
  // textfield to set the value for a control, so test this with an NSString.
  [tab2_a11y accessibilitySetValue:@"string"
                      forAttribute:NSAccessibilityValueAttribute];
  EXPECT_EQ(0, [AttributeValueAtPoint(NSAccessibilityValueAttribute,
                                      TabCenterPoint(0)) intValue]);
  EXPECT_EQ(1, [AttributeValueAtPoint(NSAccessibilityValueAttribute,
                                      TabCenterPoint(1)) intValue]);
  EXPECT_FALSE([AttributeValueAtPoint(NSAccessibilitySelectedAttribute,
                                      TabCenterPoint(0)) boolValue]);
  EXPECT_TRUE([AttributeValueAtPoint(NSAccessibilitySelectedAttribute,
                                     TabCenterPoint(1)) boolValue]);
  EXPECT_TRUE(GetTabAt(1)->selected());

  // It doesn't make sense to 'deselect' a tab (i.e., without specifying another
  // to select). So any value passed to -accessibilitySetValue: should select
  // that tab. Try an empty string.
  [tab1_a11y accessibilitySetValue:@""
                      forAttribute:NSAccessibilityValueAttribute];
  EXPECT_EQ(1, [AttributeValueAtPoint(NSAccessibilityValueAttribute,
                                      TabCenterPoint(0)) intValue]);
  EXPECT_EQ(0, [AttributeValueAtPoint(NSAccessibilityValueAttribute,
                                      TabCenterPoint(1)) intValue]);
  EXPECT_TRUE([AttributeValueAtPoint(NSAccessibilitySelectedAttribute,
                                     TabCenterPoint(0)) boolValue]);
  EXPECT_FALSE([AttributeValueAtPoint(NSAccessibilitySelectedAttribute,
                                      TabCenterPoint(1)) boolValue]);
  EXPECT_TRUE(GetTabAt(0)->selected());
}

}  // namespace test
}  // namespace views
