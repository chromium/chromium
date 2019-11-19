// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/caspian/tree_builder.h"

#include <algorithm>
#include <iostream>
#include <map>

namespace caspian {

namespace {
/** Name used by a directory created to hold symbols with no name. */
constexpr const char* kNoName = "(No path)";
constexpr const char* kNoComponent = "(No component)";
const char kPathSep = '/';
const char kComponentSep = '>';

size_t LastSeparatorIndex(std::string_view str, char sep, char othersep) {
  size_t sep_idx = str.find_last_of(sep);
  size_t path_idx = str.find_last_of(othersep);
  if (sep_idx != std::string_view::npos && path_idx != std::string_view::npos) {
    return std::max(sep_idx, path_idx);
  } else if (sep_idx == std::string_view::npos) {
    return path_idx;
  } else {
    return sep_idx;
  }
}

std::string_view DirName(std::string_view path, char sep, char othersep) {
  size_t sep_idx = LastSeparatorIndex(path, sep, othersep);
  return sep_idx != std::string_view::npos ? path.substr(0, sep_idx)
                                           : std::string_view();
}
}  // namespace

TreeBuilder::TreeBuilder(SizeInfo* size_info) {
  symbols_.reserve(size_info->raw_symbols.size());
  for (const Symbol& sym : size_info->raw_symbols) {
    symbols_.push_back(&sym);
  }
}

TreeBuilder::TreeBuilder(DeltaSizeInfo* size_info) {
  symbols_.reserve(size_info->delta_symbols.size());
  for (const DeltaSymbol& sym : size_info->delta_symbols) {
    symbols_.push_back(&sym);
  }
}

TreeBuilder::~TreeBuilder() = default;

void TreeBuilder::Build(
    bool group_by_component,
    bool method_count_mode,
    std::vector<std::function<bool(const BaseSymbol&)>> filters) {
  group_by_component_ = group_by_component;
  method_count_mode_ = method_count_mode;
  filters_ = filters;
  sep_ = group_by_component ? kComponentSep : kPathSep;

  // Initialize tree root.
  root_.container_type = ContainerType::kDirectory;
  owned_strings_.emplace_back(1, sep_);
  root_.id_path = owned_strings_.back();
  _parents[""] = &root_;

  std::unordered_map<std::string_view, std::vector<const BaseSymbol*>>
      symbols_by_source_path;
  for (const BaseSymbol* sym : symbols_) {
    if (ShouldIncludeSymbol(*sym)) {
      std::string_view key = sym->SourcePath();
      if (key == nullptr) {
        key = sym->ObjectPath();
      }
      symbols_by_source_path[key].push_back(sym);
    }
  }
  for (const auto& pair : symbols_by_source_path) {
    AddFileEntry(pair.first, pair.second);
  }
}

namespace {
bool CompareAbsSize(const TreeNode* const& l, const TreeNode* const& r) {
  float l_size = abs(l->size);
  float r_size = abs(r->size);
  if (l_size == r_size) {
    return l->id_path < r->id_path;
  }
  return abs(l->size) > abs(r->size);
}

bool CompareCount(const TreeNode* const& l, const TreeNode* const& r) {
  int32_t l_count = l->node_stats.SumCount();
  int32_t r_count = r->node_stats.SumCount();
  if (l_count == r_count) {
    return l->id_path < r->id_path;
  }
  return l_count > r_count;
}
}  // namespace

Json::Value TreeBuilder::Open(const char* path) {
  // Returns a string that can be parsed to a JS object.
  static std::string result;
  const auto node = _parents.find(path);

  TreeNode::CompareFunc node_sort_func =
      method_count_mode_ ? &CompareCount : &CompareAbsSize;

  if (node != _parents.end()) {
    Json::Value v;
    node->second->WriteIntoJson(&v, 1, node_sort_func);
    return v;
  } else {
    std::cerr << "Tried to open nonexistent node with path: " << path
              << std::endl;
    exit(1);
  }
}

void TreeBuilder::AddFileEntry(const std::string_view source_path,
                               const std::vector<const BaseSymbol*>& symbols) {
  // Creates a single file node with a child for each symbol in that file.
  TreeNode* file_node = new TreeNode();
  file_node->container_type = ContainerType::kFile;

  if (source_path.empty()) {
    file_node->id_path = kNoName;
  } else {
    file_node->id_path = source_path;
  }
  if (group_by_component_) {
    std::string component;
    if (symbols[0]->Component() && *symbols[0]->Component()) {
      component = symbols[0]->Component();
    } else {
      component = kNoComponent;
    }
    owned_strings_.push_back(component + std::string(1, kComponentSep) +
                             std::string(file_node->id_path));
    file_node->id_path = owned_strings_.back();
  }

  file_node->short_name_index =
      LastSeparatorIndex(file_node->id_path, sep_, kPathSep) + 1;
  _parents[file_node->id_path] = file_node;
  // TODO: Initialize file type, source path, component

  // Create symbol nodes.
  for (const BaseSymbol* sym : symbols) {
    TreeNode* symbol_node = new TreeNode();
    symbol_node->container_type = ContainerType::kSymbol;
    symbol_node->id_path = sym->FullName();
    symbol_node->size = sym->Pss();
    symbol_node->node_stats = NodeStats(sym->Section(), 1, symbol_node->size);
    symbol_node->symbol = sym;
    AttachToParent(symbol_node, file_node);
  }

  // TODO: Only add if there are unfiltered symbols in this file.
  TreeNode* orphan_node = file_node;
  while (orphan_node != &root_) {
    orphan_node = GetOrMakeParentNode(orphan_node);
  }

  JoinDexMethodClasses(file_node);
}

TreeNode* TreeBuilder::GetOrMakeParentNode(TreeNode* child_node) {
  std::string_view parent_path;
  if (child_node->id_path.empty()) {
    parent_path = kNoName;
  } else {
    parent_path = DirName(child_node->id_path, sep_, kPathSep);
  }

  TreeNode*& parent = _parents[parent_path];
  if (parent == nullptr) {
    parent = new TreeNode();
    parent->id_path = parent_path;
    parent->short_name_index =
        LastSeparatorIndex(parent_path, sep_, kPathSep) + 1;
    parent->container_type = ContainerTypeFromChild(child_node->id_path);
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
    exit(1);
  }

  parent->children.push_back(child);
  child->parent = parent;

  // Update size information along tree.
  TreeNode* node = child;
  while (node->parent) {
    node->parent->size += child->size;
    node->parent->node_stats += child->node_stats;
    node = node->parent;
  }
}

ContainerType TreeBuilder::ContainerTypeFromChild(
    std::string_view child_id_path) const {
  // When grouping by component, id paths use '>' separators for components and
  // '/' separators for the file tree - e.g. Blink>third_party/blink/common...
  // We know that Blink is a component because its children have the form
  // Blink>third_party rather than Blink/third_party.
  size_t idx = LastSeparatorIndex(child_id_path, sep_, kPathSep);
  if (idx == std::string_view::npos || child_id_path[idx] == kPathSep) {
    return ContainerType::kDirectory;
  } else {
    return ContainerType::kComponent;
  }
}

bool TreeBuilder::ShouldIncludeSymbol(const BaseSymbol& symbol) const {
  for (const auto& filter : filters_) {
    if (!filter(symbol)) {
      return false;
    }
  }
  return true;
}

void TreeBuilder::JoinDexMethodClasses(TreeNode* node) {
  const bool is_file_node = node->container_type == ContainerType::kFile;
  const bool has_dex =
      node->node_stats.child_stats.count(SectionId::kDex) ||
      node->node_stats.child_stats.count(SectionId::kDexMethod);
  // Don't try to merge dex symbols for catch-all symbols under (No path).
  const bool is_no_path = node->id_path == kNoName;

  if (!is_file_node || !has_dex || is_no_path || node->children.empty()) {
    return;
  }

  std::map<std::string_view, TreeNode*> java_class_containers;
  std::vector<TreeNode*> other_symbols;

  // Bucket dex symbols by their class.
  for (TreeNode* child : node->children) {
    // Unlike in .ndjson fields, Java classes loaded from .size files are just
    // the classname, such as "android.support.v7.widget.toolbar".
    // Method names contain the classname followed by the method definition,
    // like "android.support.v7.widget.toolbar void onMeasure(int, int)".
    const size_t split_index = child->id_path.find_first_of(' ');
    // No return type / field type means it's a class node.
    const bool is_class_node =
        child->id_path.find_first_of(' ', child->short_name_index) ==
        std::string_view::npos;
    const bool has_class_prefix =
        is_class_node || split_index != std::string_view::npos;

    if (has_class_prefix) {
      const std::string_view class_id_path =
          split_index == std::string_view::npos
              ? child->id_path
              : child->id_path.substr(0, split_index);

      // Strip package from the node name for classes in .java files since the
      // directory tree already shows it.
      int short_name_index = child->short_name_index;
      size_t java_idx = node->id_path.find(".java");
      if (java_idx != std::string_view::npos) {
        size_t dot_idx = class_id_path.find_last_of('.');
        short_name_index += dot_idx + 1;
      }

      TreeNode*& class_node = java_class_containers[class_id_path];
      if (class_node == nullptr) {
        class_node = new TreeNode();
        class_node->id_path = class_id_path;
        class_node->src_path = node->src_path;
        class_node->component = node->component;
        class_node->short_name_index = short_name_index;
        class_node->container_type = ContainerType::kJavaClass;
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
