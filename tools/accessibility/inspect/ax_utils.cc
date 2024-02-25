// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_utils.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
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

char kFiltersSwitch[] = "filters";

#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_MAC)
char kIdSwitch[] = "pid";
#else
char kIdSwitch[] = "window";
#endif  // defined(WINDOWS)

using ui::AXTreeSelector;

gfx::AcceleratedWidget CastToAcceleratedWidget(unsigned int window_id) {
#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_MAC)
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

void PrintHelpShared() {
  printf("options:\n");
  PrintHelpTreeSelectors();
  PrintHelpFilters();

  PrintHelpFooter();
}

void PrintHelpTreeSelectors() {
  printf("  --pattern\ttitle of an application\n");
#if defined(WINDOWS)
  printf("  --window\tHWND of a window\n");
#else
  printf("  --pid\t\tprocess id of an application\n");
#endif
  printf("  pre-defined application selectors:\n");
  printf("    --chrome\tChrome browser\n");
  printf("    --chromium\tChromium browser\n");
#if defined(WINDOWS)
  printf("    --edge\tEdge browser\n");
#endif
  printf("    --firefox\tFirefox browser\n");
#if BUILDFLAG(IS_MAC)
  printf("    --safari\tSafari browser\n");
#endif
  printf(
      "  --active-tab\tactive tab of browser, if application is a browser\n");
}

void PrintHelpFilters() {
  printf(
      "  --filters\tfile containing property filters used to filter out\n"
      "  \t\taccessible tree, for example:\n"
      "  \t\t--filters=/absolute/path/to/filters/file\n");
}

void PrintHelpFooter() {
  printf(
      "\nmore info at "
      "https://www.chromium.org/developers/accessibility/testing/"
      "automated-testing/ax-inspect\n");
}

std::optional<AXTreeSelector> TreeSelectorFromCommandLine(
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
      LOG(ERROR) << "Error: can't convert window id string to integer.";
      return std::nullopt;
    }
    return AXTreeSelector(selectors, pattern_str,
                          CastToAcceleratedWidget(hwnd_or_pid));
  }
  return AXTreeSelector(selectors, pattern_str);
}

std::string DirectivePrefixFromAPIType(ui::AXApiType::Type api) {
  switch (api) {
    case ui::AXApiType::kMac:
      return "@AXAPI-";
    case ui::AXApiType::kLinux:
      return "@ATSPI-";
    case ui::AXApiType::kWinIA2:
      return "@IA2-";
    case ui::AXApiType::kWinUIA:
      return "@UIA-";
    // If no or unsupported API, use the generic prefix
    default:
      return "@";
  }
}

std::optional<ui::AXInspectScenario> ScenarioFromCommandLine(
    const base::CommandLine& command_line,
    ui::AXApiType::Type api) {
  base::FilePath filters_path = command_line.GetSwitchValuePath(kFiltersSwitch);
  if (filters_path.empty() && command_line.HasSwitch(kFiltersSwitch)) {
    LOG(ERROR) << "Error: empty filter path given. Run with --help for help.";
    return std::nullopt;
  }

  std::string directive_prefix = DirectivePrefixFromAPIType(api);

  // Return with the default filter scenario if no file is provided.
  if (filters_path.empty()) {
    return ui::AXInspectScenario::From(directive_prefix,
                                       std::vector<std::string>());
  }

  std::optional<ui::AXInspectScenario> scenario =
      ui::AXInspectScenario::From(directive_prefix, filters_path);
  if (!scenario) {
    LOG(ERROR) << "Error: failed to open filters file " << filters_path
               << ". Note: path traversal components ('..') are not allowed "
                  "for security reasons";
    return std::nullopt;
  }
  return scenario;
}

}  // namespace tools
