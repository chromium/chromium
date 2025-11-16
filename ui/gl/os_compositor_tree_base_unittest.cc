// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/os_compositor_tree_base.h"

#include <algorithm>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/overlay_layer_id.h"

namespace gl {
namespace {

// Concise alias to create a test layer ID.
gfx::OverlayLayerId LayerId(uint32_t layer_id) {
  return gfx::OverlayLayerId::MakeForTesting(layer_id);
}

// A mock OS compositor tree, imitating Direct Composition or Core Animation. It
// minimally implements a DAG with a root node and basic operations to
// manipulate it. It uses `gfx::OverlayLayerId` as the node type.
class MiniTree {
 public:
  using node_type = gfx::OverlayLayerId;

  // This contains information about accesses to the `MiniTree`. It's intended
  // to aid debugging test failures.
  struct Change {
    enum class Type {
      kCreate,
      kRemoveChild,
      kAddChild,
      kDestroy,
      kCommit,
    };

    Type type;
    node_type node;
    // For `kRemoveChild`, `kAddChild`
    node_type child;
    // For `kAddChild`
    std::optional<node_type> below;

    base::Value ToValue() const {
      base::Value value(base::Value::Type::DICT);
      base::Value::Dict& dict = value.GetDict();
      switch (type) {
        case Type::kCreate:
          dict.Set("_type", "Create");
          dict.Set("node", node.ToString());
          break;
        case Type::kRemoveChild:
          dict.Set("_type", "RemoveChild");
          dict.Set("parent", node.ToString());
          dict.Set("child", child.ToString());
          break;
        case Type::kAddChild:
          dict.Set("_type", "AddChild");
          dict.Set("parent", node.ToString());
          dict.Set("child", child.ToString());
          if (below) {
            dict.Set("below", below->ToString());
          } else {
            // Nothing is below the node, so it is added below all siblings.
          }
          break;
        case Type::kDestroy:
          dict.Set("_type", "Destroy");
          dict.Set("node", node.ToString());
          break;
        case Type::kCommit:
          dict.Set("_type", "Commit");
          break;
      }
      return value;
    }
  };

  MiniTree()
      : root_node_(
            // Insert the implicit root node.
            CreateNode(gfx::OverlayLayerId())) {
    // We're not interested in root node changes since they are the same for all
    // trees.
    changes_.clear();
    nodes_touched_this_frame_.clear();
  }

  node_type CreateNode(const node_type& node) {
    CHECK(!hierarchy_.contains(node) || hierarchy_.at(node).empty());

    hierarchy_.insert({node, {}});

    changes_.push_back(Change{.type = Change::Type::kCreate, .node = node});
    nodes_touched_this_frame_.insert(node);

    return node;
  }

  void RemoveChild(const node_type& parent, const node_type& child) {
    CHECK(hierarchy_.contains(parent)) << "parent node must exist";
    CHECK(hierarchy_.contains(child)) << "child node must exist";

    auto& siblings = hierarchy_[parent];
    auto it = std::ranges::find(siblings, child);
    CHECK(it != siblings.end())
        << "child node must have been previously added under parent";
    siblings.erase(it);

    changes_.push_back(Change{
        .type = Change::Type::kRemoveChild, .node = parent, .child = child});
    nodes_touched_this_frame_.insert(child);
  }

  void AddChild(const node_type& parent,
                const node_type& child,
                const std::optional<node_type>& below) {
    CHECK(hierarchy_.contains(parent)) << "parent node must exist";
    CHECK(hierarchy_.contains(child)) << "child node must exist";
    CHECK(std::ranges::none_of(hierarchy_, [&](const auto& entry) {
      const auto& siblings = entry.second;
      return std::ranges::find(siblings, child) != siblings.end();
    })) << "child node  must not be attached to any node";

    auto& siblings = hierarchy_[parent];
    if (below) {
      auto insert_after = std::ranges::find(siblings, below.value());
      CHECK(insert_after != siblings.end()) << "below node must exist";
      insert_after++;
      siblings.insert(insert_after, child);
    } else {
      siblings.insert(siblings.begin(), child);
    }

    changes_.push_back(Change{.type = Change::Type::kAddChild,
                              .node = parent,
                              .child = child,
                              .below = below});
    nodes_touched_this_frame_.insert(child);
  }

  struct CommitStats {
    std::vector<Change> changes;
    base::flat_set<gfx::OverlayLayerId> nodes_touched_this_frame;
  };

  CommitStats Commit() {
    changes_.push_back(Change{.type = Change::Type::kCommit});

    // Trim the tree storage
    {
      base::flat_set<node_type> referenced_nodes{root_node()};
      WalkChildrenRecursive(
          root_node(), [&](const node_type& parent, const node_type& node) {
            referenced_nodes.insert(node);
          });

      auto it = hierarchy_.begin();
      while (it != hierarchy_.end()) {
        if (!referenced_nodes.contains(it->first)) {
          changes_.push_back(
              Change{.type = Change::Type::kDestroy, .node = it->first});
          it = hierarchy_.erase(it);
        } else {
          it++;
        }
      }
    }

    return {
        .changes = std::move(changes_),
        .nodes_touched_this_frame = std::move(nodes_touched_this_frame_),
    };
  }

  node_type root_node() const { return root_node_; }

  base::Value RootNodeToValue() const {
    return NodeToValueRecursive(root_node());
  }

 private:
  using RecursiveWalkCallback =
      base::FunctionRef<void(const node_type& parent, const node_type& node)>;
  void WalkChildrenRecursive(const node_type& node,
                             RecursiveWalkCallback callback) const {
    for (const auto& child : hierarchy_.at(node)) {
      callback(node, child);
      WalkChildrenRecursive(child, callback);
    }
  }

  base::Value NodeToValueRecursive(node_type node) const {
    base::Value value(base::Value::Type::DICT);
    base::Value::Dict& dict = value.GetDict();
    dict.Set("_id", node == root_node() ? "root" : node.ToString());
    if (!hierarchy_.at(node).empty()) {
      base::Value children(base::Value::Type::LIST);
      for (const auto& child : hierarchy_.at(node)) {
        children.GetList().Append(NodeToValueRecursive(child));
      }
      dict.Set("children", std::move(children));
    }
    return value;
  }

  std::vector<Change> changes_;
  base::flat_set<node_type> nodes_touched_this_frame_;

  // Represents a N-ary tree where each node has ordered (back-to-front)
  // children. We expect this to be acyclic.
  base::flat_map<node_type, std::vector<node_type>> hierarchy_;

  const node_type root_node_;
};

TEST(MiniTreeTest, EmptyTreesAreEqual) {
  MiniTree tree1;
  MiniTree tree2;
  EXPECT_EQ(tree1.RootNodeToValue(), tree2.RootNodeToValue());
}

TEST(MiniTreeTest, SimpleTreesAreEqual) {
  MiniTree tree1;
  {
    tree1.AddChild(tree1.root_node(), tree1.CreateNode(LayerId(1)), {});
    tree1.AddChild(tree1.root_node(), tree1.CreateNode(LayerId(2)), {});
  }

  MiniTree tree2;
  {
    tree2.AddChild(tree2.root_node(), tree2.CreateNode(LayerId(1)), {});
    tree2.AddChild(tree2.root_node(), tree2.CreateNode(LayerId(2)), {});
  }

  EXPECT_EQ(tree1.RootNodeToValue(), tree2.RootNodeToValue());
}

TEST(MiniTreeTest, SimpleTreesWithDifferentStructureAreNotEqual) {
  MiniTree tree1;
  {
    tree1.AddChild(tree1.root_node(), tree1.CreateNode(LayerId(1)), {});
    tree1.AddChild(tree1.root_node(), tree1.CreateNode(LayerId(2)), {});
  }

  MiniTree tree2;
  {
    tree2.AddChild(tree2.root_node(), tree2.CreateNode(LayerId(1)), {});
  }

  EXPECT_NE(tree1.RootNodeToValue(), tree2.RootNodeToValue());
}

TEST(MiniTreeTest, SimpleTreesWithSameStructureButDifferentContentAreNotEqual) {
  MiniTree tree1;
  {
    tree1.AddChild(tree1.root_node(), tree1.CreateNode(LayerId(1)), {});
    tree1.AddChild(tree1.root_node(), tree1.CreateNode(LayerId(2)), {});
  }

  MiniTree tree2;
  {
    tree2.AddChild(tree2.root_node(), tree2.CreateNode(LayerId(1)), {});
    tree2.AddChild(tree2.root_node(), tree2.CreateNode(LayerId(3)), {});
  }

  EXPECT_NE(tree1.RootNodeToValue(), tree2.RootNodeToValue());
}

// A minimal overlay layer type. This is the minimum required by
// `OsCompositorTreeBase`.
struct TestOverlayParams {
  TestOverlayParams() = default;

  TestOverlayParams(TestOverlayParams&&) = default;
  TestOverlayParams& operator=(TestOverlayParams&&) = default;

  int z_order;
  gfx::OverlayLayerId layer_id;
  gfx::OverlayLayerId parent_layer_id;

  base::Value ToValue() const {
    base::Value value(base::Value::Type::DICT);
    base::Value::Dict& dict = value.GetDict();
    dict.Set("z_order", z_order);
    dict.Set("layer_id", layer_id.ToString());
    dict.Set("parent_layer_id", parent_layer_id.ToString());
    return value;
  }
};

// Helper functions to build test input (i.e. overlay params list) and expected
// test output (i.e. `MiniTree` generated "from scratch"). This allows test to
// be written with mostly declarative input.
class TestTreeBuilder {
 public:
  struct Layer {
    gfx::OverlayLayerId id;
    // Ordered back-to-front.
    std::vector<Layer> children;

    Layer() = default;
    Layer(uint32_t layer_id, std::vector<Layer> child_layers)
        : id(gfx::OverlayLayerId::MakeForTesting(layer_id)),
          children(child_layers) {}
  };

  static MiniTree MiniTreeFromRootLayers(std::vector<Layer> root_layers) {
    MiniTree tree;
    WalkRootTopologically(root_layers,
                          [&](const gfx::OverlayLayerId& parent,
                              const std::optional<gfx::OverlayLayerId>& below,
                              const gfx::OverlayLayerId& id, int z_order) {
                            tree.AddChild(parent, tree.CreateNode(id), below);
                          });
    return tree;
  }

  // Returns a topologically sorted list of overlays adding the correct z-index
  // relative to siblings.
  static std::vector<TestOverlayParams> OverlaysFromRootLayers(
      std::vector<Layer> root_layers) {
    std::vector<TestOverlayParams> overlays;
    WalkRootTopologically(root_layers,
                          [&](const gfx::OverlayLayerId& parent,
                              const std::optional<gfx::OverlayLayerId>& below,
                              const gfx::OverlayLayerId& id, int z_order) {
                            overlays.emplace_back();
                            overlays.back().layer_id = id;
                            overlays.back().parent_layer_id = parent;
                            overlays.back().z_order = z_order;
                          });
    return overlays;
  }

 private:
  using TopologicalWalkCallback =
      base::FunctionRef<void(const gfx::OverlayLayerId& parent,
                             const std::optional<gfx::OverlayLayerId>& below,
                             const gfx::OverlayLayerId& id,
                             int z_order)>;

  static void WalkRootTopologically(std::vector<Layer>& root_layers,
                                    TopologicalWalkCallback callback) {
    Layer root_layer;
    root_layer.id = gfx::OverlayLayerId();
    root_layer.children = root_layers;
    WalkTopologically(root_layer, std::move(callback));
  }

  static void WalkTopologically(const Layer& layer,
                                TopologicalWalkCallback callback) {
    int z_order = 1;
    std::optional<gfx::OverlayLayerId> below;
    for (const Layer& child_layer : layer.children) {
      callback(layer.id, below, child_layer.id, z_order++);
      WalkTopologically(child_layer, callback);
      below = child_layer.id;
    }
  }
};

// Alias to make tests less verbose.
using Layer = TestTreeBuilder::Layer;

// A minimal layer type. This correctly returns `true` from `Update` if things
// have changed.
class MiniTreeNodeWrapper {
 public:
  MiniTreeNodeWrapper() = default;

  MiniTreeNodeWrapper(const MiniTreeNodeWrapper&) = delete;
  MiniTreeNodeWrapper& operator=(const MiniTreeNodeWrapper&) = delete;

  void unsafe_set_below_layer(const MiniTreeNodeWrapper* below_layer) {
    below_node_ = below_layer ? below_layer->container_node() : std::nullopt;
  }

  bool Update(MiniTree& tree,
              const gfx::OverlayLayerId& layer_id,
              MiniTree::node_type parent_node,
              std::optional<MiniTree::node_type> below_node) {
    bool did_change = false;

    std::optional<MiniTree::node_type> old_parent_node = parent_node_;

    auto SetField = [&did_change](auto& field, auto& parameter) -> bool {
      const bool changed = field != parameter;
      if (changed) {
        field = std::move(parameter);
        did_change = true;
      }
      return changed;
    };

    const bool parent_node_changed = SetField(parent_node_, parent_node);
    const bool below_node_changed = SetField(below_node_, below_node);

    if (!container_node()) {
      container_node_ = tree.CreateNode(layer_id);
    }

    if (parent_node_changed || below_node_changed) {
      if (old_parent_node) {
        tree.RemoveChild(old_parent_node.value(), container_node().value());
      }

      tree.AddChild(parent_node_.value(), container_node().value(),
                    below_node_);
    }

    return did_change;
  }

  std::optional<MiniTree::node_type> container_node() const {
    return container_node_;
  }
  std::optional<MiniTree::node_type> parent_node() const {
    return parent_node_;
  }

 private:
  std::optional<MiniTree::node_type> container_node_;
  std::optional<MiniTree::node_type> parent_node_;
  std::optional<MiniTree::node_type> below_node_;
};

class TestCompositorTree
    : public OsCompositorTreeBase<TestOverlayParams, MiniTreeNodeWrapper> {
 public:
  explicit TestCompositorTree(TestCompositorTree::UpdateMode update_mode)
      : OsCompositorTreeBase(update_mode) {}

  ~TestCompositorTree() override = default;

  const MiniTree& mini_tree() const { return mini_tree_; }

  const MiniTree::CommitStats& last_frame_commit_stats() const {
    return last_frame_commit_stats_;
  }

 protected:
  bool UpdateLayer(const TestOverlayParams& overlay,
                   const MiniTreeNodeWrapper* parent_layer,
                   const MiniTreeNodeWrapper* below_layer,
                   MiniTreeNodeWrapper& layer) override {
    // This test layer is logically a single node (not a stack of several nodes
    // that apply transform, clipping, etc). We can consider the top-level
    // container node to be the same node that contains the children.
    const MiniTree::node_type parent_node =
        parent_layer ? parent_layer->container_node().value()
                     : mini_tree_.root_node();

    return layer.Update(
        mini_tree_, overlay.layer_id, parent_node,
        below_layer ? below_layer->container_node() : std::nullopt);
  }

  void DestroyLayer(std::unique_ptr<MiniTreeNodeWrapper> layer) override {
    mini_tree_.RemoveChild(layer->parent_node().value(),
                           layer->container_node().value());
  }

  bool CommitTree() override {
    last_frame_commit_stats_ = mini_tree_.Commit();
    return true;
  }

 private:
  MiniTree mini_tree_;
  MiniTree::CommitStats last_frame_commit_stats_;
};

class OsCompositorTreeBaseTest
    : public testing::TestWithParam<TestCompositorTree::UpdateMode> {
 public:
  static std::string GetParamName(
      const testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case TestCompositorTree::UpdateMode::kFromScratch:
        return "FromScratch";
      case TestCompositorTree::UpdateMode::
          kIncrementalNoPatchSiblingsOptimization:
        return "IncrementalNoPatchSiblingsOptimization";
      case TestCompositorTree::UpdateMode::kIncremental:
        return "Incremental";
    }
    NOTREACHED();
  }

  OsCompositorTreeBaseTest() : tree_(GetParam()) {}

  // This function creates an overlay list from `layers`, passes it to `tree_`
  // and compares `tree_`'s incremental output with `layers`.
  //
  // - `layers` defines a tree where they are the children of an implicit root
  //   layer.
  // - `expected_num_layers_modified` is the expected number of layers
  //   touched in the fully incremental mode. This function will adjust the
  //   expectation for other modes.
  void UpdateTree(std::vector<Layer> layers, int expected_num_layers_modified) {
    const base::Value prev_tree = tree_.mini_tree().RootNodeToValue();

    std::vector<TestOverlayParams> overlays =
        TestTreeBuilder::OverlaysFromRootLayers(layers);

    // Dump the generated overlay candidates to help understand the input tree.
    base::Value overlays_list(base::Value::Type::LIST);
    for (const auto& overlay : overlays) {
      overlays_list.GetList().Append(overlay.ToValue());
    }
    SCOPED_TRACE(base::StringPrintf("Generated overlay candidates = %s",
                                    overlays_list.DebugString()));

    EXPECT_TRUE(tree_.UpdateTree(overlays));

    const base::Value actual_tree = tree_.mini_tree().RootNodeToValue();
    const base::Value expected_tree =
        TestTreeBuilder::MiniTreeFromRootLayers(layers).RootNodeToValue();

    // Dump the previous tree and the set of changes that happened to help
    // understand why.
    const auto& commit_stats = tree_.last_frame_commit_stats();
    base::Value changes_list(base::Value::Type::LIST);
    for (const auto& change : commit_stats.changes) {
      changes_list.GetList().Append(change.ToValue());
    }
    base::Value touched_layer_ids_list(base::Value::Type::LIST);
    for (const auto& touched_layer_id : commit_stats.nodes_touched_this_frame) {
      touched_layer_ids_list.GetList().Append(touched_layer_id.ToString());
    }
    SCOPED_TRACE(base::StringPrintf(
        "Incremental update info:\n"
        "Prev tree: %s\n"
        "Changes from this update: %s\n"
        "Layers directly touched: %s\n"
        "Current tree: %s\n"
        "Run with `--vmodule=os_compositor_tree_base=3` for more output.",
        prev_tree.DebugString(), changes_list.DebugString(),
        touched_layer_ids_list.DebugString(), actual_tree.DebugString()));

    // The output tree should always be the same, regardless of how we update.
    EXPECT_EQ(actual_tree, expected_tree);

    switch (GetParam()) {
      case TestCompositorTree::UpdateMode::kFromScratch:
        EXPECT_EQ(static_cast<size_t>(
                      tree_.num_layers_modified_last_frame_for_testing()),
                  overlays.size());
        break;
      case TestCompositorTree::UpdateMode::
          kIncrementalNoPatchSiblingsOptimization:
        // We only really care about the fully optimized incremental case, but
        // if the patch optimization is causing us to touch more trees, we
        // should know about it.
        EXPECT_GE(tree_.num_layers_modified_last_frame_for_testing(),
                  expected_num_layers_modified);
        break;
      case TestCompositorTree::UpdateMode::kIncremental:
        EXPECT_EQ(tree_.num_layers_modified_last_frame_for_testing(),
                  expected_num_layers_modified);
        break;
    }
  }

  // Same as `UpdateTree(layers, size(layers))`. Expected to be called from the
  // default empty initialized state. Intended for initializing a test state,
  // but can be skipped if the empty state is desired.
  void InitializeTree(std::vector<Layer> layers) {
    UpdateTree(layers, TestTreeBuilder::OverlaysFromRootLayers(layers).size());
  }

  TestCompositorTree tree_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    OsCompositorTreeBaseTest,
    testing::ValuesIn({
        TestCompositorTree::UpdateMode::kFromScratch,
        TestCompositorTree::UpdateMode::kIncrementalNoPatchSiblingsOptimization,
        TestCompositorTree::UpdateMode::kIncremental,
    }),
    &OsCompositorTreeBaseTest::GetParamName);

TEST_P(OsCompositorTreeBaseTest, CommitEmpty) {
  UpdateTree({}, 0);
}

TEST_P(OsCompositorTreeBaseTest, InsertOneIntoEmpty) {
  UpdateTree(
      {
          Layer(1, {}),
      },
      1);
}

TEST_P(OsCompositorTreeBaseTest, LayerNotReused) {
  InitializeTree({
      Layer(1, {}),
  });

  // We expect layer 1 to be removed (and destroyed) and a new layer 2 added.
  UpdateTree(
      {
          Layer(2, {}),
      },
      2);
}

TEST_P(OsCompositorTreeBaseTest, DeleteOne) {
  InitializeTree({
      Layer(1, {}),
  });

  UpdateTree({}, 1);
}

TEST_P(OsCompositorTreeBaseTest, DeleteOneAndReAddIt) {
  InitializeTree({
      Layer(1, {}),
  });

  UpdateTree({}, 1);

  UpdateTree(
      {
          Layer(1, {}),
      },
      1);
}

TEST_P(OsCompositorTreeBaseTest, DeleteTwo) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
  });
  UpdateTree({}, 2);
}

TEST_P(OsCompositorTreeBaseTest, DeleteLayer) {
  InitializeTree({
      Layer(1,
            {
                Layer(2, {}),
            }),
  });
  UpdateTree({}, 2);
}

TEST_P(OsCompositorTreeBaseTest, InsertTwoIntoEmpty) {
  UpdateTree(
      {
          Layer(1, {}),
          Layer(2, {}),
      },
      2);
}

TEST_P(OsCompositorTreeBaseTest, InsertLayerIntoEmpty) {
  UpdateTree(
      {
          Layer(1,
                {
                    Layer(2, {}),
                }),
      },
      2);
}

TEST_P(OsCompositorTreeBaseTest, SwitchOrderOfTwo) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
  });
  UpdateTree(
      {
          Layer(2, {}),
          Layer(1, {}),
      },
      1);
}

TEST_P(OsCompositorTreeBaseTest, ThreeSwapFirstTwo) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  // Swap index 0 and 1
  UpdateTree(
      {
          Layer(2, {}),
          Layer(1, {}),
          Layer(3, {}),
      },
      1);
}

TEST_P(OsCompositorTreeBaseTest, ThreeSwapLastTwo) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  // Swap index 1 and 2
  UpdateTree(
      {
          Layer(1, {}),
          Layer(3, {}),
          Layer(2, {}),
      },
      1);
}

TEST_P(OsCompositorTreeBaseTest, ThreeSwapOuter) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  // Swap index 0 and 2
  // This requires touching two layers.
  UpdateTree(
      {
          Layer(3, {}),
          Layer(2, {}),
          Layer(1, {}),
      },
      2);
}

TEST_P(OsCompositorTreeBaseTest, ThreeRotateToBelow) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  UpdateTree(
      {
          Layer(3, {}),
          Layer(1, {}),
          Layer(2, {}),
      },
      1);
}

TEST_P(OsCompositorTreeBaseTest, ThreeRotateToAbove) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  // This requires touching two layers, due to the fact we walk the tree in
  // overlay candidate order.
  UpdateTree(
      {
          Layer(2, {}),
          Layer(3, {}),
          Layer(1, {}),
      },
      2);
}

TEST_P(OsCompositorTreeBaseTest, MoveChildToSiblingOfParent) {
  InitializeTree({
      Layer(1,
            {
                Layer(3, {}),
            }),
      Layer(2, {}),
  });

  UpdateTree(
      {
          Layer(1, {}),
          Layer(2, {}),
          Layer(3, {}),
      },
      1);
}

TEST_P(OsCompositorTreeBaseTest, MoveMultipleSiblingsToNewParent) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  UpdateTree(
      {
          Layer(1,
                {
                    Layer(2, {}),
                    Layer(3, {}),
                }),
      },
      2);
}

TEST_P(OsCompositorTreeBaseTest, ReparentSiblingsToChain) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  UpdateTree(
      {
          Layer(1,
                {
                    Layer(2,
                          {
                              Layer(3, {}),
                          }),
                }),
      },
      2);
}

TEST_P(OsCompositorTreeBaseTest, ReparentLayer) {
  InitializeTree({
      Layer(1,
            {
                Layer(3,
                      {
                          Layer(2, {}),
                      }),
            }),
  });

  // We move layer 3 while layer 2 is a child of it. Layer 2 does not need to be
  // updated in this case.
  UpdateTree(
      {
          Layer(3,
                {
                    Layer(2, {}),
                }),
          Layer(1, {}),
      },
      1);
}

TEST_P(OsCompositorTreeBaseTest, RemoveAndAddAtStart) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  UpdateTree(
      {
          Layer(10, {}),
          Layer(2, {}),
          Layer(3, {}),
      },
      2);
}

TEST_P(OsCompositorTreeBaseTest, RemoveAndAddAtMiddle) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  UpdateTree(
      {
          Layer(1, {}),
          Layer(10, {}),
          Layer(3, {}),
      },
      2);
}

TEST_P(OsCompositorTreeBaseTest, RemoveAndAddAtEnd) {
  InitializeTree({
      Layer(1, {}),
      Layer(2, {}),
      Layer(3, {}),
  });

  UpdateTree(
      {
          Layer(1, {}),
          Layer(2, {}),
          Layer(10, {}),
      },
      2);
}

#if EXPENSIVE_DCHECKS_ARE_ON() && defined(GTEST_HAS_DEATH_TEST)
// Check that `OsCompositorTreeBase` rejects invalid input since invalid input
// can invalidate its assumptions.
class OsCompositorTreeBaseTestInvalidInput : public OsCompositorTreeBaseTest {};

INSTANTIATE_TEST_SUITE_P(
    ,
    OsCompositorTreeBaseTestInvalidInput,
    testing::Values(TestCompositorTree::UpdateMode::kIncremental),
    &OsCompositorTreeBaseTest::GetParamName);

constexpr std::string_view kZOrderCheckMessage = "Siblings must be sorted.";
constexpr std::string_view kMustHaveLayerIdMessage =
    "Overlay must have non-default layer ID.";
constexpr std::string_view kHierarchyCheckMessage =
    "Overlays must be topologically sorted.";
constexpr std::string_view kLayerIdUniquenessCheckMessage =
    "Overlay layer IDs must all be unique.";

TEST_P(OsCompositorTreeBaseTestInvalidInput, UnsortedSiblings) {
  std::vector<TestOverlayParams> overlays;

  overlays.emplace_back();
  overlays.back().z_order = 2;
  overlays.back().layer_id = LayerId(2);

  overlays.emplace_back();
  overlays.back().z_order = 1;
  overlays.back().layer_id = LayerId(1);

  EXPECT_DEATH(tree_.UpdateTree(overlays), kZOrderCheckMessage);
}

TEST_P(OsCompositorTreeBaseTestInvalidInput, UnsortedSiblingsInLayer) {
  std::vector<TestOverlayParams> overlays;

  overlays.emplace_back();
  overlays.back().z_order = 1;
  overlays.back().layer_id = LayerId(1);

  overlays.emplace_back();
  overlays.back().z_order = 2;
  overlays.back().layer_id = LayerId(2);
  overlays.back().parent_layer_id = LayerId(1);

  overlays.emplace_back();
  overlays.back().z_order = 1;
  overlays.back().layer_id = LayerId(3);
  overlays.back().parent_layer_id = LayerId(1);

  EXPECT_DEATH(tree_.UpdateTree(overlays), kZOrderCheckMessage);
}

TEST_P(OsCompositorTreeBaseTestInvalidInput, NoSameZOrder) {
  std::vector<TestOverlayParams> overlays;

  overlays.emplace_back();
  overlays.back().z_order = 1;
  overlays.back().layer_id = LayerId(1);

  overlays.emplace_back();
  overlays.back().z_order = 1;
  overlays.back().layer_id = LayerId(2);

  EXPECT_DEATH(tree_.UpdateTree(overlays), kZOrderCheckMessage);
}

TEST_P(OsCompositorTreeBaseTestInvalidInput, NotTopologicallySorted) {
  std::vector<TestOverlayParams> overlays;

  overlays.emplace_back();
  overlays.back().z_order = 1;
  overlays.back().layer_id = LayerId(2);
  overlays.back().parent_layer_id = LayerId(1),

  overlays.emplace_back();
  overlays.back().z_order = 1;
  overlays.back().layer_id = LayerId(1);

  EXPECT_DEATH(tree_.UpdateTree(overlays), kHierarchyCheckMessage);
}

TEST_P(OsCompositorTreeBaseTestInvalidInput, NeedsExplicitLayerId) {
  std::vector<TestOverlayParams> overlays;

  overlays.emplace_back();
  overlays.back().z_order = 1;

  EXPECT_DEATH(tree_.UpdateTree(overlays), kMustHaveLayerIdMessage);
}

TEST_P(OsCompositorTreeBaseTestInvalidInput, NoDuplicateLayerId) {
  std::vector<TestOverlayParams> overlays;

  overlays.emplace_back();
  overlays.back().z_order = 1;
  overlays.back().layer_id = LayerId(1);

  overlays.emplace_back();
  overlays.back().z_order = 2;
  overlays.back().layer_id = LayerId(1);

  EXPECT_DEATH(tree_.UpdateTree(overlays), kLayerIdUniquenessCheckMessage);
}
#endif

class MockTestCompositorTree : public TestCompositorTree {
 public:
  MockTestCompositorTree()
      : TestCompositorTree(TestCompositorTree::UpdateMode::kIncremental) {}

  MOCK_METHOD(bool, CommitTree, (), (override));
};

TEST(OsCompositorTreeBaseTestCommit, NoUpdatesSkipCommit) {
  MockTestCompositorTree tree;
  EXPECT_CALL(tree, CommitTree()).Times(0);
  EXPECT_TRUE(tree.UpdateTree({}));
}

TEST(OsCompositorTreeBaseTestCommit, Commit) {
  MockTestCompositorTree tree;
  EXPECT_CALL(tree, CommitTree()).WillOnce(testing::Return(true));
  const auto overlays = TestTreeBuilder::OverlaysFromRootLayers({
      Layer(1, {}),
  });
  EXPECT_TRUE(tree.UpdateTree(overlays));
}

TEST(OsCompositorTreeBaseTestCommit, NoUpdatesOnSubsequentFrameSkipCommit) {
  MockTestCompositorTree tree;
  {
    EXPECT_CALL(tree, CommitTree()).WillOnce(testing::Return(true));
    const auto overlays = TestTreeBuilder::OverlaysFromRootLayers({
        Layer(1, {}),
    });
    EXPECT_TRUE(tree.UpdateTree(overlays));
  }

  {
    EXPECT_CALL(tree, CommitTree()).Times(0);
    const auto overlays = TestTreeBuilder::OverlaysFromRootLayers({
        Layer(1, {}),
    });
    EXPECT_TRUE(tree.UpdateTree(overlays));
  }
}

TEST(OsCompositorTreeBaseTestCommit, FailedCommit) {
  MockTestCompositorTree tree;
  EXPECT_CALL(tree, CommitTree()).WillOnce(testing::Return(false));
  const auto overlays = TestTreeBuilder::OverlaysFromRootLayers({
      Layer(1, {}),
  });
  EXPECT_FALSE(tree.UpdateTree(overlays));
}

}  // namespace
}  // namespace gl
