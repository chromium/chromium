#ifndef ONE_EURO_ONE_EURO_FILTER_H_
#define ONE_EURO_ONE_EURO_FILTER_H_

#include <memory>

#include "low_pass_filter.h"

namespace one_euro_filter {
namespace test {
class OneEuroFilterTest;
}

namespace {
typedef double TimeStamp;  // in seconds
static const TimeStamp UndefinedTime = -1.0;
}  // namespace

class OneEuroFilter {
 public:
  // Creates a 1euro filter
  OneEuroFilter(double freq,
                double mincutoff = 1.0,
                double beta = 0.0,
                double dcutoff = 1.0);

  ~OneEuroFilter();

  // Filter a value
  // param value: value to be filtered
  // returns: the filtered value;
  double Filter(double value, TimeStamp timestamp = UndefinedTime);

  OneEuroFilter* Clone();

  // Reset the filter to its initial values
  void Reset();

 private:
  friend class test::OneEuroFilterTest;

  // Save initial values
  double init_freq_;
  double init_mincutoff_;
  double init_beta_;
  double init_dcutoff_;

  double freq_;
  double mincutoff_;
  double beta_;
  double dcutoff_;
  std::unique_ptr<LowPassFilter> x_;
  std::unique_ptr<LowPassFilter> dx_;
  TimeStamp lasttime_;

  OneEuroFilter();

  double Alpha(double cutoff);

  void SetFrequency(double f);

  void SetMinCutoff(double mc);

  void SetBeta(double b);

  void SetDerivateCutoff(double dc);
};

}  // namespace one_euro_filter

#endif  // ONE_EURO_ONE_EURO_FILTER_H_
