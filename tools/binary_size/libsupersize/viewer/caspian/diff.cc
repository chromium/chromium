// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/diff.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {
struct SymbolMatchIndex {
  SymbolMatchIndex() = default;
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

  template <typename H>
  friend H AbslHashValue(H h, const SymbolMatchIndex& k) {
    return H::combine(std::move(h), k.id, k.container_name, k.name, k.path,
                      k.size_without_padding);
  }

  bool nonempty = false;
  caspian::SectionId id = caspian::SectionId::kNone;
  std::string_view container_name;
  std::string_view name;
  std::string_view path;
  int size_without_padding = 0;
};

// Copied from /base/stl_util.h
template <class T, class Allocator, class Value>
void Erase(std::vector<T, Allocator>& container, const Value& value) {
  std::erase(container, value);
}

std::string_view GetIdPath(const caspian::Symbol& sym) {
  return (sym.SourcePath() && *sym.SourcePath()) ? sym.SourcePath()
                                                 : sym.ObjectPath();
}

struct MatchBucket {
  int32_t head = -1;
  int32_t tail = -1;
};

template <typename KeyFunc>
int MatchSymbols(
    KeyFunc key_func,
    std::vector<caspian::DeltaSymbol>* delta_symbols,
    std::vector<const caspian::Symbol*>* unmatched_before,
    std::vector<const caspian::Symbol*>* unmatched_after,
    absl::flat_hash_map<caspian::SectionId, float>* padding_by_section_name,
    bool is_sparse) {
  int n_matched_symbols = 0;
  const int32_t n_before = static_cast<int32_t>(unmatched_before->size());
  std::vector<int32_t> next_in_bucket(n_before, -1);
  absl::flat_hash_map<SymbolMatchIndex, MatchBucket> before_symbols_by_key;
  before_symbols_by_key.reserve(n_before);
  for (int32_t i = 0; i < n_before; ++i) {
    const caspian::Symbol* before_sym = (*unmatched_before)[i];
    SymbolMatchIndex key = key_func(*before_sym);
    if (key) {
      auto [it, inserted] =
          before_symbols_by_key.try_emplace(key, MatchBucket{i, i});
      if (!inserted) {
        next_in_bucket[it->second.tail] = i;
        it->second.tail = i;
      }
    }
  }

  for (const caspian::Symbol*& after_sym : *unmatched_after) {
    SymbolMatchIndex key = key_func(*after_sym);
    if (key) {
      auto found = before_symbols_by_key.find(key);
      if (found != before_symbols_by_key.end() && found->second.head != -1) {
        const int32_t idx = found->second.head;
        found->second.head = next_in_bucket[idx];
        const caspian::Symbol*& before_sym = (*unmatched_before)[idx];
        // Padding tracked in aggregate, except for padding-only symbols. Skip
        // if |is_sparse|, since padding symbols would have been created when
        // sparse symbols were created by SuperSize-save-diff.
        if (!is_sparse && before_sym->SizeWithoutPadding() != 0) {
          (*padding_by_section_name)[before_sym->section_id_] +=
              after_sym->PaddingPss() - before_sym->PaddingPss();
        }
        delta_symbols->emplace_back(before_sym, after_sym);
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
    size_t first_digit = in.find_first_of("0123456789");
    if (first_digit == std::string_view::npos) {
      return in;
    }
    std::string& out = tmp_strings_.emplace_back();
    out.reserve(in.size() - 1);
    out.append(in.data(), first_digit);
    for (size_t i = first_digit + 1; i < in.size(); ++i) {
      char c = in[i];
      if (c < '0' || c > '9') {
        out.push_back(c);
      }
    }
    return out;
  }

  std::string_view NormalizeStarSymbols(std::string_view in) {
    // Used only for "*" symbols. Replaces a trailing "<whitespace><digits>",
    // optionally followed by " (<anything>)", with "s". E.g.:
    // "* symbol gap 3 (end of section)" -> "* symbol gaps".
    auto is_space = [](char c) {
      return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
    };
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    const size_t n = in.size();
    for (size_t i = 0; i < n; ++i) {
      if (!is_space(in[i]) || (i > 0 && is_space(in[i - 1]))) {
        continue;
      }
      size_t p = i + 1;
      while (p < n && is_space(in[p])) {
        ++p;
      }
      if (p >= n || !is_digit(in[p])) {
        continue;
      }
      size_t q = p + 1;
      while (q < n && is_digit(in[q])) {
        ++q;
      }
      if (q == n ||
          (q + 2 < n && in[q] == ' ' && in[q + 1] == '(' && in.back() == ')')) {
        std::string& out = tmp_strings_.emplace_back();
        out.reserve(i + 1);
        out.append(in.data(), i);
        out.push_back('s');
        return out;
      }
    }
    return in;
  }

  auto SectionAndFullNameAndPathAndSize() {
    return [](const caspian::Symbol& sym) {
      return SymbolMatchIndex(sym.section_id_, sym.ContainerName(),
                              sym.full_name_, GetIdPath(sym),
                              sym.SizeWithoutPadding());
    };
  }

  auto SectionAndFullNameAndPath() {
    return [this](const caspian::Symbol& sym) {
      return SymbolMatchIndex(sym.section_id_, sym.ContainerName(),
                              StripNumbers(sym.full_name_), GetIdPath(sym), 0);
    };
  }

  // Allows signature changes (uses |Name()| rather than |FullName()|)
  auto SectionAndNameAndPath() {
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
  auto SectionAndFullName() {
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
  // Over-allocate the expected size by 20% to hopefully make the second reserve
  // a no-op and avoid a second reallocation.
  ret.delta_symbols.reserve(static_cast<size_t>(
      std::max(before->raw_symbols.size(), after->raw_symbols.size()) * 1.2));

  std::vector<const Symbol*> unmatched_before;
  unmatched_before.reserve(before->raw_symbols.size());
  for (const Symbol& sym : before->raw_symbols) {
    unmatched_before.push_back(&sym);
  }

  std::vector<const Symbol*> unmatched_after;
  unmatched_after.reserve(after->raw_symbols.size());
  for (const Symbol& sym : after->raw_symbols) {
    unmatched_after.push_back(&sym);
  }

  // Attempt several rounds to use increasingly loose matching on unmatched
  // symbols.  Any symbols still unmatched are tried in the next round.
  int step = 0;
  DiffHelper helper;
  absl::flat_hash_map<SectionId, float> padding_by_section_name;
  auto run_pass = [&](auto key_func) {
    int n_matched_symbols =
        MatchSymbols(key_func, &ret.delta_symbols, &unmatched_before,
                     &unmatched_after, &padding_by_section_name, is_sparse);
    std::cout << "Matched " << n_matched_symbols << " symbols in matching pass "
              << ++step << std::endl;
    helper.ClearStrings();
  };
  run_pass(helper.SectionAndFullNameAndPathAndSize());
  run_pass(helper.SectionAndFullNameAndPath());
  run_pass(helper.SectionAndNameAndPath());
  run_pass(helper.SectionAndFullName());

  const size_t num_padding_symbols = std::ranges::count_if(
      padding_by_section_name,
      [](const auto& pair) { return pair.second != 0.0f; });
  ret.delta_symbols.reserve(ret.delta_symbols.size() + unmatched_after.size() +
                            unmatched_before.size() + num_padding_symbols);
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
