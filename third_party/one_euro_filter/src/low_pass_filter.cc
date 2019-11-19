#include "low_pass_filter.h"

namespace one_euro_filter {

void LowPassFilter::SetAlpha(double alpha) {
  if (alpha <= 0.0 || alpha > 1.0)
    a_ = 0.5;
  else
    a_ = alpha;
}

LowPassFilter::LowPassFilter() {}

LowPassFilter::LowPassFilter(double alpha, double initval) {
  init_alpha_ = alpha;
  initval_ = initval;

  y_ = s_ = initval;
  SetAlpha(alpha);
  initialized_ = false;
}

double LowPassFilter::Filter(double value) {
  double result;
  if (initialized_)
    result = a_ * value + (1.0 - a_) * s_;
  else {
    result = value;
    initialized_ = true;
  }
  y_ = value;
  s_ = result;
  return result;
}

double LowPassFilter::FilterWithAlpha(double value, double alpha) {
  SetAlpha(alpha);
  return Filter(value);
}

bool LowPassFilter::HasLastRawValue(void) {
  return initialized_;
}

double LowPassFilter::LastRawValue(void) {
  return y_;
}

void LowPassFilter::Reset() {
  y_ = s_ = initval_;
  SetAlpha(init_alpha_);
  initialized_ = false;
}

LowPassFilter* LowPassFilter::Clone(void) {
  LowPassFilter* new_filter = new LowPassFilter();
  new_filter->y_ = y_;
  new_filter->a_ = a_;
  new_filter->s_ = s_;
  new_filter->initialized_ = initialized_;

  return new_filter;
}

}  // namespace one_euro_filter