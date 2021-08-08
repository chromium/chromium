// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_utils.h"

#include "base/command_line.h"

char kActiveTabSwitch[] = "active-tab";
char kChromeSwitch[] = "chrome";
char kChromiumSwitch[] = "chromium";
char kFirefoxSwitch[] = "firefox";
char kEdgeSwitch[] = "edge";
char kPatternSwitch[] = "pattern";
char kSafariSwitch[] = "safari";

using ui::AXTreeSelector;

namespace tools {

void PrintHelpForTreeSelectors() {
  printf("  --pattern\ttitle of an application to dump accessible tree for\n");
  printf("  pre-defined application selectors to dump accessible tree for:\n");
  printf("    --chrome\tChrome browser\n");
  printf("    --chromium\tChromium browser\n");
#if defined(WINDOWS)
  printf("    --edge\tEdge browser\n");
#endif
  printf("    --firefox\tFirefox browser\n");
#if defined(MACOS)
  printf("    --safari\tSafari browser\n");
#endif
  printf("    --active-tab\tActive tab of a choosen browser\n");
}

AXTreeSelector TreeSelectorFromCommandLine(
    const base::CommandLine* command_line) {
  int selectors = AXTreeSelector::None;
  if (command_line->HasSwitch(kChromeSwitch)) {
    selectors = AXTreeSelector::Chrome;
  } else if (command_line->HasSwitch(kChromiumSwitch)) {
    selectors = AXTreeSelector::Chromium;
  } else if (command_line->HasSwitch(kEdgeSwitch)) {
    selectors = AXTreeSelector::Edge;
  } else if (command_line->HasSwitch(kFirefoxSwitch)) {
    selectors = AXTreeSelector::Firefox;
  } else if (command_line->HasSwitch(kSafariSwitch)) {
    selectors = AXTreeSelector::Safari;
  }
  if (command_line->HasSwitch(kActiveTabSwitch)) {
    selectors |= AXTreeSelector::ActiveTab;
  }

  std::string pattern_str = command_line->GetSwitchValueASCII(kPatternSwitch);
  return AXTreeSelector(selectors, pattern_str);
}

}  // namespace tools
