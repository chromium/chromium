// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/atomic_view_ax_tree_manager.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {
class AtomicViewAXTreeManagerTest : public ViewsTestBase {
 public:
  AtomicViewAXTreeManagerTest() = default;
  ~AtomicViewAXTreeManagerTest() override = default;
  void SetUp() override {
    ViewsTestBase::SetUp();

    scoped_feature_list_.InitAndEnableFeature(features::kUiaProvider);

    widget_ = std::make_unique<Widget>();

    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(50, 50, 200, 200);
    widget_->Init(std::move(params));

    textfield_ = new Textfield();
    textfield_->SetBounds(10, 20, 30, 40);
    widget_->GetContentsView()->AddChildView(textfield_.get());

    delegate_ = static_cast<ViewAXPlatformNodeDelegate*>(
        &textfield_->GetViewAccessibility());

    // TODO(accessibility): This is not obvious, but the AtomicViewAXTreeManager
    // gets initialized from this GetData() call. Might want to improve that.
    delegate_->GetData();
    CHECK(delegate_->GetAtomicViewAXTreeManagerForTesting());
  }

  void TearDown() override {
    delegate_ = nullptr;
    textfield_ = nullptr;
    if (!widget_->IsClosed()) {
      widget_->Close();
    }
    ViewsTestBase::TearDown();
  }

  void CompareNodeData(ui::AXNodeData expected, ui::AXNodeData actual) {
    EXPECT_EQ(expected.id, actual.id);
    EXPECT_EQ(expected.role, actual.role);
    EXPECT_EQ(expected.state, actual.state);
    EXPECT_EQ(expected.actions, actual.actions);
    EXPECT_EQ(expected.string_attributes, actual.string_attributes);
    EXPECT_EQ(expected.float_attributes, actual.float_attributes);
    EXPECT_EQ(expected.bool_attributes, actual.bool_attributes);
    EXPECT_EQ(expected.intlist_attributes, actual.intlist_attributes);
    EXPECT_EQ(expected.stringlist_attributes, actual.stringlist_attributes);
    EXPECT_EQ(expected.relative_bounds, actual.relative_bounds);
  }

  ui::AXNodeData delegate_data() { return delegate_->data(); }

 protected:
  raw_ptr<Textfield> textfield_ = nullptr;  // Owned by views hierarchy.
  std::unique_ptr<Widget> widget_;
  raw_ptr<ViewAXPlatformNodeDelegate> delegate_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AtomicViewAXTreeManagerTest, UpdateNode) {
  ui::AXNodeData previous =
      delegate_->GetAtomicViewAXTreeManagerForTesting()->GetRoot()->data();
  textfield_->SetText(u"text");
  ui::AXNodeData actual =
      delegate_->GetAtomicViewAXTreeManagerForTesting()->GetRoot()->data();
  CompareNodeData(delegate_data(), actual);
  EXPECT_EQ(actual.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            u"text");
  EXPECT_EQ(previous.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            u"");
}

TEST_F(AtomicViewAXTreeManagerTest, GetRootAsAXNode) {
  CompareNodeData(
      delegate_data(),
      delegate_->GetAtomicViewAXTreeManagerForTesting()->GetRoot()->data());
}

TEST_F(AtomicViewAXTreeManagerTest, GetNodeFromTree) {
  CompareNodeData(
      delegate_data(),
      delegate_->GetAtomicViewAXTreeManagerForTesting()
          ->GetNodeFromTree(
              delegate_->GetAtomicViewAXTreeManagerForTesting()->GetTreeID(),
              delegate_->GetAtomicViewAXTreeManagerForTesting()
                  ->GetRoot()
                  ->id())
          ->data());
}

}  // namespace views
