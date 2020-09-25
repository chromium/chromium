// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_utils.h"

#include "base/command_line.h"

char kActiveTabSwitch[] = "active-tab";
char kChromeSwitch[] = "chrome";
char kChromiumSwitch[] = "chromium";
char kFirefoxSwitch[] = "firefox";
char kPatternSwitch[] = "pattern";
char kSafariSwitch[] = "safari";

namespace tools {

void PrintHelpForTreeSelectors() {
  printf("  --pattern\ttitle of an application to dump accessible tree for\n");
  printf("  pre-defined application selectors to dump accessible tree for:\n");
  printf("    --chrome\tChrome browser\n");
  printf("    --chromium\tChromium browser\n");
  printf("    --firefox\tFirefox browser\n");
  printf("    --safari\tSafari browser\n");
  printf("    --active-tab\tActive tab of a choosen browser\n");
}

TreeSelector TreeSelectorFromCommandLine(
    const base::CommandLine* command_line) {
  int selectors = TreeSelector::None;
  if (command_line->HasSwitch(kChromeSwitch)) {
    selectors = TreeSelector::Chrome;
  } else if (command_line->HasSwitch(kChromiumSwitch)) {
    selectors = TreeSelector::Chromium;
  } else if (command_line->HasSwitch(kFirefoxSwitch)) {
    selectors = TreeSelector::Firefox;
  } else if (command_line->HasSwitch(kSafariSwitch)) {
    selectors = TreeSelector::Safari;
  }
  if (command_line->HasSwitch(kActiveTabSwitch)) {
    selectors |= TreeSelector::ActiveTab;
  }

  std::string pattern_str = command_line->GetSwitchValueASCII(kPatternSwitch);
  return TreeSelector(selectors, pattern_str);
}

}  // namespace tools
