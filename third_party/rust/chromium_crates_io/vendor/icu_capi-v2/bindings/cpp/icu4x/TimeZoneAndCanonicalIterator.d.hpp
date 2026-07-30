#ifndef ICU4X_TimeZoneAndCanonicalIterator_D_HPP
#define ICU4X_TimeZoneAndCanonicalIterator_D_HPP

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
struct TimeZoneAndCanonical;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TimeZoneAndCanonicalIterator;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `TimeZoneAndCanonicalIter`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.TimeZoneAndCanonicalIter.html) for more information.
 */
class TimeZoneAndCanonicalIterator {
public:

  /**
   * See the [Rust documentation for `next`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.TimeZoneAndCanonicalIter.html#method.next) for more information.
   */
  inline std::optional<icu4x::TimeZoneAndCanonical> next();

    inline const icu4x::capi::TimeZoneAndCanonicalIterator* AsFFI() const;
    inline icu4x::capi::TimeZoneAndCanonicalIterator* AsFFI();
    inline static const icu4x::TimeZoneAndCanonicalIterator* FromFFI(const icu4x::capi::TimeZoneAndCanonicalIterator* ptr);
    inline static icu4x::TimeZoneAndCanonicalIterator* FromFFI(icu4x::capi::TimeZoneAndCanonicalIterator* ptr);
    inline static void operator delete(void* ptr);
private:
    TimeZoneAndCanonicalIterator() = delete;
    TimeZoneAndCanonicalIterator(const icu4x::TimeZoneAndCanonicalIterator&) = delete;
    TimeZoneAndCanonicalIterator(icu4x::TimeZoneAndCanonicalIterator&&) noexcept = delete;
    TimeZoneAndCanonicalIterator operator=(const icu4x::TimeZoneAndCanonicalIterator&) = delete;
    TimeZoneAndCanonicalIterator operator=(icu4x::TimeZoneAndCanonicalIterator&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_TimeZoneAndCanonicalIterator_D_HPP
