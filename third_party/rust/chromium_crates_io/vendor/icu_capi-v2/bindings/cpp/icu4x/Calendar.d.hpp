#ifndef ICU4X_Calendar_D_HPP
#define ICU4X_Calendar_D_HPP

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
namespace capi { struct Calendar; }
class Calendar;
namespace capi { struct DataProvider; }
class DataProvider;
class CalendarKind;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct Calendar;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `AnyCalendar`](https://docs.rs/icu/2.2.0/icu/calendar/enum.AnyCalendar.html) for more information.
 */
class Calendar {
public:

  /**
   * Creates a new {@link Calendar} for the specified kind, using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/calendar/enum.AnyCalendar.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::Calendar> create(icu4x::CalendarKind kind);

  /**
   * Creates a new {@link Calendar} for the specified kind, using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/calendar/enum.AnyCalendar.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Calendar>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider, icu4x::CalendarKind kind);

  /**
   * Returns the kind of this calendar
   *
   * See the [Rust documentation for `kind`](https://docs.rs/icu/2.2.0/icu/calendar/enum.AnyCalendar.html#method.kind) for more information.
   */
  inline icu4x::CalendarKind kind() const;

    inline const icu4x::capi::Calendar* AsFFI() const;
    inline icu4x::capi::Calendar* AsFFI();
    inline static const icu4x::Calendar* FromFFI(const icu4x::capi::Calendar* ptr);
    inline static icu4x::Calendar* FromFFI(icu4x::capi::Calendar* ptr);
    inline static void operator delete(void* ptr);
private:
    Calendar() = delete;
    Calendar(const icu4x::Calendar&) = delete;
    Calendar(icu4x::Calendar&&) noexcept = delete;
    Calendar operator=(const icu4x::Calendar&) = delete;
    Calendar operator=(icu4x::Calendar&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_Calendar_D_HPP
