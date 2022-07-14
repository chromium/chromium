// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/linux_ui_factory.h"

#include <utility>

#include "ui/base/buildflags.h"
#include "ui/linux/linux_ui.h"

#if BUILDFLAG(USE_GTK)
#include "ui/gtk/gtk_ui_factory.h"
#endif
#if BUILDFLAG(USE_QT)
#include "ui/qt/qt_ui.h"
#endif

namespace ui {

std::unique_ptr<LinuxUi> CreateLinuxUi() {
  // TODO(thomasanderson): LinuxUI backend should be chosen depending on the
  // environment.
#if BUILDFLAG(USE_QT)
  {
    std::unique_ptr<LinuxUi> fallback_linux_ui;
#if BUILDFLAG(USE_GTK)
    fallback_linux_ui = BuildGtkUi();
    if (!fallback_linux_ui->Initialize())
      fallback_linux_ui.reset();
#endif
    auto qt_ui = qt::CreateQtUi(std::move(fallback_linux_ui));
    if (qt_ui->Initialize())
      return qt_ui;
  }
#endif
#if BUILDFLAG(USE_GTK)
  {
    auto gtk_ui = BuildGtkUi();
    if (gtk_ui->Initialize())
      return gtk_ui;
  }
#endif
  return nullptr;
}

}  // namespace ui