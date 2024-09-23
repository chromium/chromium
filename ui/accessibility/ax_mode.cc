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

bool AXMode::HasExperimentalFlags(uint32_t experimental_flag) const {
  return (experimental_flags_ & experimental_flag) == experimental_flag;
}

void AXMode::SetExperimentalFlags(uint32_t experimental_flag, bool value) {
  experimental_flags_ = value ? (experimental_flags_ | experimental_flag)
                              : (experimental_flags_ & ~experimental_flag);
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
      case AXMode::kScreenReader:
        flag_name = "kScreenReader";
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
    }

    DCHECK(!flag_name.empty());

    if (has_mode(mode_flag))
      tokens.push_back(flag_name);
  }

  for (uint32_t experimental_mode_flag = AXMode::kExperimentalFirstFlag;
       experimental_mode_flag <= AXMode::kExperimentalLastFlag;
       experimental_mode_flag = experimental_mode_flag << 1) {
    std::string_view flag_name;
    switch (experimental_mode_flag) {
      case AXMode::kExperimentalFormControls:
        flag_name = "kExperimentalFormControls";
        break;
    }

    DCHECK(!flag_name.empty());

    if (HasExperimentalFlags(experimental_mode_flag)) {
      tokens.push_back(flag_name);
    }
  }

  return base::JoinString(tokens, " | ");
}

}  // namespace ui
