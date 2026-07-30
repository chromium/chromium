#ifndef ICU4X_IanaParserExtended_D_HPP
#define ICU4X_IanaParserExtended_D_HPP

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
namespace capi { struct IanaParserExtended; }
class IanaParserExtended;
namespace capi { struct TimeZoneAndCanonicalAndNormalizedIterator; }
class TimeZoneAndCanonicalAndNormalizedIterator;
namespace capi { struct TimeZoneAndCanonicalIterator; }
class TimeZoneAndCanonicalIterator;
struct TimeZoneAndCanonicalAndNormalized;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct IanaParserExtended;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
 *
 * This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
 * It also supports normalizing and canonicalizing the IANA strings.
 *
 * See the [Rust documentation for `IanaParserExtended`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParserExtended.html) for more information.
 */
class IanaParserExtended {
public:

  /**
   * Create a new {@link IanaParserExtended} using compiled data
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParserExtended.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::IanaParserExtended> create();

  /**
   * Create a new {@link IanaParserExtended} using a particular data source
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParserExtended.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::IanaParserExtended>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `parse`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParserExtendedBorrowed.html#method.parse) for more information.
   */
  inline icu4x::TimeZoneAndCanonicalAndNormalized parse(std::string_view value) const;

  /**
   * See the [Rust documentation for `iter`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParserExtendedBorrowed.html#method.iter) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZoneAndCanonicalIterator> iter() const;

  /**
   * See the [Rust documentation for `iter_all`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParserExtendedBorrowed.html#method.iter_all) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZoneAndCanonicalAndNormalizedIterator> iter_all() const;

    inline const icu4x::capi::IanaParserExtended* AsFFI() const;
    inline icu4x::capi::IanaParserExtended* AsFFI();
    inline static const icu4x::IanaParserExtended* FromFFI(const icu4x::capi::IanaParserExtended* ptr);
    inline static icu4x::IanaParserExtended* FromFFI(icu4x::capi::IanaParserExtended* ptr);
    inline static void operator delete(void* ptr);
private:
    IanaParserExtended() = delete;
    IanaParserExtended(const icu4x::IanaParserExtended&) = delete;
    IanaParserExtended(icu4x::IanaParserExtended&&) noexcept = delete;
    IanaParserExtended operator=(const icu4x::IanaParserExtended&) = delete;
    IanaParserExtended operator=(icu4x::IanaParserExtended&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_IanaParserExtended_D_HPP
