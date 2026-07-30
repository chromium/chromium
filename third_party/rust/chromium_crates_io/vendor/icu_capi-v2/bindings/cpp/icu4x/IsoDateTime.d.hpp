#ifndef ICU4X_IsoDateTime_D_HPP
#define ICU4X_IsoDateTime_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
namespace capi { struct IsoDate; }
class IsoDate;
namespace capi { struct Time; }
class Time;
struct IsoDateTime;
class Rfc9557ParseError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct IsoDateTime {
      icu4x::capi::IsoDate* date;
      icu4x::capi::Time* time;
    };

    typedef struct IsoDateTime_option {union { IsoDateTime ok; }; bool is_ok; } IsoDateTime_option;
} // namespace capi
} // namespace


namespace icu4x {
/**
 * An ICU4X `DateTime` object capable of containing a ISO-8601 date and time.
 *
 * See the [Rust documentation for `DateTime`](https://docs.rs/icu/2.2.0/icu/time/struct.DateTime.html) for more information.
 */
struct IsoDateTime {
    std::unique_ptr<icu4x::IsoDate> date;
    std::unique_ptr<icu4x::Time> time;

  /**
   * Creates a new {@link IsoDateTime} from an IXDTF string.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/icu/2.2.0/icu/time/struct.DateTime.html#method.try_from_str) for more information.
   */
  inline static icu4x::diplomat::result<icu4x::IsoDateTime, icu4x::Rfc9557ParseError> from_string(std::string_view v);

    inline icu4x::capi::IsoDateTime AsFFI() const;
    inline static icu4x::IsoDateTime FromFFI(icu4x::capi::IsoDateTime c_struct);
};

} // namespace
#endif // ICU4X_IsoDateTime_D_HPP
