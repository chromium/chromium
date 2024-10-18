// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/throbber.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/paint_vector_icon.h"

namespace views {

namespace {

// The larger the diameter, the smaller the delay returned. This is intended
// because large diameters need less delay to look smooth and not jarring.
int GetFrameDelay(int diameter) {
  int frames_per_second = std::clamp(diameter * 2, 30, 120);
  return base::Seconds(1).InMilliseconds() / frames_per_second;
}

}  // namespace

Throbber::Throbber(int diameter) : diameter_(diameter) {}

Throbber::~Throbber() {
  Stop();
}

void Throbber::Start() {
  if (IsRunning())
    return;

  start_time_ = base::TimeTicks::Now();
  timer_.Start(
      FROM_HERE, base::Milliseconds(GetFrameDelay(diameter_)),
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

gfx::Size Throbber::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  return gfx::Size(diameter_, diameter_);
}

void Throbber::OnPaint(gfx::Canvas* canvas) {
  SkColor color = GetColorProvider()->GetColor(ui::kColorThrobber);

  if (!IsRunning()) {
    if (checked_) {
      // The size of the checkmark, in DIP. This magic number matches the
      // diameter plus padding inherent in the checkmark SVG.
      const int kCheckmarkDipSize = diameter_ + 2;
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
ADD_PROPERTY_METADATA(bool, Checked)
END_METADATA

// Smoothed throbber ---------------------------------------------------------

SmoothedThrobber::SmoothedThrobber(int diameter)
    : Throbber(diameter),
      start_delay_(base::Milliseconds(200)),
      stop_delay_(base::Milliseconds(50)) {}

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
ADD_PROPERTY_METADATA(base::TimeDelta, StartDelay)
ADD_PROPERTY_METADATA(base::TimeDelta, StopDelay)
END_METADATA

}  // namespace views
