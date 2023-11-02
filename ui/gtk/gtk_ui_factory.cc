// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_ui_factory.h"

#include "ui/gtk/gtk_ui.h"

std::unique_ptr<ui::LinuxUiAndTheme> BuildGtkUi() {
  return std::make_unique<gtk::GtkUi>();
}
