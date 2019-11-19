// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/throbber.h"

#include "base/bind.h"
#include "base/location.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

namespace views {

// The default diameter of a Throbber. If you change this, also change
// kCheckmarkDipSize.
constexpr int kDefaultDiameter = 16;
// The size of the checkmark, in DIP. This magic number matches the default
// diameter plus padding inherent in the checkmark SVG.
constexpr int kCheckmarkDipSize = kDefaultDiameter + 2;

Throbber::Throbber() = default;

Throbber::~Throbber() {
  Stop();
}

void Throbber::Start() {
  if (IsRunning())
    return;

  start_time_ = base::TimeTicks::Now();
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(30),
      base::BindRepeating(&Throbber::SchedulePaint, base::Unretained(this)));
  SchedulePaint();  // paint right away
}

void Throbber::Stop() {
  if (!IsRunning())
    return;

  timer_.Stop();
  SchedulePaint();
}

bool Throbber::GetChecked() const {
  return checked_;
}

void Throbber::SetChecked(bool checked) {
  if (checked == checked_)
    return;

  checked_ = checked;
  OnPropertyChanged(&checked_, kPropertyEffectsPaint);
}

gfx::Size Throbber::CalculatePreferredSize() const {
  return gfx::Size(kDefaultDiameter, kDefaultDiameter);
}

void Throbber::OnPaint(gfx::Canvas* canvas) {
  SkColor color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ThrobberSpinningColor);

  if (!IsRunning()) {
    if (checked_) {
      canvas->Translate(gfx::Vector2d((width() - kCheckmarkDipSize) / 2,
                                      (height() - kCheckmarkDipSize) / 2));
      gfx::PaintVectorIcon(canvas, vector_icons::kCheckCircleIcon,
                           kCheckmarkDipSize, color);
    }
    return;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  gfx::PaintThrobberSpinning(canvas, GetContentsBounds(), color, elapsed_time);
}

bool Throbber::IsRunning() const {
  return timer_.IsRunning();
}

BEGIN_METADATA(Throbber)
METADATA_PARENT_CLASS(View)
ADD_PROPERTY_METADATA(Throbber, bool, Checked)
END_METADATA()

// Smoothed throbber ---------------------------------------------------------

SmoothedThrobber::SmoothedThrobber()
    : start_delay_(base::TimeDelta::FromMilliseconds(200)),
      stop_delay_(base::TimeDelta::FromMilliseconds(50)) {}

SmoothedThrobber::~SmoothedThrobber() = default;

void SmoothedThrobber::Start() {
  stop_timer_.Stop();

  if (!IsRunning() && !start_timer_.IsRunning()) {
    start_timer_.Start(FROM_HERE, start_delay_, this,
                       &SmoothedThrobber::StartDelayOver);
  }
}

void SmoothedThrobber::StartDelayOver() {
  Throbber::Start();
}

void SmoothedThrobber::Stop() {
  if (!IsRunning())
    start_timer_.Stop();

  stop_timer_.Stop();
  stop_timer_.Start(FROM_HERE, stop_delay_, this,
                    &SmoothedThrobber::StopDelayOver);
}

base::TimeDelta SmoothedThrobber::GetStartDelay() const {
  return start_delay_;
}

void SmoothedThrobber::SetStartDelay(const base::TimeDelta& start_delay) {
  if (start_delay == start_delay_)
    return;
  start_delay_ = start_delay;
  OnPropertyChanged(&start_delay_, kPropertyEffectsNone);
}

base::TimeDelta SmoothedThrobber::GetStopDelay() const {
  return stop_delay_;
}

void SmoothedThrobber::SetStopDelay(const base::TimeDelta& stop_delay) {
  if (stop_delay == stop_delay_)
    return;
  stop_delay_ = stop_delay;
  OnPropertyChanged(&stop_delay_, kPropertyEffectsNone);
}

void SmoothedThrobber::StopDelayOver() {
  Throbber::Stop();
}

BEGIN_METADATA(SmoothedThrobber)
METADATA_PARENT_CLASS(Throbber)
ADD_PROPERTY_METADATA(SmoothedThrobber, base::TimeDelta, StartDelay)
ADD_PROPERTY_METADATA(SmoothedThrobber, base::TimeDelta, StopDelay)
END_METADATA()

}  // namespace views
