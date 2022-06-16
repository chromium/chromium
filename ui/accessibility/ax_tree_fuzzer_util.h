// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_ACCESSIBILITY_AX_TREE_FUZZER_UTIL_H_
#define UI_ACCESSIBILITY_AX_TREE_FUZZER_UTIL_H_

#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/test_ax_tree_manager.h"

class FuzzerData {
 public:
  FuzzerData(const unsigned char* data, size_t size);
  size_t RemainingBytes();
  unsigned char NextByte();
  const unsigned char* NextBytes(size_t amount);

 private:
  const unsigned char* data_;
  const size_t data_size_;
  size_t data_index_;
};

class AXTreeFuzzerGenerator {
 public:
  AXTreeFuzzerGenerator() = default;
  ~AXTreeFuzzerGenerator() = default;

  ui::AXTree* GetTree();

  void GenerateInitialUpdate(FuzzerData& fuzzer_data,
                             int node_count,
                             ui::AXNodeID& max_node_id);
  void GenerateTreeUpdate(FuzzerData& fuzzer_data);

  // This must be kept in sync with the minimum amount of data needed to create
  // any node. Any optional node data should check to ensure there is space.
  static constexpr size_t kMinimumNewNodeFuzzDataSize = 3;
  static constexpr size_t kMinTextFuzzDataSize = 10;
  static constexpr size_t kMaxTextFuzzDataSize = 200;

  // When creating a node, we allow for the next node to be a sibling of an
  // ancestor, this constant determines the maximum nodes we will pop when
  // building the tree.
  static constexpr size_t kMaxAncestorPopCount = 3;

 private:
  ax::mojom::Role GetInterestingRole(unsigned char byte,
                                     ax::mojom::Role parent_role);
  bool CanHaveChildren(ax::mojom::Role role);
  bool CanHaveText(ax::mojom::Role role);
  std::u16string GenerateInterestingText(const unsigned char* data,
                                         size_t size);
  ui::TestAXTreeManager tree_manager_;
};
#endif  //  UI_ACCESSIBILITY_AX_TREE_FUZZER_UTIL_H_