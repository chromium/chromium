// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/tree_builder.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <stack>
#include <utility>

namespace caspian {

namespace {
/** Name used by a directory created to hold symbols with no name. */
constexpr const char kComponentSep = '>';
constexpr const char kPathSep = '/';
}  // namespace

TreeBuilder::TreeBuilder(SizeInfo* size_info)
    : diff_mode_(false),
      root_(tree_node_factory_.Make(ArtifactType::kDirectory)) {
  symbols_.reserve(size_info->raw_symbols.size());
  for (const Symbol& sym : size_info->raw_symbols) {
    symbols_.push_back(&sym);
  }
  size_info_ = size_info;
}

TreeBuilder::TreeBuilder(DeltaSizeInfo* size_info)
    : diff_mode_(true),
      root_(tree_node_factory_.Make(ArtifactType::kDirectory)) {
  symbols_.reserve(size_info->delta_symbols.size());
  for (const DeltaSymbol& sym : size_info->delta_symbols) {
    symbols_.push_back(&sym);
  }
  if (size_info->removed_sources) {
    for (const auto& s : *size_info->removed_sources) {
      source_to_diff_status_[s] = DiffStatus::kRemoved;
    }
  }
  if (size_info->added_sources) {
    for (const auto& s : *size_info->added_sources) {
      source_to_diff_status_[s] = DiffStatus::kAdded;
    }
  }
  size_info_ = size_info;
}

TreeBuilder::~TreeBuilder() = default;

void TreeBuilder::Build(std::unique_ptr<BaseLens> lens,
                        char separator,
                        bool method_count_mode,
                        std::vector<FilterFunc> filters) {
  lens_ = std::move(lens);
  method_count_mode_ = method_count_mode;
  filters_ = filters;
  sep_ = separator;

  // Initialize tree root.
  root_->id_path = GroupedPath{"", ""};
  _parents[root_->id_path] = root_.get();

  std::unordered_map<GroupedPath, std::vector<const BaseSymbol*>>
      symbols_by_grouped_path;
  for (const BaseSymbol* sym : symbols_) {
    const char* path = sym->GroupingPath();
    GroupedPath key = GroupedPath{lens_->ParentName(*sym), path};
    if (ShouldIncludeSymbol(key, *sym)) {
      symbols_by_grouped_path[key].push_back(sym);
    }
  }
  for (const auto& pair : symbols_by_grouped_path) {
    AddFileEntry(pair.first, pair.second);
  }
}

namespace {
bool CompareAbsSize(const TreeNode* const& l, const TreeNode* const& r) {
  // Sort nodes by size in descending order.
  // Sort nodes with same size in alphabetically ascending order.
  float l_size = abs(l->size);
  float r_size = abs(r->size);
  return (l_size != r_size) ? l_size > r_size : l->id_path < r->id_path;
}

bool CompareCount(const TreeNode* const& l, const TreeNode* const& r) {
  // Sort nodes by size in descending order.
  // Sort nodes with same count in alphabetically ascending order.
  // Particularly relevant for method count mode, where we get a lot of dex
  // symbols with exactly the same size.
  int32_t l_count = l->node_stats.SumCount();
  int32_t r_count = r->node_stats.SumCount();
  return (l_count != r_count) ? l_count > r_count : l->id_path < r->id_path;
}
}  // namespace

TreeNode* TreeBuilder::Find(std::string_view path) {
  std::vector<std::string_view> id_paths;
  while (!path.empty()) {
    size_t idx = (sep_ == kComponentSep) ? path.find_first_of("/>")
                                         : path.find_first_of(kPathSep);

    id_paths.push_back(path.substr(0, idx));
    if (idx == std::string_view::npos) {
      break;
    }
    path = path.substr(idx + 1);
  }

  TreeNode* node = root_.get();
  for (std::string_view id_path : id_paths) {
    TreeNode* old_node = node;
    node = nullptr;
    for (auto* child : old_node->children) {
      if (child->id_path.ShortName(sep_) == id_path) {
        node = child;
        break;
      }
    }
    if (node == nullptr) {
      return nullptr;
    }
  }
  return node;
}

Json::Value TreeBuilder::Open(const char* path) {
  // Returns a string that can be parsed to a JS object.
  static std::string result;

  TreeNode::CompareFunc node_sort_func =
      method_count_mode_ ? &CompareCount : &CompareAbsSize;

  TreeNode* node = Find(path);
  if (node == nullptr) {
    std::cerr << "Tried to open nonexistent node with path: " << path
              << std::endl;
    exit(1);
  }

  JsonWriteOptions opts = {
      .is_sparse = size_info_->IsSparse(),
      .diff_mode = diff_mode_,
      .method_count_mode = method_count_mode_,
  };
  Json::Value v;
  node->WriteIntoJson(opts, node_sort_func, 1, &v);
  return v;
}

Json::Value TreeBuilder::GetAncestryById(uint32_t id) {
  Json::Value v(Json::arrayValue);
  TreeNode* node = FindNodeById(id);
  if (node) {
    int end = 0;  // >= 0 always in the loops below.
    for (TreeNode* cur = node; cur; cur = cur->parent) {
      ++end;
    }
    for (TreeNode* cur = node; cur; cur = cur->parent) {
      v[--end] = cur->id;
    }
  }
  return v;
}

void TreeBuilder::AddFileEntry(GroupedPath grouped_path,
                               const std::vector<const BaseSymbol*>& symbols) {
  // Creates a single file node with a child for each symbol in that file.

  // In legacy .size files, unattributed .dex symbols symbols aggregated and
  // attributed to a path which is actually a directory. Therefore it's
  // possible that a TreeNode has already been created for |grouped_path|. This
  // is made slightly more complicated by the fact that _parents[""] is root,
  // but we do want to create a a new (No path) file entry.

  std::vector<TreeNode*> symbol_nodes;
  // Create symbol nodes.
  NodeStats node_stats;
  for (const BaseSymbol* sym : symbols) {
    if (sym->Pss() == 0.0f) {
      // Even though unchanged symbols aren't displayed in the viewer, we need
      // to aggregate counts of all symbol types in |node_stats.count| to know
      // if all child symbols of a node have been added or removed, which is
      // true if |count| == |added| or |count| == |removed|.
      node_stats += NodeStats(*sym);
      continue;
    }
    TreeNode* symbol_node = tree_node_factory_.Make(ArtifactType::kSymbol);
    symbol_node->id_path =
        GroupedPath{"", sym->IsDex() ? sym->TemplateName() : sym->FullName()};
    symbol_node->size = sym->Pss();
    symbol_node->padding = sym->PaddingPss();
    symbol_node->address = sym->Address();
    symbol_node->node_stats = NodeStats(*sym);
    symbol_node->symbol = sym;
    if (diff_mode_) {
      symbol_node->before_size = sym->BeforePss();
    }
    symbol_nodes.push_back(symbol_node);
  }

  if (symbol_nodes.empty()) {
    return;
  }

  TreeNode* file_node = _parents[grouped_path];
  if (file_node == nullptr || grouped_path.path.empty()) {
    file_node = tree_node_factory_.Make(ArtifactType::kFile);
    file_node->id_path = grouped_path;
    file_node->short_name_index =
        file_node->id_path.size() - file_node->id_path.ShortName(sep_).size();
    _parents[file_node->id_path] = file_node;
    file_node->node_stats = node_stats;
    auto it = source_to_diff_status_.find(grouped_path.path);
    std::string p(grouped_path.path);
    if (it != source_to_diff_status_.end()) {
      file_node->node_stats.imposed_diff_status = it->second;
    }
  }

  for (TreeNode* symbol_node : symbol_nodes) {
    AttachToParent(symbol_node, file_node);
  }

  TreeNode* orphan_node = file_node;
  while (orphan_node != root_.get()) {
    orphan_node = GetOrMakeParentNode(orphan_node);
  }

  JoinDexMethodClasses(file_node);
}

TreeNode* TreeBuilder::GetOrMakeParentNode(TreeNode* child_node) {
  GroupedPath parent_path = child_node->id_path.Parent(sep_);

  TreeNode*& parent = _parents[parent_path];
  if (parent == nullptr) {
    parent =
        tree_node_factory_.Make(ArtifactTypeFromChild(child_node->id_path));
    parent->id_path = parent_path;
    parent->short_name_index =
        parent->id_path.size() - parent->id_path.ShortName(sep_).size();
  }
  if (child_node->parent != parent) {
    AttachToParent(child_node, parent);
  }
  return parent;
}

void TreeBuilder::AttachToParent(TreeNode* child, TreeNode* parent) {
  if (child->parent != nullptr) {
    std::cerr << "Child " << child->id_path << " already attached to parent "
              << child->parent->id_path << std::endl;
    std::cerr << "Cannot be attached to " << parent->id_path << std::endl;
    std::cerr << child->parent << " " << parent << std::endl;
    exit(1);
  }

  parent->children.push_back(child);
  child->parent = parent;

  // Update size information along tree.
  TreeNode* node = child;
  while (node->parent) {
    node->parent->size += child->size;
    if (diff_mode_) {
      node->parent->before_size += child->before_size;
    }
    node->parent->padding += child->padding;
    node->parent->node_stats += child->node_stats;
    node = node->parent;
  }
}

TreeNode* TreeBuilder::FindNodeById(int32_t id) {
  // Recursion-free preorder tree traversal using a stack.
  struct Frame {
    Frame(TreeNode* node_in, std::vector<TreeNode*>::const_iterator it_in)
        : node(node_in), it(it_in) {}
    TreeNode* node;
    std::vector<TreeNode*>::const_iterator it;
  };
  if (root_->id == id) {
    return root_.get();
  }
  std::stack<Frame> st;
  st.emplace(root_.get(), root_->children.begin());
  while (!st.empty()) {
    Frame& fr = st.top();
    if (fr.it == fr.node->children.end()) {
      st.pop();
    } else {
      TreeNode* child = *(fr.it++);
      if (child->id == id) {
        return child;
      }
      st.emplace(child, child->children.begin());
    }
  }
  return nullptr;
}

ArtifactType TreeBuilder::ArtifactTypeFromChild(GroupedPath child_path) const {
  // When grouping by component, id paths use '>' separators for components and
  // '/' separators for the file tree - e.g. Blink>third_party/blink/common...
  // We know that Blink is a component because its children have the form
  // Blink>third_party rather than Blink/third_party.
  return child_path.IsTopLevelPath() ? ArtifactType::kGroup
                                     : ArtifactType::kDirectory;
}

bool TreeBuilder::ShouldIncludeSymbol(const GroupedPath& id_path,
                                      const BaseSymbol& symbol) const {
  for (const auto& filter : filters_) {
    if (!filter(id_path, symbol)) {
      return false;
    }
  }
  return true;
}

void TreeBuilder::JoinDexMethodClasses(TreeNode* node) {
  const bool is_file_node = node->artifact_type == ArtifactType::kFile;
  const bool has_dex =
      node->node_stats.child_stats.count(SectionId::kDex) ||
      node->node_stats.child_stats.count(SectionId::kDexMethod);
  if (!is_file_node || !has_dex || node->children.empty()) {
    return;
  }

  std::map<std::string_view, TreeNode*> java_class_containers;
  std::vector<TreeNode*> other_symbols;

  // Bucket dex symbols by their class.
  for (TreeNode* child : node->children) {
    bool is_string_literal = child->id_path.path.starts_with("\"");
    const size_t split_index = is_string_literal
                                   ? std::string_view::npos
                                   : child->id_path.path.find_first_of('#');
    // No return type / field type means it's a class node.
    const bool is_class_node =
        !is_string_literal &&
        (child->id_path.path.find_first_of(' ', child->short_name_index) ==
         std::string_view::npos);
    const bool has_class_prefix =
        is_class_node || split_index != std::string_view::npos;

    const SectionId section =
        child->symbol ? child->symbol->Section() : SectionId::kNone;
    if (has_class_prefix &&
        (section == SectionId::kDex || section == SectionId::kDexMethod)) {
      const std::string_view class_id_path =
          child->id_path.path.substr(0, split_index);

      // Strip package from the node name for classes in .java files since the
      // directory tree already shows it.
      int short_name_index = child->short_name_index;
      size_t java_idx = node->id_path.path.find(".java");
      if (java_idx != std::string_view::npos) {
        size_t dot_idx = class_id_path.find_last_of('.');
        short_name_index += dot_idx + 1;
      }

      TreeNode*& class_node = java_class_containers[class_id_path];
      if (class_node == nullptr) {
        // We have to construct the class node id_path, because parent nodes
        // need to have an id_path that describes how to reach them from root.
        // Symbol (leaf) nodes typically store their full name in the id_path,
        // which for class nodes would be as "org.x.y.ClassName" even if that
        // node's parent is the file "a/b/c". So if we want an id_path of the
        // form "a/b/c/ClassName$0", we have to create it.
        class_node = tree_node_factory_.Make(ArtifactType::kJavaClass);
        owned_strings_.push_back(std::string(node->id_path.path) + "/" +
                                 std::string(class_id_path));
        class_node->id_path =
            GroupedPath{node->id_path.group, owned_strings_.back()};
        class_node->short_name_index =
            short_name_index + node->id_path.size() + 1;
        class_node->src_path = node->src_path;
        class_node->component = node->component;
        _parents[class_node->id_path] = class_node;
      }

      // Adjust the dex method's short name so it starts after the " "
      if (split_index != std::string_view::npos) {
        child->short_name_index = split_index + 1;
      }
      child->parent = nullptr;
      AttachToParent(child, class_node);
    } else {
      other_symbols.push_back(child);
    }
  }

  node->children = other_symbols;
  for (auto& iter : java_class_containers) {
    TreeNode* container_node = iter.second;
    // Delay setting the parent until here so that `_attachToParent`
    // doesn't add method stats twice
    container_node->parent = node;
    node->children.push_back(container_node);
  }
}

}  // namespace caspian
