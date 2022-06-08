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
  QtShim(QtInterface::Delegate* delegate, int* argc, char** argv);

  ~QtShim() override;

  // QtInterface:
  double GetScaleFactor() const override;
  FontRenderParams GetFontRenderParams() const override;
  FontDescription GetFontDescription() const override;
  Image GetIconForContentType(const String& content_type,
                              int size) const override;
  SkColor GetColor(ColorType role, ColorState state) const override;

 private slots:
  void FontChanged(const QFont& font);

 private:
  QtInterface::Delegate* const delegate_;

  QApplication app_;
};

}  // namespace qt

#endif  // UI_QT_QT_SHIM_H_
