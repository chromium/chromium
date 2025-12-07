// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/vector_icon_utils.h"

#include <ostream>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {

namespace {
const char kCanvasDimensions[] = "CANVAS_DIMENSIONS";
}  // namespace

int GetCommandArgumentCount(CommandType command) {
  switch (command) {
    case STROKE:
    case H_LINE_TO:
    case R_H_LINE_TO:
    case V_LINE_TO:
    case R_V_LINE_TO:
    case CANVAS_DIMENSIONS:
    case PATH_COLOR_ALPHA:
      return 1;

    case MOVE_TO:
    case R_MOVE_TO:
    case LINE_TO:
    case R_LINE_TO:
    case QUADRATIC_TO_SHORTHAND:
    case R_QUADRATIC_TO_SHORTHAND:
      return 2;

    case CIRCLE:
      return 3;

    case PATH_COLOR_ARGB:
    case CUBIC_TO_SHORTHAND:
    case CLIP:
    case QUADRATIC_TO:
    case R_QUADRATIC_TO:
    case OVAL:
      return 4;

    case ROUND_RECT:
      return 5;

    case CUBIC_TO:
    case R_CUBIC_TO:
      return 6;

    case ARC_TO:
    case R_ARC_TO:
      return 7;

    case FILL_RULE_NONZERO:
    case NEW_PATH:
    case PATH_MODE_CLEAR:
    case CAP_SQUARE:
    case CLOSE:
    case DISABLE_AA:
    case FLIPS_IN_RTL:
      return 0;
  }

  NOTREACHED();
}

int GetDefaultSizeOfVectorIcon(const VectorIcon& icon) {
  if (icon.is_empty())
    return -1;
  const VectorIconRep& default_rep = icon.reps.back();
  DCHECK_EQ(default_rep.path[0].command, CANVAS_DIMENSIONS)
      << " " << icon.name
      << " has no size in its icon definition, and it seems unlikely you want "
         "to display at the default of 48dip. Please specify a size in "
         "CreateVectorIcon().";
  return default_rep.path[1].arg;
}

#define DECLARE_VECTOR_COMMAND(x) {#x, gfx::x},

PathElement ParsePathElement(std::string_view s) {
  static base::NoDestructor<absl::flat_hash_map<std::string_view, CommandType>>
      kCommandMap({DECLARE_VECTOR_COMMANDS});

  // Attempt to parse as a command.
  auto it = kCommandMap->find(s);
  if (it != kCommandMap->end()) {
    return PathElement(it->second);
  }

  // Attempt to parse as hex.
  if (base::StartsWith(s, "0x")) {
    int int_value = 0;
    CHECK(base::HexStringToInt(s, &int_value));
    return PathElement(SkIntToScalar(int_value));
  }

  // Finally, parse as double.
  double value = 0.0;
  if (base::EndsWith(s, "f")) {
    auto trimmed = s.substr(0, s.length() - 1);
    CHECK(base::StringToDouble(trimmed, &value));
  } else {
    CHECK(base::StringToDouble(s, &value));
  }

  return PathElement(SkDoubleToScalar(value));
}

void ParsePathElements(std::string_view s,
                       std::vector<std::vector<PathElement>>& path_elements) {
  auto lines = base::SplitString(s, "\n", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : lines) {
    if (base::StartsWith(line, "//")) {
      continue;
    }
    auto elements = base::SplitString(line, ", ", base::TRIM_WHITESPACE,
                                      base::SPLIT_WANT_NONEMPTY);
    for (const auto& element : elements) {
      if (element == kCanvasDimensions || path_elements.empty()) {
        path_elements.emplace_back();
      }
      path_elements.back().push_back(ParsePathElement(element));
    }
  }
}

}  // namespace gfx
