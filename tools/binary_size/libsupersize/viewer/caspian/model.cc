// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <list>
#include <sstream>
#include <tuple>

#include "tools/binary_size/libsupersize/viewer/caspian/file_format.h"
#include "tools/binary_size/libsupersize/viewer/caspian/function_signature.h"

namespace caspian {

namespace {
constexpr const char kNoPath[] = "(No path)";
constexpr size_t kNoArenaSlot = static_cast<size_t>(-1);
constinit thread_local size_t g_thread_arena_slot = kNoArenaSlot;
}  // namespace

ScopedOwnedStringsArena::ScopedOwnedStringsArena(size_t slot)
    : prev_slot_(g_thread_arena_slot) {
  g_thread_arena_slot = slot;
}

ScopedOwnedStringsArena::~ScopedOwnedStringsArena() {
  g_thread_arena_slot = prev_slot_;
}

Container::Container(const std::string& name_in) : name(name_in) {}
Container::~Container() = default;
Container::Container(const Container& other) = default;

// static
void Container::AssignShortNames(std::vector<Container>* containers) {
  for (size_t i = 0; i < containers->size(); ++i) {
    Container& c = (*containers)[i];
    std::ostringstream oss;
    if (!c.name.empty()) {
      oss << i;
    }
    c.short_name = oss.str();
  }
}

BaseSymbol::~BaseSymbol() = default;

Symbol::Symbol() = default;
Symbol::~Symbol() = default;
Symbol::Symbol(const Symbol& other) = default;

void Symbol::DeriveNames() const {
  if (name_.data() != nullptr) {
    return;
  }
  if (IsPak()) {
    // full_name: "about_ui_resources.grdp: IDR_ABOUT_UI_CREDITS_HTML".
    size_t space_idx = full_name_.rfind(' ');
    template_name_ = full_name_.substr(space_idx + 1);
    name_ = template_name_;
  } else if ((!full_name_.empty() && full_name_[0] == '*') || IsOverhead() ||
             IsOther()) {
    template_name_ = full_name_;
    name_ = full_name_;
  } else if (IsStringLiteral()) {
    template_name_ = full_name_;
    name_ = full_name_;
  } else if (IsDex() || IsNative()) {
    std::deque<std::string>* owned_strings =
        g_thread_arena_slot < size_info_->thread_owned_strings.size()
            ? &size_info_->thread_owned_strings[g_thread_arena_slot]
            : &size_info_->owned_strings;
    std::tuple<std::string_view, std::string_view, std::string_view>
        parsed_names = IsDex() ? ParseJava(full_name_, owned_strings)
                               : ParseCpp(full_name_, owned_strings);
    template_name_ = std::get<1>(parsed_names);
    name_ = std::get<2>(parsed_names);
  } else {
    template_name_ = full_name_;
    name_ = full_name_;
  }
}

int32_t Symbol::Size() const {
  return size_;
}
int32_t Symbol::Padding() const {
  return padding_;
}
int32_t Symbol::Address() const {
  return address_;
}
int32_t Symbol::Flags() const {
  return flags_;
}

std::string_view Symbol::FullName() const {
  return full_name_;
}
// Derived from |full_name|. Generated lazily and cached.
std::string_view Symbol::TemplateName() const {
  DeriveNames();
  return template_name_;
}
std::string_view Symbol::Name() const {
  DeriveNames();
  return name_;
}
const std::vector<Symbol*>* Symbol::Aliases() const {
  return aliases_;
}
SectionId Symbol::Section() const {
  return section_id_;
}

std::string_view Symbol::ContainerName() const {
  return container_ ? container_->name : std::string_view();
}
const char* Symbol::ObjectPath() const {
  return object_path_;
}
const char* Symbol::SourcePath() const {
  return source_path_;
}
const char* Symbol::GroupingPath() const {
  if (source_path_ && *source_path_) {
    return source_path_;
  }
  if (object_path_ && *object_path_) {
    return object_path_;
  }
  return kNoPath;
}
const char* Symbol::SectionName() const {
  return section_name_;
}
const char* Symbol::Component() const {
  return component_;
}
std::string* Symbol::Disassembly() const {
  return disassembly_;
}

float Symbol::Pss() const {
  return static_cast<float>(Size()) / NumAliases();
}

float Symbol::PssWithoutPadding() const {
  return Pss() - PaddingPss();
}

float Symbol::PaddingPss() const {
  return static_cast<float>(Padding()) / NumAliases();
}

float Symbol::BeforePss() const {
  // This function should only used for diff mode.
  assert(false);
  return 0.0f;
}

DiffStatus Symbol::GetDiffStatus() const {
  return DiffStatus::kUnchanged;
}

// delta symbol

DeltaSymbol::DeltaSymbol(const Symbol* before, const Symbol* after)
    : before_(before), after_(after) {
  if (!before_ && !after_) {
    exit(1);
  }
}

DeltaSymbol::~DeltaSymbol() = default;

int32_t DeltaSymbol::Size() const {
  if (!after_) {
    return -before_->Size();
  }
  if (!before_) {
    return after_->Size();
  }
  // Padding tracked in aggregate, except for padding-only symbols.
  if (before_->SizeWithoutPadding() == 0) {
    return after_->Padding() - before_->Padding();
  }
  return after_->SizeWithoutPadding() - before_->SizeWithoutPadding();
}

int32_t DeltaSymbol::Padding() const {
  if (!after_) {
    return -before_->Padding();
  }
  if (!before_) {
    return after_->Padding();
  }
  // Padding tracked in aggregate, except for padding-only symbols.
  if (before_->SizeWithoutPadding() == 0) {
    return after_->Padding() - before_->Padding();
  }
  return 0;
}

int32_t DeltaSymbol::Address() const {
  if (after_) {
    return after_->Address();
  }
  return 0;
}

int32_t DeltaSymbol::Flags() const {
  // Compute the union of flags (|) instead of symmetric difference (^), as
  // that is more useful when querying for symbols with flags.
  int32_t before_flags = before_ ? before_->Flags() : 0;
  int32_t after_flags = after_ ? after_->Flags() : 0;
  return before_flags | after_flags;
}

std::string_view DeltaSymbol::FullName() const {
  return (after_ ? after_ : before_)->FullName();
}

// Derived from |full_name|. Generated lazily and cached.
std::string_view DeltaSymbol::TemplateName() const {
  return (after_ ? after_ : before_)->TemplateName();
}

std::string_view DeltaSymbol::Name() const {
  return (after_ ? after_ : before_)->Name();
}

const std::vector<Symbol*>* DeltaSymbol::Aliases() const {
  return nullptr;
}

SectionId DeltaSymbol::Section() const {
  return (after_ ? after_ : before_)->Section();
}

std::string_view DeltaSymbol::ContainerName() const {
  return (after_ ? after_ : before_)->ContainerName();
}

const char* DeltaSymbol::ObjectPath() const {
  return (after_ ? after_ : before_)->ObjectPath();
}

const char* DeltaSymbol::SourcePath() const {
  return (after_ ? after_ : before_)->SourcePath();
}

const char* DeltaSymbol::GroupingPath() const {
  return (after_ ? after_ : before_)->GroupingPath();
}

const char* DeltaSymbol::SectionName() const {
  return (after_ ? after_ : before_)->SectionName();
}

const char* DeltaSymbol::Component() const {
  return (after_ ? after_ : before_)->Component();
}

std::string* DeltaSymbol::Disassembly() const {
  return (after_ ? after_ : before_)->Disassembly();
}

float DeltaSymbol::Pss() const {
  if (!after_) {
    return -before_->Pss();
  }
  if (!before_) {
    return after_->Pss();
  }
  // Padding tracked in aggregate, except for padding-only symbols.
  if (before_->SizeWithoutPadding() == 0) {
    return after_->Pss() - before_->Pss();
  }
  return after_->PssWithoutPadding() - before_->PssWithoutPadding();
}

float DeltaSymbol::PssWithoutPadding() const {
  return Pss() - PaddingPss();
}

float DeltaSymbol::PaddingPss() const {
  if (!after_) {
    return -before_->PaddingPss();
  }
  if (!before_) {
    return after_->PaddingPss();
  }
  // Padding tracked in aggregate, except for padding-only symbols.
  if (before_->SizeWithoutPadding() == 0) {
    return after_->PaddingPss() - before_->PaddingPss();
  }
  return 0;
}

float DeltaSymbol::BeforePss() const {
  return before_ ? before_->Pss() : 0.0f;
}

DiffStatus DeltaSymbol::GetDiffStatus() const {
  if (!before_) {
    return DiffStatus::kAdded;
  }
  if (!after_) {
    return DiffStatus::kRemoved;
  }
  if (Size() || Pss() != 0) {
    return DiffStatus::kChanged;
  }
  return DiffStatus::kUnchanged;
}

TreeNode::TreeNode(ArtifactType artifact_type_in, int32_t id_in)
    : artifact_type(artifact_type_in), id(id_in) {}

TreeNode::TreeNode(TreeNode&&) = default;

TreeNode::~TreeNode() = default;

std::ostream& operator<<(std::ostream& os, const Symbol& sym) {
  return os << "Symbol(full_name=" << sym.FullName()
            << ", section=" << static_cast<char>(sym.section_id_)
            << ", section=" << sym.section_name_ << ", address=" << sym.address_
            << ", size=" << sym.size_ << ", flags=" << sym.flags_
            << ", padding=" << sym.padding_ << ")";
}

BaseSizeInfo::BaseSizeInfo() = default;
BaseSizeInfo::BaseSizeInfo(const BaseSizeInfo&) = default;
BaseSizeInfo::~BaseSizeInfo() = default;

SectionId BaseSizeInfo::ShortSectionName(const char* section_name) {
  if (!strcmp(section_name, ".text")) {
    return SectionId::kText;
  }
  if (!strcmp(section_name, ".dex")) {
    return SectionId::kDex;
  }
  if (!strcmp(section_name, ".dex.method")) {
    return SectionId::kDexMethod;
  }
  if (!strcmp(section_name, ".native")) {
    // For simplicity, merge .native into .rodata.
    // This contains things like ** section .shstrtab.
    return SectionId::kRoData;
  }
  if (!strcmp(section_name, ".other")) {
    return SectionId::kOther;
  }
  if (!strcmp(section_name, ".rodata")) {
    return SectionId::kRoData;
  }
  if (!strcmp(section_name, ".data")) {
    return SectionId::kData;
  }
  if (!strcmp(section_name, ".tdata")) {
    return SectionId::kData;
  }
  if (!strcmp(section_name, ".data.rel.ro")) {
    return SectionId::kDataRelRo;
  }
  if (!strcmp(section_name, ".bss")) {
    return SectionId::kBss;
  }
  if (!strcmp(section_name, ".bss.rel.ro")) {
    return SectionId::kBss;
  }
  if (!strcmp(section_name, ".part.end")) {
    return SectionId::kBss;
  }
  if (!strcmp(section_name, ".relro_padding")) {
    return SectionId::kBss;
  }
  if (!strcmp(section_name, ".tbss")) {
    return SectionId::kBss;
  }
  if (!strcmp(section_name, ".pak.nontranslated")) {
    return SectionId::kPakNontranslated;
  }
  if (!strcmp(section_name, ".pak.translations")) {
    return SectionId::kPakTranslations;
  }
  if (!strcmp(section_name, ".arsc")) {
    return SectionId::kArsc;
  }
  std::cerr << "Attributing unrecognized section name to .other: "
            << section_name << std::endl;
  return SectionId::kOther;
}

SizeInfo::SizeInfo() = default;
SizeInfo::~SizeInfo() = default;

bool SizeInfo::IsSparse() const {
  return is_sparse;
}

DeltaSizeInfo::DeltaSizeInfo(const SizeInfo* before_in,
                             const SizeInfo* after_in,
                             const std::vector<std::string>* removed_sources_in,
                             const std::vector<std::string>* added_sources_in)
    : before(before_in),
      after(after_in),
      removed_sources(removed_sources_in),
      added_sources(added_sources_in) {}

DeltaSizeInfo::~DeltaSizeInfo() = default;
DeltaSizeInfo::DeltaSizeInfo(const DeltaSizeInfo&) = default;
DeltaSizeInfo& DeltaSizeInfo::operator=(const DeltaSizeInfo&) = default;

bool DeltaSizeInfo::IsSparse() const {
  return before->IsSparse() && after->IsSparse();
}

void TreeNode::WriteIntoJson(
    const JsonWriteOptions& opts,
    std::function<bool(const TreeNode* const& l, const TreeNode* const& r)>
        compare_func,
    int depth,
    Json::Value* out) {
  (*out)["id"] = id;
  if (symbol) {
    (*out)["container"] = std::string(symbol->ContainerName());
    (*out)["helpme"] = std::string(symbol->Name());
    (*out)["idPath"] = std::string(symbol->TemplateName());
    (*out)["fullName"] = std::string(symbol->FullName());
    if (opts.diff_mode) {
      const auto* delta_sym = static_cast<const DeltaSymbol*>(symbol);
      if (delta_sym->Before() && delta_sym->Before()->NumAliases() > 1) {
        (*out)["beforeNumAliases"] = delta_sym->Before()->NumAliases();
      }
      if (delta_sym->After() && delta_sym->After()->NumAliases() > 1) {
        (*out)["afterNumAliases"] = delta_sym->After()->NumAliases();
      }
    } else {
      if (symbol->NumAliases() > 1) {
        (*out)["numAliases"] = symbol->NumAliases();
      }
    }
    if (symbol->ObjectPath()) {
      (*out)["objPath"] = symbol->ObjectPath();
    }
    if (symbol->SourcePath()) {
      (*out)["srcPath"] = symbol->SourcePath();
    }
    if (symbol->Component()) {
      (*out)["component"] = symbol->Component();
    }
    if (symbol->Disassembly()) {
      (*out)["disassembly"] = *(symbol->Disassembly());
    }
  } else {
    (*out)["idPath"] = id_path.ToString();
    if (opts.is_sparse) {
      if (node_stats.imposed_diff_status != DiffStatus::kUnchanged) {
        (*out)["diffStatus"] =
            static_cast<uint8_t>(node_stats.imposed_diff_status);
      }
    } else if (!children.empty()) {
      // Add tag to containers in which all child symbols were added/removed.
      DiffStatus diff_status = node_stats.GetGlobalDiffStatus();
      if (diff_status != DiffStatus::kUnchanged) {
        (*out)["diffStatus"] = static_cast<uint8_t>(diff_status);
      }
    }
  }
  (*out)["shortNameIndex"] = short_name_index;
  std::string type;
  if (artifact_type != ArtifactType::kSymbol) {
    type += static_cast<char>(artifact_type);
  }
  SectionId biggest_section = node_stats.ComputeBiggestSection();
  type += static_cast<char>(biggest_section);
  (*out)["type"] = type;
  (*out)["size"] = size;
  if (opts.diff_mode) {
    (*out)["beforeSize"] = before_size;
  }
  if (padding) {
    (*out)["padding"] = padding;
  }
  if (address) {
    (*out)["address"] = address;
  }
  (*out)["flags"] = flags;
  node_stats.WriteIntoJson(opts, &(*out)["childStats"]);

  const size_t kMaxChildNodesToExpand = 1000;
  if (children.size() > kMaxChildNodesToExpand) {
    // When the tree is very flat, don't expand child nodes to avoid cost of
    // sending thousands of children and grandchildren to renderer.
    depth = 0;
  }
  if (depth < 0 && children.size() > 1) {
    (*out)["children"] = Json::Value();  // null
  } else {
    (*out)["children"] = Json::Value(Json::arrayValue);
    // Reorder children for output.
    // TODO: Support additional compare functions.
    std::sort(children.begin(), children.end(), compare_func);
    for (unsigned int i = 0; i < children.size(); i++) {
      children[i]->WriteIntoJson(opts, compare_func, depth - 1,
                                 &(*out)["children"][i]);
    }
  }
}

namespace {
Stat& GetOrCreateStat(
    absl::InlinedVector<std::pair<SectionId, Stat>, 2>* child_stats,
    SectionId section) {
  for (auto& pair : *child_stats) {
    if (pair.first == section) {
      return pair.second;
    }
  }
  child_stats->push_back({section, Stat{}});
  return child_stats->back().second;
}
}  // namespace

NodeStats::NodeStats() = default;
NodeStats::NodeStats(const NodeStats&) = default;
NodeStats& NodeStats::operator=(const NodeStats&) = default;
NodeStats::NodeStats(NodeStats&&) = default;
NodeStats& NodeStats::operator=(NodeStats&&) = default;
NodeStats::~NodeStats() = default;

void NodeStats::AddSymbol(const BaseSymbol& symbol) {
  Stat& section_stats = GetOrCreateStat(&child_stats, symbol.Section());
  section_stats.count += 1;
  section_stats.size += symbol.Pss();
  switch (symbol.GetDiffStatus()) {
    case DiffStatus::kUnchanged:
      break;
    case DiffStatus::kAdded:
      section_stats.added += 1;
      break;
    case DiffStatus::kRemoved:
      section_stats.removed += 1;
      break;
    case DiffStatus::kChanged:
      section_stats.changed += 1;
      break;
  }
}

bool NodeStats::HasSection(SectionId section) const {
  for (const auto& pair : child_stats) {
    if (pair.first == section) {
      return true;
    }
  }
  return false;
}

void NodeStats::WriteIntoJson(const JsonWriteOptions& opts,
                              Json::Value* out) const {
  (*out) = Json::Value(Json::objectValue);
  bool is_diff_count = opts.diff_mode && opts.method_count_mode;
  for (const auto& kv : child_stats) {
    const std::string sectionId = std::string(1, static_cast<char>(kv.first));
    const Stat stats = kv.second;
    (*out)[sectionId] = Json::Value(Json::objectValue);
    (*out)[sectionId]["size"] = stats.size;
    (*out)[sectionId]["added"] = stats.added;
    (*out)[sectionId]["removed"] = stats.removed;
    (*out)[sectionId]["changed"] = stats.changed;
    // Count is used to store value for "method count" mode.
    // Why? Because that's how it was implemented in the (now removed) .ndjson
    // worker.
    int count = is_diff_count ? stats.added - stats.removed : stats.count;
    (*out)[sectionId]["count"] = count;
  }
}

NodeStats& NodeStats::operator+=(const NodeStats& other) {
  for (const auto& it : other.child_stats) {
    GetOrCreateStat(&child_stats, it.first) += it.second;
  }
  return *this;
}

SectionId NodeStats::ComputeBiggestSection() const {
  SectionId ret = SectionId::kNone;
  float max = 0.0f;
  for (const auto& pair : child_stats) {
    float abs_size = abs(pair.second.size);
    if (abs_size > max || (abs_size == max && max > 0.0f && pair.first < ret)) {
      ret = pair.first;
      max = abs_size;
    }
  }
  return ret;
}

int32_t NodeStats::SumCount() const {
  int32_t count = 0;
  for (auto& pair : child_stats) {
    count += pair.second.count;
  }
  return count;
}

int32_t NodeStats::SumAdded() const {
  int32_t count = 0;
  for (auto& pair : child_stats) {
    count += pair.second.added;
  }
  return count;
}

int32_t NodeStats::SumRemoved() const {
  int32_t count = 0;
  for (auto& pair : child_stats) {
    count += pair.second.removed;
  }
  return count;
}

DiffStatus NodeStats::GetGlobalDiffStatus() const {
  int32_t count = SumCount();
  if (SumAdded() == count) {
    return DiffStatus::kAdded;
  } else if (SumRemoved() == count) {
    return DiffStatus::kRemoved;
  }
  return DiffStatus::kUnchanged;
}

TreeNodeFactory::TreeNodeFactory() = default;
TreeNodeFactory::~TreeNodeFactory() = default;

TreeNode* TreeNodeFactory::Make(ArtifactType artifact_type) {
  nodes_.push_back(TreeNode(artifact_type, next_id++));
  return &nodes_.back();
}

}  // namespace caspian
