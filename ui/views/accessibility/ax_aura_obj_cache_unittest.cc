// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_aura_obj_cache.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/widget_test.h"

namespace views {
namespace test {
namespace {

// This class can be used as a deleter for std::unique_ptr<Widget>
// to call function Widget::CloseNow automatically.
struct WidgetCloser {
  inline void operator()(Widget* widget) const { widget->CloseNow(); }
};

using WidgetAutoclosePtr = std::unique_ptr<Widget, WidgetCloser>;

bool HasNodeWithName(ui::AXNode* node, const std::string& name) {
  if (node->GetStringAttribute(ax::mojom::StringAttribute::kName) == name)
    return true;
  for (auto* child : node->children()) {
    if (HasNodeWithName(child, name))
      return true;
  }
  return false;
}

bool HasNodeWithName(const ui::AXTree& tree, const std::string& name) {
  return HasNodeWithName(tree.root(), name);
}

class AXAuraObjCacheTest : public WidgetTest {
 public:
  AXAuraObjCacheTest() = default;
  ~AXAuraObjCacheTest() override = default;
};

TEST_F(AXAuraObjCacheTest, TestViewRemoval) {
  AXAuraObjCache cache;
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  View* parent = new View();
  widget->GetRootView()->AddChildView(parent);
  View* child = new View();
  parent->AddChildView(child);

  AXAuraObjWrapper* ax_widget = cache.GetOrCreate(widget.get());
  ASSERT_NE(nullptr, ax_widget);
  AXAuraObjWrapper* ax_parent = cache.GetOrCreate(parent);
  ASSERT_NE(nullptr, ax_parent);
  AXAuraObjWrapper* ax_child = cache.GetOrCreate(child);
  ASSERT_NE(nullptr, ax_child);

  // Everything should have an ID, indicating it's in the cache.
  ASSERT_GT(cache.GetID(widget.get()), 0);
  ASSERT_GT(cache.GetID(parent), 0);
  ASSERT_GT(cache.GetID(child), 0);

  // Removing the parent view should remove both the parent and child
  // from the cache, but leave the widget.
  widget->GetRootView()->RemoveChildView(parent);
  ASSERT_GT(cache.GetID(widget.get()), 0);
  ASSERT_EQ(ui::AXNode::kInvalidAXID, cache.GetID(parent));
  ASSERT_EQ(ui::AXNode::kInvalidAXID, cache.GetID(child));

  // Explicitly delete |parent| to prevent a memory leak, since calling
  // RemoveChildView() doesn't delete it.
  delete parent;
}

TEST_F(AXAuraObjCacheTest, ValidTree) {
  // Create a parent window.
  auto parent_widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_widget->Init(std::move(params));
  parent_widget->GetNativeWindow()->SetTitle(
      base::ASCIIToUTF16("ParentWindow"));
  parent_widget->Show();

  // Create a child window.
  Widget* child_widget = new Widget();  // Owned by parent_widget.
  params = CreateParams(Widget::InitParams::TYPE_BUBBLE);
  params.parent = parent_widget->GetNativeWindow();
  params.child = true;
  params.bounds = gfx::Rect(100, 100, 200, 200);
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  child_widget->Init(std::move(params));
  child_widget->GetNativeWindow()->SetTitle(base::ASCIIToUTF16("ChildWindow"));
  child_widget->Show();

  // Create a child view.
  auto* button = new LabelButton(nullptr, base::ASCIIToUTF16("ChildButton"));
  button->SetSize(gfx::Size(20, 20));
  child_widget->GetContentsView()->AddChildView(button);

  // Use AXAuraObjCache to serialize the node tree.
  AXAuraObjCache cache;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  AXTreeSourceViews tree_source(
      cache.GetOrCreate(parent_widget->GetNativeWindow()), tree_id, &cache);
  ui::AXTreeSerializer<AXAuraObjWrapper*, ui::AXNodeData, ui::AXTreeData>
      serializer(&tree_source);
  ui::AXTreeUpdate serialized_tree;
  serializer.SerializeChanges(tree_source.GetRoot(), &serialized_tree);

  // Verify tree is valid.
  ui::AXTreeSourceChecker<AXAuraObjWrapper*, ui::AXNodeData, ui::AXTreeData>
      checker(&tree_source);
  std::string error_string;
  EXPECT_TRUE(checker.CheckAndGetErrorString(&error_string)) << error_string;
  ui::AXTree ax_tree(serialized_tree);
  EXPECT_TRUE(HasNodeWithName(ax_tree, "ParentWindow"));
  EXPECT_TRUE(HasNodeWithName(ax_tree, "ChildWindow"));
  EXPECT_TRUE(HasNodeWithName(ax_tree, "ChildButton"));
}

}  // namespace
}  // namespace test
}  // namespace views
