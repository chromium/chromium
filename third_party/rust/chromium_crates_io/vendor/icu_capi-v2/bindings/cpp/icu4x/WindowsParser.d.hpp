#ifndef ICU4X_WindowsParser_D_HPP
#define ICU4X_WindowsParser_D_HPP

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
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct WindowsParser; }
class WindowsParser;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct WindowsParser;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * A mapper between Windows time zone identifiers and BCP-47 time zone identifiers.
 *
 * This mapper supports two-way mapping, but it is optimized for the case of Windows to BCP-47.
 * It also supports normalizing and canonicalizing the Windows strings.
 *
 * See the [Rust documentation for `WindowsParser`](https://docs.rs/icu/2.2.0/icu/time/zone/windows/struct.WindowsParser.html) for more information.
 */
class WindowsParser {
public:

  /**
   * Create a new {@link WindowsParser} using compiled data
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/time/zone/windows/struct.WindowsParser.html#method.new) for more information.
   */
  inline static std::unique_ptr<icu4x::WindowsParser> create();

  /**
   * Create a new {@link WindowsParser} using a particular data source
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/time/zone/windows/struct.WindowsParser.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::WindowsParser>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * See the [Rust documentation for `parse`](https://docs.rs/icu/2.2.0/icu/time/zone/windows/struct.WindowsParserBorrowed.html#method.parse) for more information.
   */
  inline std::unique_ptr<icu4x::TimeZone> parse(std::string_view value, std::string_view region) const;

    inline const icu4x::capi::WindowsParser* AsFFI() const;
    inline icu4x::capi::WindowsParser* AsFFI();
    inline static const icu4x::WindowsParser* FromFFI(const icu4x::capi::WindowsParser* ptr);
    inline static icu4x::WindowsParser* FromFFI(icu4x::capi::WindowsParser* ptr);
    inline static void operator delete(void* ptr);
private:
    WindowsParser() = delete;
    WindowsParser(const icu4x::WindowsParser&) = delete;
    WindowsParser(icu4x::WindowsParser&&) noexcept = delete;
    WindowsParser operator=(const icu4x::WindowsParser&) = delete;
    WindowsParser operator=(icu4x::WindowsParser&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_WindowsParser_D_HPP
