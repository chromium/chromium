// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#include "ui/gfx/geometry/point.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {

namespace {

id<NSAccessibility> ToNSAccessibility(id obj) {
  return [obj conformsToProtocol:@protocol(NSAccessibility)] ? obj : nil;
}

// Unboxes an accessibilityValue into an int via NSNumber.
int IdToInt(id value) {
  return base::mac::ObjCCastStrict<NSNumber>(value).intValue;
}

// TODO(https://crbug.com/936990): NSTabItemView is not an NSView (despite the
// name) and doesn't conform to NSAccessibility, so we have to fall back to the
// legacy NSObject accessibility API to get its accessibility properties.
id GetLegacyA11yAttributeValue(id obj, NSString* attribute) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  return [obj accessibilityAttributeValue:attribute];
#pragma clang diagnostic pop
}

}  // namespace

class TabbedPaneAccessibilityMacTest : public WidgetTest {
 public:
  TabbedPaneAccessibilityMacTest() = default;

  // WidgetTest:
  void SetUp() override {
    WidgetTest::SetUp();
    widget_ = CreateTopLevelPlatformWidget();
    widget_->SetBounds(gfx::Rect(50, 50, 100, 100));
    auto tabbed_pane = std::make_unique<TabbedPane>();
    tabbed_pane->SetSize(gfx::Size(100, 100));

    // Create two tabs and position/size them.
    tabbed_pane->AddTab(base::ASCIIToUTF16("Tab 1"), std::make_unique<View>());
    tabbed_pane->AddTab(base::ASCIIToUTF16("Tab 2"), std::make_unique<View>());
    tabbed_pane->Layout();

    tabbed_pane_ =
        widget_->GetContentsView()->AddChildView(std::move(tabbed_pane));
    widget_->Show();
  }

  void TearDown() override {
    widget_->CloseNow();
    WidgetTest::TearDown();
  }

  Tab* GetTabAt(size_t index) {
    return static_cast<Tab*>(tabbed_pane_->tab_strip_->children()[index]);
  }

  id<NSAccessibility> A11yElementAtPoint(const gfx::Point& point) {
    // Accessibility hit tests come in Cocoa screen coordinates.
    NSPoint ns_point = gfx::ScreenPointToNSPoint(point);
    return ToNSAccessibility([widget_->GetNativeWindow().GetNativeNSWindow()
        accessibilityHitTest:ns_point]);
  }

  gfx::Point TabCenterPoint(size_t index) {
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
      GetLegacyA11yAttributeValue(cocoa_tabs[0], NSAccessibilityRoleAttribute),
      A11yElementAtPoint(TabCenterPoint(0)).accessibilityRole);
  EXPECT_NSEQ(
      GetLegacyA11yAttributeValue(cocoa_tabs[0],
                                  NSAccessibilityRoleDescriptionAttribute),
      A11yElementAtPoint(TabCenterPoint(0)).accessibilityRoleDescription);
  EXPECT_NSEQ(
      GetLegacyA11yAttributeValue(cocoa_tabs[0], NSAccessibilityTitleAttribute),
      A11yElementAtPoint(TabCenterPoint(0)).accessibilityTitle);

  // Compare the value attribute against native Cocoa and check it matches up
  // with whether tabs are actually selected.
  for (size_t i : {0, 1}) {
    NSNumber* cocoa_value = GetLegacyA11yAttributeValue(
        cocoa_tabs[i], NSAccessibilityValueAttribute);
    // Verify that only the second tab is selected.
    EXPECT_EQ(i ? 0 : 1, [cocoa_value intValue]);
    EXPECT_NSEQ(cocoa_value,
                A11yElementAtPoint(TabCenterPoint(i)).accessibilityValue);
  }

  // NSTabViewItem doesn't support NSAccessibilitySelectedAttribute, so don't
  // compare against Cocoa here.
  EXPECT_TRUE(A11yElementAtPoint(TabCenterPoint(0)).accessibilitySelected);
  EXPECT_FALSE(A11yElementAtPoint(TabCenterPoint(1)).accessibilitySelected);
}

// Make sure tabs can be selected by writing the value attribute.
TEST_F(TabbedPaneAccessibilityMacTest, WritableValue) {
  id<NSAccessibility> tab1_a11y = A11yElementAtPoint(TabCenterPoint(0));
  id<NSAccessibility> tab2_a11y = A11yElementAtPoint(TabCenterPoint(1));

  // Only unselected tabs should be writable.
  EXPECT_FALSE([tab1_a11y
      isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)]);
  EXPECT_TRUE([tab2_a11y
      isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)]);

  // Select the second tab. AXValue actually accepts any type, but for tabs,
  // Cocoa uses an integer. Despite this, the Accessibility Inspector provides a
  // textfield to set the value for a control, so test this with an NSString.
  tab2_a11y.accessibilityValue = @"string";

  EXPECT_EQ(0, IdToInt(tab1_a11y.accessibilityValue));
  EXPECT_EQ(1, IdToInt(tab2_a11y.accessibilityValue));
  EXPECT_FALSE(tab1_a11y.accessibilitySelected);
  EXPECT_TRUE(tab2_a11y.accessibilitySelected);

  EXPECT_TRUE(GetTabAt(1)->selected());

  // It doesn't make sense to 'deselect' a tab (i.e., without specifying another
  // to select). So any value passed to -accessibilitySetValue: should select
  // that tab. Try an empty string.
  tab1_a11y.accessibilityValue = @"";
  EXPECT_EQ(1, IdToInt(tab1_a11y.accessibilityValue));
  EXPECT_EQ(0, IdToInt(tab2_a11y.accessibilityValue));
  EXPECT_TRUE(tab1_a11y.accessibilitySelected);
  EXPECT_FALSE(tab2_a11y.accessibilitySelected);
  EXPECT_TRUE(GetTabAt(0)->selected());
}

}  // namespace test
}  // namespace views
