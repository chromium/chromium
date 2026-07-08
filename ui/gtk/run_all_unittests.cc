// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/linux/linux_ui.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  // Load GTK libraries. Since we run tests in xvfb, load X11 backend.
  if (!gtk::LoadGtk(ui::LinuxUiBackend::kX11)) {
    return 1;
  }

  // Initialize GTK
  if (!gtk::GtkInitCheck(&argc, argv)) {
    return 1;
  }

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
