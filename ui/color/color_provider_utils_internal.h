// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_UTILS_INTERNAL_H_
#define UI_COLOR_COLOR_PROVIDER_UTILS_INTERNAL_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "ui/color/color_id.h"

namespace ui::internal {

struct ColorIdEntry {
  ColorId id;
  std::string_view name;
};

// Compact compile-time lookup table mapping a contiguous range of `ColorId`
// enum values `[kStartId, kStartId + kCount)` to their kebab-case CSS variable
// names (e.g. "kColorSysPrimary" -> "--color-sys-primary").
//
// All CSS variable names are packed contiguously into `data` and indexed by
// `offsets` in `.rodata`, avoiding per-entry pointer/size relocations in
// `.data.rel.ro` and enabling O(1) lookup.
template <ColorId kStartId, size_t kCount, size_t kTotalChars>
struct CssColorIdTable {
  std::array<char, kTotalChars> data{};
  std::array<uint16_t, kCount + 1> offsets{};

  constexpr std::string_view Lookup(ColorId color_id) const {
    if (color_id < kStartId ||
        color_id >= kStartId + static_cast<ColorId>(kCount)) {
      return std::string_view();
    }
    const size_t index = static_cast<size_t>(color_id - kStartId);
    return std::string_view(data.data())
        .substr(offsets[index], offsets[index + 1] - offsets[index]);
  }
};

consteval bool AreColorIdsContiguous(base::span<const ColorIdEntry> entries) {
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].id != entries[0].id + static_cast<ColorId>(i)) {
      return false;
    }
  }
  return true;
}

consteval bool DoColorIdNamesStartWithKColor(
    base::span<const ColorIdEntry> entries) {
  for (const ColorIdEntry& entry : entries) {
    if (!entry.name.starts_with("kColor")) {
      return false;
    }
  }
  return true;
}

consteval size_t ComputeTotalCssChars(base::span<const ColorIdEntry> entries) {
  size_t total = 0;
  for (const ColorIdEntry& entry : entries) {
    // Leading 'k' is replaced by '-'.
    ++total;
    for (char c : entry.name.substr(1)) {
      total += base::IsAsciiUpper(c) ? 2 : 1;
    }
  }
  return total;
}

template <ColorId kStartId, size_t kCount, size_t kTotalChars>
consteval auto BuildCssColorIdTable(base::span<const ColorIdEntry> entries) {
  CssColorIdTable<kStartId, kCount, kTotalChars> table;
  size_t pos = 0;
  for (size_t i = 0; i < kCount; ++i) {
    table.offsets[i] = static_cast<uint16_t>(pos);
    // Entries are packed back-to-back in `table.data` without delimiters
    // (sliced via `offsets`). Replace `name[0]` ('k') with the first '-' of
    // the CSS custom property "--" prefix; `name[1]` ('C') is then converted
    // to "-c" in the loop below, producing "--color-...".
    table.data[pos++] = '-';
    for (char c : entries[i].name.substr(1)) {
      if (base::IsAsciiUpper(c)) {
        table.data[pos++] = '-';
        table.data[pos++] = static_cast<char>(base::ToLowerASCII(c));
      } else {
        table.data[pos++] = c;
      }
    }
  }
  table.offsets[kCount] = static_cast<uint16_t>(pos);
  return table;
}

// Evaluates each compile-time step in its own `static constexpr` declaration so
// large tables (such as `CHROME_COLOR_IDS`) stay well within Clang's
// per-declaration `-fconstexpr-steps` limit using bounds-checked `base::span`.
template <auto EntriesFn>
struct CssColorIdTableHolder {
  static constexpr auto kEntries = EntriesFn();
  static_assert(!kEntries.empty(), "ColorId table must not be empty.");
  static_assert(AreColorIdsContiguous(kEntries),
                "ColorId entries must form a contiguous enum range.");
  static_assert(DoColorIdNamesStartWithKColor(kEntries),
                "ColorId names must start with \"kColor\".");
  static constexpr ColorId kStartId = kEntries[0].id;
  static constexpr size_t kCount = kEntries.size();
  static constexpr size_t kTotalChars = ComputeTotalCssChars(kEntries);
  static_assert(kTotalChars <= std::numeric_limits<uint16_t>::max(),
                "Total CSS color chars must fit in uint16_t offsets.");
  static constexpr auto kTable =
      BuildCssColorIdTable<kStartId, kCount, kTotalChars>(kEntries);
};

// Returns the compile-time `CssColorIdTable` built from a callable `EntriesFn`
// that returns a `std::array<ColorIdEntry, N>`.
template <auto EntriesFn>
consteval const auto& MakeCssColorIdTable() {
  return CssColorIdTableHolder<EntriesFn>::kTable;
}

}  // namespace ui::internal

#endif  // UI_COLOR_COLOR_PROVIDER_UTILS_INTERNAL_H_
