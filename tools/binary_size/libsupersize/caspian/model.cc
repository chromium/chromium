// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/caspian/model.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <iostream>
#include <list>
#include <string>
#include <tuple>
#include <unordered_map>

#include "tools/binary_size/libsupersize/caspian/file_format.h"
#include "tools/binary_size/libsupersize/caspian/function_signature.h"

namespace caspian {

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
  } else if (IsDex()) {
    std::tuple<std::string_view, std::string_view, std::string_view>
        parsed_names = ParseJava(full_name_, &size_info_->owned_strings);
    template_name_ = std::get<1>(parsed_names);
    name_ = std::get<2>(parsed_names);
  } else if (IsStringLiteral()) {
    template_name_ = full_name_;
    name_ = full_name_;
  } else if (IsNative()) {
    std::tuple<std::string_view, std::string_view, std::string_view>
        parsed_names = ParseCpp(full_name_, &size_info_->owned_strings);
    template_name_ = std::get<1>(parsed_names);
    name_ = std::get<2>(parsed_names);
  } else {
    template_name_ = full_name_;
    name_ = full_name_;
  }
}

int32_t Symbol::Address() const {
  return address_;
}
int32_t Symbol::Size() const {
  return size_;
}
int32_t Symbol::Flags() const {
  return flags_;
}
int32_t Symbol::Padding() const {
  return padding_;
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

const char* Symbol::ObjectPath() const {
  return object_path_;
}
const char* Symbol::SourcePath() const {
  return source_path_;
}
const char* Symbol::SectionName() const {
  return section_name_;
}
const char* Symbol::Component() const {
  return component_;
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

// delta symbol

DeltaSymbol::DeltaSymbol(const Symbol* before, const Symbol* after)
    : before_(before), after_(after) {
  if (!before_ && !after_) {
    exit(1);
  }
}

DeltaSymbol::~DeltaSymbol() = default;

int32_t DeltaSymbol::Address() const {
  if (after_) {
    return after_->Address();
  }
  return 0;
}

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

int32_t DeltaSymbol::Flags() const {
  int32_t before_flags = before_ ? before_->Flags() : 0;
  int32_t after_flags = after_ ? after_->Flags() : 0;
  return before_flags ^ after_flags;
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

const char* DeltaSymbol::ObjectPath() const {
  return (after_ ? after_ : before_)->ObjectPath();
}

const char* DeltaSymbol::SourcePath() const {
  return (after_ ? after_ : before_)->SourcePath();
}

const char* DeltaSymbol::SectionName() const {
  return (after_ ? after_ : before_)->SectionName();
}

const char* DeltaSymbol::Component() const {
  return (after_ ? after_ : before_)->Component();
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

TreeNode::TreeNode() = default;
TreeNode::~TreeNode() {
  // TODO(jaspercb): Could use custom allocator to delete all nodes in one go.
  for (TreeNode* child : children) {
    delete child;
  }
}

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
  static std::map<const char*, SectionId> short_section_name_cache;
  SectionId& ret = short_section_name_cache[section_name];
  if (ret == SectionId::kNone) {
    if (!strcmp(section_name, ".text")) {
      ret = SectionId::kText;
    } else if (!strcmp(section_name, ".dex")) {
      ret = SectionId::kDex;
    } else if (!strcmp(section_name, ".dex.method")) {
      ret = SectionId::kDexMethod;
    } else if (!strcmp(section_name, ".other")) {
      ret = SectionId::kOther;
    } else if (!strcmp(section_name, ".rodata")) {
      ret = SectionId::kRoData;
    } else if (!strcmp(section_name, ".data")) {
      ret = SectionId::kData;
    } else if (!strcmp(section_name, ".data.rel.ro")) {
      ret = SectionId::kDataRelRo;
    } else if (!strcmp(section_name, ".bss")) {
      ret = SectionId::kBss;
    } else if (!strcmp(section_name, ".bss.rel.ro")) {
      ret = SectionId::kBss;
    } else if (!strcmp(section_name, ".pak.nontranslated")) {
      ret = SectionId::kPakNontranslated;
    } else if (!strcmp(section_name, ".pak.translations")) {
      ret = SectionId::kPakTranslations;
    } else {
      std::cerr << "Attributing unrecognized section name to .other: "
                << section_name << std::endl;
      ret = SectionId::kOther;
    }
  }
  return ret;
}

SizeInfo::SizeInfo() = default;
SizeInfo::~SizeInfo() = default;

DeltaSizeInfo::DeltaSizeInfo(const SizeInfo* before, const SizeInfo* after)
    : before(before), after(after) {}

DeltaSizeInfo::~DeltaSizeInfo() = default;
DeltaSizeInfo::DeltaSizeInfo(const DeltaSizeInfo&) = default;
DeltaSizeInfo& DeltaSizeInfo::operator=(const DeltaSizeInfo&) = default;

void TreeNode::WriteIntoJson(
    Json::Value* out,
    int depth,
    std::function<bool(const TreeNode* const& l, const TreeNode* const& r)>
        compare_func) {
  if (symbol) {
    if (symbol->IsDex()) {
      (*out)["idPath"] = std::string(symbol->FullName());
    } else {
      (*out)["idPath"] = std::string(symbol->TemplateName());
      (*out)["fullName"] = std::string(symbol->FullName());
    }
  } else {
    (*out)["idPath"] = std::string(this->id_path);
  }
  (*out)["shortNameIndex"] = this->short_name_index;
  std::string type;
  if (container_type != ContainerType::kSymbol) {
    type += static_cast<char>(container_type);
  }
  SectionId biggest_section = this->node_stats.ComputeBiggestSection();
  type += static_cast<char>(biggest_section);
  (*out)["type"] = type;

  (*out)["size"] = this->size;
  (*out)["flags"] = this->flags;
  this->node_stats.WriteIntoJson(&(*out)["childStats"]);
  if (depth < 0 && this->children.size() > 1) {
    (*out)["children"] = Json::Value();  // null
  } else {
    (*out)["children"] = Json::Value(Json::arrayValue);
    // Reorder children for output.
    // TODO: Support additional compare functions.
    std::sort(this->children.begin(), this->children.end(), compare_func);
    for (unsigned int i = 0; i < this->children.size(); i++) {
      this->children[i]->WriteIntoJson(&(*out)["children"][i], depth - 1,
                                       compare_func);
    }
  }
}

NodeStats::NodeStats() = default;
NodeStats::~NodeStats() = default;

NodeStats::NodeStats(SectionId sectionId,
                     int32_t count,
                     float size) {
  child_stats[sectionId] = {count, size};
}

void NodeStats::WriteIntoJson(Json::Value* out) const {
  (*out) = Json::Value(Json::objectValue);
  for (const auto kv : this->child_stats) {
    const std::string sectionId = std::string(1, static_cast<char>(kv.first));
    const Stat stats = kv.second;
    (*out)[sectionId] = Json::Value(Json::objectValue);
    (*out)[sectionId]["size"] = stats.size;
    (*out)[sectionId]["count"] = stats.count;
  }
}

NodeStats& NodeStats::operator+=(const NodeStats& other) {
  for (const auto& it : other.child_stats) {
    child_stats[it.first] += it.second;
  }
  return *this;
}

SectionId NodeStats::ComputeBiggestSection() const {
  SectionId ret = SectionId::kNone;
  float max = 0.0f;
  for (auto& pair : child_stats) {
    if (abs(pair.second.size) > max) {
      ret = pair.first;
      max = abs(pair.second.size);
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
}  // namespace caspian
