// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/tree_builder.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <stack>
#include <tuple>
#include <utility>

#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace caspian {

namespace {
/** Name used by a directory created to hold symbols with no name. */
constexpr const char kComponentSep = '>';
constexpr const char kPathSep = '/';
// Minimum |delta| to show an individual symbol in diff mode (0.1 bytes, or
// 0.05 in relocations mode to match 1-decimal-place tooltip rounding in
// state.js). Tiny fractional PSS deltas caused by alias count changes in large
// ICF alias groups (e.g. 2/27607 - 2/27606) are aggregated per
// (lens group, section) and only emitted if their combined |delta| reaches this
// threshold.
constexpr float kMinDiffSize = 0.1f;
constexpr float kMinDiffRelocations = 0.05f;
constexpr const char* kSmallDiffsSymbolName =
    "Overhead: aggregate small symbol diffs";

struct GroupedEntry {
  GroupedPath key;
  std::vector<const BaseSymbol*> symbols;
};
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
                        std::vector<FilterFunc> filters,
                        bool relocations_mode,
                        int minimum_size) {
  lens_ = std::move(lens);
  method_count_mode_ = method_count_mode;
  relocations_mode_ = relocations_mode;
  minimum_size_ = minimum_size;
  filters_ = filters;
  sep_ = separator;

  // Initialize tree root.
  root_->id_path = GroupedPath{"", ""};
  _parents[root_->id_path] = root_;
  filtered_symbols_.clear();

  // Group symbols by (group, path) using a two-level lookup:
  // 1. |fast_key_to_entry| keys on the raw (group.data(), group.size(), path)
  //    pointers so that symbols from the same file do not repeatedly call
  //    strlen(path) or hash path strings.
  // 2. |key_to_entry| keys on string content (GroupedPath) so that distinct
  //    char* pointers with identical strings (e.g. "before" vs "after" path
  //    tables in diff mode) resolve to the same GroupedEntry.
  std::vector<GroupedEntry> grouped_entries;
  absl::flat_hash_map<GroupedPath, uint32_t> key_to_entry;
  absl::flat_hash_map<std::tuple<const char*, size_t, const char*>, uint32_t>
      fast_key_to_entry;
  const bool has_filters =
      !filters_.empty() || relocations_mode_ || minimum_size_ > 0;
  for (const BaseSymbol* sym : symbols_) {
    const char* path = sym->GroupingPath();
    std::string_view group = lens_->ParentName(*sym);
    auto [fast_it, fast_inserted] =
        fast_key_to_entry.try_emplace({group.data(), group.size(), path}, 0);
    if (fast_inserted) {
      GroupedPath key{group, path};
      auto [it, inserted] =
          key_to_entry.try_emplace(key, grouped_entries.size());
      if (inserted) {
        grouped_entries.push_back(GroupedEntry{key, {}});
      }
      fast_it->second = it->second;
    }
    GroupedEntry& entry = grouped_entries[fast_it->second];
    if (!has_filters || ShouldIncludeSymbol(entry.key, *sym)) {
      entry.symbols.push_back(sym);
    }
  }
  SmallDiffMap small_diffs;
  for (const GroupedEntry& entry : grouped_entries) {
    if (!entry.symbols.empty()) {
      AddFileEntry(entry.key, entry.symbols, &small_diffs);
    }
  }
  float min_diff_size = relocations_mode_ ? kMinDiffRelocations : kMinDiffSize;
  for (const auto& [key, agg] : small_diffs) {
    if (std::abs(agg.size) >= min_diff_size) {
      AddSmallDiffAggregate(key.first, key.second, agg);
    }
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

  TreeNode* node = root_;
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
  if (node == root_) {
    v["symbolCount"] = static_cast<Json::UInt>(filtered_symbols_.size());
  }
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

namespace {
std::string_view PathForCsv(const BaseSymbol& sym) {
  if (sym.SourcePath() && *sym.SourcePath()) {
    return sym.SourcePath();
  }
  if (sym.ObjectPath() && *sym.ObjectPath()) {
    return sym.ObjectPath();
  }
  return "";
}

void AppendCsvField(std::string* out, std::string_view field) {
  if (field.find_first_of(",\"\r\n") == std::string_view::npos) {
    out->append(field);
    return;
  }
  out->push_back('"');
  for (char c : field) {
    if (c == '"') {
      out->push_back('"');
    }
    out->push_back(c);
  }
  out->push_back('"');
}
}  // namespace

std::string TreeBuilder::DumpCsv() const {
  std::vector<const BaseSymbol*> sorted = filtered_symbols_;
  std::sort(sorted.begin(), sorted.end(),
            [this](const BaseSymbol* a, const BaseSymbol* b) {
              if (!method_count_mode_) {
                float a_size =
                    std::abs(relocations_mode_ ? a->RelocationPss() : a->Pss());
                float b_size =
                    std::abs(relocations_mode_ ? b->RelocationPss() : b->Pss());
                if (a_size != b_size) {
                  return a_size > b_size;
                }
              }
              std::string_view a_path = PathForCsv(*a);
              std::string_view b_path = PathForCsv(*b);
              if (a_path != b_path) {
                return a_path < b_path;
              }
              if (a->FullName() != b->FullName()) {
                return a->FullName() < b->FullName();
              }
              return static_cast<uint32_t>(a->Address()) <
                     static_cast<uint32_t>(b->Address());
            });

  std::string out = diff_mode_ ? "full_name,path," : "address,full_name,path,";
  if (method_count_mode_) {
    out += "method_count";
  } else if (relocations_mode_) {
    out += "relocation_count";
  } else {
    out += "size_without_padding";
  }
  out += ",num_aliases\n";

  for (const BaseSymbol* sym : sorted) {
    if (!diff_mode_) {
      absl::StrAppendFormat(&out, "0x%x,",
                            static_cast<uint32_t>(sym->Address()));
    }
    AppendCsvField(&out, sym->FullName());
    out.push_back(',');
    AppendCsvField(&out, PathForCsv(*sym));

    int32_t metric_value = 0;
    if (method_count_mode_) {
      if (!diff_mode_) {
        metric_value = 1;
      } else {
        DiffStatus status = sym->GetDiffStatus();
        metric_value = status == DiffStatus::kAdded
                           ? 1
                           : (status == DiffStatus::kRemoved ? -1 : 0);
      }
    } else if (relocations_mode_) {
      metric_value = sym->RelocationCount();
    } else {
      metric_value = sym->SizeWithoutPadding();
    }

    int32_t num_aliases = 1;
    if (diff_mode_) {
      const auto* delta = static_cast<const DeltaSymbol*>(sym);
      num_aliases =
          (delta->After() ? delta->After() : delta->Before())->NumAliases();
    } else {
      num_aliases = sym->NumAliases();
    }
    absl::StrAppendFormat(&out, ",%d,%d\n", metric_value, num_aliases);
  }
  return out;
}

void TreeBuilder::AddFileEntry(const GroupedPath& grouped_path,
                               const std::vector<const BaseSymbol*>& symbols,
                               SmallDiffMap* small_diffs) {
  // Creates a single file node with a child for each symbol in that file.
  std::vector<TreeNode*> symbol_nodes;
  // Create symbol nodes.
  NodeStats node_stats;
  float min_diff_size = relocations_mode_ ? kMinDiffRelocations : kMinDiffSize;
  // Track whether any skipped small-diff symbols existed in `before` or
  // `after` so we can keep file-level added/removed diffStatus accurate below.
  bool has_skipped_before = false;
  bool has_skipped_after = false;
  for (const BaseSymbol* sym : symbols) {
    float sym_size = relocations_mode_ ? sym->RelocationPss() : sym->Pss();
    if (sym_size == 0.0f) {
      // Even though unchanged symbols aren't displayed in the viewer, we need
      // to aggregate counts of all symbol types in |node_stats.count| to know
      // if all child symbols of a node have been added or removed, which is
      // true if |count| == |added| or |count| == |removed|.
      node_stats.AddSymbol(*sym, relocations_mode_);
      continue;
    }
    float padding = relocations_mode_ ? 0.0f : sym->PaddingPss();
    const DeltaSymbol* delta =
        diff_mode_ ? static_cast<const DeltaSymbol*>(sym) : nullptr;
    float before_size = 0.0f;
    if (delta && delta->Before()) {
      before_size = relocations_mode_ ? delta->Before()->RelocationPss()
                                      : delta->Before()->Pss();
    }
    // Skip creating individual leaf nodes for tiny diff-mode deltas (e.g.
    // fractional PSS ripple when an alias joins/leaves a large ICF group) so
    // they don't clutter every directory with ~0-byte rows. Method-count mode
    // is excluded because every added/removed method counts as +/-1 regardless
    // of byte size.
    //
    // Skipped deltas are accumulated per (lens group, section) so that:
    // - zero-net alias groups cancel out and emit no overhead node at all, and
    // - non-zero remainders (e.g. asymmetric ICF splits or wholly added/removed
    //   small-alias groups) are emitted as one aggregate overhead symbol,
    //   keeping group, section, and root totals accurate.
    if (delta && !method_count_mode_ && std::abs(sym_size) < min_diff_size) {
      has_skipped_before |= delta->Before() != nullptr;
      has_skipped_after |= delta->After() != nullptr;
      auto [it, inserted] =
          small_diffs->try_emplace({grouped_path.group, sym->Section()});
      SmallDiffAggregate& agg = it->second;
      // Retain the container on the aggregate symbol when all skipped symbols
      // in this bucket share one container (always true under ContainerLens);
      // clear it if multiple containers are pooled under another lens.
      if (inserted) {
        agg.container =
            (delta->After() ? delta->After() : delta->Before())->container_;
      } else if (agg.container && agg.container->name != sym->ContainerName()) {
        agg.container = nullptr;
      }
      agg.size += sym_size;
      agg.before_size += before_size;
      agg.padding += padding;
      continue;
    }
    filtered_symbols_.push_back(sym);
    TreeNode* symbol_node = tree_node_factory_.Make(ArtifactType::kSymbol);
    symbol_node->id_path =
        GroupedPath{"", sym->IsDex() ? sym->TemplateName() : sym->FullName()};
    symbol_node->size = sym_size;
    symbol_node->padding = padding;
    symbol_node->address = sym->Address();
    symbol_node->node_stats.AddSymbol(*sym, relocations_mode_);
    symbol_node->symbol = sym;
    symbol_node->before_size = before_size;
    symbol_nodes.push_back(symbol_node);
  }

  // Omit the file node (and any otherwise-empty parent directories) when all
  // of its symbols were zero-delta or skipped as tiny diffs.
  if (symbol_nodes.empty()) {
    return;
  }

  TreeNode* file_node = GetOrMakeFileNode(grouped_path, node_stats);

  file_node->children.reserve(file_node->children.size() + symbol_nodes.size());
  for (TreeNode* symbol_node : symbol_nodes) {
    AttachToParent(symbol_node, file_node);
  }

  // NodeStats::GetGlobalDiffStatus() treats a node as wholly added when
  // SumAdded() == SumCount() (or wholly removed when SumRemoved() ==
  // SumCount()). If all visible symbols are added (or removed) but we skipped
  // a symbol that existed in `before` (or `after`), bump `count` by 1 so an
  // existing or mixed-polarity file is not misclassified as wholly added or
  // removed. Done before attaching to ancestors so parent directories inherit
  // the adjusted count; guarded by `parent == nullptr` so legacy reused
  // directory nodes aren't mutated after already being attached.
  if (file_node->parent == nullptr &&
      (has_skipped_before || has_skipped_after)) {
    DiffStatus status = file_node->node_stats.GetGlobalDiffStatus();
    if ((status == DiffStatus::kAdded && has_skipped_before) ||
        (status == DiffStatus::kRemoved && has_skipped_after)) {
      file_node->node_stats.child_stats[0].second.count += 1;
    }
  }

  TreeNode* orphan_node = file_node;
  while (orphan_node != root_) {
    orphan_node = GetOrMakeParentNode(orphan_node);
  }

  JoinDexMethodClasses(file_node);
}

void TreeBuilder::AddSmallDiffAggregate(std::string_view group,
                                        SectionId section,
                                        const SmallDiffAggregate& agg) {
  Symbol& sym = owned_symbols_.emplace_back();
  sym.section_id_ = section;
  sym.container_ = agg.container;
  sym.full_name_ = kSmallDiffsSymbolName;
  sym.template_name_ = sym.full_name_;
  sym.name_ = sym.full_name_;
  // DeltaSymbol(&sym, &sym) satisfies WriteIntoJson's diff_mode downcast and
  // gives a zero-delta base symbol; size and child_stats are populated below.
  DeltaSymbol& delta_sym = owned_delta_symbols_.emplace_back(&sym, &sym);

  TreeNode* symbol_node = tree_node_factory_.Make(ArtifactType::kSymbol);
  symbol_node->id_path = GroupedPath{"", delta_sym.FullName()};
  symbol_node->size = static_cast<float>(agg.size);
  symbol_node->before_size = static_cast<float>(agg.before_size);
  symbol_node->padding = static_cast<float>(agg.padding);
  symbol_node->node_stats.child_stats.push_back(
      {section, {.count = 1, .changed = 1, .size = symbol_node->size}});
  symbol_node->symbol = &delta_sym;

  GroupedPath grouped_path{group, delta_sym.GroupingPath()};
  TreeNode* file_node = GetOrMakeFileNode(grouped_path, NodeStats{});
  AttachToParent(symbol_node, file_node);

  TreeNode* orphan_node = file_node;
  while (orphan_node != root_) {
    orphan_node = GetOrMakeParentNode(orphan_node);
  }
}

TreeNode* TreeBuilder::GetOrMakeFileNode(const GroupedPath& grouped_path,
                                         const NodeStats& node_stats) {
  // In legacy .size files, unattributed .dex symbols were aggregated and
  // attributed to a path which is actually a directory. Therefore it's
  // possible that a TreeNode has already been created for |grouped_path|. This
  // is made slightly more complicated by the fact that _parents[""] is root,
  // but we do want to create a new (No path) file entry.
  auto [file_node_it, inserted] = _parents.try_emplace(grouped_path);
  TreeNode* file_node = file_node_it->second;
  if (inserted || file_node == nullptr || grouped_path.path.empty()) {
    file_node = tree_node_factory_.Make(ArtifactType::kFile);
    file_node->id_path = grouped_path;
    file_node->short_name_index =
        file_node->id_path.size() - file_node->id_path.ShortName(sep_).size();
    file_node_it->second = file_node;
    file_node->node_stats = node_stats;
    if (auto it = source_to_diff_status_.find(grouped_path.path);
        it != source_to_diff_status_.end()) {
      file_node->node_stats.imposed_diff_status = it->second;
    }
  }
  return file_node;
}

TreeNode* TreeBuilder::GetOrMakeParentNode(TreeNode* child_node) {
  GroupedPath parent_path = child_node->id_path.Parent(sep_);

  auto [it, inserted] = _parents.try_emplace(parent_path);
  TreeNode* parent = it->second;
  if (inserted || parent == nullptr) {
    parent =
        tree_node_factory_.Make(ArtifactTypeFromChild(child_node->id_path));
    parent->id_path = parent_path;
    parent->short_name_index =
        parent->id_path.size() - parent->id_path.ShortName(sep_).size();
    it->second = parent;
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
    return root_;
  }
  std::stack<Frame> st;
  st.emplace(root_, root_->children.begin());
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

ArtifactType TreeBuilder::ArtifactTypeFromChild(
    const GroupedPath& child_path) const {
  // When grouping by component, id paths use '>' separators for components and
  // '/' separators for the file tree - e.g. Blink>third_party/blink/common...
  // We know that Blink is a component because its children have the form
  // Blink>third_party rather than Blink/third_party.
  return child_path.IsTopLevelPath() ? ArtifactType::kGroup
                                     : ArtifactType::kDirectory;
}

bool TreeBuilder::ShouldIncludeSymbol(const GroupedPath& id_path,
                                      const BaseSymbol& symbol) const {
  if (relocations_mode_) {
    if (!diff_mode_) {
      if (symbol.RelocationCount() == 0) {
        return false;
      }
    } else {
      const auto& delta = static_cast<const DeltaSymbol&>(symbol);
      if ((!delta.Before() || delta.Before()->RelocationCount() == 0) &&
          (!delta.After() || delta.After()->RelocationCount() == 0)) {
        return false;
      }
    }
  }
  if (minimum_size_ > 0) {
    float size = relocations_mode_ ? symbol.RelocationPss() : symbol.Pss();
    if ((diff_mode_ ? std::abs(size) : size) < minimum_size_) {
      return false;
    }
  }
  for (const auto& filter : filters_) {
    if (!filter(id_path, symbol)) {
      return false;
    }
  }
  return true;
}

void TreeBuilder::JoinDexMethodClasses(TreeNode* node) {
  const bool is_file_node = node->artifact_type == ArtifactType::kFile;
  const bool has_dex = node->node_stats.HasSection(SectionId::kDex) ||
                       node->node_stats.HasSection(SectionId::kDexMethod);
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

      auto [it, inserted] = java_class_containers.try_emplace(class_id_path);
      TreeNode*& class_node = it->second;
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
