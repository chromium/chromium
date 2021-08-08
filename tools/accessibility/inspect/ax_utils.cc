// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_utils.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

char kActiveTabSwitch[] = "active-tab";
char kChromeSwitch[] = "chrome";
char kChromiumSwitch[] = "chromium";
char kFirefoxSwitch[] = "firefox";
char kEdgeSwitch[] = "edge";
char kPatternSwitch[] = "pattern";
char kSafariSwitch[] = "safari";

#if defined(WINDOWS)
char kIdSwitch[] = "window";
#else
char kIdSwitch[] = "pid";
#endif  // defined(WINDOWS)

using ui::AXTreeSelector;

gfx::AcceleratedWidget CastToAcceleratedWidget(unsigned int window_id) {
#if defined(USE_OZONE) || defined(USE_X11) || defined(OS_MAC)
  return static_cast<gfx::AcceleratedWidget>(window_id);
#else
  return reinterpret_cast<gfx::AcceleratedWidget>(window_id);
#endif
}

// Convert from string to int, whether in 0x hex format or decimal format.
bool StringToInt(std::string str, unsigned* result) {
  if (str.empty())
    return false;
  bool is_hex =
      str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X');
  return is_hex ? base::HexStringToUInt(str, result)
                : base::StringToUint(str, result);
}

namespace tools {

void PrintHelpForTreeSelectors() {
  printf("  --pattern\ttitle of an application to dump accessible tree for\n");
#if defined(WINDOWS)
  printf("  --window\tHWND of a window to dump accessible tree for\n");
#else
  printf(
      "  --pid\t\tprocess id of an application to dump accessible tree for\n");
#endif
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
  printf(
      "  --active-tab\tActive tab of browser, if application is a browser\n");
}

absl::optional<AXTreeSelector> TreeSelectorFromCommandLine(
    const base::CommandLine& command_line) {
  int selectors = AXTreeSelector::None;
  if (command_line.HasSwitch(kChromeSwitch)) {
    selectors = AXTreeSelector::Chrome;
  } else if (command_line.HasSwitch(kChromiumSwitch)) {
    selectors = AXTreeSelector::Chromium;
  } else if (command_line.HasSwitch(kEdgeSwitch)) {
    selectors = AXTreeSelector::Edge;
  } else if (command_line.HasSwitch(kFirefoxSwitch)) {
    selectors = AXTreeSelector::Firefox;
  } else if (command_line.HasSwitch(kSafariSwitch)) {
    selectors = AXTreeSelector::Safari;
  }
  if (command_line.HasSwitch(kActiveTabSwitch)) {
    selectors |= AXTreeSelector::ActiveTab;
  }
  std::string pattern_str = command_line.GetSwitchValueASCII(kPatternSwitch);
  std::string id_str = command_line.GetSwitchValueASCII(kIdSwitch);

  if (!id_str.empty()) {
    unsigned hwnd_or_pid = 0;
    if (!StringToInt(id_str, &hwnd_or_pid)) {
      LOG(ERROR) << "* Error: Could not convert window id string to integer.";
      return absl::nullopt;
    }
    gfx::AcceleratedWidget widget(CastToAcceleratedWidget(hwnd_or_pid));
    return AXTreeSelector(selectors, pattern_str, widget);
  }
  return AXTreeSelector(selectors, pattern_str);
}

}  // namespace tools
