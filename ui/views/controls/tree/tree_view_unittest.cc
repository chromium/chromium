// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tree/tree_view.h"

#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessibility/view_ax_platform_node_delegate.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

using ui::TreeModel;
using ui::TreeModelNode;
using ui::TreeNode;

using base::ASCIIToUTF16;

namespace views {

namespace {

std::string AccessibilityViewAsString(const AXVirtualView& view) {
  std::string result =
      view.GetData().GetStringAttribute(ax::mojom::StringAttribute::kName);
  if (!view.GetChildCount() ||
      view.GetData().HasState(ax::mojom::State::kCollapsed)) {
    // We don't descend into collapsed nodes because they are invisible.
    return result;
  }

  result += " [";
  for (const auto& child_view : view.children()) {
    result += AccessibilityViewAsString(*child_view) + " ";
  }
  result.pop_back();
  result += "]";

  return result;
}

}  // namespace

class TestNode : public TreeNode<TestNode> {
 public:
  TestNode() = default;

  TestNode(const TestNode&) = delete;
  TestNode& operator=(const TestNode&) = delete;

  ~TestNode() override = default;
};

// Creates the following structure:
// 'root'
//   'a'
//   'b'
//     'b1'
//   'c'
class TreeViewTest : public ViewsTestBase {
 public:
  TreeViewTest() : model_(std::make_unique<TestNode>()) {
    static_cast<TestNode*>(model_.GetRoot())->SetTitle(u"root");
    Add(model_.GetRoot(), 0, "a");
    Add(Add(model_.GetRoot(), 1, "b"), 0, "b1");
    Add(model_.GetRoot(), 2, "c");
  }

  TreeViewTest(const TreeViewTest&) = delete;
  TreeViewTest& operator=(const TreeViewTest&) = delete;

  // ViewsTestBase
  void SetUp() override;
  void TearDown() override;

 protected:
  using AccessibilityEventsVector = std::vector<
      std::pair<const ui::AXPlatformNodeDelegate*, const ax::mojom::Event>>;

  const AccessibilityEventsVector accessibility_events() const {
    return accessibility_events_;
  }

  void ClearAccessibilityEvents();

  TestNode* Add(TestNode* parent, size_t index, const std::string& title);

  std::string TreeViewContentsAsString();

  std::string TreeViewAccessibilityContentsAsString() const;

  // Gets the selected node from the tree view. The result can be compared with
  // GetSelectedAccessibilityViewName() to check consistency between the tree
  // view state and the accessibility data.
  std::string GetSelectedNodeTitle();

  // Finds the selected node via iterative depth first search over the internal
  // accessibility tree, examining both ignored and unignored nodes. The result
  // can be compared with GetSelectedNodeTitle() to check consistency between
  // the tree view state and the accessibility data.
  std::string GetSelectedAccessibilityViewName() const;

  // Gets the active node from the tree view. The result can be compared with
  // GetSelectedAccessibilityViewName() to check consistency between the tree
  // view state and the accessibility data.
  std::string GetActiveNodeTitle();

  // Gets the active node from the tree view's |ViewAccessibility|. The result
  // can be compared with GetSelectedNodeTitle() to check consistency between
  // the tree view internal state and the accessibility data.
  std::string GetActiveAccessibilityViewName() const;

  std::string GetEditingNodeTitle();

  AXVirtualView* GetRootAccessibilityView() const;

  ViewAXPlatformNodeDelegate* GetTreeAccessibilityView() const;

  TestNode* GetNodeByTitle(const std::string& title);

  const AXVirtualView* GetAccessibilityViewByName(
      const std::string& name) const;

  void IncrementSelection(bool next);
  void CollapseOrSelectParent();
  void ExpandOrSelectChild();
  size_t GetRowCount();
  PrefixSelector* selector() { return tree()->GetPrefixSelector(); }
  TreeView* tree() {
    return const_cast<TreeView*>(std::as_const(*this).tree());
  }
  const TreeView* tree() const {
    return static_cast<const TreeView*>(widget_->GetContentsView());
  }

  ui::TreeNodeModel<TestNode> model_;
  UniqueWidgetPtr widget_;

 private:
  std::string InternalNodeAsString(TreeView::InternalNode* node);

  TestNode* GetNodeByTitleImpl(TestNode* node, const std::u16string& title);

  // Keeps a record of all accessibility events that have been fired on the tree
  // view.
  AccessibilityEventsVector accessibility_events_;
};

void TreeViewTest::SetUp() {
  ViewsTestBase::SetUp();
  widget_ = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  widget_->Init(std::move(params));
  widget_->SetContentsView(std::make_unique<TreeView>());
  tree()->RequestFocus();

  ViewAccessibility::AccessibilityEventsCallback accessibility_events_callback =
      base::BindRepeating(
          [](std::vector<std::pair<const ui::AXPlatformNodeDelegate*,
                                   const ax::mojom::Event>>*
                 accessibility_events,
             const ui::AXPlatformNodeDelegate* delegate,
             const ax::mojom::Event event_type) {
            DCHECK(accessibility_events);
            accessibility_events->push_back({delegate, event_type});
          },
          &accessibility_events_);
  tree()->GetViewAccessibility().set_accessibility_events_callback(
      std::move(accessibility_events_callback));
}

void TreeViewTest::TearDown() {
  widget_.reset();
  ViewsTestBase::TearDown();
}

void TreeViewTest::ClearAccessibilityEvents() {
  accessibility_events_.clear();
}

TestNode* TreeViewTest::Add(TestNode* parent,
                            size_t index,
                            const std::string& title) {
  std::unique_ptr<TestNode> new_node = std::make_unique<TestNode>();
  new_node->SetTitle(ASCIIToUTF16(title));
  return model_.Add(parent, std::move(new_node), index);
}

std::string TreeViewTest::TreeViewContentsAsString() {
  return InternalNodeAsString(&tree()->root_);
}

std::string TreeViewTest::TreeViewAccessibilityContentsAsString() const {
  AXVirtualView* ax_view = GetRootAccessibilityView();
  if (!ax_view)
    return "Empty";
  return AccessibilityViewAsString(*ax_view);
}

std::string TreeViewTest::GetSelectedNodeTitle() {
  TreeModelNode* model_node = tree()->GetSelectedNode();
  return model_node ? base::UTF16ToASCII(model_node->GetTitle())
                    : std::string();
}

std::string TreeViewTest::GetSelectedAccessibilityViewName() const {
  const AXVirtualView* ax_view = GetRootAccessibilityView();

  while (ax_view) {
    if (ax_view->GetData().GetBoolAttribute(
            ax::mojom::BoolAttribute::kSelected)) {
      return ax_view->GetData().GetStringAttribute(
          ax::mojom::StringAttribute::kName);
    }

    if (ax_view->children().size()) {
      ax_view = ax_view->children()[0].get();
      continue;
    }

    const AXVirtualView* parent_view = ax_view->virtual_parent_view();
    while (parent_view) {
      size_t sibling_index_in_parent =
          parent_view->GetIndexOf(ax_view).value() + 1;
      if (sibling_index_in_parent < parent_view->children().size()) {
        ax_view = parent_view->children()[sibling_index_in_parent].get();
        break;
      }

      ax_view = parent_view;
      parent_view = parent_view->virtual_parent_view();
    }

    if (!parent_view)
      break;
  }

  return {};
}

std::string TreeViewTest::GetActiveNodeTitle() {
  TreeModelNode* model_node = tree()->GetActiveNode();
  return model_node ? base::UTF16ToASCII(model_node->GetTitle())
                    : std::string();
}

std::string TreeViewTest::GetActiveAccessibilityViewName() const {
  const AXVirtualView* ax_view =
      tree()->GetViewAccessibility().FocusedVirtualChild();
  return ax_view ? ax_view->GetData().GetStringAttribute(
                       ax::mojom::StringAttribute::kName)
                 : std::string();
}

std::string TreeViewTest::GetEditingNodeTitle() {
  TreeModelNode* model_node = tree()->GetEditingNode();
  return model_node ? base::UTF16ToASCII(model_node->GetTitle())
                    : std::string();
}

AXVirtualView* TreeViewTest::GetRootAccessibilityView() const {
  return tree()->root_.accessibility_view();
}

ViewAXPlatformNodeDelegate* TreeViewTest::GetTreeAccessibilityView() const {
#if !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY()
  return nullptr;  // ViewAXPlatformNodeDelegate is not used on this platform.
#else
  return static_cast<ViewAXPlatformNodeDelegate*>(
      &(tree()->GetViewAccessibility()));
#endif
}

TestNode* TreeViewTest::GetNodeByTitle(const std::string& title) {
  return GetNodeByTitleImpl(model_.GetRoot(), ASCIIToUTF16(title));
}

const AXVirtualView* TreeViewTest::GetAccessibilityViewByName(
    const std::string& name) const {
  const AXVirtualView* ax_view = GetRootAccessibilityView();

  while (ax_view) {
    if (ax_view->GetData().HasStringAttribute(
            ax::mojom::StringAttribute::kName)) {
      const std::string& ax_view_name = ax_view->GetData().GetStringAttribute(
          ax::mojom::StringAttribute::kName);
      if (ax_view_name == name) {
        return ax_view;
      }
    }

    if (ax_view->children().size()) {
      ax_view = ax_view->children()[0].get();
      continue;
    }

    const AXVirtualView* parent_view = ax_view->virtual_parent_view();
    while (parent_view) {
      size_t sibling_index_in_parent =
          parent_view->GetIndexOf(ax_view).value() + 1;
      if (sibling_index_in_parent < parent_view->children().size()) {
        ax_view = parent_view->children()[sibling_index_in_parent].get();
        break;
      }

      ax_view = parent_view;
      parent_view = parent_view->virtual_parent_view();
    }

    if (!parent_view)
      break;
  }

  return nullptr;
}

void TreeViewTest::IncrementSelection(bool next) {
  tree()->IncrementSelection(next ? TreeView::IncrementType::kNext
                                  : TreeView::IncrementType::kPrevious);
}

void TreeViewTest::CollapseOrSelectParent() {
  tree()->CollapseOrSelectParent();
}

void TreeViewTest::ExpandOrSelectChild() {
  tree()->ExpandOrSelectChild();
}

size_t TreeViewTest::GetRowCount() {
  return tree()->GetRowCount();
}

TestNode* TreeViewTest::GetNodeByTitleImpl(TestNode* node,
                                           const std::u16string& title) {
  if (node->GetTitle() == title)
    return node;
  for (auto& child : node->children()) {
    TestNode* matching_node = GetNodeByTitleImpl(child.get(), title);
    if (matching_node)
      return matching_node;
  }
  return nullptr;
}

std::string TreeViewTest::InternalNodeAsString(TreeView::InternalNode* node) {
  std::string result = base::UTF16ToASCII(node->model_node()->GetTitle());
  if (node->is_expanded() && !node->children().empty()) {
    result += std::accumulate(
                  node->children().cbegin() + 1, node->children().cend(),
                  " [" + InternalNodeAsString(node->children().front().get()),
                  [this](const std::string& str, const auto& child) {
                    return str + " " + InternalNodeAsString(child.get());
                  }) +
              "]";
  }
  return result;
}

// Verify properties are accessible via metadata.
TEST_F(TreeViewTest, MetadataTest) {
  tree()->SetModel(&model_);
  test::TestViewMetadata(tree());
}

TEST_F(TreeViewTest, TreeViewPaintCoverage) {
  tree()->SetModel(&model_);
  SkBitmap bitmap;
  gfx::Size size = tree()->size();
  ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, SK_ColorTRANSPARENT,
                                   false);
  widget_->GetRootView()->Paint(
      PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
}

TEST_F(TreeViewTest, InitialAccessibilityProperties) {
  tree()->SetModel(&model_);
  ui::AXNodeData data;
  tree()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTree);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kVertical));
  EXPECT_EQ(data.GetDefaultActionVerb(),
            ax::mojom::DefaultActionVerb::kActivate);
  EXPECT_EQ(data.GetRestriction(), ax::mojom::Restriction::kReadOnly);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName), "");
  EXPECT_EQ(data.GetNameFrom(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

// Verifies setting model correctly updates internal state.
TEST_F(TreeViewTest, SetModel) {
  tree()->SetModel(&model_);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(4u, GetRowCount());

  EXPECT_EQ(
      (AccessibilityEventsVector{
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kChildrenChanged),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kChildrenChanged),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kChildrenChanged),
          std::make_pair(GetRootAccessibilityView(), ax::mojom::Event::kFocus),
          std::make_pair(GetRootAccessibilityView(),
                         ax::mojom::Event::kSelection)}),
      accessibility_events());
}

// Verifies that SetSelectedNode works.
TEST_F(TreeViewTest, SetSelectedNode) {
  tree()->SetModel(&model_);
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());

  // NULL should clear the selection.
  tree()->SetSelectedNode(nullptr);
  EXPECT_EQ(std::string(), GetSelectedNodeTitle());
  EXPECT_EQ(std::string(), GetSelectedAccessibilityViewName());

  // Select 'c'.
  ClearAccessibilityEvents();
  tree()->SetSelectedNode(GetNodeByTitle("c"));
  EXPECT_EQ("c", GetSelectedNodeTitle());
  EXPECT_EQ("c", GetSelectedAccessibilityViewName());
  EXPECT_EQ(
      (AccessibilityEventsVector{std::make_pair(GetAccessibilityViewByName("c"),
                                                ax::mojom::Event::kFocus),
                                 std::make_pair(GetAccessibilityViewByName("c"),
                                                ax::mojom::Event::kSelection)}),
      accessibility_events());

  // Select 'b1', which should expand 'b'.
  ClearAccessibilityEvents();
  tree()->SetSelectedNode(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1] c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  EXPECT_EQ("b1", GetSelectedAccessibilityViewName());
  // Node "b" must have been expanded.
  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetAccessibilityViewByName("b"),
                               ax::mojom::Event::kExpandedChanged),
                std::make_pair(GetAccessibilityViewByName("b"),
                               ax::mojom::Event::kRowExpanded),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kRowCountChanged),
                std::make_pair(GetAccessibilityViewByName("b1"),
                               ax::mojom::Event::kFocus),
                std::make_pair(GetAccessibilityViewByName("b1"),
                               ax::mojom::Event::kSelection)}),
            accessibility_events());
}

// Makes sure SetRootShown doesn't blow up.
TEST_F(TreeViewTest, HideRoot) {
  tree()->SetModel(&model_);
  ClearAccessibilityEvents();
  tree()->SetRootShown(false);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());
  EXPECT_EQ(3u, GetRowCount());

  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetAccessibilityViewByName("a"),
                               ax::mojom::Event::kFocus),
                std::make_pair(GetAccessibilityViewByName("a"),
                               ax::mojom::Event::kSelection),
                std::make_pair(GetRootAccessibilityView(),
                               ax::mojom::Event::kStateChanged)}),
            accessibility_events());
}

// Expands a node and verifies the children are loaded correctly.
TEST_F(TreeViewTest, Expand) {
  tree()->SetModel(&model_);
  ClearAccessibilityEvents();
  tree()->Expand(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1] c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(5u, GetRowCount());

  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetAccessibilityViewByName("b1"),
                               ax::mojom::Event::kExpandedChanged),
                std::make_pair(GetAccessibilityViewByName("b1"),
                               ax::mojom::Event::kRowExpanded),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kRowCountChanged)}),
            accessibility_events());
}

// Collapse a node and verifies state.
TEST_F(TreeViewTest, Collapse) {
  tree()->SetModel(&model_);
  tree()->Expand(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1] c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ(5u, GetRowCount());
  tree()->SetSelectedNode(GetNodeByTitle("b1"));
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  EXPECT_EQ("b1", GetSelectedAccessibilityViewName());

  ClearAccessibilityEvents();
  tree()->Collapse(GetNodeByTitle("b"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  // Selected node should have moved to 'b'
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());
  EXPECT_EQ(4u, GetRowCount());

  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetAccessibilityViewByName("b"),
                               ax::mojom::Event::kFocus),
                std::make_pair(GetAccessibilityViewByName("b"),
                               ax::mojom::Event::kSelection),
                std::make_pair(GetAccessibilityViewByName("b"),
                               ax::mojom::Event::kExpandedChanged),
                std::make_pair(GetAccessibilityViewByName("b"),
                               ax::mojom::Event::kRowCollapsed),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kRowCountChanged)}),
            accessibility_events());
}

// Verifies that adding nodes works.
TEST_F(TreeViewTest, TreeNodesAdded) {
  tree()->SetModel(&model_);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());

  // Add a node between b and c.
  ClearAccessibilityEvents();
  Add(model_.GetRoot(), 2, "B");
  EXPECT_EQ("root [a b B c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b B c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(5u, GetRowCount());
  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kRowCountChanged)}),
            accessibility_events());

  // Add a child of b1, which hasn't been loaded and shouldn't do anything.
  ClearAccessibilityEvents();
  Add(GetNodeByTitle("b1"), 0, "b11");
  EXPECT_EQ("root [a b B c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b B c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(5u, GetRowCount());
  // Added node is not visible, hence no accessibility event needed.
  EXPECT_EQ(AccessibilityEventsVector(), accessibility_events());

  // Add a child of b, which isn't expanded yet, so it shouldn't effect
  // anything.
  ClearAccessibilityEvents();
  Add(GetNodeByTitle("b"), 1, "b2");
  EXPECT_EQ("root [a b B c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b B c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(5u, GetRowCount());
  // Added node is not visible, hence no accessibility event needed.
  EXPECT_EQ(AccessibilityEventsVector(), accessibility_events());

  // Expand b and make sure b2 is there.
  ClearAccessibilityEvents();
  tree()->Expand(GetNodeByTitle("b"));
  EXPECT_EQ("root [a b [b1 b2] B c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1 b2] B c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(7u, GetRowCount());
  // Since the added node was not visible when it was added, no extra events
  // other than the ones for expanding a node are needed.
  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetAccessibilityViewByName("b"),
                               ax::mojom::Event::kExpandedChanged),
                std::make_pair(GetAccessibilityViewByName("b"),
                               ax::mojom::Event::kRowExpanded),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kRowCountChanged)}),
            accessibility_events());
}

// Verifies that removing nodes works.
TEST_F(TreeViewTest, TreeNodesRemoved) {
  // Add c1 as a child of c and c11 as a child of c1.
  Add(Add(GetNodeByTitle("c"), 0, "c1"), 0, "c11");
  tree()->SetModel(&model_);

  int root_children_set_size = 3;
  const int root_pos_in_set = 1;
  const int root_set_size = 1;
  int a_pos_in_set = 1;
  int b_pos_in_set = 2;
  int c_pos_in_set = 3;

  ui::AXNodeData data;

  // Remove c11, which shouldn't have any effect on the tree.
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(4u, GetRowCount());
  data = GetAccessibilityViewByName("root")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel),
            1);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            root_pos_in_set);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kInvisible));

  data = GetAccessibilityViewByName("a")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_children_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            a_pos_in_set);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(data.HasAction(ax::mojom::Action::kFocus));
  EXPECT_TRUE(data.HasAction(ax::mojom::Action::kScrollToMakeVisible));
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel),
            2);

  data = GetAccessibilityViewByName("b")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_children_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            b_pos_in_set);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(data.HasAction(ax::mojom::Action::kFocus));
  EXPECT_TRUE(data.HasAction(ax::mojom::Action::kScrollToMakeVisible));
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel),
            2);

  data = GetAccessibilityViewByName("c")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_children_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            c_pos_in_set);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(data.HasAction(ax::mojom::Action::kFocus));
  EXPECT_TRUE(data.HasAction(ax::mojom::Action::kScrollToMakeVisible));
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel),
            2);

  // Expand b1, then collapse it and remove its only child, b1. This shouldn't
  // effect the tree.
  tree()->Expand(GetNodeByTitle("b"));
  data = GetAccessibilityViewByName("b1")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize), 1);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet), 1);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(data.HasAction(ax::mojom::Action::kFocus));
  EXPECT_TRUE(data.HasAction(ax::mojom::Action::kScrollToMakeVisible));
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel),
            3);

  tree()->Collapse(GetNodeByTitle("b"));
  data = GetAccessibilityViewByName("b1")->GetData();
  EXPECT_TRUE(data.HasState(ax::mojom::State::kInvisible));
  ClearAccessibilityEvents();
  model_.Remove(GetNodeByTitle("b1")->parent(), GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(4u, GetRowCount());
  EXPECT_EQ(
      (AccessibilityEventsVector{std::make_pair(
          GetTreeAccessibilityView(), ax::mojom::Event::kChildrenChanged)}),
      accessibility_events());

  // Remove 'b'.
  root_children_set_size = 2;
  c_pos_in_set = 2;
  ClearAccessibilityEvents();
  model_.Remove(GetNodeByTitle("b")->parent(), GetNodeByTitle("b"));
  EXPECT_EQ("root [a c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(3u, GetRowCount());
  EXPECT_EQ(
      (AccessibilityEventsVector{
          std::make_pair(GetTreeAccessibilityView(), ax::mojom::Event::kFocus),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kChildrenChanged),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kRowCountChanged)}),
      accessibility_events());
  data = GetAccessibilityViewByName("root")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            root_pos_in_set);

  data = GetAccessibilityViewByName("a")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_children_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            a_pos_in_set);

  data = GetAccessibilityViewByName("c")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_children_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            c_pos_in_set);

  // Remove 'c11', shouldn't visually change anything.
  ClearAccessibilityEvents();
  model_.Remove(GetNodeByTitle("c11")->parent(), GetNodeByTitle("c11"));
  EXPECT_EQ("root [a c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(3u, GetRowCount());
  data = GetAccessibilityViewByName("root")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            root_pos_in_set);

  data = GetAccessibilityViewByName("a")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_children_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            a_pos_in_set);

  data = GetAccessibilityViewByName("c")->GetData();
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            root_children_set_size);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            c_pos_in_set);

  // Node "c11" is not visible, hence no accessibility event needed.
  EXPECT_EQ(AccessibilityEventsVector(), accessibility_events());

  // Select 'c1', remove 'c' and make sure selection changes.
  tree()->SetSelectedNode(GetNodeByTitle("c1"));
  EXPECT_EQ("c1", GetSelectedNodeTitle());
  EXPECT_EQ("c1", GetSelectedAccessibilityViewName());
  ClearAccessibilityEvents();
  model_.Remove(GetNodeByTitle("c")->parent(), GetNodeByTitle("c"));
  EXPECT_EQ("root [a]", TreeViewContentsAsString());
  EXPECT_EQ("root [a]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());
  EXPECT_EQ(2u, GetRowCount());
  EXPECT_EQ(
      (AccessibilityEventsVector{
          std::make_pair(GetTreeAccessibilityView(), ax::mojom::Event::kFocus),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kChildrenChanged),
          std::make_pair(GetAccessibilityViewByName("a"),
                         ax::mojom::Event::kFocus),
          std::make_pair(GetAccessibilityViewByName("a"),
                         ax::mojom::Event::kSelection),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kRowCountChanged)}),
      accessibility_events());

  // Add 'c1', 'c2', 'c3', select 'c2', remove it and 'c3" should be selected.
  Add(GetNodeByTitle("a"), 0, "c1");
  Add(GetNodeByTitle("a"), 1, "c2");
  Add(GetNodeByTitle("a"), 2, "c3");
  tree()->SetSelectedNode(GetNodeByTitle("c2"));
  model_.Remove(GetNodeByTitle("c2")->parent(), GetNodeByTitle("c2"));
  EXPECT_EQ("root [a [c1 c3]]", TreeViewContentsAsString());
  EXPECT_EQ("root [a [c1 c3]]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("c3", GetSelectedNodeTitle());
  EXPECT_EQ("c3", GetSelectedAccessibilityViewName());
  EXPECT_EQ(4u, GetRowCount());

  // Now delete 'c3' and then 'c1' should be selected.
  ClearAccessibilityEvents();
  model_.Remove(GetNodeByTitle("c3")->parent(), GetNodeByTitle("c3"));
  EXPECT_EQ("root [a [c1]]", TreeViewContentsAsString());
  EXPECT_EQ("root [a [c1]]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("c1", GetSelectedNodeTitle());
  EXPECT_EQ("c1", GetSelectedAccessibilityViewName());
  EXPECT_EQ(3u, GetRowCount());
  EXPECT_EQ(
      (AccessibilityEventsVector{
          std::make_pair(GetTreeAccessibilityView(), ax::mojom::Event::kFocus),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kChildrenChanged),
          std::make_pair(GetAccessibilityViewByName("c1"),
                         ax::mojom::Event::kFocus),
          std::make_pair(GetAccessibilityViewByName("c1"),
                         ax::mojom::Event::kSelection),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kRowCountChanged)}),
      accessibility_events());

  // Finally delete 'c1' and then 'a' should be selected.
  ClearAccessibilityEvents();
  model_.Remove(GetNodeByTitle("c1")->parent(), GetNodeByTitle("c1"));
  EXPECT_EQ("root [a]", TreeViewContentsAsString());
  EXPECT_EQ("root [a]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());
  EXPECT_EQ(2u, GetRowCount());
  EXPECT_EQ(
      (AccessibilityEventsVector{
          std::make_pair(GetTreeAccessibilityView(), ax::mojom::Event::kFocus),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kChildrenChanged),
          std::make_pair(GetAccessibilityViewByName("a"),
                         ax::mojom::Event::kFocus),
          std::make_pair(GetAccessibilityViewByName("a"),
                         ax::mojom::Event::kSelection),
          std::make_pair(GetTreeAccessibilityView(),
                         ax::mojom::Event::kRowCountChanged)}),
      accessibility_events());

  tree()->SetRootShown(false);
  // Add 'b' and 'c', select 'b' and remove it. Selection should change to 'c'.
  Add(GetNodeByTitle("root"), 1, "b");
  Add(GetNodeByTitle("root"), 2, "c");
  tree()->SetSelectedNode(GetNodeByTitle("b"));
  model_.Remove(GetNodeByTitle("b")->parent(), GetNodeByTitle("b"));
  EXPECT_EQ("root [a c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("c", GetSelectedNodeTitle());
  EXPECT_EQ("c", GetSelectedAccessibilityViewName());
  EXPECT_EQ(2u, GetRowCount());
}

class TestController : public TreeViewController {
 public:
  void OnTreeViewSelectionChanged(TreeView* tree_view) override {
    call_count_++;
  }

  bool CanEdit(TreeView* tree_view, ui::TreeModelNode* node) override {
    return true;
  }

  int selection_change_count() const { return call_count_; }

 private:
  int call_count_ = 0;
};

TEST_F(TreeViewTest, RemovingLastNodeNotifiesSelectionChanged) {
  TestController controller;
  tree()->SetController(&controller);
  tree()->SetRootShown(false);
  tree()->SetModel(&model_);

  // Remove all but one node.
  model_.Remove(GetNodeByTitle("b")->parent(), GetNodeByTitle("b"));
  model_.Remove(GetNodeByTitle("c")->parent(), GetNodeByTitle("c"));
  tree()->SetSelectedNode(GetNodeByTitle("a"));
  EXPECT_EQ("root [a]", TreeViewContentsAsString());
  EXPECT_EQ("root [a]", TreeViewAccessibilityContentsAsString());

  const int prior_call_count = controller.selection_change_count();
  // Remove the final node and expect
  // |TestController::OnTreeViewSelectionChanged| to be called.
  model_.Remove(GetNodeByTitle("a")->parent(), GetNodeByTitle("a"));
  EXPECT_EQ(prior_call_count + 1, controller.selection_change_count());
}

// Verifies that changing a node title works.
TEST_F(TreeViewTest, TreeNodeChanged) {
  // Add c1 as a child of c and c11 as a child of c1.
  Add(Add(GetNodeByTitle("c"), 0, "c1"), 0, "c11");
  tree()->SetModel(&model_);
  ClearAccessibilityEvents();

  // Change c11, shouldn't do anything.
  model_.SetTitle(GetNodeByTitle("c11"), u"c11.new");
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(4u, GetRowCount());
  EXPECT_EQ(AccessibilityEventsVector(), accessibility_events());

  // Change 'b1', shouldn't do anything.
  ClearAccessibilityEvents();
  model_.SetTitle(GetNodeByTitle("b1"), u"b1.new");
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(4u, GetRowCount());
  EXPECT_EQ(AccessibilityEventsVector(), accessibility_events());

  // Change 'b'.
  ClearAccessibilityEvents();
  model_.SetTitle(GetNodeByTitle("b"), u"b.new");
  EXPECT_EQ("root [a b.new c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b.new c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(4u, GetRowCount());
  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetAccessibilityViewByName("b.new"),
                               ax::mojom::Event::kLocationChanged)}),
            accessibility_events());
}

// Verifies that IncrementSelection() works.
TEST_F(TreeViewTest, IncrementSelection) {
  tree()->SetModel(&model_);
  ClearAccessibilityEvents();

  IncrementSelection(true);
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());
  EXPECT_EQ(
      (AccessibilityEventsVector{std::make_pair(GetAccessibilityViewByName("a"),
                                                ax::mojom::Event::kFocus),
                                 std::make_pair(GetAccessibilityViewByName("a"),
                                                ax::mojom::Event::kSelection)}),
      accessibility_events());

  IncrementSelection(true);
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());
  IncrementSelection(true);
  tree()->Expand(GetNodeByTitle("b"));
  IncrementSelection(false);
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  EXPECT_EQ("b1", GetSelectedAccessibilityViewName());
  IncrementSelection(true);
  EXPECT_EQ("c", GetSelectedNodeTitle());
  EXPECT_EQ("c", GetSelectedAccessibilityViewName());
  IncrementSelection(true);
  EXPECT_EQ("c", GetSelectedNodeTitle());
  EXPECT_EQ("c", GetSelectedAccessibilityViewName());

  tree()->SetRootShown(false);
  tree()->SetSelectedNode(GetNodeByTitle("a"));
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());
  IncrementSelection(false);
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());
}

// Verifies that CollapseOrSelectParent works.
TEST_F(TreeViewTest, CollapseOrSelectParent) {
  tree()->SetModel(&model_);

  tree()->SetSelectedNode(GetNodeByTitle("root"));
  CollapseOrSelectParent();
  EXPECT_EQ("root", TreeViewContentsAsString());
  EXPECT_EQ("root", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());

  // Hide the root, which should implicitly expand the root.
  tree()->SetRootShown(false);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());

  tree()->SetSelectedNode(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1] c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  EXPECT_EQ("b1", GetSelectedAccessibilityViewName());
  CollapseOrSelectParent();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1] c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());
  CollapseOrSelectParent();
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());
}

// Verifies that ExpandOrSelectChild works.
TEST_F(TreeViewTest, ExpandOrSelectChild) {
  tree()->SetModel(&model_);

  tree()->SetSelectedNode(GetNodeByTitle("root"));
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());

  ExpandOrSelectChild();
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());

  tree()->SetSelectedNode(GetNodeByTitle("b"));
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1] c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());

  ExpandOrSelectChild();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1] c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  EXPECT_EQ("b1", GetSelectedAccessibilityViewName());

  ExpandOrSelectChild();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1] c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  EXPECT_EQ("b1", GetSelectedAccessibilityViewName());
}

// Verify that selection is properly updated on each keystroke.
TEST_F(TreeViewTest, SelectOnKeyStroke) {
  tree()->SetModel(&model_);
  tree()->ExpandAll(model_.GetRoot());
  selector()->InsertText(
      u"b",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());
  selector()->InsertText(
      u"1",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  EXPECT_EQ("b1", GetSelectedAccessibilityViewName());

  // Invoke OnViewBlur() to reset time.
  selector()->OnViewBlur();
  selector()->InsertText(
      u"z",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  EXPECT_EQ("b1", GetSelectedAccessibilityViewName());

  selector()->OnViewBlur();
  selector()->InsertText(
      u"a",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());
}

// Verifies that edits are committed when focus is lost.
TEST_F(TreeViewTest, CommitOnFocusLost) {
  tree()->SetModel(&model_);

  tree()->SetSelectedNode(GetNodeByTitle("root"));
  ExpandOrSelectChild();
  tree()->SetEditable(true);
  tree()->StartEditing(GetNodeByTitle("a"));
  tree()->editor()->SetText(u"a changed");
  tree()->OnDidChangeFocus(nullptr, nullptr);
  EXPECT_TRUE(GetNodeByTitle("a changed") != nullptr);

  ASSERT_NE(nullptr, GetRootAccessibilityView());
  ASSERT_LE(1u, GetRootAccessibilityView()->children().size());
  EXPECT_EQ(
      "a changed",
      GetRootAccessibilityView()->children()[0]->GetData().GetStringAttribute(
          ax::mojom::StringAttribute::kName));
}

// Verifies that virtual accessible actions go to virtual view targets.
TEST_F(TreeViewTest, VirtualAccessibleAction) {
  tree()->SetModel(&model_);
  tree()->Expand(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b [b1] c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ(5u, GetRowCount());

  // Set to nullptr should clear the selection.
  tree()->SetSelectedNode(nullptr);
  EXPECT_EQ(std::string(), GetActiveNodeTitle());
  EXPECT_EQ(std::string(), GetActiveAccessibilityViewName());
  EXPECT_EQ(std::string(), GetSelectedNodeTitle());
  EXPECT_EQ(std::string(), GetSelectedAccessibilityViewName());

  // Test using each virtual view as target.
  ui::AXActionData data;
  const std::string test_cases[] = {"root", "a", "b", "b1", "c"};
  for (const std::string& name : test_cases) {
    data.target_node_id = GetAccessibilityViewByName(name)->GetData().id;
    data.action = ax::mojom::Action::kDoDefault;
    EXPECT_TRUE(tree()->HandleAccessibleAction(data));
    EXPECT_EQ(name, GetActiveNodeTitle());
    EXPECT_EQ(name, GetActiveAccessibilityViewName());
    EXPECT_EQ(name, GetSelectedNodeTitle());
    EXPECT_EQ(name, GetSelectedAccessibilityViewName());
  }

  // Do nothing when a valid node id is not provided. This can happen if the
  // actions target the owner view itself.
  tree()->SetSelectedNode(GetNodeByTitle("b"));
  data.target_node_id = ui::kInvalidAXNodeID;
  data.action = ax::mojom::Action::kDoDefault;
  EXPECT_FALSE(tree()->HandleAccessibleAction(data));
  EXPECT_EQ("b", GetActiveNodeTitle());
  EXPECT_EQ("b", GetActiveAccessibilityViewName());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());

  // Check that the active node is set if assistive technologies set focus.
  tree()->SetSelectedNode(GetNodeByTitle("b"));
  data.target_node_id = GetAccessibilityViewByName("a")->GetData().id;
  data.action = ax::mojom::Action::kFocus;
  EXPECT_TRUE(tree()->HandleAccessibleAction(data));
  EXPECT_EQ("a", GetActiveNodeTitle());
  EXPECT_EQ("a", GetActiveAccessibilityViewName());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());

  // Do not handle accessible actions when no node is selected.
  tree()->SetSelectedNode(nullptr);
  data.target_node_id = ui::kInvalidAXNodeID;
  data.action = ax::mojom::Action::kDoDefault;
  EXPECT_FALSE(tree()->HandleAccessibleAction(data));
  EXPECT_EQ(std::string(), GetActiveNodeTitle());
  EXPECT_EQ(std::string(), GetActiveAccessibilityViewName());
  EXPECT_EQ(std::string(), GetSelectedNodeTitle());
  EXPECT_EQ(std::string(), GetSelectedAccessibilityViewName());
}

// Verifies that accessibility focus events get fired for the correct nodes when
// the tree view is given focus.
TEST_F(TreeViewTest, OnFocusAccessibilityEvents) {
  // Without keyboard focus, model changes should not fire focus events.
  tree()->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(tree()->HasFocus());
  tree()->SetModel(&model_);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root [a b c]", TreeViewAccessibilityContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ("root", GetSelectedAccessibilityViewName());
  EXPECT_EQ(4u, GetRowCount());
  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetRootAccessibilityView(),
                               ax::mojom::Event::kSelection)}),
            accessibility_events());

  // The initial focus should fire a focus event for the active node
  // (in this case, the root node).
  ClearAccessibilityEvents();
  tree()->RequestFocus();
  EXPECT_TRUE(tree()->HasFocus());
  EXPECT_EQ((AccessibilityEventsVector{std::make_pair(
                GetRootAccessibilityView(), ax::mojom::Event::kFocus)}),
            accessibility_events());

  // Focus clear and restore should fire a focus event for the active node.
  ClearAccessibilityEvents();
  tree()->SetSelectedNode(GetNodeByTitle("b"));
  tree()->SetActiveNode(GetNodeByTitle("a"));
  EXPECT_EQ("a", GetActiveNodeTitle());
  EXPECT_EQ("a", GetActiveAccessibilityViewName());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());
  tree()->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(tree()->HasFocus());
  tree()->GetFocusManager()->RestoreFocusedView();
  EXPECT_TRUE(tree()->HasFocus());
  EXPECT_EQ("a", GetActiveNodeTitle());
  EXPECT_EQ("a", GetActiveAccessibilityViewName());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());
  EXPECT_EQ(
      (AccessibilityEventsVector{std::make_pair(GetAccessibilityViewByName("b"),
                                                ax::mojom::Event::kFocus),
                                 std::make_pair(GetAccessibilityViewByName("b"),
                                                ax::mojom::Event::kSelection),
                                 std::make_pair(GetAccessibilityViewByName("a"),
                                                ax::mojom::Event::kFocus),
                                 std::make_pair(GetAccessibilityViewByName("a"),
                                                ax::mojom::Event::kFocus)}),
      accessibility_events());

  // Without keyboard focus, selection should not fire focus events.
  ClearAccessibilityEvents();
  tree()->GetFocusManager()->ClearFocus();
  tree()->SetSelectedNode(GetNodeByTitle("a"));
  EXPECT_FALSE(tree()->HasFocus());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());
  EXPECT_EQ(
      (AccessibilityEventsVector{std::make_pair(GetAccessibilityViewByName("a"),
                                                ax::mojom::Event::kSelection)}),
      accessibility_events());

  // A direct focus action on a tree item should give focus to the tree view but
  // only fire a focus event for the target node.
  ui::AXActionData data;
  const std::string test_cases[] = {"root", "a", "b", "c"};
  for (const std::string& name : test_cases) {
    ClearAccessibilityEvents();
    tree()->GetFocusManager()->ClearFocus();
    EXPECT_FALSE(tree()->HasFocus());
    data.target_node_id = GetAccessibilityViewByName(name)->GetData().id;
    data.action = ax::mojom::Action::kFocus;
    EXPECT_TRUE(tree()->HandleAccessibleAction(data));
    EXPECT_TRUE(tree()->HasFocus());
    EXPECT_EQ(name, GetActiveNodeTitle());
    EXPECT_EQ(name, GetActiveAccessibilityViewName());
    EXPECT_EQ(name, GetSelectedNodeTitle());
    EXPECT_EQ(name, GetSelectedAccessibilityViewName());
    EXPECT_EQ((AccessibilityEventsVector{
                  std::make_pair(GetAccessibilityViewByName(name),
                                 ax::mojom::Event::kSelection),
                  std::make_pair(GetAccessibilityViewByName(name),
                                 ax::mojom::Event::kFocus)}),
              accessibility_events());
  }

  // A direct focus action on the tree view itself with an active node should
  // have no effect.
  ClearAccessibilityEvents();
  tree()->GetFocusManager()->ClearFocus();
  tree()->SetSelectedNode(GetNodeByTitle("b"));
  data.target_node_id = ui::kInvalidAXNodeID;
  data.action = ax::mojom::Action::kFocus;
  EXPECT_FALSE(tree()->HandleAccessibleAction(data));
  EXPECT_FALSE(tree()->HasFocus());
  EXPECT_EQ("b", GetActiveNodeTitle());
  EXPECT_EQ("b", GetActiveAccessibilityViewName());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ("b", GetSelectedAccessibilityViewName());
  EXPECT_EQ(
      (AccessibilityEventsVector{std::make_pair(GetAccessibilityViewByName("b"),
                                                ax::mojom::Event::kSelection)}),
      accessibility_events());

  // A direct focus action on a tree view without an active node (i.e. empty
  // tree) should fire a focus event for the tree view.
  ClearAccessibilityEvents();
  tree()->GetFocusManager()->ClearFocus();
  ui::TreeNodeModel<TestNode> empty_model(std::make_unique<TestNode>());
  static_cast<TestNode*>(empty_model.GetRoot())->SetTitle(u"root");
  tree()->SetModel(&empty_model);
  tree()->SetRootShown(false);
  data.target_node_id = ui::kInvalidAXNodeID;
  data.action = ax::mojom::Action::kFocus;
  EXPECT_TRUE(tree()->HandleAccessibleAction(data));
  EXPECT_TRUE(tree()->HasFocus());
  EXPECT_EQ(std::string(), GetActiveNodeTitle());
  EXPECT_EQ(std::string(), GetActiveAccessibilityViewName());
  EXPECT_EQ(std::string(), GetSelectedNodeTitle());
  EXPECT_EQ(std::string(), GetSelectedAccessibilityViewName());
  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetRootAccessibilityView(),
                               ax::mojom::Event::kSelection),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kSelection),
                std::make_pair(GetRootAccessibilityView(),
                               ax::mojom::Event::kStateChanged),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kFocus)}),
            accessibility_events());

  // When a focused empty tree is populated with nodes, it should immediately
  // hand off focus to one of them and select it.
  ClearAccessibilityEvents();
  tree()->SetModel(&model_);
  EXPECT_EQ("a", GetActiveNodeTitle());
  EXPECT_EQ("a", GetActiveAccessibilityViewName());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ("a", GetSelectedAccessibilityViewName());
  EXPECT_EQ((AccessibilityEventsVector{
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetTreeAccessibilityView(),
                               ax::mojom::Event::kChildrenChanged),
                std::make_pair(GetAccessibilityViewByName("a"),
                               ax::mojom::Event::kFocus),
                std::make_pair(GetAccessibilityViewByName("a"),
                               ax::mojom::Event::kSelection)}),
            accessibility_events());
}

}  // namespace views
