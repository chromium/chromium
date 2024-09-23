// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"

namespace views::test {

namespace {

static const char* kDialogName = "DialogName";
static const char* kDifferentNodeName = "DifferentNodeName";
static const char* kDescription = "SomeDescription";

class AccessibleView : public View {
 public:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = role_;
    node_data->SetNameChecked(name_);
    if (description_) {
      if (description_->empty())
        node_data->SetDescriptionExplicitlyEmpty();
      else
        node_data->SetDescription(*description_);
    }
  }

  ViewAXPlatformNodeDelegate* GetPlatformNodeDelegate() {
    return static_cast<ViewAXPlatformNodeDelegate*>(&GetViewAccessibility());
  }

  void SetDescription(const std::optional<std::string>& descritpion) {
    description_ = descritpion;
  }
  const std::optional<std::string>& GetDescription() const {
    return description_;
  }

  void SetNameChecked(const std::string& name) { name_ = name; }
  const std::string& GetName() const { return name_; }

  void SetRole(ax::mojom::Role role) { role_ = role; }
  ax::mojom::Role GetRole() const { return role_; }

 private:
  std::optional<std::string> description_ = kDescription;
  std::string name_ = kDialogName;
  ax::mojom::Role role_ = ax::mojom::Role::kDialog;
};

}  // namespace

class ViewAXPlatformNodeDelegateMacTest : public ViewsTestBase {
 public:
  ViewAXPlatformNodeDelegateMacTest() = default;
  ~ViewAXPlatformNodeDelegateMacTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->widget_delegate()->SetTitle(base::ASCIIToUTF16(kDialogName));
    widget_->SetContentsView(std::make_unique<AccessibleView>());
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  AccessibleView* view() {
    return static_cast<AccessibleView*>(widget_->GetContentsView());
  }

 private:
  std::unique_ptr<Widget> widget_;
};

TEST_F(ViewAXPlatformNodeDelegateMacTest,
       GetNameReturnsNodeNameWhenNameAndTitleAreEqual) {
  EXPECT_NE(view()->GetPlatformNodeDelegate()->GetName(),
            *view()->GetDescription());
}

TEST_F(ViewAXPlatformNodeDelegateMacTest,
       GetNameReturnsNodeNameWhenNameAndTitleAreDifferent) {
  EXPECT_NE(view()->GetPlatformNodeDelegate()->GetName(),
            *view()->GetDescription());

  view()->SetNameChecked(kDifferentNodeName);

  EXPECT_EQ(view()->GetPlatformNodeDelegate()->GetName(), kDifferentNodeName);
}

TEST_F(ViewAXPlatformNodeDelegateMacTest, GetNameReturnsNodeNameForNonDialog) {
  EXPECT_NE(view()->GetPlatformNodeDelegate()->GetName(),
            *view()->GetDescription());

  view()->SetRole(ax::mojom::Role::kDesktop);

  EXPECT_EQ(view()->GetPlatformNodeDelegate()->GetName(), kDialogName);
}

TEST_F(ViewAXPlatformNodeDelegateMacTest,
       GetNameReturnsNodeNameWhenDescriptionIsNotSet) {
  EXPECT_NE(view()->GetPlatformNodeDelegate()->GetName(),
            *view()->GetDescription());

  view()->SetDescription(std::nullopt);

  EXPECT_EQ(view()->GetPlatformNodeDelegate()->GetName(), kDialogName);
}

TEST_F(ViewAXPlatformNodeDelegateMacTest,
       GetNameReturnsNodeNameWhenDescriptionIsAnEmptyString) {
  EXPECT_NE(view()->GetPlatformNodeDelegate()->GetName(),
            *view()->GetDescription());

  view()->SetDescription("");

  EXPECT_EQ(view()->GetPlatformNodeDelegate()->GetName(), kDialogName);
}

}  // namespace views::test
