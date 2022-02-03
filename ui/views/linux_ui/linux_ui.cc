// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/linux_ui/linux_ui.h"

#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/gfx/skia_font_delegate.h"
#include "ui/shell_dialogs/shell_dialog_linux.h"

namespace {

views::LinuxUI* g_linux_ui = nullptr;

}  // namespace

namespace views {

void LinuxUI::SetInstance(std::unique_ptr<LinuxUI> instance) {
  delete g_linux_ui;
  g_linux_ui = instance.release();

  SkiaFontDelegate::SetInstance(g_linux_ui);
  ShellDialogLinux::SetInstance(g_linux_ui);
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

}  // namespace views
