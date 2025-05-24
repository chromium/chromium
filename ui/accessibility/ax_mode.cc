// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_mode.h"

#include <string_view>
#include <vector>

#include "base/strings/string_util.h"

namespace ui {

std::ostream& operator<<(std::ostream& stream, const AXMode& mode) {
  return stream << mode.ToString();
}

bool AXMode::HasFilterFlags(uint32_t filter_flag) const {
  return (filter_flags_ & filter_flag) == filter_flag;
}

void AXMode::SetFilterFlags(uint32_t filter_flag, bool value) {
  filter_flags_ =
      value ? (filter_flags_ | filter_flag) : (filter_flags_ & ~filter_flag);
}

std::string AXMode::ToString() const {
  std::vector<std::string_view> tokens;

  // Written as a loop with a switch so that this crashes if a new
  // mode flag is added without adding support for logging it.
  for (uint32_t mode_flag = AXMode::kFirstModeFlag;
       mode_flag <= AXMode::kLastModeFlag; mode_flag = mode_flag << 1) {
    std::string_view flag_name;
    switch (mode_flag) {
      case AXMode::kNativeAPIs:
        flag_name = "kNativeAPIs";
        break;
      case AXMode::kWebContents:
        flag_name = "kWebContents";
        break;
      case AXMode::kInlineTextBoxes:
        flag_name = "kInlineTextBoxes";
        break;
      case AXMode::kExtendedProperties:
        flag_name = "kExtendedProperties";
        break;
      case AXMode::kHTML:
        flag_name = "kHTML";
        break;
      case AXMode::kLabelImages:
        flag_name = "kLabelImages";
        break;
      case AXMode::kPDFPrinting:
        flag_name = "kPDFPrinting";
        break;
      case AXMode::kPDFOcr:
        flag_name = "kPDFOcr";
        break;
      case AXMode::kHTMLMetadata:
        flag_name = "kHTMLMetadata";
        break;
      case AXMode::kAnnotateMainNode:
        flag_name = "kAnnotateMainNode";
        break;
      case kFromPlatform:
        flag_name = "kFromPlatform";
        break;
      case AXMode::kScreenReader:
        flag_name = "kScreenReader";
        break;
    }

    DCHECK(!flag_name.empty());

    if (has_mode(mode_flag))
      tokens.push_back(flag_name);
  }

  for (uint32_t filter_mode_flag = AXMode::kFilterFirstFlag;
       filter_mode_flag <= AXMode::kFilterLastFlag;
       filter_mode_flag = filter_mode_flag << 1) {
    std::string_view flag_name;
    switch (filter_mode_flag) {
      case AXMode::kFormsAndLabelsOnly:
        flag_name = "kFormsAndLabelsOnly";
        break;
      case AXMode::kOnScreenOnly:
        flag_name = "kOnScreenOnly";
        break;
    }

    DCHECK(!flag_name.empty());

    if (HasFilterFlags(filter_mode_flag)) {
      tokens.push_back(flag_name);
    }
  }

  return base::JoinString(tokens, " | ");
}

}  // namespace ui
