#ifndef temporal_rs_IsoDateTime_D_HPP
#define temporal_rs_IsoDateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "IsoDate.d.hpp"
#include "IsoTime.d.hpp"

namespace temporal_rs {
struct IsoDate;
struct IsoTime;
}


namespace temporal_rs {
namespace capi {
    struct IsoDateTime {
      temporal_rs::capi::IsoDate date;
      temporal_rs::capi::IsoTime time;
    };

    typedef struct IsoDateTime_option {union { IsoDateTime ok; }; bool is_ok; } IsoDateTime_option;
} // namespace capi
} // namespace


namespace temporal_rs {
struct IsoDateTime {
  temporal_rs::IsoDate date;
  temporal_rs::IsoTime time;

  inline temporal_rs::capi::IsoDateTime AsFFI() const;
  inline static temporal_rs::IsoDateTime FromFFI(temporal_rs::capi::IsoDateTime c_struct);
};

} // namespace
#endif // temporal_rs_IsoDateTime_D_HPP
