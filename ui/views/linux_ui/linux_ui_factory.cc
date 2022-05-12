// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/linux_ui/linux_ui_factory.h"

#include "ui/base/buildflags.h"
#include "ui/views/linux_ui/linux_ui.h"

#if BUILDFLAG(USE_GTK)
#include "ui/gtk/gtk_ui_factory.h"
#endif
#if BUILDFLAG(USE_QT)
#include "ui/qt/qt_ui.h"
#endif

std::unique_ptr<views::LinuxUI> CreateLinuxUi() {
  // TODO(thomasanderson): LinuxUI backend should be chosen depending on the
  // environment.
#if BUILDFLAG(USE_QT)
  auto qt_ui = qt::CreateQtUi();
  if (qt_ui->Initialize())
    return qt_ui;
  qt_ui.reset();  // Reset to prevent 2 active LinuxUI instances.
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
