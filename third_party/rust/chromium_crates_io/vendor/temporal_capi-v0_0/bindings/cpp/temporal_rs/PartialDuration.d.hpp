#ifndef temporal_rs_PartialDuration_D_HPP
#define temporal_rs_PartialDuration_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    struct PartialDuration {
      diplomat::capi::OptionI64 years;
      diplomat::capi::OptionI64 months;
      diplomat::capi::OptionI64 weeks;
      diplomat::capi::OptionI64 days;
      diplomat::capi::OptionI64 hours;
      diplomat::capi::OptionI64 minutes;
      diplomat::capi::OptionI64 seconds;
      diplomat::capi::OptionI64 milliseconds;
      diplomat::capi::OptionF64 microseconds;
      diplomat::capi::OptionF64 nanoseconds;
    };

    typedef struct PartialDuration_option {union { PartialDuration ok; }; bool is_ok; } PartialDuration_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct PartialDuration {
  std::optional<int64_t> years;
  std::optional<int64_t> months;
  std::optional<int64_t> weeks;
  std::optional<int64_t> days;
  std::optional<int64_t> hours;
  std::optional<int64_t> minutes;
  std::optional<int64_t> seconds;
  std::optional<int64_t> milliseconds;
  std::optional<double> microseconds;
  std::optional<double> nanoseconds;

  inline bool is_empty() const;

  inline temporal_rs::capi::PartialDuration AsFFI() const;
  inline static temporal_rs::PartialDuration FromFFI(temporal_rs::capi::PartialDuration c_struct);
};

} // namespace
#endif // temporal_rs_PartialDuration_D_HPP
