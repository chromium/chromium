#ifndef ONE_EURO_LOW_PASS_FILTER_H_
#define ONE_EURO_LOW_PASS_FILTER_H_

#include <iostream>

namespace one_euro_filter {

class LowPassFilter {
 public:
  LowPassFilter(double alpha, double initval = 0.0);

  double Filter(double value);

  double FilterWithAlpha(double value, double alpha);

  bool HasLastRawValue(void);

  double LastRawValue(void);

  LowPassFilter* Clone(void);

  // Reset the filter to its initial values
  void Reset(void);

 private:
  // save init values
  double init_alpha_, initval_;

  double y_, a_, s_;
  bool initialized_;

  LowPassFilter();
  void SetAlpha(double alpha);
};

}  // namespace one_euro_filter

#endif  // ONE_EURO_LOW_PASS_FILTER_H_
