// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/linux_ui/linux_ui.h"

#include <cstdio>

#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/gfx/skia_font_delegate.h"

namespace {

views::LinuxUI* g_linux_ui = nullptr;

}  // namespace

namespace views {

void LinuxUI::SetInstance(std::unique_ptr<LinuxUI> instance) {
  delete g_linux_ui;
  g_linux_ui = instance.release();

  SkiaFontDelegate::SetInstance(g_linux_ui);
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMECAST)
  ShellDialogLinux::SetInstance(g_linux_ui);
#endif
  ui::SetTextEditKeyBindingsDelegate(g_linux_ui);

  // Do not set IME instance for ozone as we delegate creating the input method
  // to OzonePlatforms instead. If this is set, OzonePlatform never sets a
  // context factory.
}

LinuxUI* LinuxUI::instance() {
  return g_linux_ui;
}

LinuxUI::LinuxUI() = default;

LinuxUI::~LinuxUI() = default;

LinuxUI::CmdLineArgs::CmdLineArgs() = default;

LinuxUI::CmdLineArgs::CmdLineArgs(const CmdLineArgs&) = default;

LinuxUI::CmdLineArgs& LinuxUI::CmdLineArgs::operator=(const CmdLineArgs&) =
    default;

LinuxUI::CmdLineArgs::~CmdLineArgs() = default;

// static
LinuxUI::CmdLineArgs LinuxUI::CopyCmdLine(
    const base::CommandLine& command_line) {
  const auto& argv = command_line.argv();
  size_t args_chars = 0;
  for (const auto& arg : argv)
    args_chars += arg.size() + 1;

  CmdLineArgs cmd_line;
  cmd_line.args = std::vector<char>(args_chars);
  char* dst = cmd_line.args.data();
  for (const auto& arg : argv) {
    cmd_line.argv.push_back(dst);
    snprintf(dst, &cmd_line.args.back() + 1 - dst, "%s", arg.c_str());
    dst += arg.size() + 1;
  }
  cmd_line.argc = cmd_line.argv.size();

  return cmd_line;
}

}  // namespace views
