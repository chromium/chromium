#ifndef ICU4X_IanaParser_D_HPP
#define ICU4X_IanaParser_D_HPP

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
namespace capi { struct IanaParser; }
class IanaParser;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct TimeZoneIterator; }
class TimeZoneIterator;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct IanaParser;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
 *
 * This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
 * It also supports normalizing and canonicalizing the IANA strings.
 *
 * See the [Rust documentation for `IanaParser`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParser.html) for more information.
 */
class IanaParser {
public:

  /**
   * Create a new {@link IanaParser} using compiled data
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParser.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::IanaParser> create();

  /**
   * Create a new {@link IanaParser} using a particular data source
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParser.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::IanaParser>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `parse`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParserBorrowed.html#method.parse) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZone> parse(std::string_view value) const;

  /**
   * See the [Rust documentation for `iter`](https://docs.rs/icu/2.2.0/icu/time/zone/iana/struct.IanaParserBorrowed.html#method.iter) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZoneIterator> iter() const;

    inline const icu4x::capi::IanaParser* AsFFI() const;
    inline icu4x::capi::IanaParser* AsFFI();
    inline static const icu4x::IanaParser* FromFFI(const icu4x::capi::IanaParser* ptr);
    inline static icu4x::IanaParser* FromFFI(icu4x::capi::IanaParser* ptr);
    inline static void operator delete(void* ptr);
private:
    IanaParser() = delete;
    IanaParser(const icu4x::IanaParser&) = delete;
    IanaParser(icu4x::IanaParser&&) noexcept = delete;
    IanaParser operator=(const icu4x::IanaParser&) = delete;
    IanaParser operator=(icu4x::IanaParser&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_IanaParser_D_HPP
