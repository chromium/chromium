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
  AccessibleView() {
    GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
    GetViewAccessibility().SetName(kDialogName);
    GetViewAccessibility().SetDescription(kDescription);
  }

  ViewAXPlatformNodeDelegate* GetPlatformNodeDelegate() {
    return static_cast<ViewAXPlatformNodeDelegate*>(&GetViewAccessibility());
  }

  void SetDescription(const std::optional<std::string>& description) {
    if (description.has_value()) {
      if (description.value().empty()) {
        GetViewAccessibility().SetDescription(
            std::string(),
            ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
      } else {
        GetViewAccessibility().SetDescription(*description);
      }
    } else {
      GetViewAccessibility().ClearDescriptionAndDescriptionFrom();
    }
  }
  std::string GetDescription() const {
    return base::UTF16ToUTF8(GetViewAccessibility().GetCachedDescription());
  }

  std::string GetName() const {
    return base::UTF16ToUTF8(GetViewAccessibility().GetCachedName());
  }

  ax::mojom::Role GetRole() const {
    return GetViewAccessibility().GetCachedRole();
  }
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
            view()->GetDescription());
}

TEST_F(ViewAXPlatformNodeDelegateMacTest,
       GetNameReturnsNodeNameWhenNameAndTitleAreDifferent) {
  EXPECT_NE(view()->GetPlatformNodeDelegate()->GetName(),
            view()->GetDescription());

  view()->GetViewAccessibility().SetName(kDifferentNodeName);

  EXPECT_EQ(view()->GetPlatformNodeDelegate()->GetName(), kDifferentNodeName);
}

TEST_F(ViewAXPlatformNodeDelegateMacTest, GetNameReturnsNodeNameForNonDialog) {
  EXPECT_NE(view()->GetPlatformNodeDelegate()->GetName(),
            view()->GetDescription());

  view()->GetViewAccessibility().SetRole(ax::mojom::Role::kWindow);

  EXPECT_EQ(view()->GetPlatformNodeDelegate()->GetName(), kDialogName);
}

TEST_F(ViewAXPlatformNodeDelegateMacTest,
       GetNameReturnsNodeNameWhenDescriptionIsNotSet) {
  EXPECT_NE(view()->GetPlatformNodeDelegate()->GetName(),
            view()->GetDescription());

  view()->SetDescription(std::nullopt);

  EXPECT_EQ(view()->GetPlatformNodeDelegate()->GetName(), kDialogName);
}

TEST_F(ViewAXPlatformNodeDelegateMacTest,
       GetNameReturnsNodeNameWhenDescriptionIsAnEmptyString) {
  EXPECT_NE(view()->GetPlatformNodeDelegate()->GetName(),
            view()->GetDescription());

  view()->SetDescription("");

  EXPECT_EQ(view()->GetPlatformNodeDelegate()->GetName(), kDialogName);
}

}  // namespace views::test
