// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_QT_SHIM_H_
#define UI_QT_QT_SHIM_H_

#include <QApplication>
#include <QObject>

#include "ui/qt/qt_interface.h"

namespace qt {

// This class directly interacts with QT.  It's required to be a QObject
// to receive signals from QT via slots.
class QtShim : public QObject, public QtInterface {
  Q_OBJECT

 public:
  QtShim(int* argc, char** argv);

  ~QtShim() override;

  // QtShim:
  double GetScaleFactor() const override;

 private slots:
  void FontChanged(const QFont& font);

 private:
  QApplication app_;
};

}  // namespace qt

#endif  // UI_QT_QT_SHIM_H_
