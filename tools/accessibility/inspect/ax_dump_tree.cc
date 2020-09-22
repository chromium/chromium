// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "tools/accessibility/inspect/ax_tree_server.h"

using TreeSelector = content::AccessibilityTreeFormatter::TreeSelector;

char kIdSwitch[] =
#if defined(WINDOWS)
    "window";
#else
    "pid";
#endif
char kPatternSwitch[] = "pattern";
char kFiltersSwitch[] = "filters";
char kJsonSwitch[] = "json";
char kHelpSwitch[] = "help";

char kActiveTabSwitch[] = "active-tab";
char kChromeSwitch[] = "chrome";
char kChromiumSwitch[] = "chromium";
char kFirefoxSwitch[] = "firefox";
char kSafariSwitch[] = "safari";

// Convert from string to int, whether in 0x hex format or decimal format.
bool StringToInt(std::string str, unsigned* result) {
  if (str.empty())
    return false;
  bool is_hex =
      str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X');
  return is_hex ? base::HexStringToUInt(str, result)
                : base::StringToUint(str, result);
}

bool AXDumpTreeLogMessageHandler(int severity,
                                 const char* file,
                                 int line,
                                 size_t message_start,
                                 const std::string& str) {
  printf("%s", str.substr(message_start).c_str());
  return true;
}

gfx::AcceleratedWidget CastToAcceleratedWidget(unsigned window_id) {
#if defined(USE_OZONE) || defined(USE_X11) || defined(OS_MAC)
  return static_cast<gfx::AcceleratedWidget>(window_id);
#else
  return reinterpret_cast<gfx::AcceleratedWidget>(window_id);
#endif
}

void PrintHelp() {
  printf(
      "ax_dump_tree is a tool designed to dump platform accessible trees "
      "of running applications.\n");
  printf("\nusage: ax_dump_tree <options>\n");
  printf("options:\n");
#if defined(WINDOWS)
  printf("  --window\tHWND of a window to dump accessible tree for\n");
#else
  printf(
      "  --pid\t\tprocess id of an application to dump accessible tree for\n");
#endif
  printf("  --pattern\ttitle of an application to dump accessible tree for\n");
  printf("  pre-defined application selectors to dump accessible tree for:\n");
  printf("    --chrome\tChrome browser\n");
  printf("    --chromium\tChromium browser\n");
  printf("    --firefox\tFirefox browser\n");
  printf("    --safari\tSafari browser\n");
  printf("    --active-tab\tActive tab of a choosen browser\n");
  printf(
      "  --filters\tfile containing property filters used to filter out\n"
      "  \t\taccessible tree, see example-tree-filters.txt as an example\n");
  printf("  --json\toutputs tree in JSON format\n");
}

int main(int argc, char** argv) {
  logging::SetLogMessageHandler(AXDumpTreeLogMessageHandler);

  base::AtExitManager at_exit_manager;

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kHelpSwitch)) {
    PrintHelp();
    return 0;
  }

  base::FilePath filters_path =
      command_line->GetSwitchValuePath(kFiltersSwitch);

  bool use_json = command_line->HasSwitch(kJsonSwitch);

  std::string id_str = command_line->GetSwitchValueASCII(kIdSwitch);
  if (!id_str.empty()) {
    unsigned hwnd_or_pid;
    if (!StringToInt(id_str, &hwnd_or_pid)) {
      LOG(ERROR) << "* Error: Could not convert window id string to integer.";
      return 1;
    }
    gfx::AcceleratedWidget widget(CastToAcceleratedWidget(hwnd_or_pid));

    std::unique_ptr<content::AXTreeServer> server(
        new content::AXTreeServer(widget, filters_path, use_json));
    return 0;
  }

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
  if (selectors != TreeSelector::None || !pattern_str.empty()) {
    std::unique_ptr<content::AXTreeServer> server(new content::AXTreeServer(
        TreeSelector(selectors, pattern_str), filters_path, use_json));
    return 0;
  }

  LOG(ERROR)
      << "* Error: no accessible tree was identified to dump. Run with --help "
         "for help.";
  return 1;
}
