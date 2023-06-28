// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/checkbox.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

class TestCheckbox : public Checkbox {
 public:
  explicit TestCheckbox(const std::u16string& label = std::u16string(),
                        int button_context = style::CONTEXT_BUTTON)
      : Checkbox(label, Button::PressedCallback(), button_context) {}

  TestCheckbox(const TestCheckbox&) = delete;
  TestCheckbox& operator=(const TestCheckbox&) = delete;

  using Checkbox::GetIconCheckColor;
  using Checkbox::GetIconImageColor;
  using Checkbox::GetIconState;
};

class CheckboxTest : public ViewsTestBase {
 public:
  CheckboxTest() = default;

  CheckboxTest(const CheckboxTest&) = delete;
  CheckboxTest& operator=(const CheckboxTest&) = delete;

  ~CheckboxTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the Checkbox can query the hover state
    // correctly.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();

    checkbox_ = widget_->SetContentsView(std::make_unique<TestCheckbox>());
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  TestCheckbox* checkbox() { return checkbox_; }

 private:
  std::unique_ptr<Widget> widget_;
  raw_ptr<TestCheckbox, DanglingUntriaged> checkbox_ = nullptr;
};

class CheckboxTestRefreshOnly : public CheckboxTest {
 public:
  CheckboxTestRefreshOnly() {
    scoped_feature_list_.InitWithFeatures({features::kChromeRefresh2023}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CheckboxTest, AccessibilityTest) {
  const std::u16string label_text = u"Some label";
  StyledLabel label;
  label.SetText(label_text);
  checkbox()->SetAccessibleName(&label);

  // Use `ViewAccessibility::GetAccessibleNodeData` so that we can get the
  // label's accessible id to compare with the checkbox's labelled-by id.
  ui::AXNodeData label_data;
  label.GetViewAccessibility().GetAccessibleNodeData(&label_data);

  ui::AXNodeData ax_data;
  checkbox()->GetAccessibleNodeData(&ax_data);

  EXPECT_EQ(ax_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            label_text);
  EXPECT_EQ(checkbox()->GetAccessibleName(), label_text);
  EXPECT_EQ(ax_data.GetNameFrom(), ax::mojom::NameFrom::kRelatedElement);
  EXPECT_EQ(ax_data.GetIntListAttribute(
                ax::mojom::IntListAttribute::kLabelledbyIds)[0],
            label_data.id);
  EXPECT_EQ(ax_data.role, ax::mojom::Role::kCheckBox);
}

TEST_F(CheckboxTest, TestCorrectCheckColor) {
  // Enabled
  checkbox()->SetChecked(true);
  int icon_state = checkbox()->GetIconState(Button::ButtonState::STATE_NORMAL);
  SkColor actual = checkbox()->GetIconCheckColor(icon_state);
  SkColor expected =
      checkbox()->GetColorProvider()->GetColor(ui::kColorCheckboxCheck);
  EXPECT_EQ(actual, expected);

  // Disabled
  icon_state = checkbox()->GetIconState(Button::ButtonState::STATE_DISABLED);
  actual = checkbox()->GetIconCheckColor(icon_state);
  expected =
      checkbox()->GetColorProvider()->GetColor(ui::kColorCheckboxCheckDisabled);
  EXPECT_EQ(actual, expected);
}

TEST_F(CheckboxTestRefreshOnly, TestCorrectContainerColor) {
  // Enabled
  checkbox()->SetChecked(true);
  int icon_state = checkbox()->GetIconState(Button::ButtonState::STATE_NORMAL);
  SkColor actual = checkbox()->GetIconImageColor(icon_state);
  SkColor expected =
      checkbox()->GetColorProvider()->GetColor(ui::kColorCheckboxContainer);
  EXPECT_EQ(actual, expected);

  // Disabled
  icon_state = checkbox()->GetIconState(Button::ButtonState::STATE_DISABLED);
  actual = checkbox()->GetIconImageColor(icon_state);
  expected = checkbox()->GetColorProvider()->GetColor(
      ui::kColorCheckboxContainerDisabled);
  EXPECT_EQ(actual, expected);
}

}  // namespace views
