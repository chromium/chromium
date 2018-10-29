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

class AXNativeWidgetMacTest : public test::WidgetTest {
 public:
  AXNativeWidgetMacTest() {}

  void SetUp() override {
    test::WidgetTest::SetUp();
    widget_delegate_.InitWidget(CreateParams(Widget::InitParams::TYPE_WINDOW));
    widget()->Show();
  }

  void TearDown() override {
    widget()->CloseNow();
    test::WidgetTest::TearDown();
  }

  id A11yElementAtMidpoint() {
    // Accessibility hit tests come in Cocoa screen coordinates.
    NSPoint midpoint_in_screen_ = gfx::ScreenPointToNSPoint(
        widget()->GetWindowBoundsInScreen().CenterPoint());
    return [widget()->GetNativeWindow().GetNativeNSWindow()
        accessibilityHitTest:midpoint_in_screen_];
  }

  id AttributeValueAtMidpoint(NSString* attribute) {
    return [A11yElementAtMidpoint() accessibilityAttributeValue:attribute];
  }

  Textfield* AddChildTextfield(const gfx::Size& size) {
    Textfield* textfield = new Textfield;
    textfield->SetText(base::SysNSStringToUTF16(kTestStringValue));
    textfield->SetAccessibleName(base::SysNSStringToUTF16(kTestTitle));
    textfield->SetSize(size);
    widget()->GetContentsView()->AddChildView(textfield);
    return textfield;
  }

  // Shorthand helpers to get a11y properties from A11yElementAtMidpoint().
  NSString* AXRoleString() {
    return AttributeValueAtMidpoint(NSAccessibilityRoleAttribute);
  }
  id AXParent() {
    return AttributeValueAtMidpoint(NSAccessibilityParentAttribute);
  }
  id AXValue() {
    return AttributeValueAtMidpoint(NSAccessibilityValueAttribute);
  }
  NSString* AXTitle() {
    return AttributeValueAtMidpoint(NSAccessibilityTitleAttribute);
  }
  NSString* AXDescription() {
    return AttributeValueAtMidpoint(NSAccessibilityDescriptionAttribute);
  }
  NSString* AXSelectedText() {
    return AttributeValueAtMidpoint(NSAccessibilitySelectedTextAttribute);
  }
  NSValue* AXSelectedTextRange() {
    return AttributeValueAtMidpoint(NSAccessibilitySelectedTextRangeAttribute);
  }
  NSNumber* AXNumberOfCharacters() {
    return AttributeValueAtMidpoint(NSAccessibilityNumberOfCharactersAttribute);
  }
  NSValue* AXVisibleCharacterRange() {
    return AttributeValueAtMidpoint(
        NSAccessibilityVisibleCharacterRangeAttribute);
  }
  NSNumber* AXInsertionPointLineNumber() {
    return AttributeValueAtMidpoint(
        NSAccessibilityInsertionPointLineNumberAttribute);
  }
  NSNumber* AXLineForIndex(id parameter) {
    return [A11yElementAtMidpoint()
        accessibilityAttributeValue:
            NSAccessibilityLineForIndexParameterizedAttribute
                       forParameter:parameter];
  }
  NSValue* AXRangeForLine(id parameter) {
    return [A11yElementAtMidpoint()
        accessibilityAttributeValue:
            NSAccessibilityRangeForLineParameterizedAttribute
                       forParameter:parameter];
  }
  NSString* AXStringForRange(id parameter) {
    return [A11yElementAtMidpoint()
        accessibilityAttributeValue:
            NSAccessibilityStringForRangeParameterizedAttribute
                       forParameter:parameter];
  }
  NSAttributedString* AXAttributedStringForRange(id parameter) {
    return [A11yElementAtMidpoint()
        accessibilityAttributeValue:
            NSAccessibilityAttributedStringForRangeParameterizedAttribute
                       forParameter:parameter];
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

  NSString* kAttribute = NSAccessibilityValueAttribute;
  NSString* kParamAttribute =
      NSAccessibilityStringForRangeParameterizedAttribute;
  NSString* kAction = NSAccessibilityShowMenuAction;

  EXPECT_TRUE(
      [[ax_node accessibilityAttributeNames] containsObject:kAttribute]);
  EXPECT_NSEQ(kTestStringValue,
              [ax_node accessibilityAttributeValue:kAttribute]);
  EXPECT_TRUE([ax_node accessibilityIsAttributeSettable:kAttribute]);
  EXPECT_TRUE([[ax_node accessibilityActionNames] containsObject:kAction]);
  EXPECT_FALSE([ax_node accessibilityIsIgnored]);

  // Not implemented, but be sure to update this test if it ever is.
  EXPECT_FALSE(
      [ax_node respondsToSelector:@selector(accessibilityActionDescription:)]);

  EXPECT_TRUE([[ax_node accessibilityParameterizedAttributeNames]
      containsObject:kParamAttribute]);
  NSValue* range = [NSValue valueWithRange:NSMakeRange(0, kTestStringLength)];
  EXPECT_NSEQ(
      kTestStringValue,
      [ax_node accessibilityAttributeValue:kParamAttribute forParameter:range]);

  // The following is also "not implemented", but the informal protocol category
  // provides a default implementation.
  EXPECT_EQ(NSNotFound, static_cast<NSInteger>(
                            [ax_node accessibilityIndexOfChild:ax_node]));

  // The only usually available array attribute is AXChildren, so go up a level
  // to the Widget to test that a bit. The default implementation just gets the
  // attribute normally and returns its size (if it's an array).
  NSString* kChildren = NSAccessibilityChildrenAttribute;
  base::scoped_nsobject<NSObject> ax_parent(
      [ax_node accessibilityAttributeValue:NSAccessibilityParentAttribute],
      base::scoped_policy::RETAIN);
  EXPECT_EQ(1u, [ax_parent accessibilityArrayAttributeCount:kChildren]);
  EXPECT_EQ(
      ax_node.get(),
      [ax_parent accessibilityArrayAttributeValues:kChildren index:0
                                          maxCount:1][0]);

  // If it is not an array, the default implementation throws an exception, so
  // it's impossible to test these methods further on |ax_node|, apart from the
  // following, before deleting the view.
  EXPECT_EQ(0u, [ax_node accessibilityArrayAttributeCount:kChildren]);

  delete view;

  EXPECT_TRUE(
      [ax_node respondsToSelector:@selector(accessibilityAttributeNames)]);
  EXPECT_EQ(@[], [ax_node accessibilityAttributeNames]);
  EXPECT_EQ(nil, [ax_node accessibilityAttributeValue:kAttribute]);
  EXPECT_FALSE([ax_node accessibilityIsAttributeSettable:kAttribute]);
  [ax_node accessibilitySetValue:kTestStringValue forAttribute:kAttribute];

  EXPECT_EQ(@[], [ax_node accessibilityActionNames]);
  [ax_node accessibilityPerformAction:kAction];

  EXPECT_TRUE([ax_node accessibilityIsIgnored]);
  EXPECT_EQ(nil, [ax_node accessibilityHitTest:NSZeroPoint]);
  EXPECT_EQ(nil, [ax_node accessibilityFocusedUIElement]);

  EXPECT_EQ(@[], [ax_node accessibilityParameterizedAttributeNames]);
  EXPECT_NSEQ(nil, [ax_node accessibilityAttributeValue:kParamAttribute
                                           forParameter:range]);

  // Test the attributes with default implementations provided.
  EXPECT_EQ(NSNotFound, static_cast<NSInteger>(
                            [ax_node accessibilityIndexOfChild:ax_node]));

  // The Widget is currently still around, but the child should be gone.
  EXPECT_EQ(0u, [ax_parent accessibilityArrayAttributeCount:kChildren]);
}

// Check that potentially keyboard-focusable elements are always leaf nodes.
TEST_F(AXNativeWidgetMacTest, FocusableElementsAreLeafNodes) {
  // LabelButtons will have a label inside the button. The label should be
  // ignored because the button is potentially keyboard focusable.
  TestLabelButton* button = new TestLabelButton();
  button->SetSize(widget()->GetContentsView()->size());
  widget()->GetContentsView()->AddChildView(button);
  EXPECT_NSEQ(NSAccessibilityButtonRole, AXRoleString());
  EXPECT_EQ(
      0u,
      [[button->GetNativeViewAccessible()
          accessibilityAttributeValue:NSAccessibilityChildrenAttribute] count]);

  // The exception is if the child is explicitly marked accessibility focusable.
  button->label()->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  EXPECT_EQ(
      1u,
      [[button->GetNativeViewAccessible()
          accessibilityAttributeValue:NSAccessibilityChildrenAttribute] count]);
  EXPECT_EQ(
      button->label()->GetNativeViewAccessible(),
      [button->GetNativeViewAccessible()
          accessibilityAttributeValue:NSAccessibilityChildrenAttribute][0]);

  // If the child is disabled, it should still be traversable.
  button->label()->SetEnabled(false);
  EXPECT_EQ(
      1u,
      [[button->GetNativeViewAccessible()
          accessibilityAttributeValue:NSAccessibilityChildrenAttribute] count]);
  EXPECT_EQ(
      button->label()->GetNativeViewAccessible(),
      [button->GetNativeViewAccessible()
          accessibilityAttributeValue:NSAccessibilityChildrenAttribute][0]);
}

// Test for NSAccessibilityChildrenAttribute, and ensure it excludes ignored
// children from the accessibility tree.
TEST_F(AXNativeWidgetMacTest, ChildrenAttribute) {
  // Check childless views don't have accessibility children.
  EXPECT_EQ(0u,
            [AttributeValueAtMidpoint(NSAccessibilityChildrenAttribute) count]);

  const size_t kNumChildren = 3;
  for (size_t i = 0; i < kNumChildren; ++i) {
    // Make sure the labels won't interfere with the hit test.
    AddChildTextfield(gfx::Size());
  }

  EXPECT_EQ(kNumChildren,
            [AttributeValueAtMidpoint(NSAccessibilityChildrenAttribute) count]);

  // Check ignored children don't show up in the accessibility tree.
  widget()->GetContentsView()->AddChildView(
      new FlexibleRoleTestView(ax::mojom::Role::kIgnored));
  EXPECT_EQ(kNumChildren,
            [AttributeValueAtMidpoint(NSAccessibilityChildrenAttribute) count]);
}

// Test for NSAccessibilityParentAttribute, including for a Widget with no
// parent.
TEST_F(AXNativeWidgetMacTest, ParentAttribute) {
  Textfield* child = AddChildTextfield(widget()->GetContentsView()->size());

  // Views with Widget parents will have a NSAccessibilityGroupRole parent.
  // See https://crbug.com/875843 for more information.
  EXPECT_NSEQ(
      NSAccessibilityGroupRole,
      [AXParent() accessibilityAttributeValue:NSAccessibilityRoleAttribute]);

  // Views with non-Widget parents will have the role of the parent view.
  widget()->GetContentsView()->RemoveChildView(child);
  FlexibleRoleTestView* parent =
      new FlexibleRoleTestView(ax::mojom::Role::kGroup);
  parent->FitBoundsToNewChild(child);
  widget()->GetContentsView()->AddChildView(parent);
  EXPECT_NSEQ(
      NSAccessibilityGroupRole,
      [AXParent() accessibilityAttributeValue:NSAccessibilityRoleAttribute]);

  // Test an ignored role parent is skipped in favor of the grandparent.
  parent->set_role(ax::mojom::Role::kIgnored);
  EXPECT_NSEQ(
      NSAccessibilityGroupRole,
      [AXParent() accessibilityAttributeValue:NSAccessibilityRoleAttribute]);
}

// Test for NSAccessibilityPositionAttribute, including on Widget movement
// updates.
TEST_F(AXNativeWidgetMacTest, PositionAttribute) {
  NSValue* widget_origin =
      [NSValue valueWithPoint:gfx::ScreenPointToNSPoint(
                                  GetWidgetBounds().bottom_left())];
  EXPECT_NSEQ(widget_origin,
              AttributeValueAtMidpoint(NSAccessibilityPositionAttribute));

  // Check the attribute is updated when the Widget is moved.
  gfx::Rect new_bounds(60, 80, 100, 100);
  widget()->SetBounds(new_bounds);
  widget_origin = [NSValue
      valueWithPoint:gfx::ScreenPointToNSPoint(new_bounds.bottom_left())];
  EXPECT_NSEQ(widget_origin,
              AttributeValueAtMidpoint(NSAccessibilityPositionAttribute));
}

// Test for NSAccessibilityHelpAttribute.
TEST_F(AXNativeWidgetMacTest, HelpAttribute) {
  Label* label = new Label(base::SysNSStringToUTF16(kTestStringValue));
  label->SetSize(GetWidgetBounds().size());
  EXPECT_NSEQ(@"", AttributeValueAtMidpoint(NSAccessibilityHelpAttribute));
  label->SetTooltipText(base::SysNSStringToUTF16(kTestPlaceholderText));
  widget()->GetContentsView()->AddChildView(label);
  EXPECT_NSEQ(kTestPlaceholderText,
              AttributeValueAtMidpoint(NSAccessibilityHelpAttribute));
}

// Test view properties that should report the native NSWindow, and test
// specific properties on that NSWindow.
TEST_F(AXNativeWidgetMacTest, NativeWindowProperties) {
  FlexibleRoleTestView* view =
      new FlexibleRoleTestView(ax::mojom::Role::kGroup);
  view->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(view);
  // Make sure it's |view| in the hit test by checking its accessibility role.
  EXPECT_EQ(NSAccessibilityGroupRole, AXRoleString());

  NSWindow* window = widget()->GetNativeWindow().GetNativeNSWindow();
  EXPECT_NSEQ(window, AttributeValueAtMidpoint(NSAccessibilityWindowAttribute));
  EXPECT_NSEQ(window, AttributeValueAtMidpoint(
                          NSAccessibilityTopLevelUIElementAttribute));
  EXPECT_NSEQ(
      base::SysUTF8ToNSString(TestWidgetDelegate::kAccessibleWindowTitle),
      [window accessibilityAttributeValue:NSAccessibilityTitleAttribute]);
}

// Tests for accessibility attributes on a views::Textfield.
// TODO(patricialor): Test against Cocoa-provided attributes as well to ensure
// consistency between Cocoa and toolkit-views.
TEST_F(AXNativeWidgetMacTest, TextfieldGenericAttributes) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());

  // NSAccessibilityEnabledAttribute.
  textfield->SetEnabled(false);
  EXPECT_EQ(NO, [AttributeValueAtMidpoint(NSAccessibilityEnabledAttribute)
                    boolValue]);
  textfield->SetEnabled(true);
  EXPECT_EQ(YES, [AttributeValueAtMidpoint(NSAccessibilityEnabledAttribute)
                     boolValue]);

  // NSAccessibilityFocusedAttribute.
  EXPECT_EQ(NO, [AttributeValueAtMidpoint(NSAccessibilityFocusedAttribute)
                    boolValue]);
  textfield->RequestFocus();
  EXPECT_EQ(YES, [AttributeValueAtMidpoint(NSAccessibilityFocusedAttribute)
                     boolValue]);

  // NSAccessibilityTitleAttribute.
  EXPECT_NSEQ(NSAccessibilityTextFieldRole, AXRoleString());
  EXPECT_NSEQ(kTestTitle, AXTitle());
  EXPECT_NSEQ(kTestStringValue, AXValue());

  // NSAccessibilitySubroleAttribute and
  // NSAccessibilityRoleDescriptionAttribute.
  EXPECT_NSEQ(nil, AttributeValueAtMidpoint(NSAccessibilitySubroleAttribute));
  NSString* role_description =
      NSAccessibilityRoleDescription(NSAccessibilityTextFieldRole, nil);
  EXPECT_NSEQ(role_description, AttributeValueAtMidpoint(
                                    NSAccessibilityRoleDescriptionAttribute));

  // Test accessibility clients can see subroles as well.
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_NSEQ(NSAccessibilitySecureTextFieldSubrole,
              AttributeValueAtMidpoint(NSAccessibilitySubroleAttribute));
  role_description = NSAccessibilityRoleDescription(
      NSAccessibilityTextFieldRole, NSAccessibilitySecureTextFieldSubrole);
  EXPECT_NSEQ(role_description, AttributeValueAtMidpoint(
                                    NSAccessibilityRoleDescriptionAttribute));

  // Expect to see the action to show a context menu.
  EXPECT_NSEQ(@[ NSAccessibilityShowMenuAction ],
              [A11yElementAtMidpoint() accessibilityActionNames]);

  // Prevent the textfield from interfering with hit tests on the widget itself.
  widget()->GetContentsView()->RemoveChildView(textfield);

  // NSAccessibilitySizeAttribute.
  EXPECT_EQ(GetWidgetBounds().size(),
            gfx::Size([AttributeValueAtMidpoint(NSAccessibilitySizeAttribute)
                sizeValue]));
  // Check the attribute is updated when the Widget is resized.
  gfx::Size new_size(200, 40);
  widget()->SetSize(new_size);
  EXPECT_EQ(new_size, gfx::Size([AttributeValueAtMidpoint(
                          NSAccessibilitySizeAttribute) sizeValue]));
}

TEST_F(AXNativeWidgetMacTest, TextfieldEditableAttributes) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());
  textfield->set_placeholder_text(
      base::SysNSStringToUTF16(kTestPlaceholderText));

  // NSAccessibilityInsertionPointLineNumberAttribute.
  EXPECT_EQ(0, [AttributeValueAtMidpoint(
                   NSAccessibilityInsertionPointLineNumberAttribute) intValue]);

  // NSAccessibilityNumberOfCharactersAttribute.
  EXPECT_EQ(kTestStringValue.length,
            [AXNumberOfCharacters() unsignedIntegerValue]);

  // NSAccessibilityPlaceholderAttribute.
  EXPECT_NSEQ(
      kTestPlaceholderText,
      AttributeValueAtMidpoint(NSAccessibilityPlaceholderValueAttribute));

  // NSAccessibilitySelectedTextAttribute and
  // NSAccessibilitySelectedTextRangeAttribute.
  EXPECT_NSEQ(@"", AXSelectedText());
  // The cursor will be at the end of the textfield, so the selection range will
  // span 0 characters and be located at the index after the last character.
  EXPECT_EQ(gfx::Range(kTestStringValue.length, kTestStringValue.length),
            gfx::Range([AXSelectedTextRange() rangeValue]));

  // Select some text in the middle of the textfield.
  const gfx::Range forward_range(2, 6);
  const NSRange ns_range = forward_range.ToNSRange();
  textfield->SelectRange(forward_range);
  EXPECT_NSEQ([kTestStringValue substringWithRange:ns_range], AXSelectedText());
  EXPECT_EQ(textfield->GetSelectedText(),
            base::SysNSStringToUTF16(AXSelectedText()));
  EXPECT_EQ(forward_range, gfx::Range([AXSelectedTextRange() rangeValue]));

  const gfx::Range reversed_range(6, 2);
  textfield->SelectRange(reversed_range);
  // NSRange has no direction, so these are unchanged from the forward range.
  EXPECT_NSEQ([kTestStringValue substringWithRange:ns_range], AXSelectedText());
  EXPECT_EQ(textfield->GetSelectedText(),
            base::SysNSStringToUTF16(AXSelectedText()));
  EXPECT_EQ(forward_range, gfx::Range([AXSelectedTextRange() rangeValue]));

  // NSAccessibilityVisibleCharacterRangeAttribute.
  EXPECT_EQ(gfx::Range(0, kTestStringValue.length),
            gfx::Range([AXVisibleCharacterRange() rangeValue]));

  // Test an RTL string.
  textfield->SetText(base::SysNSStringToUTF16(kTestRTLStringValue));
  textfield->SelectRange(forward_range);
  EXPECT_EQ(textfield->GetSelectedText(),
            base::SysNSStringToUTF16(AXSelectedText()));
  textfield->SelectRange(reversed_range);
  EXPECT_EQ(textfield->GetSelectedText(),
            base::SysNSStringToUTF16(AXSelectedText()));
}

// Test writing accessibility attributes via an accessibility client for normal
// Views.
TEST_F(AXNativeWidgetMacTest, ViewWritableAttributes) {
  FlexibleRoleTestView* view =
      new FlexibleRoleTestView(ax::mojom::Role::kGroup);
  view->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(view);

  // Make sure the accessibility object tested is the correct one.
  id ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);
  EXPECT_NSEQ(NSAccessibilityGroupRole, AXRoleString());

  // Make sure |view| is focusable, then focus/unfocus it.
  view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_FALSE(view->HasFocus());
  EXPECT_FALSE(
      [AttributeValueAtMidpoint(NSAccessibilityFocusedAttribute) boolValue]);
  EXPECT_TRUE([ax_node
      accessibilityIsAttributeSettable:NSAccessibilityFocusedAttribute]);
  [ax_node accessibilitySetValue:@YES
                    forAttribute:NSAccessibilityFocusedAttribute];
  EXPECT_TRUE(
      [AttributeValueAtMidpoint(NSAccessibilityFocusedAttribute) boolValue]);
  EXPECT_TRUE(view->HasFocus());
}

// Test writing accessibility attributes via an accessibility client for
// editable controls (in this case, views::Textfields).
TEST_F(AXNativeWidgetMacTest, TextfieldWritableAttributes) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());

  // Get the Textfield accessibility object.
  id ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  // Make sure it's the correct accessibility object.
  EXPECT_NSEQ(kTestStringValue, AXValue());

  // Write a new NSAccessibilityValueAttribute.
  EXPECT_TRUE(
      [ax_node accessibilityIsAttributeSettable:NSAccessibilityValueAttribute]);
  [ax_node accessibilitySetValue:kTestPlaceholderText
                    forAttribute:NSAccessibilityValueAttribute];
  EXPECT_NSEQ(kTestPlaceholderText, AXValue());
  EXPECT_EQ(base::SysNSStringToUTF16(kTestPlaceholderText), textfield->text());

  // Test a read-only textfield.
  textfield->SetReadOnly(true);
  EXPECT_FALSE(
      [ax_node accessibilityIsAttributeSettable:NSAccessibilityValueAttribute]);
  [ax_node accessibilitySetValue:kTestStringValue
                    forAttribute:NSAccessibilityValueAttribute];
  EXPECT_NSEQ(kTestPlaceholderText, AXValue());
  EXPECT_EQ(base::SysNSStringToUTF16(kTestPlaceholderText), textfield->text());
  textfield->SetReadOnly(false);

  // Change the selection text when there is no selected text.
  textfield->SelectRange(gfx::Range(0, 0));
  EXPECT_TRUE([ax_node
      accessibilityIsAttributeSettable:NSAccessibilitySelectedTextAttribute]);

  NSString* new_string =
      [kTestStringValue stringByAppendingString:kTestPlaceholderText];
  [ax_node accessibilitySetValue:kTestStringValue
                    forAttribute:NSAccessibilitySelectedTextAttribute];
  EXPECT_NSEQ(new_string, AXValue());
  EXPECT_EQ(base::SysNSStringToUTF16(new_string), textfield->text());

  // Replace entire selection.
  gfx::Range test_range(0, [new_string length]);
  textfield->SelectRange(test_range);
  [ax_node accessibilitySetValue:kTestStringValue
                    forAttribute:NSAccessibilitySelectedTextAttribute];
  EXPECT_NSEQ(kTestStringValue, AXValue());
  EXPECT_EQ(base::SysNSStringToUTF16(kTestStringValue), textfield->text());
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
  textfield->SelectRange(test_range);
  [ax_node accessibilitySetValue:base::SysUTF16ToNSString(replacement)
                    forAttribute:NSAccessibilitySelectedTextAttribute];
  EXPECT_NSEQ(new_string, AXValue());
  EXPECT_EQ(base::SysNSStringToUTF16(new_string), textfield->text());
  // Make sure the cursor is at the end of the replacement.
  EXPECT_EQ(gfx::Range(front.length() + replacement.length()),
            textfield->GetSelectedRange());

  // Check it's not possible to change the selection range when read-only. Note
  // that this behavior is inconsistent with Cocoa - selections can be set via
  // a11y in selectable NSTextfields (unless they are password fields).
  // https://crbug.com/692362
  textfield->SetReadOnly(true);
  EXPECT_FALSE([ax_node accessibilityIsAttributeSettable:
                            NSAccessibilitySelectedTextRangeAttribute]);
  textfield->SetReadOnly(false);
  EXPECT_TRUE([ax_node accessibilityIsAttributeSettable:
                           NSAccessibilitySelectedTextRangeAttribute]);

  // Check whether it's possible to change text in a selection when read-only.
  textfield->SetReadOnly(true);
  EXPECT_FALSE([ax_node
      accessibilityIsAttributeSettable:NSAccessibilitySelectedTextAttribute]);
  textfield->SetReadOnly(false);
  EXPECT_TRUE([ax_node
      accessibilityIsAttributeSettable:NSAccessibilitySelectedTextAttribute]);
  // Change the selection to a valid range within the text.
  [ax_node accessibilitySetValue:[NSValue valueWithRange:NSMakeRange(2, 5)]
                    forAttribute:NSAccessibilitySelectedTextRangeAttribute];
  EXPECT_EQ(gfx::Range(2, 7), textfield->GetSelectedRange());
  // If the length is longer than the value length, default to the max possible.
  [ax_node accessibilitySetValue:[NSValue valueWithRange:NSMakeRange(0, 1000)]
                    forAttribute:NSAccessibilitySelectedTextRangeAttribute];
  EXPECT_EQ(gfx::Range(0, textfield->text().length()),
            textfield->GetSelectedRange());
  // Check just moving the cursor works, too.
  [ax_node accessibilitySetValue:[NSValue valueWithRange:NSMakeRange(5, 0)]
                    forAttribute:NSAccessibilitySelectedTextRangeAttribute];
  EXPECT_EQ(gfx::Range(5, 5), textfield->GetSelectedRange());
}

// Test parameterized text attributes.
TEST_F(AXNativeWidgetMacTest, TextParameterizedAttributes) {
  AddChildTextfield(GetWidgetBounds().size());
  id ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  NSArray* attributes = [ax_node accessibilityParameterizedAttributeNames];
  ASSERT_TRUE(attributes);

  // Ensure the method names match.
  for (NSString* attribute in attributes) {
    SEL sel = NSSelectorFromString([attribute stringByAppendingString:@":"]);
    EXPECT_TRUE([ax_node respondsToSelector:sel]);
  }

  NSNumber* line = AXLineForIndex(@5);
  EXPECT_TRUE(line);
  EXPECT_EQ(0, [line intValue]);

  EXPECT_NSEQ([NSValue valueWithRange:NSMakeRange(0, kTestStringLength)],
              AXRangeForLine(line));

  // The substring "est st" of kTestStringValue.
  NSValue* test_range = [NSValue valueWithRange:NSMakeRange(1, 6)];
  EXPECT_NSEQ(@"est st", AXStringForRange(test_range));
  EXPECT_NSEQ(@"est st", [AXAttributedStringForRange(test_range) string]);

  // Not implemented yet. Update these tests when they are.
  EXPECT_NSEQ(nil,
              [ax_node accessibilityAttributeValue:
                           NSAccessibilityRangeForPositionParameterizedAttribute
                                      forParameter:@4]);
  EXPECT_NSEQ(nil,
              [ax_node accessibilityAttributeValue:
                           NSAccessibilityRangeForIndexParameterizedAttribute
                                      forParameter:@4]);
  EXPECT_NSEQ(nil,
              [ax_node accessibilityAttributeValue:
                           NSAccessibilityBoundsForRangeParameterizedAttribute
                                      forParameter:test_range]);
  EXPECT_NSEQ(nil, [ax_node accessibilityAttributeValue:
                                NSAccessibilityRTFForRangeParameterizedAttribute
                                           forParameter:test_range]);
  EXPECT_NSEQ(
      nil, [ax_node accessibilityAttributeValue:
                        NSAccessibilityStyleRangeForIndexParameterizedAttribute
                                   forParameter:@4]);

  // Non-text shouldn't have any parameterized attributes.
  id ax_parent =
      [ax_node accessibilityAttributeValue:NSAccessibilityParentAttribute];
  EXPECT_TRUE(ax_parent);
  EXPECT_FALSE([ax_parent accessibilityParameterizedAttributeNames]);
}

// Test performing a 'click' on Views with clickable roles work.
TEST_F(AXNativeWidgetMacTest, PressAction) {
  FlexibleRoleTestView* view =
      new FlexibleRoleTestView(ax::mojom::Role::kButton);
  widget()->GetContentsView()->AddChildView(view);
  view->SetSize(GetWidgetBounds().size());

  id ax_node = A11yElementAtMidpoint();
  EXPECT_NSEQ(NSAccessibilityButtonRole, AXRoleString());

  EXPECT_TRUE([[ax_node accessibilityActionNames]
      containsObject:NSAccessibilityPressAction]);
  [ax_node accessibilityPerformAction:NSAccessibilityPressAction];
  EXPECT_TRUE(view->mouse_was_pressed());
}

// Test text-specific attributes that should not be supported for protected
// textfields.
TEST_F(AXNativeWidgetMacTest, ProtectedTextfields) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  // Get the Textfield accessibility object.
  NSPoint midpoint = gfx::ScreenPointToNSPoint(GetWidgetBounds().CenterPoint());
  id ax_node = [widget()->GetNativeWindow().GetNativeNSWindow()
      accessibilityHitTest:midpoint];
  EXPECT_TRUE(ax_node);

  // Create a native Cocoa NSSecureTextField to compare against.
  base::scoped_nsobject<NSSecureTextField> cocoa_secure_textfield(
      [[NSSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)]);

  NSArray* views_attributes = [ax_node accessibilityAttributeNames];
  NSArray* cocoa_attributes =
      [cocoa_secure_textfield accessibilityAttributeNames];

  NSArray* const expected_supported_attributes = @[
    NSAccessibilityValueAttribute, NSAccessibilityPlaceholderValueAttribute
  ];
  NSArray* const expected_unsupported_attributes = @[
    NSAccessibilitySelectedTextAttribute,
    NSAccessibilitySelectedTextRangeAttribute,
    NSAccessibilityNumberOfCharactersAttribute,
    NSAccessibilityVisibleCharacterRangeAttribute,
    NSAccessibilityInsertionPointLineNumberAttribute
  ];

  for (NSString* attribute_name in expected_supported_attributes) {
    SCOPED_TRACE(base::SysNSStringToUTF8([NSString
        stringWithFormat:@"Missing attribute is: %@", attribute_name]));
    EXPECT_TRUE([views_attributes containsObject:attribute_name]);
  }
  if (base::mac::IsAtLeastOS10_10()) {
    // Check Cocoa's attribute values for PlaceHolder and Value here separately
    // - these are using the new NSAccessibility protocol.
    EXPECT_TRUE([cocoa_secure_textfield isAccessibilitySelectorAllowed:@selector
                                        (accessibilityPlaceholderValue)]);
    EXPECT_TRUE([cocoa_secure_textfield
        isAccessibilitySelectorAllowed:@selector(accessibilityValue)]);
  }

  for (NSString* attribute_name in expected_unsupported_attributes) {
    SCOPED_TRACE(base::SysNSStringToUTF8([NSString
        stringWithFormat:@"Missing attribute is: %@", attribute_name]));
    EXPECT_FALSE([views_attributes containsObject:attribute_name]);
    EXPECT_FALSE([cocoa_attributes containsObject:attribute_name]);
  }

  // Explicit checks done without comparing to NSTextField.
  EXPECT_TRUE(
      [ax_node accessibilityIsAttributeSettable:NSAccessibilityValueAttribute]);
  EXPECT_NSEQ(NSAccessibilityTextFieldRole, AXRoleString());

  NSString* kShownValue =
      @"•"
      @"••••••••••••••••";
  // Sanity check.
  EXPECT_EQ(kTestStringLength, static_cast<int>([kShownValue length]));
  EXPECT_NSEQ(kShownValue, AXValue());

  // Cursor currently at the end of input.
  EXPECT_NSEQ(@"", AXSelectedText());
  EXPECT_NSEQ([NSValue valueWithRange:NSMakeRange(kTestStringLength, 0)],
              AXSelectedTextRange());

  EXPECT_EQ(kTestStringLength, [AXNumberOfCharacters() intValue]);
  EXPECT_NSEQ(([NSValue valueWithRange:{0, kTestStringLength}]),
              AXVisibleCharacterRange());
  EXPECT_EQ(0, [AXInsertionPointLineNumber() intValue]);

  // Test replacing text.
  textfield->SetText(base::ASCIIToUTF16("123"));
  EXPECT_NSEQ(@"•••", AXValue());
  EXPECT_EQ(3, [AXNumberOfCharacters() intValue]);

  textfield->SelectRange(gfx::Range(2, 3));  // Selects "3".
  [ax_node accessibilitySetValue:@"ab"
                    forAttribute:NSAccessibilitySelectedTextAttribute];
  EXPECT_EQ(base::ASCIIToUTF16("12ab"), textfield->text());
  EXPECT_NSEQ(@"••••", AXValue());
  EXPECT_EQ(4, [AXNumberOfCharacters() intValue]);
}

// Test text-specific attributes of Labels.
TEST_F(AXNativeWidgetMacTest, Label) {
  Label* label = new Label;
  label->SetText(base::SysNSStringToUTF16(kTestStringValue));
  label->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(label);

  // Get the Label's accessibility object.
  id ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  EXPECT_NSEQ(NSAccessibilityStaticTextRole, AXRoleString());
  EXPECT_NSEQ(kTestStringValue, AXValue());

  // Title and description for StaticTextRole should always be empty.
  EXPECT_NSEQ(@"", AXTitle());

  // The description is "The purpose of the element, not including the role.".
  // BrowserAccessibility returns an empty string instead of nil. Either should
  // be OK.
  EXPECT_EQ(nil, AXDescription());

  // No selection by default. TODO(tapted): Test selection when views::Label
  // uses RenderTextHarfBuzz on Mac. See http://crbug.com/454835.
  // For now, this tests that the codepaths are valid for views::Label.
  EXPECT_NSEQ(@"", AXSelectedText());
  EXPECT_NSEQ([NSValue valueWithRange:NSMakeRange(0, 0)],
              AXSelectedTextRange());

  EXPECT_EQ(kTestStringLength, [AXNumberOfCharacters() intValue]);
  EXPECT_NSEQ(([NSValue valueWithRange:{0, kTestStringLength}]),
              AXVisibleCharacterRange());
  EXPECT_EQ(0, [AXInsertionPointLineNumber() intValue]);

  // Test parameterized attributes for Static Text.
  NSNumber* line = AXLineForIndex(@5);
  EXPECT_TRUE(line);
  EXPECT_EQ(0, [line intValue]);
  EXPECT_NSEQ([NSValue valueWithRange:NSMakeRange(0, kTestStringLength)],
              AXRangeForLine(line));
  NSValue* test_range = [NSValue valueWithRange:NSMakeRange(1, 6)];
  EXPECT_NSEQ(@"est st", AXStringForRange(test_range));
  EXPECT_NSEQ(@"est st", [AXAttributedStringForRange(test_range) string]);

  // TODO(tapted): Add a test for multiline Labels (currently not supported).
}

// Labels used as title bars should be exposed as normal static text on Mac.
TEST_F(AXNativeWidgetMacTest, LabelUsedAsTitleBar) {
  Label* label = new Label(base::SysNSStringToUTF16(kTestStringValue),
                           style::CONTEXT_DIALOG_TITLE, style::STYLE_PRIMARY);
  label->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(label);

  // Get the Label's accessibility object.
  id ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  EXPECT_NSEQ(NSAccessibilityStaticTextRole, AXRoleString());
  EXPECT_NSEQ(kTestStringValue, AXValue());
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

  id ax_node = A11yElementAtMidpoint();
  EXPECT_TRUE(ax_node);

  EXPECT_NSEQ(NSAccessibilityPopUpButtonRole, AXRoleString());

  // The initial value should be the first item in the menu.
  EXPECT_NSEQ(kTestStringValue, AXValue());
  combobox->SetSelectedIndex(1);
  EXPECT_NSEQ(@"Second Item", AXValue());

  // Expect to see both a press action and a show menu action. This matches
  // Cocoa behavior.
  EXPECT_NSEQ((@[ NSAccessibilityPressAction, NSAccessibilityShowMenuAction ]),
              [ax_node accessibilityActionNames]);
}

}  // namespace views
