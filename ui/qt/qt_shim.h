// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_QT_SHIM_H_
#define UI_QT_QT_SHIM_H_

#include <vector>

#include <QApplication>
#include <QImage>
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
  size_t GetMonitorConfig(MonitorScale** monitors,
                          float* primary_scale) override;
  FontRenderParams GetFontRenderParams() const override;
  FontDescription GetFontDescription() const override;
  Image GetIconForContentType(const String& content_type,
                              int size) const override;
  SkColor GetColor(ColorType role, ColorState state) const override;
  SkColor GetFrameColor(ColorState state, bool use_custom_frame) const override;
  Image DrawHeader(int width,
                   int height,
                   SkColor default_color,
                   ColorState state,
                   bool use_custom_frame) const override;
  int GetCursorBlinkIntervalMs() const override;
  int GetAnimationDurationMs() const override;

 private slots:
  void FontChanged(const QFont& font);
  void PaletteChanged(const QPalette& palette);
  void ScreenAdded(QScreen* screen);
  void ScreenRemoved(QScreen* screen);
  void LogicalDotsPerInchChanged(qreal dpi);
  void PhysicalDotsPerInchChanged(qreal dpi);

 private:
  QImage DrawHeaderImpl(int width,
                        int height,
                        SkColor default_color,
                        ColorState state,
                        bool use_custom_frame) const;
  QtInterface::Delegate* const delegate_;

  QApplication app_;
  std::vector<MonitorScale> monitor_scales_;
};

}  // namespace qt

#endif  // UI_QT_QT_SHIM_H_
