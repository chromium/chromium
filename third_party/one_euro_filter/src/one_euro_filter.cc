#include "one_euro_filter.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace one_euro_filter {

OneEuroFilter::OneEuroFilter(double freq,
                             double mincutoff,
                             double beta,
                             double dcutoff) {
  init_freq_ = freq;
  init_mincutoff_ = mincutoff;
  init_beta_ = beta;
  init_dcutoff_ = dcutoff;

  SetFrequency(freq);
  SetMinCutoff(mincutoff);
  SetBeta(beta);
  SetDerivateCutoff(dcutoff);
  x_ = std::make_unique<LowPassFilter>(Alpha(mincutoff_));
  dx_ = std::make_unique<LowPassFilter>(Alpha(dcutoff_));
  lasttime_ = UndefinedTime;
}

OneEuroFilter::OneEuroFilter() {}

OneEuroFilter::~OneEuroFilter() {}

double OneEuroFilter::Filter(double value, TimeStamp timestamp) {
  // update the sampling frequency based on timestamps
  if (lasttime_ != UndefinedTime && timestamp != UndefinedTime &&
      timestamp - lasttime_ > 0)
    freq_ = 1.0 / (timestamp - lasttime_);
  lasttime_ = timestamp;
  // estimate the current variation per second
  double dvalue = x_->HasLastRawValue() ? (value - x_->LastRawValue()) * freq_
                                        : 0.0;  // FIXME: 0.0 or value?
  double edvalue = dx_->FilterWithAlpha(dvalue, Alpha(dcutoff_));
  // use it to update the cutoff frequency
  double cutoff = mincutoff_ + beta_ * std::fabs(edvalue);
  // filter the given value
  return x_->FilterWithAlpha(value, Alpha(cutoff));
}

void OneEuroFilter::Reset() {
  SetFrequency(init_freq_);
  SetMinCutoff(init_mincutoff_);
  SetBeta(init_beta_);
  SetDerivateCutoff(init_dcutoff_);
  x_->Reset();
  dx_->Reset();
  lasttime_ = UndefinedTime;
}

OneEuroFilter* OneEuroFilter::Clone() {
  OneEuroFilter* new_filter = new OneEuroFilter();
  new_filter->freq_ = freq_;
  new_filter->beta_ = beta_;
  new_filter->dcutoff_ = dcutoff_;
  new_filter->mincutoff_ = mincutoff_;
  new_filter->lasttime_ = lasttime_;
  new_filter->x_.reset(x_->Clone());
  new_filter->dx_.reset(dx_->Clone());

  return new_filter;
}

double OneEuroFilter::Alpha(double cutoff) {
  double te = 1.0 / freq_;
  double tau = 1.0 / (2 * M_PI * cutoff);
  return 1.0 / (1.0 + tau / te);
}

void OneEuroFilter::SetFrequency(double f) {
  freq_ = f > 0.0 ? f : 120;  // 120Hz default
}

void OneEuroFilter::SetMinCutoff(double mc) {
  mincutoff_ = mc > 0.0 ? mc : 1.0;
}

void OneEuroFilter::SetBeta(double b) {
  beta_ = b;
}

void OneEuroFilter::SetDerivateCutoff(double dc) {
  dcutoff_ = dc > 0.0 ? dc : 1.0;
}

}  // namespace one_euro_filter