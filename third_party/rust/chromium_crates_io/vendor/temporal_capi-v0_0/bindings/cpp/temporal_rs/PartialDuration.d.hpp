#ifndef temporal_rs_PartialDuration_D_HPP
#define temporal_rs_PartialDuration_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    struct PartialDuration {
      diplomat::capi::OptionF64 years;
      diplomat::capi::OptionF64 months;
      diplomat::capi::OptionF64 weeks;
      diplomat::capi::OptionF64 days;
      diplomat::capi::OptionF64 hours;
      diplomat::capi::OptionF64 minutes;
      diplomat::capi::OptionF64 seconds;
      diplomat::capi::OptionF64 milliseconds;
      diplomat::capi::OptionF64 microseconds;
      diplomat::capi::OptionF64 nanoseconds;
    };
    
    typedef struct PartialDuration_option {union { PartialDuration ok; }; bool is_ok; } PartialDuration_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct PartialDuration {
  std::optional<double> years;
  std::optional<double> months;
  std::optional<double> weeks;
  std::optional<double> days;
  std::optional<double> hours;
  std::optional<double> minutes;
  std::optional<double> seconds;
  std::optional<double> milliseconds;
  std::optional<double> microseconds;
  std::optional<double> nanoseconds;

  inline bool is_empty();

  inline temporal_rs::capi::PartialDuration AsFFI() const;
  inline static temporal_rs::PartialDuration FromFFI(temporal_rs::capi::PartialDuration c_struct);
};

} // namespace
#endif // temporal_rs_PartialDuration_D_HPP
