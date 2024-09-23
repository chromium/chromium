// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/color_chooser/color_chooser_view.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/color_chooser/color_chooser_listener.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

constexpr int kHueBarWidth = 20;
constexpr int kSaturationValueSize = 200;
constexpr int kMarginWidth = 5;
constexpr int kSaturationValueIndicatorSize = 6;
constexpr int kHueIndicatorSize = 5;
constexpr int kBorderWidth = 1;
constexpr int kTextfieldLengthInChars = 14;

std::u16string GetColorText(SkColor color) {
  return base::ASCIIToUTF16(skia::SkColorToHexString(color));
}

bool GetColorFromText(const std::u16string& text, SkColor* result) {
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
  METADATA_HEADER(LocatedEventHandlerView, views::View)

 public:
  LocatedEventHandlerView(const LocatedEventHandlerView&) = delete;
  LocatedEventHandlerView& operator=(const LocatedEventHandlerView&) = delete;
  ~LocatedEventHandlerView() override = default;

 protected:
  LocatedEventHandlerView() = default;

  // Handles an event (mouse or gesture) at the specified location.
  virtual void ProcessEventAtLocation(const gfx::Point& location) = 0;

  // views::View
  bool OnMousePressed(const ui::MouseEvent& event) override {
    ProcessEventAtLocation(event.location());
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    ProcessEventAtLocation(event.location());
    return true;
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::EventType::kGestureTap ||
        event->type() == ui::EventType::kGestureTapDown ||
        event->IsScrollGestureEvent()) {
      ProcessEventAtLocation(event->location());
      event->SetHandled();
    }
  }
};

BEGIN_METADATA(LocatedEventHandlerView)
END_METADATA

void DrawGradientRect(const gfx::Rect& rect,
                      SkColor start_color,
                      SkColor end_color,
                      bool is_horizontal,
                      gfx::Canvas* canvas) {
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  SkColor4f colors[2] = {SkColor4f::FromColor(start_color),
                         SkColor4f::FromColor(end_color)};
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
// HueView
//
// The class to choose the hue of the color.  It draws a vertical bar and
// the indicator for the currently selected hue.
class HueView : public LocatedEventHandlerView {
  METADATA_HEADER(HueView, LocatedEventHandlerView)

 public:
  using HueChangedCallback = base::RepeatingCallback<void(SkScalar)>;
  explicit HueView(const HueChangedCallback& changed_callback);
  HueView(const HueView&) = delete;
  HueView& operator=(const HueView&) = delete;
  ~HueView() override = default;

  // views::View
  void OnThemeChanged() override;

  void OnHueChanged(SkScalar hue);

 private:
  // LocatedEventHandlerView
  void ProcessEventAtLocation(const gfx::Point& point) override;

  // views::View
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  void OnPaint(gfx::Canvas* canvas) override;

  HueChangedCallback changed_callback_;
  int level_ = 0;
  SkColor background_color_;
  SkColor indicator_color_;
};

HueView::HueView(const HueChangedCallback& changed_callback)
    : changed_callback_(changed_callback) {}

void HueView::OnThemeChanged() {
  LocatedEventHandlerView::OnThemeChanged();
  background_color_ = GetColorProvider()->GetColor(ui::kColorWindowBackground);
  indicator_color_ = GetColorProvider()->GetColor(ui::kColorMenuDropmarker);
  SchedulePaint();
}

void HueView::OnHueChanged(SkScalar hue) {
  SkScalar height = SkIntToScalar(kSaturationValueSize - 1);
  SkScalar hue_max = SkIntToScalar(360);
  int level = (hue_max - hue) * height / hue_max;
  level += kBorderWidth;
  if (level_ != level) {
    level_ = level;
    SchedulePaint();
  }
}

void HueView::ProcessEventAtLocation(const gfx::Point& point) {
  level_ =
      std::max(kBorderWidth, std::min(height() - 1 - kBorderWidth, point.y()));
  int base_height = kSaturationValueSize - 1;
  changed_callback_.Run(360.f * (base_height - (level_ - kBorderWidth)) /
                        base_height);
  SchedulePaint();
}

gfx::Size HueView::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  // We put indicators on the both sides of the hue bar.
  return gfx::Size(kHueBarWidth + kHueIndicatorSize * 2 + kBorderWidth * 2,
                   kSaturationValueSize + kBorderWidth * 2);
}

void HueView::OnPaint(gfx::Canvas* canvas) {
  SkScalar hsv[3];
  // In the hue bar, saturation and value for the color should be always 100%.
  hsv[1] = SK_Scalar1;
  hsv[2] = SK_Scalar1;

  canvas->FillRect(gfx::Rect(kHueIndicatorSize, 0, kHueBarWidth + kBorderWidth,
                             height() - 1),
                   background_color_);
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
  left_indicator_path.moveTo(SK_ScalarHalf,
                             SkIntToScalar(level_ - kHueIndicatorSize));
  left_indicator_path.lineTo(kHueIndicatorSize, SkIntToScalar(level_));
  left_indicator_path.lineTo(SK_ScalarHalf,
                             SkIntToScalar(level_ + kHueIndicatorSize));
  left_indicator_path.lineTo(SK_ScalarHalf,
                             SkIntToScalar(level_ - kHueIndicatorSize));
  right_indicator_path.moveTo(SkIntToScalar(width()) - SK_ScalarHalf,
                              SkIntToScalar(level_ - kHueIndicatorSize));
  right_indicator_path.lineTo(
      SkIntToScalar(width() - kHueIndicatorSize) - SK_ScalarHalf,
      SkIntToScalar(level_));
  right_indicator_path.lineTo(SkIntToScalar(width()) - SK_ScalarHalf,
                              SkIntToScalar(level_ + kHueIndicatorSize));
  right_indicator_path.lineTo(SkIntToScalar(width()) - SK_ScalarHalf,
                              SkIntToScalar(level_ - kHueIndicatorSize));

  cc::PaintFlags indicator_flags;
  indicator_flags.setColor(indicator_color_);
  indicator_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(left_indicator_path, indicator_flags);
  canvas->DrawPath(right_indicator_path, indicator_flags);
}

BEGIN_METADATA(HueView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// SaturationValueView
//
// The class to choose the saturation and the value of the color.  It draws
// a square area and the indicator for the currently selected saturation and
// value.
class SaturationValueView : public LocatedEventHandlerView {
  METADATA_HEADER(SaturationValueView, LocatedEventHandlerView)

 public:
  using SaturationValueChangedCallback =
      base::RepeatingCallback<void(SkScalar, SkScalar)>;
  explicit SaturationValueView(
      const SaturationValueChangedCallback& changed_callback);
  SaturationValueView(const SaturationValueView&) = delete;
  SaturationValueView& operator=(const SaturationValueView&) = delete;
  ~SaturationValueView() override = default;

  // views::View
  void OnThemeChanged() override;

  void OnHueChanged(SkScalar hue);
  void OnSaturationValueChanged(SkScalar saturation, SkScalar value);

 private:
  // LocatedEventHandlerView
  void ProcessEventAtLocation(const gfx::Point& point) override;

  // views::View
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  void OnPaint(gfx::Canvas* canvas) override;

  void UpdateMarkerColor();

  SaturationValueChangedCallback changed_callback_;
  SkScalar hue_ = 0;
  SkScalar saturation_ = 0;
  SkScalar value_ = 0;
  gfx::Point marker_position_;
  SkColor marker_color_;
};

SaturationValueView::SaturationValueView(
    const SaturationValueChangedCallback& changed_callback)
    : changed_callback_(changed_callback),

      marker_color_(gfx::kPlaceholderColor) {
  SetBorder(CreateSolidBorder(kBorderWidth, gfx::kPlaceholderColor));
}

void SaturationValueView::OnThemeChanged() {
  LocatedEventHandlerView::OnThemeChanged();
  GetBorder()->set_color(
      GetColorProvider()->GetColor(ui::kColorFocusableBorderUnfocused));
  SchedulePaint();
}

void SaturationValueView::OnHueChanged(SkScalar hue) {
  if (hue_ != hue) {
    hue_ = hue;
    UpdateMarkerColor();
    SchedulePaint();
  }
}

void SaturationValueView::OnSaturationValueChanged(SkScalar saturation,
                                                   SkScalar value) {
  if (saturation_ == saturation && value_ == value)
    return;

  saturation_ = saturation;
  value_ = value;
  SkScalar scalar_size = SkIntToScalar(kSaturationValueSize - 1);
  int x = SkScalarFloorToInt(saturation * scalar_size) + kBorderWidth;
  int y = SkScalarFloorToInt((SK_Scalar1 - value) * scalar_size) + kBorderWidth;
  if (gfx::Point(x, y) == marker_position_)
    return;

  marker_position_.set_x(x);
  marker_position_.set_y(y);

  UpdateMarkerColor();

  SchedulePaint();
}

void SaturationValueView::ProcessEventAtLocation(const gfx::Point& point) {
  SkScalar scalar_size = SkIntToScalar(kSaturationValueSize - 1);
  SkScalar saturation = (point.x() - kBorderWidth) / scalar_size;
  SkScalar value = SK_Scalar1 - (point.y() - kBorderWidth) / scalar_size;
  saturation = std::clamp(saturation, 0.0f, SK_Scalar1);
  value = std::clamp(value, 0.0f, SK_Scalar1);
  OnSaturationValueChanged(saturation, value);
  changed_callback_.Run(saturation, value);
}

gfx::Size SaturationValueView::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  return gfx::Size(kSaturationValueSize + kBorderWidth * 2,
                   kSaturationValueSize + kBorderWidth * 2);
}

void SaturationValueView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect color_bounds = bounds();
  color_bounds.Inset(GetInsets());

  // Paints horizontal gradient first for saturation.
  SkScalar hsv[3] = {hue_, SK_Scalar1, SK_Scalar1};
  SkScalar left_hsv[3] = {hue_, 0, SK_Scalar1};
  DrawGradientRect(color_bounds, SkHSVToColor(255, left_hsv),
                   SkHSVToColor(255, hsv), true /* is_horizontal */, canvas);

  // Overlays vertical gradient for value.
  SkScalar hsv_bottom[3] = {0, SK_Scalar1, 0};
  DrawGradientRect(color_bounds, SK_ColorTRANSPARENT,
                   SkHSVToColor(255, hsv_bottom), false /* is_horizontal */,
                   canvas);

  // Draw the crosshair marker.
  canvas->FillRect(
      gfx::Rect(marker_position_.x(),
                marker_position_.y() - kSaturationValueIndicatorSize, 1,
                kSaturationValueIndicatorSize * 2 + 1),
      marker_color_);
  canvas->FillRect(
      gfx::Rect(marker_position_.x() - kSaturationValueIndicatorSize,
                marker_position_.y(), kSaturationValueIndicatorSize * 2 + 1, 1),
      marker_color_);

  OnPaintBorder(canvas);
}

void SaturationValueView::UpdateMarkerColor() {
  SkScalar hsv[3] = {hue_, saturation_, value_};
  marker_color_ = color_utils::GetColorWithMaxContrast(SkHSVToColor(hsv));
}

BEGIN_METADATA(SaturationValueView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// SelectedColorPatchView
//
// A view to simply show the selected color in a rectangle.
class SelectedColorPatchView : public views::View {
  METADATA_HEADER(SelectedColorPatchView, views::View)

 public:
  SelectedColorPatchView();
  SelectedColorPatchView(const SelectedColorPatchView&) = delete;
  SelectedColorPatchView& operator=(const SelectedColorPatchView&) = delete;
  ~SelectedColorPatchView() override = default;

  void SetColor(SkColor color);
};

SelectedColorPatchView::SelectedColorPatchView() {
  SetVisible(true);
  SetBorder(CreateThemedSolidBorder(kBorderWidth,
                                    ui::kColorFocusableBorderUnfocused));
}

void SelectedColorPatchView::SetColor(SkColor color) {
  if (!background())
    SetBackground(CreateSolidBackground(color));
  else
    background()->SetNativeControlColor(color);
  SchedulePaint();
}

BEGIN_METADATA(SelectedColorPatchView)
END_METADATA

std::unique_ptr<View> ColorChooser::BuildView() {
  auto view = std::make_unique<View>();
  tracker_.SetView(view.get());
  view->SetBackground(CreateThemedSolidBackground(ui::kColorWindowBackground));
  view->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical,
                                  gfx::Insets(kMarginWidth), kMarginWidth));

  auto container = std::make_unique<View>();
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(), kMarginWidth));
  saturation_value_ =
      container->AddChildView(std::make_unique<SaturationValueView>(
          base::BindRepeating(&ColorChooser::OnSaturationValueChosen,
                              weak_ptr_factory_.GetWeakPtr())));
  hue_ = container->AddChildView(std::make_unique<HueView>(base::BindRepeating(
      &ColorChooser::OnHueChosen, weak_ptr_factory_.GetWeakPtr())));
  view->AddChildView(std::move(container));

  auto container2 = std::make_unique<View>();
  BoxLayout* layout =
      container2->SetLayoutManager(std::make_unique<views::BoxLayout>(
          BoxLayout::Orientation::kHorizontal, gfx::Insets(), kMarginWidth));
  auto textfield = std::make_unique<Textfield>();
  textfield->set_controller(this);
  textfield->SetDefaultWidthInChars(kTextfieldLengthInChars);
  textfield->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_COLOR_CHOOSER_HEX_INPUT));
  textfield_ = container2->AddChildView(std::move(textfield));
  selected_color_patch_ =
      container2->AddChildView(std::make_unique<SelectedColorPatchView>());
  layout->SetFlexForView(selected_color_patch_, 1);
  view->AddChildView(std::move(container2));

  OnColorChanged(initial_color_);

  return view;
}

bool ColorChooser::IsViewAttached() const {
  return tracker_.view();
}

void ColorChooser::OnColorChanged(SkColor color) {
  SetColor(color);
  if (IsViewAttached()) {
    hue_->OnHueChanged(hue());
    saturation_value_->OnHueChanged(hue());
    saturation_value_->OnSaturationValueChanged(saturation(), value());
    selected_color_patch_->SetColor(color);
    textfield_->SetText(GetColorText(color));
  }
}

void ColorChooser::OnHueChosen(SkScalar hue) {
  SetHue(hue);
  SkColor color = GetColor();
  saturation_value_->OnHueChanged(hue);
  selected_color_patch_->SetColor(color);
  textfield_->SetText(GetColorText(color));
}

void ColorChooser::OnSaturationValueChosen(SkScalar saturation,
                                           SkScalar value) {
  SetSaturationValue(saturation, value);
  SkColor color = GetColor();
  selected_color_patch_->SetColor(color);
  textfield_->SetText(GetColorText(color));
}

View* ColorChooser::hue_view_for_testing() {
  return hue_;
}

View* ColorChooser::saturation_value_view_for_testing() {
  return saturation_value_;
}

Textfield* ColorChooser::textfield_for_testing() {
  return textfield_;
}

View* ColorChooser::selected_color_patch_for_testing() {
  return selected_color_patch_;
}

void ColorChooser::ContentsChanged(Textfield* sender,
                                   const std::u16string& new_contents) {
  DCHECK(IsViewAttached());

  SkColor color = gfx::kPlaceholderColor;
  if (GetColorFromText(new_contents, &color)) {
    SetColor(color);
    hue_->OnHueChanged(hue());
    saturation_value_->OnHueChanged(hue());
    saturation_value_->OnSaturationValueChanged(saturation(), value());
    selected_color_patch_->SetColor(color);
  }
}

bool ColorChooser::HandleKeyEvent(Textfield* sender,
                                  const ui::KeyEvent& key_event) {
  DCHECK(IsViewAttached());

  if (key_event.type() != ui::EventType::kKeyPressed ||
      (key_event.key_code() != ui::VKEY_RETURN &&
       key_event.key_code() != ui::VKEY_ESCAPE)) {
    return false;
  }

  tracker_.view()->GetWidget()->Close();
  return true;
}

std::unique_ptr<WidgetDelegate> ColorChooser::MakeWidgetDelegate() {
  DCHECK(!IsViewAttached());

  auto delegate = std::make_unique<WidgetDelegate>();
  delegate->SetCanMinimize(false);
  delegate->SetContentsView(BuildView());
  delegate->SetInitiallyFocusedView(textfield_);
  delegate->SetModalType(ui::mojom::ModalType::kWindow);
  delegate->RegisterWindowClosingCallback(base::BindOnce(
      &ColorChooser::OnViewClosing, weak_ptr_factory_.GetWeakPtr()));

  return delegate;
}

ColorChooser::ColorChooser(ColorChooserListener* listener, SkColor initial)
    : listener_(listener), initial_color_(initial) {}

ColorChooser::~ColorChooser() {
  textfield_->set_controller(nullptr);
}

void ColorChooser::SetColor(SkColor color) {
  UNSAFE_TODO(SkColorToHSV(color, hsv_.data()));
  listener_->OnColorChosen(GetColor());
}

void ColorChooser::SetHue(SkScalar hue) {
  hsv_[0] = hue;
  listener_->OnColorChosen(GetColor());
}

void ColorChooser::SetSaturationValue(SkScalar saturation, SkScalar value) {
  hsv_[1] = saturation;
  hsv_[2] = value;
  listener_->OnColorChosen(GetColor());
}

SkColor ColorChooser::GetColor() const {
  UNSAFE_TODO(return SkHSVToColor(255, hsv_.data()));
}

void ColorChooser::OnViewClosing() {
  listener_->OnColorChooserDialogClosed();
}

}  // namespace views
