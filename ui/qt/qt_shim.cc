// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// IMPORTANT NOTE: All QtShim members that use `delegate_` must be decorated
// with DISABLE_CFI_VCALL.

#include "ui/qt/qt_shim.h"

#include <cmath>

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPainter>
#include <QPalette>
#include <QScreen>
#include <QStyle>
#include <QStyleOptionTitleBar>

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

// Obtain the average color of a gradient.
SkColor GradientColor(const QGradient& gradient) {
  QGradientStops stops = gradient.stops();
  if (stops.empty()) {
    return qRgba(0, 0, 0, 0);
  }

  float a = 0;
  float r = 0;
  float g = 0;
  float b = 0;
  for (int i = 0; i < stops.size(); i++) {
    // Determine the extents of this stop.  The whole gradient interval is [0,
    // 1], so extend to the endpoint if this is the first or last stop.
    float left_interval =
        i == 0 ? stops[i].first : (stops[i].first - stops[i - 1].first) / 2;
    float right_interval = i == stops.size() - 1
                               ? 1 - stops[i].first
                               : (stops[i + 1].first - stops[i].first) / 2;
    float length = left_interval + right_interval;

    // alpha() returns a value in [0, 255] and alphaF() returns a value in
    // [0, 1]. The color values are not premultiplied so the RGB channels need
    // to be multiplied by the alpha (in range [0, 1]) before summing.  The
    // alpha doesn't need to be multiplied, so we just sum color.alpha() in
    // range [0, 255] directly.
    const QColor& color = stops[i].second;
    a += color.alpha() * length;
    r += color.alphaF() * color.red() * length;
    g += color.alphaF() * color.green() * length;
    b += color.alphaF() * color.blue() * length;
  }
  return qRgba(r, g, b, a);
}

// Obtain the average color of a texture.
SkColor TextureColor(QImage image) {
  size_t size = image.width() * image.height();
  if (!size) {
    return qRgba(0, 0, 0, 0);
  }

  if (image.format() != QImage::Format_ARGB32_Premultiplied) {
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  }

  size_t a = 0;
  size_t r = 0;
  size_t g = 0;
  size_t b = 0;
  const auto* pixels = reinterpret_cast<QRgb*>(image.bits());
  for (size_t i = 0; i < size; i++) {
    auto color = QColor::fromRgba(pixels[i]);
    a += color.alpha();
    r += color.red();
    g += color.green();
    b += color.blue();
  }
  return qRgba(r / size, g / size, b / size, a / size);
}

SkColor BrushColor(const QBrush& brush) {
  QColor color = brush.color();
  auto alpha_blend = [&](uint8_t alpha) {
    QColor blended = color;
    blended.setAlpha(blended.alpha() * alpha / 255);
    return blended.rgba();
  };

  switch (brush.style()) {
    case Qt::SolidPattern:
      return alpha_blend(0xFF);
    case Qt::Dense1Pattern:
      return alpha_blend(0xE0);
    case Qt::Dense2Pattern:
      return alpha_blend(0xC0);
    case Qt::Dense3Pattern:
      return alpha_blend(0xA0);
    case Qt::Dense4Pattern:
      return alpha_blend(0x80);
    case Qt::Dense5Pattern:
      return alpha_blend(0x60);
    case Qt::Dense6Pattern:
      return alpha_blend(0x40);
    case Qt::Dense7Pattern:
      return alpha_blend(0x20);
    case Qt::NoBrush:
      return alpha_blend(0x00);
    case Qt::HorPattern:
    case Qt::VerPattern:
      return alpha_blend(0x20);
    case Qt::CrossPattern:
      return alpha_blend(0x40);
    case Qt::BDiagPattern:
    case Qt::FDiagPattern:
      return alpha_blend(0x20);
    case Qt::DiagCrossPattern:
      return alpha_blend(0x40);
    case Qt::LinearGradientPattern:
    case Qt::RadialGradientPattern:
    case Qt::ConicalGradientPattern:
      return GradientColor(*brush.gradient());
    case Qt::TexturePattern:
      return TextureColor(brush.textureImage());
  }
}

QPalette::ColorRole ColorTypeToColorRole(ColorType type) {
  switch (type) {
    case ColorType::kWindowBg:
      return QPalette::Window;
    case ColorType::kWindowFg:
      return QPalette::WindowText;
    case ColorType::kHighlightBg:
      return QPalette::Highlight;
    case ColorType::kHighlightFg:
      return QPalette::HighlightedText;
    case ColorType::kEntryBg:
      return QPalette::Base;
    case ColorType::kEntryFg:
      return QPalette::Text;
    case ColorType::kButtonBg:
      return QPalette::Button;
    case ColorType::kButtonFg:
      return QPalette::ButtonText;
    case ColorType::kLight:
      return QPalette::Light;
    case ColorType::kMidlight:
      return QPalette::Midlight;
    case ColorType::kMidground:
      return QPalette::Mid;
    case ColorType::kDark:
      return QPalette::Dark;
    case ColorType::kShadow:
      return QPalette::Shadow;
  }
}

QPalette::ColorGroup ColorStateToColorGroup(ColorState state) {
  switch (state) {
    case ColorState::kNormal:
      return QPalette::Normal;
    case ColorState::kDisabled:
      return QPalette::Disabled;
    case ColorState::kInactive:
      return QPalette::Inactive;
  }
}

float GetScreenScale(const QScreen* screen) {
  constexpr double kDefaultPixelDpi = 96.0;
  double scale = screen->devicePixelRatio() * screen->logicalDotsPerInch() /
                 kDefaultPixelDpi;
  // Round to the nearest 1/16th so that UI can losslessly multiply and divide
  // by the scale factor using floating point arithmetic.  GtkUi also rounds
  // in this way, but to 1/64th.  1/16th is chosen here since that's what
  // KDE settings uses.
  scale = std::round(scale * 16) / 16;
  return scale > 0 ? scale : 1.0;
}

}  // namespace

QtShim::QtShim(QtInterface::Delegate* delegate, int* argc, char** argv)
    : delegate_(delegate), app_(*argc, argv) {
  connect(&app_, SIGNAL(fontChanged(const QFont&)), this,
          SLOT(FontChanged(const QFont&)));
  connect(&app_, SIGNAL(paletteChanged(const QPalette&)), this,
          SLOT(PaletteChanged(const QPalette&)));
  connect(&app_, SIGNAL(screenAdded(QScreen*)), this,
          SLOT(ScreenAdded(QScreen*)));
  connect(&app_, SIGNAL(screenRemoved(QScreen*)), this,
          SLOT(ScreenRemoved(QScreen*)));
  for (QScreen* screen : app_.screens()) {
    ScreenAdded(screen);
  }
}

QtShim::~QtShim() = default;

size_t QtShim::GetMonitorConfig(MonitorScale** monitors, float* primary_scale) {
  size_t n_monitors = app_.screens().size();
  monitor_scales_.resize(n_monitors);
  for (size_t i = 0; i < n_monitors; i++) {
    const QScreen* screen = app_.screens()[i];
    MonitorScale monitor = monitor_scales_[i];
    monitor.x_px = screen->geometry().x();
    monitor.y_px = screen->geometry().y();
    monitor.width_px = screen->geometry().width();
    monitor.height_px = screen->geometry().height();
    monitor.scale = GetScreenScale(screen);
  }
  *monitors = monitor_scales_.data();
  *primary_scale = GetScreenScale(app_.primaryScreen());
  return n_monitors;
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

Image QtShim::GetIconForContentType(const String& content_type,
                                    int size) const {
  QMimeDatabase db;
  for (const char* mime : {content_type.c_str(), "application/octet-stream"}) {
    auto mt = db.mimeTypeForName(mime);
    for (const auto& name : {mt.iconName(), mt.genericIconName()}) {
      auto icon = QIcon::fromTheme(name);
      auto pixmap = icon.pixmap(size);
      auto image = pixmap.toImage();
      if (image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
      }
      if (auto bytes = image.sizeInBytes()) {
        return {image.width(), image.height(),
                static_cast<float>(image.devicePixelRatio()),
                Buffer(image.bits(), bytes)};
      }
    }
  }
  return {};
}

SkColor QtShim::GetColor(ColorType role, ColorState state) const {
  return BrushColor(app_.palette().brush(ColorStateToColorGroup(state),
                                         ColorTypeToColorRole(role)));
}

SkColor QtShim::GetFrameColor(ColorState state, bool use_custom_frame) const {
  constexpr int kSampleSize = 32;
  return TextureColor(DrawHeaderImpl(kSampleSize, kSampleSize,
                                     GetColor(ColorType::kWindowBg, state),
                                     state, use_custom_frame));
}

int QtShim::GetCursorBlinkIntervalMs() const {
  return app_.cursorFlashTime();
}

int QtShim::GetAnimationDurationMs() const {
  return app_.style()->styleHint(QStyle::SH_Widget_Animation_Duration);
}

DISABLE_CFI_VCALL
void QtShim::FontChanged(const QFont& font) {
  delegate_->FontChanged();
}

DISABLE_CFI_VCALL
void QtShim::PaletteChanged(const QPalette& palette) {
  delegate_->ThemeChanged();
}

DISABLE_CFI_VCALL
void QtShim::ScreenAdded(QScreen* screen) {
  connect(screen, SIGNAL(logicalDotsPerInchChanged(qreal)), this,
          SLOT(LogicalDotsPerInchChanged(qreal)));
  connect(screen, SIGNAL(physicalDotsPerInchChanged(qreal)), this,
          SLOT(PhysicalDotsPerInchChanged(qreal)));
  delegate_->ScaleFactorMaybeChanged();
}

DISABLE_CFI_VCALL
void QtShim::ScreenRemoved(QScreen* screen) {
  delegate_->ScaleFactorMaybeChanged();
}

DISABLE_CFI_VCALL
void QtShim::LogicalDotsPerInchChanged(qreal dpi) {
  delegate_->ScaleFactorMaybeChanged();
}

DISABLE_CFI_VCALL
void QtShim::PhysicalDotsPerInchChanged(qreal dpi) {
  delegate_->ScaleFactorMaybeChanged();
}

Image QtShim::DrawHeader(int width,
                         int height,
                         SkColor default_color,
                         ColorState state,
                         bool use_custom_frame) const {
  QImage image =
      DrawHeaderImpl(width, height, default_color, state, use_custom_frame);
  return {width, height, 1.0f, Buffer(image.bits(), image.sizeInBytes())};
}

QImage QtShim::DrawHeaderImpl(int width,
                              int height,
                              SkColor default_color,
                              ColorState state,
                              bool use_custom_frame) const {
  QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
  image.fill(default_color);
  QPainter painter(&image);
  if (use_custom_frame) {
    // Chrome renders it's own window border, so clip the border out by
    // rendering the titlebar larger than the image.
    constexpr int kBorderWidth = 5;

    QStyleOptionTitleBar opt;
    opt.rect = QRect(-kBorderWidth, -kBorderWidth, width + 2 * kBorderWidth,
                     height + 2 * kBorderWidth);
    if (state == ColorState::kNormal) {
      opt.titleBarState = QStyle::State_Active;
    }
    app_.style()->drawComplexControl(QStyle::CC_TitleBar, &opt, &painter,
                                     nullptr);
  } else {
    painter.fillRect(
        0, 0, width, height,
        app_.palette().brush(ColorStateToColorGroup(state), QPalette::Window));
  }
  return image;
}

}  // namespace qt

qt::QtInterface* CreateQtInterface(qt::QtInterface::Delegate* delegate,
                                   int* argc,
                                   char** argv) {
  return new qt::QtShim(delegate, argc, argv);
}
