// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BINARY_SIZE_LIBSUPERSIZE_VIEWER_CASPIAN_TREE_BUILDER_H_
#define TOOLS_BINARY_SIZE_LIBSUPERSIZE_VIEWER_CASPIAN_TREE_BUILDER_H_

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "tools/binary_size/libsupersize/viewer/caspian/lens.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

namespace caspian {
class TreeBuilder {
 public:
  using FilterFunc = std::function<bool(const GroupedPath&, const BaseSymbol&)>;

  explicit TreeBuilder(SizeInfo* size_info);
  explicit TreeBuilder(DeltaSizeInfo* size_info);
  ~TreeBuilder();
  void Build(std::unique_ptr<BaseLens> lens,
             char separator,
             bool method_count_mode,
             std::vector<FilterFunc> filters,
             bool relocations_mode,
             int minimum_size);
  TreeNode* Find(std::string_view path);
  Json::Value Open(const char* path);
  Json::Value GetAncestryById(uint32_t id);
  std::string DumpCsv() const;

 private:
  struct SmallDiffAggregate {
    const Container* container = nullptr;
    double size = 0.0;
    double before_size = 0.0;
    double padding = 0.0;
  };
  // Keyed by (lens group, section) so skipped deltas stay attributed to their
  // active lens group (e.g. container, component, generated type, or template).
  using SmallDiffMap =
      std::map<std::pair<std::string_view, SectionId>, SmallDiffAggregate>;

  void AddFileEntry(const GroupedPath& source_path,
                    const std::vector<const BaseSymbol*>& symbols,
                    SmallDiffMap* small_diffs);

  void AddSmallDiffAggregate(std::string_view group,
                             SectionId section,
                             const SmallDiffAggregate& agg);

  TreeNode* GetOrMakeFileNode(const GroupedPath& grouped_path,
                              const NodeStats& node_stats);

  TreeNode* GetOrMakeParentNode(TreeNode* child_node);

  void AttachToParent(TreeNode* child, TreeNode* parent);

  TreeNode* FindNodeById(int32_t id);

  ArtifactType ArtifactTypeFromChild(const GroupedPath& child_path) const;

  bool ShouldIncludeSymbol(const GroupedPath& id_path,
                           const BaseSymbol& symbol) const;

  // Merges dex method symbols into containers based on the class of the dex
  // method.
  void JoinDexMethodClasses(TreeNode* node);

  BaseSizeInfo* size_info_;
  bool diff_mode_;
  TreeNodeFactory tree_node_factory_;
  TreeNode* root_;
  absl::flat_hash_map<GroupedPath, TreeNode*> _parents;

  // Contained TreeNode hold lightweight string_views to fields in SizeInfo.
  // If grouping by component, this isn't possible: TreeNode id_paths are not
  // substrings of SizeInfo-owned strings. In that case, the strings are stored
  // in |owned_strings_|.
  // Deque is used for stability, to support string_view.
  std::deque<std::string> owned_strings_;
  // Synthetic symbols created for aggregated small symbol diffs. Deques are
  // used for pointer stability since DeltaSymbol and TreeNode::symbol retain
  // raw pointers into these containers.
  std::deque<Symbol> owned_symbols_;
  std::deque<DeltaSymbol> owned_delta_symbols_;
  std::unique_ptr<BaseLens> lens_;
  bool method_count_mode_;
  bool relocations_mode_ = false;
  int minimum_size_ = 0;
  // The current path separator: '>' if grouping by component, '/' otherwise.
  // Note that we split paths on '/' no matter the value of separator, since
  // when grouping by component, paths look like Component>path/to/file.
  char sep_;
  std::vector<FilterFunc> filters_;
  std::vector<const BaseSymbol*> symbols_;
  std::vector<const BaseSymbol*> filtered_symbols_;
  absl::flat_hash_map<std::string_view, DiffStatus> source_to_diff_status_;
};  // TreeBuilder
}  // namespace caspian
#endif  // TOOLS_BINARY_SIZE_LIBSUPERSIZE_VIEWER_CASPIAN_TREE_BUILDER_H_
