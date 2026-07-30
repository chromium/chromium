#ifndef ICU4X_WeekInformation_D_HPP
#define ICU4X_WeekInformation_D_HPP

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
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct WeekInformation; }
class WeekInformation;
namespace capi { struct WeekdaySetIterator; }
class WeekdaySetIterator;
class DataError;
class Weekday;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct WeekInformation;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A Week calculator, useful to be passed in to `week_of_year()` on Date and `DateTime` types
 *
 * See the [Rust documentation for `WeekInformation`](https://docs.rs/icu/2.2.0/icu/calendar/week/struct.WeekInformation.html) for more information.
 */
class WeekInformation {
public:

  /**
   * Creates a new {@link WeekInformation} from locale data using compiled data.
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/calendar/week/struct.WeekInformation.html#method.try_new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WeekInformation>, icu4x::DataError> create(const icu4x::Locale& locale);

  /**
   * Creates a new {@link WeekInformation} from locale data using a particular data source.
   *
   * See the [Rust documentation for `try_new`](https://docs.rs/icu/2.2.0/icu/calendar/week/struct.WeekInformation.html#method.try_new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WeekInformation>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  /**
   * Returns the weekday that starts the week for this object's locale
   *
   * See the [Rust documentation for `first_weekday`](https://docs.rs/icu/2.2.0/icu/calendar/week/struct.WeekInformation.html#structfield.first_weekday) for more information.
   */
  inline icu4x::Weekday first_weekday() const;

  /**
   * See the [Rust documentation for `weekend`](https://docs.rs/icu/2.2.0/icu/calendar/week/struct.WeekInformation.html#structfield.weekend) for more information.
   *
   * See the [Rust documentation for `contains`](https://docs.rs/icu/2.2.0/icu/calendar/provider/struct.WeekdaySet.html#method.contains) for more information.
   */
  inline bool is_weekend(icu4x::Weekday day) const;

  /**
   * See the [Rust documentation for `weekend`](https://docs.rs/icu/2.2.0/icu/calendar/week/struct.WeekInformation.html#method.weekend) for more information.
   */
  inline std::unique_ptr<icu4x::WeekdaySetIterator> weekend() const;

    inline const icu4x::capi::WeekInformation* AsFFI() const;
    inline icu4x::capi::WeekInformation* AsFFI();
    inline static const icu4x::WeekInformation* FromFFI(const icu4x::capi::WeekInformation* ptr);
    inline static icu4x::WeekInformation* FromFFI(icu4x::capi::WeekInformation* ptr);
    inline static void operator delete(void* ptr);
private:
    WeekInformation() = delete;
    WeekInformation(const icu4x::WeekInformation&) = delete;
    WeekInformation(icu4x::WeekInformation&&) noexcept = delete;
    WeekInformation operator=(const icu4x::WeekInformation&) = delete;
    WeekInformation operator=(icu4x::WeekInformation&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_WeekInformation_D_HPP
