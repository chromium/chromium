// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/qt/qt_shim.h"

#include <stdio.h>

#include <QApplication>

namespace qt {

QtShim::QtShim(int* argc, char** argv) : app_(*argc, argv) {
  connect(&app_, SIGNAL(fontChanged(const QFont&)), this,
          SLOT(FontChanged(const QFont&)));
}

QtShim::~QtShim() = default;

double QtShim::GetScaleFactor() const {
  return app_.devicePixelRatio();
}

void QtShim::FontChanged(const QFont& font) {
  // TODO(thomasanderson): implement this.
}

}  // namespace qt

qt::QtInterface* CreateQtInterface(int* argc, char** argv) {
  return new qt::QtShim(argc, argv);
}
