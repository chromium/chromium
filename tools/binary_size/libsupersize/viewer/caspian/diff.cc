// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/diff.h"

#include <cmath>
#include <deque>
#include <functional>
#include <iostream>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "third_party/re2/src/re2/re2.h"

namespace {
struct SymbolMatchIndex {
  SymbolMatchIndex() {}
  SymbolMatchIndex(caspian::SectionId id,
                   std::string_view container_name,
                   std::string_view name,
                   std::string_view path,
                   int size_without_padding)
      : nonempty(true),
        id(id),
        container_name(container_name),
        name(name),
        path(path),
        size_without_padding(size_without_padding) {
    this->name = name;
  }

  operator bool() const { return nonempty; }

  bool operator==(const SymbolMatchIndex& other) const {
    return id == other.id && container_name == other.container_name &&
           name == other.name && path == other.path &&
           size_without_padding == other.size_without_padding;
  }

  bool nonempty = false;
  caspian::SectionId id;
  std::string_view container_name;
  std::string_view name;
  std::string_view path;
  int size_without_padding;
};
}  // namespace

namespace std {
template <>
struct hash<SymbolMatchIndex> {
  static constexpr size_t kPrime1 = 105929;
  static constexpr size_t kPrime2 = 8543;
  size_t operator()(const SymbolMatchIndex& k) const {
    return ((kPrime1 * static_cast<size_t>(k.id)) ^
            hash<string_view>()(k.container_name) ^
            hash<string_view>()(k.name) ^ hash<string_view>()(k.path) ^
            (kPrime2 * k.size_without_padding));
  }
};
}  // namespace std

namespace {
// Copied from /base/stl_util.h
template <class T, class Allocator, class Value>
void Erase(std::vector<T, Allocator>& container, const Value& value) {
  container.erase(std::remove(container.begin(), container.end(), value),
                  container.end());
}

std::string_view GetIdPath(const caspian::Symbol& sym) {
  return (sym.SourcePath() && *sym.SourcePath()) ? sym.SourcePath()
                                                 : sym.ObjectPath();
}

int MatchSymbols(
    std::function<SymbolMatchIndex(const caspian::Symbol&)> key_func,
    std::vector<caspian::DeltaSymbol>* delta_symbols,
    std::vector<const caspian::Symbol*>* unmatched_before,
    std::vector<const caspian::Symbol*>* unmatched_after,
    std::unordered_map<caspian::SectionId, float>* padding_by_section_name,
    bool is_sparse) {
  int n_matched_symbols = 0;
  std::unordered_map<SymbolMatchIndex,
                     std::list<std::reference_wrapper<const caspian::Symbol*>>>
      before_symbols_by_key;
  for (const caspian::Symbol*& before_sym : *unmatched_before) {
    SymbolMatchIndex key = key_func(*before_sym);
    if (key) {
      before_symbols_by_key[key].emplace_back(before_sym);
    }
  }

  for (const caspian::Symbol*& after_sym : *unmatched_after) {
    SymbolMatchIndex key = key_func(*after_sym);
    if (key) {
      const auto& found = before_symbols_by_key.find(key);
      if (found != before_symbols_by_key.end() && found->second.size()) {
        const caspian::Symbol*& before_sym = found->second.front().get();
        found->second.pop_front();
        // Padding tracked in aggregate, except for padding-only symbols. Skip
        // if |is_sparse|, since padding symbols would have been created when
        // sparse symbols were created by SuperSize-save-diff.
        if (!is_sparse && before_sym->SizeWithoutPadding() != 0) {
          (*padding_by_section_name)[before_sym->section_id_] +=
              after_sym->PaddingPss() - before_sym->PaddingPss();
        }
        caspian::DeltaSymbol delta_sym(before_sym, after_sym);
        delta_symbols->push_back(delta_sym);
        // Null associated pointers in |unmatched_before|, |unmatched_after|.
        before_sym = nullptr;
        after_sym = nullptr;
        n_matched_symbols++;
      }
    }
  }

  // Compact out nulled entries.
  Erase(*unmatched_before, nullptr);
  Erase(*unmatched_after, nullptr);
  return n_matched_symbols;
}

class DiffHelper {
 public:
  DiffHelper() = default;

  std::string_view StripNumbers(std::string_view in) {
    static const RE2 number_regex("\\d+");
    if (RE2::PartialMatch(in, number_regex)) {
      tmp_strings_.emplace_back(in);
      RE2::GlobalReplace(&tmp_strings_.back(), number_regex, "");
      return tmp_strings_.back();
    }
    return in;
  }

  std::string_view NormalizeStarSymbols(std::string_view in) {
    // Used only for "*" symbols to strip suffixes "abc123" or "abc123 (any)".
    static const RE2 normalize_star_symbols("\\s+\\d+(?: \\(.*\\))?$");
    if (RE2::PartialMatch(in, normalize_star_symbols)) {
      tmp_strings_.emplace_back(in);
      RE2::Replace(&tmp_strings_.back(), normalize_star_symbols, "s");
      return tmp_strings_.back();
    }
    return in;
  }

  using MatchFunc = std::function<SymbolMatchIndex(const caspian::Symbol&)>;

  MatchFunc SectionAndFullNameAndPathAndSize() {
    return [](const caspian::Symbol& sym) {
      return SymbolMatchIndex(sym.section_id_, sym.ContainerName(),
                              sym.full_name_, GetIdPath(sym),
                              sym.SizeWithoutPadding());
    };
  }

  MatchFunc SectionAndFullNameAndPath() {
    return [this](const caspian::Symbol& sym) {
      return SymbolMatchIndex(sym.section_id_, sym.ContainerName(),
                              StripNumbers(sym.full_name_), GetIdPath(sym), 0);
    };
  }

  // Allows signature changes (uses |Name()| rather than |FullName()|)
  MatchFunc SectionAndNameAndPath() {
    return [this](const caspian::Symbol& sym) {
      std::string_view name = sym.Name();
      if (!name.empty() && name[0] == '*') {
        name = NormalizeStarSymbols(name);
      }
      return SymbolMatchIndex(sym.section_id_, sym.ContainerName(), name,
                              GetIdPath(sym), 0);
    };
  }

  // Match on full name, but without path (to account for file moves)
  MatchFunc SectionAndFullName() {
    return [](const caspian::Symbol& sym) {
      // For string literals that contain a prefix of the string in the name,
      // allow matching up via name + size.
      if (!sym.full_name_.empty() && sym.full_name_[0] == '"') {
        return SymbolMatchIndex(sym.section_id_, sym.ContainerName(),
                                sym.full_name_, "", sym.SizeWithoutPadding());
      }
      if (!sym.IsNameUnique()) {
        return SymbolMatchIndex();
      }
      return SymbolMatchIndex(sym.section_id_, sym.ContainerName(),
                              sym.full_name_, "", 0);
    };
  }

  void ClearStrings() { tmp_strings_.clear(); }

 private:
  // Holds strings created during number stripping/star symbol normalization.
  std::deque<std::string> tmp_strings_;
};
}  // namespace

namespace caspian {

// See docs/diffs.md for diffing algorithm.
DeltaSizeInfo Diff(const SizeInfo* before,
                   const SizeInfo* after,
                   const std::vector<std::string>* removed_sources,
                   const std::vector<std::string>* added_sources) {
  DeltaSizeInfo ret(before, after, removed_sources, added_sources);
  bool is_sparse = before->IsSparse() && after->IsSparse();

  std::vector<const Symbol*> unmatched_before;
  for (const Symbol& sym : before->raw_symbols) {
    unmatched_before.push_back(&sym);
  }

  std::vector<const Symbol*> unmatched_after;
  for (const Symbol& sym : after->raw_symbols) {
    unmatched_after.push_back(&sym);
  }

  // Attempt several rounds to use increasingly loose matching on unmatched
  // symbols.  Any symbols still unmatched are tried in the next round.
  int step = 0;
  DiffHelper helper;
  std::vector<DiffHelper::MatchFunc> key_funcs = {
      helper.SectionAndFullNameAndPathAndSize(),
      helper.SectionAndFullNameAndPath(), helper.SectionAndNameAndPath(),
      helper.SectionAndFullName()};
  std::unordered_map<SectionId, float> padding_by_section_name;
  for (const auto& key_function : key_funcs) {
    int n_matched_symbols =
        MatchSymbols(key_function, &ret.delta_symbols, &unmatched_before,
                     &unmatched_after, &padding_by_section_name, is_sparse);
    std::cout << "Matched " << n_matched_symbols << " symbols in matching pass "
              << ++step << std::endl;
    helper.ClearStrings();
  }

  // Add removals or deletions for any unmatched symbols.
  for (const Symbol* after_sym : unmatched_after) {
    ret.delta_symbols.push_back(DeltaSymbol(nullptr, after_sym));
  }
  for (const Symbol* before_sym : unmatched_before) {
    ret.delta_symbols.push_back(DeltaSymbol(before_sym, nullptr));
  }

  // Create a DeltaSymbol to represent the zeroed out padding of matched
  // symbols.
  for (const auto& pair : padding_by_section_name) {
    SectionId section_id = pair.first;
    float padding = pair.second;
    if (padding != 0.0f) {
      float abs_padding = std::abs(padding);
      ret.owned_symbols.emplace_back();
      Symbol& abs_sym = ret.owned_symbols.back();
      abs_sym.section_id_ = section_id;
      abs_sym.size_ = abs_padding;
      abs_sym.padding_ = abs_padding;
      abs_sym.full_name_ = "Overhead: aggregate padding of diff'ed symbols";
      abs_sym.template_name_ = abs_sym.full_name_;
      abs_sym.name_ = abs_sym.full_name_;
      if (padding < 0.0f) {
        ret.delta_symbols.emplace_back(&abs_sym, nullptr);
      } else {
        ret.delta_symbols.emplace_back(nullptr, &abs_sym);
      }
    }
  }
  return ret;
}
}  // namespace caspian
