#ifndef temporal_rs_PartialDate_D_HPP
#define temporal_rs_PartialDate_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "AnyCalendarKind.d.hpp"

namespace temporal_rs {
class AnyCalendarKind;
}


namespace temporal_rs {
namespace capi {
    struct PartialDate {
      diplomat::capi::OptionI32 year;
      diplomat::capi::OptionU8 month;
      diplomat::capi::DiplomatStringView month_code;
      diplomat::capi::OptionU8 day;
      diplomat::capi::DiplomatStringView era;
      diplomat::capi::OptionI32 era_year;
      temporal_rs::capi::AnyCalendarKind calendar;
    };

    typedef struct PartialDate_option {union { PartialDate ok; }; bool is_ok; } PartialDate_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct PartialDate {
  std::optional<int32_t> year;
  std::optional<uint8_t> month;
  std::string_view month_code;
  std::optional<uint8_t> day;
  std::string_view era;
  std::optional<int32_t> era_year;
  temporal_rs::AnyCalendarKind calendar;

  inline temporal_rs::capi::PartialDate AsFFI() const;
  inline static temporal_rs::PartialDate FromFFI(temporal_rs::capi::PartialDate c_struct);
};

} // namespace
#endif // temporal_rs_PartialDate_D_HPP
