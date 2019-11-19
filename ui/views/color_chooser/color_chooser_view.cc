// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/color_chooser/color_chooser_view.h"

#include <memory>
#include <utility>

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/color_chooser/color_chooser_listener.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kHueBarWidth = 20;
constexpr int kSaturationValueSize = 200;
constexpr int kMarginWidth = 5;
constexpr int kSaturationValueIndicatorSize = 6;
constexpr int kHueIndicatorSize = 5;
constexpr int kBorderWidth = 1;
constexpr int kTextfieldLengthInChars = 14;

base::string16 GetColorText(SkColor color) {
  return base::ASCIIToUTF16(base::StringPrintf("#%02x%02x%02x",
                                               SkColorGetR(color),
                                               SkColorGetG(color),
                                               SkColorGetB(color)));
}

bool GetColorFromText(const base::string16& text, SkColor* result) {
  if (text.size() != 6 && !(text.size() == 7 && text[0] == '#'))
    return false;

  std::string input =
      base::UTF16ToUTF8((text.size() == 6) ? text : text.substr(1));
  std::array<uint8_t, 3> hex;
  if (!base::HexStringToSpan(input, hex))
    return false;

  *result = SkColorSetRGB(hex[0], hex[1], hex[2]);
  return true;
}

// A view that processes mouse events and gesture events using a common
// interface.
class LocatedEventHandlerView : public views::View {
 public:
  ~LocatedEventHandlerView() override = default;

 protected:
  LocatedEventHandlerView() = default;

  // Handles an event (mouse or gesture) at the specified location.
  virtual void ProcessEventAtLocation(const gfx::Point& location) = 0;

  // views::View overrides:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    ProcessEventAtLocation(event.location());
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    ProcessEventAtLocation(event.location());
    return true;
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP ||
        event->type() == ui::ET_GESTURE_TAP_DOWN ||
        event->IsScrollGestureEvent()) {
      ProcessEventAtLocation(event->location());
      event->SetHandled();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(LocatedEventHandlerView);
};

void DrawGradientRect(const gfx::Rect& rect, SkColor start_color,
                      SkColor end_color, bool is_horizontal,
                      gfx::Canvas* canvas) {
  SkColor colors[2] = { start_color, end_color };
  SkPoint points[2];
  points[0].iset(0, 0);
  if (is_horizontal)
    points[1].iset(rect.width() + 1, 0);
  else
    points[1].iset(0, rect.height() + 1);
  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeLinearGradient(points, colors, nullptr,
                                                      2, SkTileMode::kClamp));
  canvas->DrawRect(rect, flags);
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// ColorChooserView::HueView
//
// The class to choose the hue of the color.  It draws a vertical bar and
// the indicator for the currently selected hue.
class ColorChooserView::HueView : public LocatedEventHandlerView {
 public:
  explicit HueView(ColorChooserView* chooser_view);

  void OnHueChanged(SkScalar hue);

 private:
  // LocatedEventHandlerView overrides:
  void ProcessEventAtLocation(const gfx::Point& point) override;

  // View overrides:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  ColorChooserView* chooser_view_;
  int level_;

  DISALLOW_COPY_AND_ASSIGN(HueView);
};

ColorChooserView::HueView::HueView(ColorChooserView* chooser_view)
    : chooser_view_(chooser_view),
      level_(0) {
}

void ColorChooserView::HueView::OnHueChanged(SkScalar hue) {
  SkScalar height = SkIntToScalar(kSaturationValueSize - 1);
  SkScalar hue_max = SkIntToScalar(360);
  int level = (hue_max - hue) * height / hue_max;
  level += kBorderWidth;
  if (level_ != level) {
    level_ = level;
    SchedulePaint();
  }
}

void ColorChooserView::HueView::ProcessEventAtLocation(
    const gfx::Point& point) {
  level_ = std::max(kBorderWidth,
                    std::min(height() - 1 - kBorderWidth, point.y()));
  int base_height = kSaturationValueSize - 1;
  chooser_view_->OnHueChosen(360.f * (base_height - (level_ - kBorderWidth)) /
                             base_height);
  SchedulePaint();
}

gfx::Size ColorChooserView::HueView::CalculatePreferredSize() const {
  // We put indicators on the both sides of the hue bar.
  return gfx::Size(kHueBarWidth + kHueIndicatorSize * 2 + kBorderWidth * 2,
                   kSaturationValueSize + kBorderWidth * 2);
}

void ColorChooserView::HueView::OnPaint(gfx::Canvas* canvas) {
  SkScalar hsv[3];
  // In the hue bar, saturation and value for the color should be always 100%.
  hsv[1] = SK_Scalar1;
  hsv[2] = SK_Scalar1;

  canvas->FillRect(gfx::Rect(kHueIndicatorSize, 0,
                             kHueBarWidth + kBorderWidth, height() - 1),
                   SK_ColorGRAY);
  int base_left = kHueIndicatorSize + kBorderWidth;
  for (int y = 0; y < kSaturationValueSize; ++y) {
    hsv[0] =
        360.f * (kSaturationValueSize - 1 - y) / (kSaturationValueSize - 1);
    canvas->FillRect(gfx::Rect(base_left, y + kBorderWidth, kHueBarWidth, 1),
                     SkHSVToColor(hsv));
  }

  // Put the triangular indicators besides.
  SkPath left_indicator_path;
  SkPath right_indicator_path;
  left_indicator_path.moveTo(
      SK_ScalarHalf, SkIntToScalar(level_ - kHueIndicatorSize));
  left_indicator_path.lineTo(
      kHueIndicatorSize, SkIntToScalar(level_));
  left_indicator_path.lineTo(
      SK_ScalarHalf, SkIntToScalar(level_ + kHueIndicatorSize));
  left_indicator_path.lineTo(
      SK_ScalarHalf, SkIntToScalar(level_ - kHueIndicatorSize));
  right_indicator_path.moveTo(
      SkIntToScalar(width()) - SK_ScalarHalf,
      SkIntToScalar(level_ - kHueIndicatorSize));
  right_indicator_path.lineTo(
      SkIntToScalar(width() - kHueIndicatorSize) - SK_ScalarHalf,
      SkIntToScalar(level_));
  right_indicator_path.lineTo(
      SkIntToScalar(width()) - SK_ScalarHalf,
      SkIntToScalar(level_ + kHueIndicatorSize));
  right_indicator_path.lineTo(
      SkIntToScalar(width()) - SK_ScalarHalf,
      SkIntToScalar(level_ - kHueIndicatorSize));

  cc::PaintFlags indicator_flags;
  indicator_flags.setColor(SK_ColorBLACK);
  indicator_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(left_indicator_path, indicator_flags);
  canvas->DrawPath(right_indicator_path, indicator_flags);
}

////////////////////////////////////////////////////////////////////////////////
// ColorChooserView::SaturationValueView
//
// The class to choose the saturation and the value of the color.  It draws
// a square area and the indicator for the currently selected saturation and
// value.
class ColorChooserView::SaturationValueView : public LocatedEventHandlerView {
 public:
  explicit SaturationValueView(ColorChooserView* chooser_view);

  void OnHueChanged(SkScalar hue);
  void OnSaturationValueChanged(SkScalar saturation, SkScalar value);

 private:
  // LocatedEventHandlerView overrides:
  void ProcessEventAtLocation(const gfx::Point& point) override;

  // View overrides:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  ColorChooserView* chooser_view_;
  SkScalar hue_;
  gfx::Point marker_position_;

  DISALLOW_COPY_AND_ASSIGN(SaturationValueView);
};

ColorChooserView::SaturationValueView::SaturationValueView(
    ColorChooserView* chooser_view)
    : chooser_view_(chooser_view),
      hue_(0) {
  SetBorder(CreateSolidBorder(kBorderWidth, SK_ColorGRAY));
}

void ColorChooserView::SaturationValueView::OnHueChanged(SkScalar hue) {
  if (hue_ != hue) {
    hue_ = hue;
    SchedulePaint();
  }
}

void ColorChooserView::SaturationValueView::OnSaturationValueChanged(
    SkScalar saturation,
    SkScalar value) {
  SkScalar scalar_size = SkIntToScalar(kSaturationValueSize - 1);
  int x = SkScalarFloorToInt(saturation * scalar_size) + kBorderWidth;
  int y = SkScalarFloorToInt((SK_Scalar1 - value) * scalar_size) + kBorderWidth;
  if (gfx::Point(x, y) == marker_position_)
    return;

  marker_position_.set_x(x);
  marker_position_.set_y(y);
  SchedulePaint();
}

void ColorChooserView::SaturationValueView::ProcessEventAtLocation(
    const gfx::Point& point) {
  SkScalar scalar_size = SkIntToScalar(kSaturationValueSize - 1);
  SkScalar saturation = (point.x() - kBorderWidth) / scalar_size;
  SkScalar value = SK_Scalar1 - (point.y() - kBorderWidth) / scalar_size;
  saturation = SkScalarPin(saturation, 0, SK_Scalar1);
  value = SkScalarPin(value, 0, SK_Scalar1);
  OnSaturationValueChanged(saturation, value);
  chooser_view_->OnSaturationValueChosen(saturation, value);
}

gfx::Size ColorChooserView::SaturationValueView::CalculatePreferredSize()
    const {
  return gfx::Size(kSaturationValueSize + kBorderWidth * 2,
                   kSaturationValueSize + kBorderWidth * 2);
}

void ColorChooserView::SaturationValueView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect color_bounds = bounds();
  color_bounds.Inset(GetInsets());

  // Paints horizontal gradient first for saturation.
  SkScalar hsv[3] = { hue_, SK_Scalar1, SK_Scalar1 };
  SkScalar left_hsv[3] = { hue_, 0, SK_Scalar1 };
  DrawGradientRect(color_bounds, SkHSVToColor(255, left_hsv),
                   SkHSVToColor(255, hsv), true /* is_horizontal */, canvas);

  // Overlays vertical gradient for value.
  SkScalar hsv_bottom[3] = { 0, SK_Scalar1, 0 };
  DrawGradientRect(color_bounds, SK_ColorTRANSPARENT,
                   SkHSVToColor(255, hsv_bottom), false /* is_horizontal */,
                   canvas);

  // Draw the crosshair marker.
  // The background is very dark at the bottom of the view.  Use a white
  // marker in that case.
  SkColor indicator_color =
      (marker_position_.y() > width() * 3 / 4) ? SK_ColorWHITE : SK_ColorBLACK;
  canvas->FillRect(
      gfx::Rect(marker_position_.x(),
                marker_position_.y() - kSaturationValueIndicatorSize,
                1, kSaturationValueIndicatorSize * 2 + 1),
      indicator_color);
  canvas->FillRect(
      gfx::Rect(marker_position_.x() - kSaturationValueIndicatorSize,
                marker_position_.y(),
                kSaturationValueIndicatorSize * 2 + 1, 1),
      indicator_color);

  OnPaintBorder(canvas);
}

////////////////////////////////////////////////////////////////////////////////
// ColorChooserView::SelectedColorPatchView
//
// A view to simply show the selected color in a rectangle.
class ColorChooserView::SelectedColorPatchView : public views::View {
 public:
  SelectedColorPatchView();

  void SetColor(SkColor color);

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectedColorPatchView);
};

ColorChooserView::SelectedColorPatchView::SelectedColorPatchView() {
  SetVisible(true);
  SetBorder(CreateSolidBorder(kBorderWidth, SK_ColorGRAY));
}

void ColorChooserView::SelectedColorPatchView::SetColor(SkColor color) {
  if (!background())
    SetBackground(CreateSolidBackground(color));
  else
    background()->SetNativeControlColor(color);
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// ColorChooserView
//

ColorChooserView::ColorChooserView(ColorChooserListener* listener,
                                   SkColor initial_color)
    : listener_(listener) {
  DCHECK(listener_);

  SetBackground(CreateSolidBackground(SK_ColorLTGRAY));
  SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical,
                                  gfx::Insets(kMarginWidth), kMarginWidth));

  auto container = std::make_unique<View>();
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(), kMarginWidth));
  saturation_value_ =
      container->AddChildView(std::make_unique<SaturationValueView>(this));
  hue_ = container->AddChildView(std::make_unique<HueView>(this));
  AddChildView(std::move(container));

  auto container2 = std::make_unique<View>();
  GridLayout* layout =
      container2->SetLayoutManager(std::make_unique<views::GridLayout>());
  ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(
      GridLayout::LEADING, GridLayout::FILL, 0, GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kMarginWidth);
  columns->AddColumn(
      GridLayout::FILL, GridLayout::FILL, 1, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  auto textfield = std::make_unique<Textfield>();
  textfield->set_controller(this);
  textfield->SetDefaultWidthInChars(kTextfieldLengthInChars);
  textfield->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_COLOR_CHOOSER_HEX_INPUT));
  textfield_ = layout->AddView(std::move(textfield));
  selected_color_patch_ =
      layout->AddView(std::make_unique<SelectedColorPatchView>());
  AddChildView(std::move(container2));

  OnColorChanged(initial_color);
}

ColorChooserView::~ColorChooserView() = default;

void ColorChooserView::OnColorChanged(SkColor color) {
  SkColorToHSV(color, hsv_);
  hue_->OnHueChanged(hsv_[0]);
  saturation_value_->OnHueChanged(hsv_[0]);
  saturation_value_->OnSaturationValueChanged(hsv_[1], hsv_[2]);
  selected_color_patch_->SetColor(color);
  textfield_->SetText(GetColorText(color));
}

void ColorChooserView::OnHueChosen(SkScalar hue) {
  hsv_[0] = hue;
  SkColor color = SkHSVToColor(255, hsv_);
  if (listener_)
    listener_->OnColorChosen(color);
  saturation_value_->OnHueChanged(hue);
  selected_color_patch_->SetColor(color);
  textfield_->SetText(GetColorText(color));
}

void ColorChooserView::OnSaturationValueChosen(SkScalar saturation,
                                               SkScalar value) {
  hsv_[1] = saturation;
  hsv_[2] = value;
  SkColor color = SkHSVToColor(255, hsv_);
  if (listener_)
    listener_->OnColorChosen(color);
  selected_color_patch_->SetColor(color);
  textfield_->SetText(GetColorText(color));
}

bool ColorChooserView::CanMinimize() const {
  return false;
}

View* ColorChooserView::GetInitiallyFocusedView() {
  return textfield_;
}

ui::ModalType ColorChooserView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ColorChooserView::WindowClosing() {
  if (listener_)
    listener_->OnColorChooserDialogClosed();
}

void ColorChooserView::ContentsChanged(Textfield* sender,
                                       const base::string16& new_contents) {
  SkColor color = SK_ColorBLACK;
  if (GetColorFromText(new_contents, &color)) {
    SkColorToHSV(color, hsv_);
    if (listener_)
      listener_->OnColorChosen(color);
    hue_->OnHueChanged(hsv_[0]);
    saturation_value_->OnHueChanged(hsv_[0]);
    saturation_value_->OnSaturationValueChanged(hsv_[1], hsv_[2]);
    selected_color_patch_->SetColor(color);
  }
}

bool ColorChooserView::HandleKeyEvent(Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::ET_KEY_PRESSED ||
      (key_event.key_code() != ui::VKEY_RETURN &&
       key_event.key_code() != ui::VKEY_ESCAPE))
    return false;

  GetWidget()->Close();
  return true;
}

}  // namespace views
