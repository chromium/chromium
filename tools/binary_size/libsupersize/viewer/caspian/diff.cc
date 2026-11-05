// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/diff.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {
// Sentinel indicating that a symbol cannot match in the current pass (either
// because it fails a pass precondition like IsNameUnique(), or because its
// scope has no unmatched symbols on the opposite side).
constexpr uint32_t kInvalidScopeId = ~0u;
constexpr size_t kNumShards = 8;
// Avoid thread-pool dispatch overhead on small inputs (e.g. unit tests).
constexpr size_t kMinSymbolsForThreads = 1024;

// Hash table key for a single matching pass in MatchSymbols().
// Rather than storing and hashing (SectionId, container_name, path) strings on
// every pass, DiffHelper interns each unique scope tuple into a 32-bit
// |scope_id|.
struct SymbolMatchIndex {
  SymbolMatchIndex() = default;
  SymbolMatchIndex(uint32_t scope_id,
                   std::string_view name,
                   int size_without_padding)
      : scope_id(scope_id),
        size_without_padding(size_without_padding),
        name(name) {}

  operator bool() const { return scope_id != kInvalidScopeId; }

  bool operator==(const SymbolMatchIndex& other) const {
    return scope_id == other.scope_id &&
           size_without_padding == other.size_without_padding &&
           name == other.name;
  }

  template <typename H>
  friend H AbslHashValue(H h, const SymbolMatchIndex& k) {
    return H::combine(std::move(h), k.scope_id, k.size_without_padding, k.name);
  }

  uint32_t scope_id = kInvalidScopeId;
  int size_without_padding = 0;
  std::string_view name;
};

// Copied from /base/stl_util.h
template <class T, class Allocator, class Value>
void Erase(std::vector<T, Allocator>& container, const Value& value) {
  std::erase(container, value);
}

const char* GetIdPath(const caspian::Symbol& sym) {
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
  DiffHelper(const caspian::SizeInfo* before, const caspian::SizeInfo* after)
      : before_base_(before->raw_symbols.data()),
        after_base_(after->raw_symbols.data()) {
    // Passes 1-3 only match symbols that share the same (path, container_name,
    // section_id) "scope". Pre-compute an integer scope_id for every symbol in
    // |before| and |after| using a two-level lookup:
    // 1. |ptr_to_scope| is a per-SizeInfo map keyed by raw (const char* path,
    //    const Container*, SectionId) pointers. Because symbols within a single
    //    SizeInfo share path pointers (into info->source_paths / object_paths)
    //    and Container pointers, this avoids calling strlen(path) and hashing
    //    strings for every symbol.
    // 2. |scope_ids_| is shared across |before| and |after| and keyed by
    //    string_view contents, ensuring identical (path, container, section)
    //    strings in the two separate SizeInfo buffers receive the same
    //    scope_id.
    auto intern = [&](const caspian::SizeInfo* info) {
      absl::flat_hash_map<std::tuple<const char*, const caspian::Container*,
                                     caspian::SectionId>,
                          uint32_t>
          ptr_to_scope;
      ptr_to_scope.reserve(info->object_paths.size() + 256);
      std::vector<uint32_t> ret;
      ret.reserve(info->raw_symbols.size());
      for (const caspian::Symbol& sym : info->raw_symbols) {
        const char* path = GetIdPath(sym);
        auto [it, inserted] = ptr_to_scope.try_emplace(
            {path, sym.container_, sym.section_id_}, 0);
        if (inserted) {
          it->second = scope_ids_
                           .try_emplace({path ? path : "", sym.ContainerName(),
                                         sym.section_id_},
                                        scope_ids_.size())
                           .first->second;
        }
        ret.push_back(it->second);
      }
      return ret;
    };
    scope_ids_.reserve(before->object_paths.size() +
                       after->object_paths.size() + 256);
    before_scopes_ = intern(before);
    after_scopes_ = intern(after);
  }

  // Shares the interned scope tables of |root| for use on a worker thread.
  explicit DiffHelper(const DiffHelper* root) : root_(root) {}

  // Looks up the precomputed scope_id for |sym|. Every Symbol passed to
  // MatchSymbols() or Diff() lives in either |before->raw_symbols| or
  // |after->raw_symbols|. Using unsigned uintptr_t subtraction checks whether
  // |&sym| falls within [before_base_, before_base_ + before_scopes_.size()) in
  // a single comparison, and pointer subtraction recovers its vector index in
  // O(1) without storing a scope field on Symbol.
  uint32_t RawScopeId(const caspian::Symbol& sym) const {
    uintptr_t offset = reinterpret_cast<uintptr_t>(&sym) -
                       reinterpret_cast<uintptr_t>(root_->before_base_);
    if (offset < root_->before_scopes_.size() * sizeof(caspian::Symbol)) {
      return root_->before_scopes_[&sym - root_->before_base_];
    }
    return root_->after_scopes_[&sym - root_->after_base_];
  }

  // Recomputes which scope_ids still have unmatched symbols in both |before|
  // and |after|. A symbol in scope S can only match in passes 1-3 if S has at
  // least one unmatched symbol on each side (bit 1 = after, bit 2 = before,
  // mask == 3 = active on both sides).
  void UpdateActiveScopes(
      const std::vector<const caspian::Symbol*>& unmatched_before,
      const std::vector<const caspian::Symbol*>& unmatched_after) {
    scope_mask_.assign(root_->scope_ids_.size(), 0);
    for (const caspian::Symbol* sym : unmatched_after) {
      scope_mask_[RawScopeId(*sym)] |= 1;
    }
    for (const caspian::Symbol* sym : unmatched_before) {
      scope_mask_[RawScopeId(*sym)] |= 2;
    }
  }

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
    return [this](const caspian::Symbol& sym) {
      return SymbolMatchIndex(ActiveScopeId(sym), sym.full_name_,
                              sym.SizeWithoutPadding());
    };
  }

  auto SectionAndFullNameAndPath() {
    return [this](const caspian::Symbol& sym) {
      // Skip StripNumbers() if this symbol's scope has no unmatched symbols on
      // the opposite side.
      uint32_t scope_id = ActiveScopeId(sym);
      if (scope_id == kInvalidScopeId) {
        return SymbolMatchIndex();
      }
      return SymbolMatchIndex(scope_id, StripNumbers(sym.full_name_), 0);
    };
  }

  // Allows signature changes (uses |Name()| rather than |FullName()|)
  auto SectionAndNameAndPath() {
    return [this](const caspian::Symbol& sym) {
      // Skip sym.Name() (which triggers lazy DeriveNames() template/argument
      // parsing) if this symbol's scope has no unmatched symbols on the
      // opposite side.
      uint32_t scope_id = ActiveScopeId(sym);
      if (scope_id == kInvalidScopeId) {
        return SymbolMatchIndex();
      }
      std::string_view name = sym.Name();
      if (!name.empty() && name[0] == '*') {
        name = NormalizeStarSymbols(name);
      }
      return SymbolMatchIndex(scope_id, name, 0);
    };
  }

  // Match on full name, but without path (to account for file moves)
  auto SectionAndFullName() {
    return [this](const caspian::Symbol& sym) {
      // For string literals that contain a prefix of the string in the name,
      // allow matching up via name + size.
      if (!sym.full_name_.empty() && sym.full_name_[0] == '"') {
        return SymbolMatchIndex(NoPathScopeId(sym), sym.full_name_,
                                sym.SizeWithoutPadding());
      }
      if (!sym.IsNameUnique()) {
        return SymbolMatchIndex();
      }
      return SymbolMatchIndex(NoPathScopeId(sym), sym.full_name_, 0);
    };
  }

  void ClearStrings() { tmp_strings_.clear(); }

 private:
  // Returns the symbol's scope_id if that scope has unmatched symbols in both
  // |before| and |after| (mask == 3), or kInvalidScopeId so MatchSymbols()
  // skips the symbol.
  uint32_t ActiveScopeId(const caspian::Symbol& sym) const {
    uint32_t id = RawScopeId(sym);
    return scope_mask_[id] == 3 ? id : kInvalidScopeId;
  }

  // Pass 4 matches symbols across file moves (ignoring path), so intern a
  // pathless ("", container_name, section_id) scope tuple.
  uint32_t NoPathScopeId(const caspian::Symbol& sym) {
    return scope_ids_
        .try_emplace({"", sym.ContainerName(), sym.section_id_},
                     scope_ids_.size())
        .first->second;
  }

  const DiffHelper* root_ = this;
  // Base pointers of |before->raw_symbols| and |after->raw_symbols|, used by
  // RawScopeId() to compute a symbol's index in |before_scopes_| or
  // |after_scopes_|.
  const caspian::Symbol* before_base_ = nullptr;
  const caspian::Symbol* after_base_ = nullptr;
  // Maps (path, container_name, section_id) -> dense scope_id.
  absl::flat_hash_map<
      std::tuple<std::string_view, std::string_view, caspian::SectionId>,
      uint32_t>
      scope_ids_;
  // Parallel to |before->raw_symbols| and |after->raw_symbols|: maps symbol
  // index -> scope_id.
  std::vector<uint32_t> before_scopes_;
  std::vector<uint32_t> after_scopes_;
  // Indexed by scope_id: bit 1 = has unmatched after symbols, bit 2 = has
  // unmatched before symbols.
  std::vector<uint8_t> scope_mask_;
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

  // Passes 1-3 only match symbols with identical scope_id, so partition
  // symbols by (scope_id % kNumShards) and run passes 1-3 per shard.
  struct DiffShard {
    std::vector<const Symbol*> unmatched_before;
    std::vector<const Symbol*> unmatched_after;
    std::array<std::vector<DeltaSymbol>, 3> pass_deltas;
    absl::flat_hash_map<SectionId, float> padding_by_section_name;
  };
  DiffHelper helper(before, after);
  std::array<DiffShard, kNumShards> shards;
  for (const Symbol& sym : before->raw_symbols) {
    shards[helper.RawScopeId(sym) % kNumShards].unmatched_before.push_back(
        &sym);
  }
  for (const Symbol& sym : after->raw_symbols) {
    shards[helper.RawScopeId(sym) % kNumShards].unmatched_after.push_back(&sym);
  }

  for (const SizeInfo* info : {before, after}) {
    info->thread_owned_strings.resize(kNumShards);
  }

  auto run_shard = [&](size_t s) {
    ScopedOwnedStringsArena arena(s);
    DiffShard& shard = shards[s];
    DiffHelper shard_helper(&helper);
    auto run_pass = [&](auto key_func, size_t pass) {
      shard_helper.UpdateActiveScopes(shard.unmatched_before,
                                      shard.unmatched_after);
      MatchSymbols(key_func, &shard.pass_deltas[pass], &shard.unmatched_before,
                   &shard.unmatched_after, &shard.padding_by_section_name,
                   is_sparse);
      shard_helper.ClearStrings();
    };
    run_pass(shard_helper.SectionAndFullNameAndPathAndSize(), 0);
    run_pass(shard_helper.SectionAndFullNameAndPath(), 1);
    run_pass(shard_helper.SectionAndNameAndPath(), 2);
  };

  if (before->raw_symbols.size() + after->raw_symbols.size() >=
      kMinSymbolsForThreads) {
    std::array<std::thread, kNumShards - 1> threads;
    for (size_t s = 1; s < kNumShards; ++s) {
      threads[s - 1] = std::thread(run_shard, s);
    }
    run_shard(0);
    for (std::thread& t : threads) {
      t.join();
    }
  } else {
    for (size_t s = 0; s < kNumShards; ++s) {
      run_shard(s);
    }
  }

  // Merge per-shard results back into single-threaded (raw_symbols) order.
  auto append_sorted = [](auto&& get_vec, auto* out, auto proj) {
    size_t start = out->size();
    for (size_t s = 0; s < kNumShards; ++s) {
      auto& v = get_vec(s);
      out->insert(out->end(), v.begin(), v.end());
    }
    std::sort(out->begin() + start, out->end(),
              [&](const auto& a, const auto& b) { return proj(a) < proj(b); });
  };

  int step = 0;
  for (size_t p = 0; p < 3; ++p) {
    size_t before_count = ret.delta_symbols.size();
    append_sorted([&](size_t s) -> auto& { return shards[s].pass_deltas[p]; },
                  &ret.delta_symbols,
                  [](const DeltaSymbol& d) { return d.After(); });
    std::cout << "Matched " << (ret.delta_symbols.size() - before_count)
              << " symbols in matching pass " << ++step << std::endl;
  }

  std::vector<const Symbol*> unmatched_before;
  append_sorted([&](size_t s) -> auto& { return shards[s].unmatched_before; },
                &unmatched_before, std::identity{});
  std::vector<const Symbol*> unmatched_after;
  append_sorted([&](size_t s) -> auto& { return shards[s].unmatched_after; },
                &unmatched_after, std::identity{});

  absl::flat_hash_map<SectionId, float> padding_by_section_name;
  for (const DiffShard& shard : shards) {
    for (const auto& [sec, pad] : shard.padding_by_section_name) {
      padding_by_section_name[sec] += pad;
    }
  }

  // Pass 4 matches across paths on the remaining unmatched symbols.
  int n_matched_symbols = MatchSymbols(
      helper.SectionAndFullName(), &ret.delta_symbols, &unmatched_before,
      &unmatched_after, &padding_by_section_name, is_sparse);
  std::cout << "Matched " << n_matched_symbols << " symbols in matching pass "
            << ++step << std::endl;

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
