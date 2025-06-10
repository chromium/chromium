#ifndef temporal_rs_IsoDate_D_HPP
#define temporal_rs_IsoDate_D_HPP

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
    struct IsoDate {
      int32_t year;
      uint8_t month;
      uint8_t day;
    };

    typedef struct IsoDate_option {union { IsoDate ok; }; bool is_ok; } IsoDate_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct IsoDate {
  int32_t year;
  uint8_t month;
  uint8_t day;

  inline temporal_rs::capi::IsoDate AsFFI() const;
  inline static temporal_rs::IsoDate FromFFI(temporal_rs::capi::IsoDate c_struct);
};

} // namespace
#endif // temporal_rs_IsoDate_D_HPP
