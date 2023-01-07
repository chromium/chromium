// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_mode.h"

#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace ui {

std::ostream& operator<<(std::ostream& stream, const AXMode& mode) {
  return stream << mode.ToString();
}

std::string AXMode::ToString() const {
  std::vector<base::StringPiece> tokens;

  // Written as a loop with a switch so that this crashes if a new
  // mode flag is added without adding support for logging it.
  for (uint32_t mode_flag = AXMode::kFirstModeFlag;
       mode_flag <= AXMode::kLastModeFlag; mode_flag = mode_flag << 1) {
    base::StringPiece flag_name;
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
      case AXMode::kPDF:
        flag_name = "kPDF";
        break;
      case AXMode::kHTMLMetadata:
        flag_name = "kHTMLMetadata";
        break;
    }

    DCHECK(!flag_name.empty());

    if (has_mode(mode_flag))
      tokens.push_back(flag_name);
  }
  return base::JoinString(tokens, " | ");
}

}  // namespace ui
