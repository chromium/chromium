#ifndef ICU4X_TimeZoneIterator_D_HPP
#define ICU4X_TimeZoneIterator_D_HPP

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
namespace capi { struct TimeZone; }
class TimeZone;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TimeZoneIterator;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `TimeZoneIter`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.TimeZoneIter.html) for more information.
 */
class TimeZoneIterator {
public:

  /**
   * See the [Rust documentation for `next`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.TimeZoneIter.html#method.next) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZone> next();

    inline const icu4x::capi::TimeZoneIterator* AsFFI() const;
    inline icu4x::capi::TimeZoneIterator* AsFFI();
    inline static const icu4x::TimeZoneIterator* FromFFI(const icu4x::capi::TimeZoneIterator* ptr);
    inline static icu4x::TimeZoneIterator* FromFFI(icu4x::capi::TimeZoneIterator* ptr);
    inline static void operator delete(void* ptr);
private:
    TimeZoneIterator() = delete;
    TimeZoneIterator(const icu4x::TimeZoneIterator&) = delete;
    TimeZoneIterator(icu4x::TimeZoneIterator&&) noexcept = delete;
    TimeZoneIterator operator=(const icu4x::TimeZoneIterator&) = delete;
    TimeZoneIterator operator=(icu4x::TimeZoneIterator&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_TimeZoneIterator_D_HPP
