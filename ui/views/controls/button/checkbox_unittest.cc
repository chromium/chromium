// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/checkbox.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/views_test_base.h"

namespace views {

class CheckboxTest : public ViewsTestBase {
 public:
  CheckboxTest() = default;
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

    checkbox_ = new Checkbox(base::string16());
    widget_->SetContentsView(checkbox_);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  Checkbox* checkbox() { return checkbox_; }

 private:
  std::unique_ptr<Widget> widget_;
  Checkbox* checkbox_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CheckboxTest);
};

TEST_F(CheckboxTest, AccessibilityTest) {
  const base::string16 label_text = base::ASCIIToUTF16("Some label");
  StyledLabel label(label_text, nullptr);
  checkbox()->SetAssociatedLabel(&label);

  ui::AXNodeData ax_data;
  checkbox()->GetAccessibleNodeData(&ax_data);

  EXPECT_EQ(ax_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            label_text);
  EXPECT_EQ(ax_data.role, ax::mojom::Role::kCheckBox);
}

}  // namespace views
