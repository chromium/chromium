// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#import "ui/accessibility/platform/ax_platform_node_mac.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/models/combobox_model.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

NSString* const kTestPlaceholderText = @"Test placeholder text";
NSString* const kTestStringValue = @"Test string value";
constexpr int kTestStringLength = 17;
NSString* const kTestRTLStringValue = @"אבגדהוזאבגדהוז";
NSString* const kTestTitle = @"Test textfield";

id<NSAccessibility> ToNSAccessibility(id obj) {
  return [obj conformsToProtocol:@protocol(NSAccessibility)] ? obj : nil;
}

id<NSAccessibility> AXParentOf(id<NSAccessibility> child) {
  return ToNSAccessibility(child.accessibilityParent);
}

bool AXObjectHandlesSelector(id<NSAccessibility> ax_obj, SEL action) {
  return [ax_obj respondsToSelector:action] &&
         [ax_obj isAccessibilitySelectorAllowed:action];
}

class FlexibleRoleTestView : public View {
 public:
  explicit FlexibleRoleTestView(ax::mojom::Role role) : role_(role) {}
  void set_role(ax::mojom::Role role) { role_ = role; }

  // Add a child view and resize to fit the child.
  void FitBoundsToNewChild(View* view) {
    AddChildView(view);
    // Fit the parent widget to the size of the child for accurate hit tests.
    SetBoundsRect(view->bounds());
  }

  bool mouse_was_pressed() const { return mouse_was_pressed_; }

  // View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    View::GetAccessibleNodeData(node_data);
    node_data->role = role_;
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    mouse_was_pressed_ = true;
    return false;
  }

 private:
  ax::mojom::Role role_;
  bool mouse_was_pressed_ = false;

  DISALLOW_COPY_AND_ASSIGN(FlexibleRoleTestView);
};

class TestLabelButton : public LabelButton {
 public:
  TestLabelButton() : LabelButton(nullptr, base::string16()) {
    // Make sure the label doesn't cover the hit test co-ordinates.
    label()->SetSize(gfx::Size(1, 1));
  }

  using LabelButton::label;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLabelButton);
};

class TestWidgetDelegate : public test::TestDesktopWidgetDelegate {
 public:
  TestWidgetDelegate() = default;

  static constexpr char kAccessibleWindowTitle[] = "My Accessible Window";

  // WidgetDelegate:
  base::string16 GetAccessibleWindowTitle() const override {
    return base::ASCIIToUTF16(kAccessibleWindowTitle);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

constexpr char TestWidgetDelegate::kAccessibleWindowTitle[];

// Widget-level tests for accessibility properties - these are actually mostly
// tests of accessibility behavior for individual Views *as they appear* in
// Widgets.
class AXNativeWidgetMacTest : public test::WidgetTest {
 public:
  AXNativeWidgetMacTest() = default;

  void SetUp() override {
    test::WidgetTest::SetUp();
    widget_delegate_.InitWidget(CreateParams(Widget::InitParams::TYPE_WINDOW));
    widget()->Show();
  }

  void TearDown() override {
    widget()->CloseNow();
    test::WidgetTest::TearDown();
  }

  id<NSAccessibility> A11yElementAtMidpoint() {
    // Accessibility hit tests come in Cocoa screen coordinates.
    NSPoint midpoint_in_screen_ = gfx::ScreenPointToNSPoint(
        widget()->GetWindowBoundsInScreen().CenterPoint());
    return ToNSAccessibility([widget()->GetNativeWindow().GetNativeNSWindow()
        accessibilityHitTest:midpoint_in_screen_]);
  }

  Textfield* AddChildTextfield(const gfx::Size& size) {
    Textfield* textfield = new Textfield;
    textfield->SetText(base::SysNSStringToUTF16(kTestStringValue));
    textfield->SetAccessibleName(base::SysNSStringToUTF16(kTestTitle));
    textfield->SetSize(size);
    widget()->GetContentsView()->AddChildView(textfield);
    return textfield;
  }

  Widget* widget() { return widget_delegate_.GetWidget(); }
  gfx::Rect GetWidgetBounds() {
    return widget()->GetClientAreaBoundsInScreen();
  }

 private:
  TestWidgetDelegate widget_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AXNativeWidgetMacTest);
};

}  // namespace

// Test that all methods in the NSAccessibility informal protocol can be called
// on a retained accessibility object after the source view is deleted.
TEST_F(AXNativeWidgetMacTest, Lifetime) {
  Textfield* view = AddChildTextfield(widget()->GetContentsView()->size());
  base::scoped_nsobject<NSObject> ax_node(view->GetNativeViewAccessible(),
                                          base::scoped_policy::RETAIN);
  id<NSAccessibility> ax_obj = ToNSAccessibility(ax_node.get());

  EXPECT_TRUE(AXObjectHandlesSelector(ax_obj, @selector(accessibilityValue)));
  EXPECT_NSEQ(kTestStringValue, ax_obj.accessibilityValue);
  EXPECT_TRUE(
      AXObjectHandlesSelector(ax_obj, @selector(setAccessibilityValue:)));
  EXPECT_TRUE(
      AXObjectHandlesSelector(ax_obj, @selector(accessibilityPerformShowMenu)));
  EXPECT_TRUE(ax_obj.isAccessibilityElement);

  EXPECT_TRUE(
      AXObjectHandlesSelector(ax_obj, @selector(accessibilityStringForRange:)));

  NSRange range = NSMakeRange(0, kTestStringLength);
  EXPECT_NSEQ(kTestStringValue, [ax_obj accessibilityStringForRange:range]);

  // The following is also "not implemented", but the informal protocol category
  // provides a default implementation.
  EXPECT_EQ(NSNotFound, static_cast<NSInteger>(
                            [ax_node accessibilityIndexOfChild:ax_node]));

  // The only usually available array attribute is AXChildren, so go up a level
  // to the Widget to test that a bit. The default implementation just gets the
  // attribute normally and returns its size (if it's an array).
  base::scoped_nsprotocol<id<NSAccessibility>> ax_parent(
      ax_obj.accessibilityParent, base::scoped_policy::RETAIN);
  EXPECT_EQ(1u, ax_parent.get().accessibilityChildren.count);
  EXPECT_EQ(ax_node.get(), ax_parent.get().accessibilityChildren[0]);

  // If it is not an array, the default implementation throws an exception, so
  // it's impossible to test these methods further on |ax_node|, apart from the
  // following, before deleting the view.
  EXPECT_EQ(0u, ax_obj.accessibilityChildren.count);

  delete view;

  // ax_obj should still respond to setAccessibilityValue: (because the NSObject
  // is still live), but isAccessibilitySelectorAllowed: should return NO
  // because the backing View is gone. Invoking those selectors, which AppKit
  // sometimes does even if the object returns NO from
  // isAccessibilitySelectorAllowed:, should not crash.
  EXPECT_TRUE([ax_obj respondsToSelector:@selector(setAccessibilityValue:)]);
  EXPECT_FALSE(
      AXObjectHandlesSelector(ax_obj, @selector(setAccessibilityValue:)));
  [ax_obj setAccessibilityValue:kTestStringValue];

  EXPECT_FALSE(
      AXObjectHandlesSelector(ax_obj, @selector(accessibilityPerformShowMenu)));
  [ax_obj accessibilityPerformShowMenu];

  EXPECT_FALSE([ax_obj isAccessibilityElement]);
  EXPECT_EQ(nil, [ax_node accessibilityHitTest:NSZeroPoint]);
  EXPECT_EQ(nil, [ax_node accessibilityFocusedUIElement]);

  EXPECT_NSEQ(nil, [ax_obj accessibilityStringForRange:range]);

  // Test the attributes with default implementations provided.
  EXPECT_EQ(NSNotFound, static_cast<NSInteger>(
                            [ax_node accessibilityIndexOfChild:ax_node]));

  // The Widget is currently still around, but the child should be gone.
  EXPECT_EQ(0u, ax_parent.get().accessibilityChildren.count);
}

// Check that potentially keyboard-focusable elements are always leaf nodes.
TEST_F(AXNativeWidgetMacTest, FocusableElementsAreLeafNodes) {
  // LabelButtons will have a label inside the button. The label should be
  // ignored because the button is potentially keyboard focusable.
  TestLabelButton* button = new TestLabelButton();
  button->SetSize(widget()->GetContentsView()->size());
  widget()->GetContentsView()->AddChildView(button);

  id<NSAccessibility> ax_button =
      ToNSAccessibility(button->GetNativeViewAccessible());
  EXPECT_NSEQ(NSAccessibilityButtonRole, ax_button.accessibilityRole);
  id<NSAccessibility> ax_label =
      ToNSAccessibility(button->label()->GetNativeViewAccessible());

  EXPECT_EQ(0u, ax_button.accessibilityChildren.count);

  // The exception is if the child is explicitly marked accessibility focusable.
  button->label()->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  EXPECT_EQ(1u, ax_button.accessibilityChildren.count);
  EXPECT_NSEQ(ax_label, ax_button.accessibilityChildren[0]);

  // If the child is disabled, it should still be traversable.
  button->label()->SetEnabled(false);
  EXPECT_EQ(1u, ax_button.accessibilityChildren.count);
  EXPECT_EQ(ax_label, ax_button.accessibilityChildren[0]);
}

// Test for NSAccessibilityChildrenAttribute, and ensure it excludes ignored
// children from the accessibility tree.
TEST_F(AXNativeWidgetMacTest, ChildrenAttribute) {
  // Check childless views don't have accessibility children.
  id<NSAccessibility> ax_node = A11yElementAtMidpoint();
  EXPECT_EQ(0u, ax_node.accessibilityChildren.count);

  const size_t kNumChildren = 3;
  for (size_t i = 0; i < kNumChildren; ++i) {
    // Make sure the labels won't interfere with the hit test.
    AddChildTextfield(gfx::Size());
  }

  EXPECT_EQ(kNumChildren, ax_node.accessibilityChildren.count);

  // Check ignored children don't show up in the accessibility tree.
  widget()->GetContentsView()->AddChildView(
      new FlexibleRoleTestView(ax::mojom::Role::kIgnored));
  EXPECT_EQ(kNumChildren, ax_node.accessibilityChildren.count);
}

// Test for NSAccessibilityParentAttribute, including for a Widget with no
// parent.
TEST_F(AXNativeWidgetMacTest, ParentAttribute) {
  Textfield* child = AddChildTextfield(widget()->GetContentsView()->size());

  id<NSAccessibility> ax_child = A11yElementAtMidpoint();
  id<NSAccessibility> ax_parent = AXParentOf(ax_child);

  // Views with Widget parents will have a NSAccessibilityGroupRole parent.
  // See https://crbug.com/875843 for more information.
  ASSERT_NSNE(nil, ax_parent);
  EXPECT_NSEQ(NSAccessibilityGroupRole, ax_parent.accessibilityRole);

  // Views with non-Widget parents will have the role of the parent view.
  widget()->GetContentsView()->RemoveChildView(child);
  FlexibleRoleTestView* parent =
      new FlexibleRoleTestView(ax::mojom::Role::kGroup);
  parent->FitBoundsToNewChild(child);
  widget()->GetContentsView()->AddChildView(parent);

  // These might have been invalidated by the View gyrations just above, so
  // recompute them.
  ax_child = A11yElementAtMidpoint();
  ax_parent = AXParentOf(ax_child);

  EXPECT_NSEQ(NSAccessibilityGroupRole, ax_parent.accessibilityRole);

  // Test an ignored role parent is skipped in favor of the grandparent.
  parent->set_role(ax::mojom::Role::kIgnored);
  ASSERT_NSNE(nil, AXParentOf(ax_child));
  EXPECT_NSEQ(NSAccessibilityGroupRole, AXParentOf(ax_child).accessibilityRole);
}

// Test for NSAccessibilityPositionAttribute, including on Widget movement
// updates.
TEST_F(AXNativeWidgetMacTest, PositionAttribute) {
  NSPoint widget_origin =
      gfx::ScreenPointToNSPoint(GetWidgetBounds().bottom_left());
  EXPECT_NSEQ(widget_origin, A11yElementAtMidpoint().accessibilityFrame.origin);

  // Check the attribute is updated when the Widget is moved.
  gfx::Rect new_bounds(60, 80, 100, 100);
  widget()->SetBounds(new_bounds);
  widget_origin = gfx::ScreenPointToNSPoint(new_bounds.bottom_left());
  EXPECT_NSEQ(widget_origin, A11yElementAtMidpoint().accessibilityFrame.origin);
}

// Test for NSAccessibilityHelpAttribute.
TEST_F(AXNativeWidgetMacTest, HelpAttribute) {
  Label* label = new Label(base::SysNSStringToUTF16(kTestStringValue));
  label->SetSize(GetWidgetBounds().size());
  EXPECT_NSEQ(@"", A11yElementAtMidpoint().accessibilityHelp);
  label->SetTooltipText(base::SysNSStringToUTF16(kTestPlaceholderText));
  widget()->GetContentsView()->AddChildView(label);
  EXPECT_NSEQ(kTestPlaceholderText, A11yElementAtMidpoint().accessibilityHelp);
}

// Test view properties that should report the native NSWindow, and test
// specific properties on that NSWindow.
TEST_F(AXNativeWidgetMacTest, NativeWindowProperties) {
  FlexibleRoleTestView* view =
      new FlexibleRoleTestView(ax::mojom::Role::kGroup);
  view->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(view);
  id<NSAccessibility> ax_view = A11yElementAtMidpoint();

  // Make sure it's |view| in the hit test by checking its accessibility role.
  EXPECT_EQ(NSAccessibilityGroupRole, ax_view.accessibilityRole);

  NSWindow* window = widget()->GetNativeWindow().GetNativeNSWindow();
  EXPECT_NSEQ(window, ax_view.accessibilityWindow);
  EXPECT_NSEQ(window, ax_view.accessibilityTopLevelUIElement);
  EXPECT_NSEQ(
      base::SysUTF8ToNSString(TestWidgetDelegate::kAccessibleWindowTitle),
      window.accessibilityTitle);
}

// Tests for accessibility attributes on a views::Textfield.
// TODO(patricialor): Test against Cocoa-provided attributes as well to ensure
// consistency between Cocoa and toolkit-views.
TEST_F(AXNativeWidgetMacTest, TextfieldGenericAttributes) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());
  id<NSAccessibility> ax_obj = A11yElementAtMidpoint();

  // NSAccessibilityEnabledAttribute.
  textfield->SetEnabled(false);
  EXPECT_EQ(NO, ax_obj.isAccessibilityEnabled);
  textfield->SetEnabled(true);
  EXPECT_EQ(YES, ax_obj.isAccessibilityEnabled);

  // NSAccessibilityFocusedAttribute.
  EXPECT_EQ(NO, ax_obj.isAccessibilityFocused);
  textfield->RequestFocus();
  EXPECT_EQ(YES, ax_obj.isAccessibilityFocused);

  // NSAccessibilityTitleAttribute.
  EXPECT_NSEQ(NSAccessibilityTextFieldRole, ax_obj.accessibilityRole);
  EXPECT_NSEQ(kTestTitle, ax_obj.accessibilityTitle);
  EXPECT_NSEQ(kTestStringValue, ax_obj.accessibilityValue);

  // NSAccessibilitySubroleAttribute and
  // NSAccessibilityRoleDescriptionAttribute.
  EXPECT_NSEQ(nil, ax_obj.accessibilitySubrole);
  NSString* role_description =
      NSAccessibilityRoleDescription(NSAccessibilityTextFieldRole, nil);
  EXPECT_NSEQ(role_description, ax_obj.accessibilityRoleDescription);

  // Test accessibility clients can see subroles as well.
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_NSEQ(NSAccessibilitySecureTextFieldSubrole,
              ax_obj.accessibilitySubrole);
  role_description = NSAccessibilityRoleDescription(
      NSAccessibilityTextFieldRole, NSAccessibilitySecureTextFieldSubrole);
  EXPECT_NSEQ(role_description, ax_obj.accessibilityRoleDescription);

  // Expect to see the action to show a context menu.
  EXPECT_TRUE(AXObjectHandlesSelector(A11yElementAtMidpoint(),
                                      @selector(accessibilityPerformShowMenu)));

  // Prevent the textfield from interfering with hit tests on the widget itself.
  widget()->GetContentsView()->RemoveChildView(textfield);

  // NSAccessibilitySizeAttribute.
  EXPECT_EQ(GetWidgetBounds().size(),
            gfx::Size(ax_obj.accessibilityFrame.size));
  // Check the attribute is updated when the Widget is resized.
  gfx::Size new_size(200, 40);
  widget()->SetSize(new_size);
  // TODO(https://crbug.com/939860): Why does this fail to update with the new
  // API but not the old one? With the new API, the frame is the same as it was
  // before the change - perhaps we need to invalidate a cache somewhere? This
  // EXPECT_NE() is actually checking that the behavior is *wrong*, so if it
  // ever starts failing, you fixed 939860 :)
  EXPECT_NE(new_size, gfx::Size(ax_obj.accessibilityFrame.size));
}

TEST_F(AXNativeWidgetMacTest, TextfieldEditableAttributes) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());
  textfield->SetPlaceholderText(base::SysNSStringToUTF16(kTestPlaceholderText));
  id<NSAccessibility> ax_node = A11yElementAtMidpoint();

  // NSAccessibilityInsertionPointLineNumberAttribute.
  EXPECT_EQ(0, ax_node.accessibilityInsertionPointLineNumber);

  // NSAccessibilityNumberOfCharactersAttribute.
  EXPECT_EQ(kTestStringValue.length,
            static_cast<NSUInteger>(ax_node.accessibilityNumberOfCharacters));

  // NSAccessibilityPlaceholderAttribute.
  EXPECT_NSEQ(kTestPlaceholderText, ax_node.accessibilityPlaceholderValue);

  // NSAccessibilitySelectedTextAttribute and
  // NSAccessibilitySelectedTextRangeAttribute.
  EXPECT_NSEQ(@"", ax_node.accessibilitySelectedText);
  // The cursor will be at the end of the textfield, so the selection range will
  // span 0 characters and be located at the index after the last character.
  EXPECT_NSEQ(NSMakeRange(kTestStringValue.length, 0),
              ax_node.accessibilitySelectedTextRange);

  // Select some text in the middle of the textfield.
  const gfx::Range forward_range(2, 6);
  const NSRange ns_range = forward_range.ToNSRange();
  textfield->SetSelectedRange(forward_range);
  EXPECT_NSEQ([kTestStringValue substringWithRange:ns_range],
              ax_node.accessibilitySelectedText);
  EXPECT_EQ(textfield->GetSelectedText(),
            base::SysNSStringToUTF16(ax_node.accessibilitySelectedText));
  EXPECT_EQ(forward_range, gfx::Range(ax_node.accessibilitySelectedTextRange));

  const gfx::Range reversed_range(6, 2);
  textfield->SetSelectedRange(reversed_range);
  // NSRange has no direction, so these are unchanged from the forward range.
  EXPECT_NSEQ([kTestStringValue substringWithRange:ns_range],
              ax_node.accessibilitySelectedText);
  EXPECT_EQ(textfield->GetSelectedText(),
            base::SysNSStringToUTF16(ax_node.accessibilitySelectedText));
  EXPECT_EQ(forward_range, gfx::Range(ax_node.accessibilitySelectedTextRange));

  // NSAccessibilityVisibleCharacterRangeAttribute.
  EXPECT_EQ(gfx::Range(0, kTestStringValue.length),
            gfx::Range(ax_node.accessibilityVisibleCharacterRange));

  // accessibilityLineForIndex:
  EXPECT_EQ(0, [ax_node accessibilityLineForIndex:3]);

  // accessibilityStringForRange:
  EXPECT_NSEQ(@"string",
              [ax_node accessibilityStringForRange:NSMakeRange(5, 6)]);

  // Test an RTL string.
  textfield->SetText(base::SysNSStringToUTF16(kTestRTLStringValue));
  textfield->SetSelectedRange(forward_range);
  EXPECT_EQ(textfield->GetSelectedText(),
            base::SysNSStringToUTF16(ax_node.accessibilitySelectedText));
  textfield->SetSelectedRange(reversed_range);
  EXPECT_EQ(textfield->GetSelectedText(),
            base::SysNSStringToUTF16(ax_node.accessibilitySelectedText));
}

// Test writing accessibility attributes via an accessibility client for normal
// Views.
TEST_F(AXNativeWidgetMacTest, ViewWritableAttributes) {
  FlexibleRoleTestView* view =
      new FlexibleRoleTestView(ax::mojom::Role::kGroup);
  view->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(view);

  // Make sure the accessibility object tested is the correct one.
  id<NSAccessibility> ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);
  EXPECT_NSEQ(NSAccessibilityGroupRole, ax_node.accessibilityRole);

  // Make sure |view| is focusable, then focus/unfocus it.
  view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_FALSE(view->HasFocus());
  EXPECT_FALSE(ax_node.accessibilityFocused);
  EXPECT_TRUE(
      AXObjectHandlesSelector(ax_node, @selector(setAccessibilityFocused:)));
  ax_node.accessibilityFocused = YES;
  EXPECT_TRUE(ax_node.accessibilityFocused);
  EXPECT_TRUE(view->HasFocus());
}

// Test writing accessibility attributes via an accessibility client for
// editable controls (in this case, views::Textfields).
TEST_F(AXNativeWidgetMacTest, TextfieldWritableAttributes) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());

  // Get the Textfield accessibility object.
  id<NSAccessibility> ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  // Make sure it's the correct accessibility object.
  EXPECT_NSEQ(kTestStringValue, ax_node.accessibilityValue);

  // Write a new NSAccessibilityValueAttribute.
  EXPECT_TRUE(
      AXObjectHandlesSelector(ax_node, @selector(setAccessibilityValue:)));
  ax_node.accessibilityValue = kTestPlaceholderText;
  EXPECT_NSEQ(kTestPlaceholderText, ax_node.accessibilityValue);
  EXPECT_EQ(base::SysNSStringToUTF16(kTestPlaceholderText),
            textfield->GetText());

  // Test a read-only textfield.
  textfield->SetReadOnly(true);
  EXPECT_FALSE([ax_node
      isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)]);
  ax_node.accessibilityValue = kTestStringValue;
  EXPECT_NSEQ(kTestPlaceholderText, ax_node.accessibilityValue);
  EXPECT_EQ(base::SysNSStringToUTF16(kTestPlaceholderText),
            textfield->GetText());
  textfield->SetReadOnly(false);

  // Change the selection text when there is no selected text.
  textfield->SetSelectedRange(gfx::Range(0, 0));
  EXPECT_TRUE(AXObjectHandlesSelector(
      ax_node, @selector(setAccessibilitySelectedText:)));

  NSString* new_string =
      [kTestStringValue stringByAppendingString:kTestPlaceholderText];
  ax_node.accessibilitySelectedText = kTestStringValue;
  EXPECT_NSEQ(new_string, ax_node.accessibilityValue);
  EXPECT_EQ(base::SysNSStringToUTF16(new_string), textfield->GetText());

  // Replace entire selection.
  gfx::Range test_range(0, [new_string length]);
  textfield->SetSelectedRange(test_range);
  ax_node.accessibilitySelectedText = kTestStringValue;
  EXPECT_NSEQ(kTestStringValue, ax_node.accessibilityValue);
  EXPECT_EQ(base::SysNSStringToUTF16(kTestStringValue), textfield->GetText());
  // Make sure the cursor is at the end of the Textfield.
  EXPECT_EQ(gfx::Range([kTestStringValue length]),
            textfield->GetSelectedRange());

  // Replace a middle section only (with a backwards selection range).
  base::string16 front = base::ASCIIToUTF16("Front ");
  base::string16 middle = base::ASCIIToUTF16("middle");
  base::string16 back = base::ASCIIToUTF16(" back");
  base::string16 replacement = base::ASCIIToUTF16("replaced");
  textfield->SetText(front + middle + back);
  test_range = gfx::Range(front.length() + middle.length(), front.length());
  new_string = base::SysUTF16ToNSString(front + replacement + back);
  textfield->SetSelectedRange(test_range);
  ax_node.accessibilitySelectedText = base::SysUTF16ToNSString(replacement);
  EXPECT_NSEQ(new_string, ax_node.accessibilityValue);
  EXPECT_EQ(base::SysNSStringToUTF16(new_string), textfield->GetText());
  // Make sure the cursor is at the end of the replacement.
  EXPECT_EQ(gfx::Range(front.length() + replacement.length()),
            textfield->GetSelectedRange());

  // Check it's not possible to change the selection range when read-only. Note
  // that this behavior is inconsistent with Cocoa - selections can be set via
  // a11y in selectable NSTextfields (unless they are password fields).
  // https://crbug.com/692362
  textfield->SetReadOnly(true);
  EXPECT_FALSE([ax_node isAccessibilitySelectorAllowed:@selector
                        (setAccessibilitySelectedTextRange:)]);
  textfield->SetReadOnly(false);
  EXPECT_TRUE([ax_node isAccessibilitySelectorAllowed:@selector
                       (setAccessibilitySelectedTextRange:)]);

  // Check whether it's possible to change text in a selection when read-only.
  textfield->SetReadOnly(true);
  EXPECT_FALSE([ax_node isAccessibilitySelectorAllowed:@selector
                        (setAccessibilitySelectedTextRange:)]);
  textfield->SetReadOnly(false);
  EXPECT_TRUE([ax_node isAccessibilitySelectorAllowed:@selector
                       (setAccessibilitySelectedTextRange:)]);

  // Change the selection to a valid range within the text.
  ax_node.accessibilitySelectedTextRange = NSMakeRange(2, 5);
  EXPECT_EQ(gfx::Range(2, 7), textfield->GetSelectedRange());
  // If the length is longer than the value length, default to the max possible.
  ax_node.accessibilitySelectedTextRange = NSMakeRange(0, 1000);
  EXPECT_EQ(gfx::Range(0, textfield->GetText().length()),
            textfield->GetSelectedRange());
  // Check just moving the cursor works, too.
  ax_node.accessibilitySelectedTextRange = NSMakeRange(5, 0);
  EXPECT_EQ(gfx::Range(5, 5), textfield->GetSelectedRange());
}

// Test parameterized text attributes.
TEST_F(AXNativeWidgetMacTest, TextParameterizedAttributes) {
  AddChildTextfield(GetWidgetBounds().size());
  id<NSAccessibility> ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  NSInteger line = [ax_node accessibilityLineForIndex:5];
  EXPECT_EQ(0, line);

  EXPECT_NSEQ(NSMakeRange(0, kTestStringLength),
              [ax_node accessibilityRangeForLine:line]);

  // The substring "est st" of kTestStringValue.
  NSRange test_range = NSMakeRange(1, 6);
  EXPECT_NSEQ(@"est st", [ax_node accessibilityStringForRange:test_range]);
  EXPECT_NSEQ(
      @"est st",
      [[ax_node accessibilityAttributedStringForRange:test_range] string]);

  // Not implemented yet. Update these tests when they are.
  EXPECT_NSEQ(NSMakeRange(0, 0),
              [ax_node accessibilityRangeForPosition:NSZeroPoint]);
  EXPECT_NSEQ(NSMakeRange(0, 0), [ax_node accessibilityRangeForIndex:4]);
  EXPECT_NSEQ(NSZeroRect, [ax_node accessibilityFrameForRange:test_range]);
  EXPECT_NSEQ(nil, [ax_node accessibilityRTFForRange:test_range]);
  EXPECT_NSEQ(NSMakeRange(0, kTestStringLength),
              [ax_node accessibilityStyleRangeForIndex:4]);
}

// Test performing a 'click' on Views with clickable roles work.
TEST_F(AXNativeWidgetMacTest, PressAction) {
  FlexibleRoleTestView* view =
      new FlexibleRoleTestView(ax::mojom::Role::kButton);
  widget()->GetContentsView()->AddChildView(view);
  view->SetSize(GetWidgetBounds().size());

  id<NSAccessibility> ax_node = A11yElementAtMidpoint();
  EXPECT_NSEQ(NSAccessibilityButtonRole, ax_node.accessibilityRole);

  [ax_node accessibilityPerformPress];
  EXPECT_TRUE(view->mouse_was_pressed());
}

// Test text-specific attributes that should not be supported for protected
// textfields.
TEST_F(AXNativeWidgetMacTest, ProtectedTextfields) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  // Get the Textfield accessibility object.
  id<NSAccessibility> ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  // Create a native Cocoa NSSecureTextField to compare against.
  base::scoped_nsobject<NSSecureTextField> cocoa_secure_textfield(
      [[NSSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)]);

  const SEL expected_supported_selectors[] = {
    @selector(accessibilityValue),
    @selector(accessibilityPlaceholderValue),
    @selector(accessibilityNumberOfCharacters),
    @selector(accessibilitySelectedText),
    @selector(accessibilitySelectedTextRange),
    @selector(accessibilityVisibleCharacterRange),
    @selector(accessibilityInsertionPointLineNumber)
  };

  for (auto* sel : expected_supported_selectors) {
    EXPECT_TRUE(AXObjectHandlesSelector(ax_node, sel));
    EXPECT_TRUE(AXObjectHandlesSelector(cocoa_secure_textfield.get(), sel));
  }

  // TODO(https://crbug.com/939965): This should assert the same behavior of
  // Views textfields and NSSecureTextField, but right now it can't.
  EXPECT_TRUE(
      AXObjectHandlesSelector(ax_node, @selector(setAccessibilityValue:)));
  EXPECT_NSEQ(NSAccessibilityTextFieldRole, ax_node.accessibilityRole);

  // Explicit checks done without comparing to NSTextField.
  EXPECT_TRUE(
      AXObjectHandlesSelector(ax_node, @selector(setAccessibilityValue:)));
  EXPECT_NSEQ(NSAccessibilityTextFieldRole, ax_node.accessibilityRole);

  NSString* kShownValue =
      @"•"
      @"••••••••••••••••";
  // Sanity check.
  EXPECT_EQ(static_cast<NSUInteger>(kTestStringLength), [kShownValue length]);
  EXPECT_NSEQ(kShownValue, ax_node.accessibilityValue);

  // Cursor currently at the end of input.
  EXPECT_NSEQ(@"", ax_node.accessibilitySelectedText);
  EXPECT_NSEQ(NSMakeRange(kTestStringLength, 0),
              ax_node.accessibilitySelectedTextRange);

  EXPECT_EQ(kTestStringLength, ax_node.accessibilityNumberOfCharacters);
  EXPECT_NSEQ(NSMakeRange(0, kTestStringLength),
              ax_node.accessibilityVisibleCharacterRange);
  EXPECT_EQ(0, ax_node.accessibilityInsertionPointLineNumber);

  // Test replacing text.
  textfield->SetText(base::ASCIIToUTF16("123"));
  EXPECT_NSEQ(@"•••", ax_node.accessibilityValue);
  EXPECT_EQ(3, ax_node.accessibilityNumberOfCharacters);

  textfield->SetSelectedRange(gfx::Range(2, 3));  // Selects "3".
  ax_node.accessibilitySelectedText = @"ab";
  EXPECT_EQ(base::ASCIIToUTF16("12ab"), textfield->GetText());
  EXPECT_NSEQ(@"••••", ax_node.accessibilityValue);
  EXPECT_EQ(4, ax_node.accessibilityNumberOfCharacters);
}

// Test text-specific attributes of Labels.
TEST_F(AXNativeWidgetMacTest, Label) {
  Label* label = new Label;
  label->SetText(base::SysNSStringToUTF16(kTestStringValue));
  label->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(label);

  // Get the Label's accessibility object.
  id<NSAccessibility> ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  EXPECT_NSEQ(NSAccessibilityStaticTextRole, ax_node.accessibilityRole);
  EXPECT_NSEQ(kTestStringValue, ax_node.accessibilityValue);

  // Title and label for StaticTextRole should always be empty.
  EXPECT_NSEQ(@"", ax_node.accessibilityTitle);
  EXPECT_NSEQ(@"", ax_node.accessibilityLabel);

  // No selection by default. TODO(tapted): Test selection when views::Label
  // uses RenderTextHarfBuzz on Mac. See http://crbug.com/454835.
  // For now, this tests that the codepaths are valid for views::Label.
  EXPECT_NSEQ(@"", ax_node.accessibilitySelectedText);
  EXPECT_NSEQ(NSMakeRange(0, 0), ax_node.accessibilitySelectedTextRange);

  EXPECT_EQ(kTestStringLength, ax_node.accessibilityNumberOfCharacters);
  EXPECT_NSEQ(NSMakeRange(0, kTestStringLength),
              ax_node.accessibilityVisibleCharacterRange);
  EXPECT_EQ(0, ax_node.accessibilityInsertionPointLineNumber);

  // Test parameterized attributes for Static Text.
  NSInteger line = [ax_node accessibilityLineForIndex:5];
  EXPECT_EQ(0, line);
  EXPECT_NSEQ(NSMakeRange(0, kTestStringLength),
              [ax_node accessibilityRangeForLine:line]);
  NSRange test_range = NSMakeRange(1, 6);
  EXPECT_NSEQ(@"est st", [ax_node accessibilityStringForRange:test_range]);
  EXPECT_NSEQ(
      @"est st",
      [[ax_node accessibilityAttributedStringForRange:test_range] string]);

  // TODO(tapted): Add a test for multiline Labels (currently not supported).
}

// Labels used as title bars should be exposed as normal static text on Mac.
TEST_F(AXNativeWidgetMacTest, LabelUsedAsTitleBar) {
  Label* label = new Label(base::SysNSStringToUTF16(kTestStringValue),
                           style::CONTEXT_DIALOG_TITLE, style::STYLE_PRIMARY);
  label->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(label);

  // Get the Label's accessibility object.
  id<NSAccessibility> ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  EXPECT_NSEQ(NSAccessibilityStaticTextRole, ax_node.accessibilityRole);
  EXPECT_NSEQ(kTestStringValue, ax_node.accessibilityValue);
}

class TestComboboxModel : public ui::ComboboxModel {
 public:
  TestComboboxModel() = default;

  // ui::ComboboxModel:
  int GetItemCount() const override { return 2; }
  base::string16 GetItemAt(int index) override {
    return index == 0 ? base::SysNSStringToUTF16(kTestStringValue)
                      : base::ASCIIToUTF16("Second Item");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestComboboxModel);
};

// Test a11y attributes of Comboboxes.
TEST_F(AXNativeWidgetMacTest, Combobox) {
  Combobox* combobox = new Combobox(std::make_unique<TestComboboxModel>());
  combobox->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(combobox);

  id<NSAccessibility> ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  EXPECT_NSEQ(NSAccessibilityPopUpButtonRole, ax_node.accessibilityRole);

  // The initial value should be the first item in the menu.
  EXPECT_NSEQ(kTestStringValue, ax_node.accessibilityValue);
  combobox->SetSelectedIndex(1);
  EXPECT_NSEQ(@"Second Item", ax_node.accessibilityValue);

  // Expect to see both a press action and a show menu action. This matches
  // Cocoa behavior.
  EXPECT_TRUE(
      AXObjectHandlesSelector(ax_node, @selector(accessibilityPerformPress)));
  EXPECT_TRUE(AXObjectHandlesSelector(ax_node,
                                      @selector(accessibilityPerformShowMenu)));
}

}  // namespace views
