#ifndef ICU4X_WeekdaySetIterator_D_HPP
#define ICU4X_WeekdaySetIterator_D_HPP

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
class Weekday;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct WeekdaySetIterator;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * Documents which days of the week are considered to be a part of the weekend
 *
 * See the [Rust documentation for `WeekdaySetIterator`](https://docs.rs/icu/2.2.0/icu/calendar/week/struct.WeekdaySetIterator.html) for more information.
 */
class WeekdaySetIterator {
public:

  /**
   * See the [Rust documentation for `next`](https://docs.rs/icu/2.2.0/icu/calendar/week/struct.WeekdaySetIterator.html#method.next) for more information.
   */
  inline std::optional<icu4x::Weekday> next();

    inline const icu4x::capi::WeekdaySetIterator* AsFFI() const;
    inline icu4x::capi::WeekdaySetIterator* AsFFI();
    inline static const icu4x::WeekdaySetIterator* FromFFI(const icu4x::capi::WeekdaySetIterator* ptr);
    inline static icu4x::WeekdaySetIterator* FromFFI(icu4x::capi::WeekdaySetIterator* ptr);
    inline static void operator delete(void* ptr);
private:
    WeekdaySetIterator() = delete;
    WeekdaySetIterator(const icu4x::WeekdaySetIterator&) = delete;
    WeekdaySetIterator(icu4x::WeekdaySetIterator&&) noexcept = delete;
    WeekdaySetIterator operator=(const icu4x::WeekdaySetIterator&) = delete;
    WeekdaySetIterator operator=(icu4x::WeekdaySetIterator&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_WeekdaySetIterator_D_HPP
