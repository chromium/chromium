// Copyright (c) Microsoft Corporation

#include <conio.h>

#include <cstdio>
#include <limits>
#include <optional>
#include <vector>

#include "base/logging.h"
#include "third_party/win_virtual_display/controller/display_driver_controller.h"

namespace {
using display::test::DisplayDriverController;
using display::test::DriverProperties;
using display::test::MonitorConfig;
}  // namespace

int __cdecl main(int argc, char** argv) {
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  printf(
      "\nPress 'x' to exit or a number to add/remove the virtual displays:\n"
      "  - 0 => Remove all displays\n  - 1 => 1024x768\n  - 2 => 1440x900\n"
      "  - 3 => 1920x1080\n  - 4 => 1920x1080 @ 120Hz\n  - 5 => 2560x1440\n"
      "  - 6 => 2560x1440 @ 120Hz\n");

  int monitor_id = 0;
  DisplayDriverController controller;
  std::vector<MonitorConfig> monitor_configs;
  while (true) {
    // Wait for key press
    int key = _getch();

    if (key == 'x' || key == 'X') {
      break;
    }

    if (key == '0') {
      LOG(INFO) << "Destroying display(s)";
      controller.Reset();
      monitor_configs.clear();
      continue;
    }

    std::optional<MonitorConfig> monitor_config;
    switch (key) {
      case '1':
        LOG(INFO) << "Creating display for 1024x768";
        monitor_config = MonitorConfig::k1024x768;
        break;
      case '2':
        LOG(INFO) << "Creating display for 1440X900";
        monitor_config = MonitorConfig::k1440x900;
        break;
      case '3':
        LOG(INFO) << "Creating display for 1920x1080 @ 60Hz";
        monitor_config = MonitorConfig::k1920x1080;
        break;
      case '4':
        LOG(INFO) << "Creating display for 1920x1080 @ 120Hz";
        monitor_config = MonitorConfig::k1920x1080_120;
        break;
      case '5':
        LOG(INFO) << "Creating display for 2560x1440 @ 60Hz";
        monitor_config = MonitorConfig::k2560x1440;
        break;
      case '6':
        LOG(INFO) << "Creating display for 2560x1440 @ 120Hz";
        monitor_config = MonitorConfig::k2560x1440_120;
        break;
    }

    if (monitor_config.has_value()) {
      monitor_config->set_product_code(
          (monitor_id++) % std::numeric_limits<unsigned short>::max());
      monitor_configs.push_back(*monitor_config);
      if (!controller.SetDisplayConfig(DriverProperties(monitor_configs))) {
        LOG(ERROR) << "Failed to set display configuration, are you running "
                   << "this tool with elevated privileges (admin)?";
      }
    }
  }

  return 0;
}
