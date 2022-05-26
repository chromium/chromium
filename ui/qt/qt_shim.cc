// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/qt/qt_shim.h"

#include <stdio.h>

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QMimeDatabase>
#include <QMimeType>

namespace qt {

namespace {

bool IsStyleItalic(QFont::Style style) {
  switch (style) {
    case QFont::Style::StyleNormal:
      return false;
    case QFont::Style::StyleItalic:
    // gfx::Font::FontStyle doesn't support oblique, so treat it as italic.
    case QFont::Style::StyleOblique:
      return true;
  }
}

FontHinting QtHintingToFontHinting(QFont::HintingPreference hinting) {
  switch (hinting) {
    case QFont::PreferDefaultHinting:
      return FontHinting::kDefault;
    case QFont::PreferNoHinting:
      return FontHinting::kNone;
    case QFont::PreferVerticalHinting:
      // QT treats vertical hinting as "light" for Freetype:
      // https://doc.qt.io/qt-5/qfont.html#HintingPreference-enum
      return FontHinting::kLight;
    case QFont::PreferFullHinting:
      return FontHinting::kFull;
  }
}

}  // namespace

QtShim::QtShim(QtInterface::Delegate* delegate, int* argc, char** argv)
    : delegate_(delegate), app_(*argc, argv) {
  connect(&app_, SIGNAL(fontChanged(const QFont&)), this,
          SLOT(FontChanged(const QFont&)));
}

QtShim::~QtShim() = default;

double QtShim::GetScaleFactor() const {
  return app_.devicePixelRatio();
}

FontRenderParams QtShim::GetFontRenderParams() const {
  QFont font = app_.font();
  auto style = font.styleStrategy();
  return {
      .antialiasing = !(style & QFont::StyleStrategy::NoAntialias),
      .use_bitmaps = !!(style & QFont::StyleStrategy::PreferBitmap),
      .hinting = QtHintingToFontHinting(font.hintingPreference()),
  };
}

FontDescription QtShim::GetFontDescription() const {
  QFont font = app_.font();
  return {
      .family = String(font.family().toStdString().c_str()),
      .size_pixels = font.pixelSize(),
      .size_points = font.pointSize(),
      .is_italic = IsStyleItalic(font.style()),
      .weight = font.weight(),
  };
}

Image QtShim::GetIconForContentType(const String& content_type, int size) {
  QMimeDatabase db;
  for (const char* mime : {content_type.c_str(), "application/octet-stream"}) {
    auto mt = db.mimeTypeForName(mime);
    for (const auto& name : {mt.iconName(), mt.genericIconName()}) {
      auto icon = QIcon::fromTheme(name);
      auto pixmap = icon.pixmap(size);
      auto image = pixmap.toImage();
      if (image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
      if (auto bytes = image.sizeInBytes()) {
        return {image.width(), image.height(),
                static_cast<float>(image.devicePixelRatio()),
                Buffer(image.bits(), bytes)};
      }
    }
  }
  return {};
}

void QtShim::FontChanged(const QFont& font) {
  delegate_->FontChanged();
}

}  // namespace qt

qt::QtInterface* CreateQtInterface(qt::QtInterface::Delegate* delegate,
                                   int* argc,
                                   char** argv) {
  return new qt::QtShim(delegate, argc, argv);
}
