#ifndef icu4x_WindowsParser_D_HPP
#define icu4x_WindowsParser_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct WindowsParser; }
class WindowsParser;
class DataError;
}


namespace icu4x {
namespace capi {
    struct WindowsParser;
} // namespace capi
} // namespace

namespace icu4x {
class WindowsParser {
public:

  inline static std::unique_ptr<icu4x::WindowsParser> create();

  inline static diplomat::result<std::unique_ptr<icu4x::WindowsParser>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

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
#endif // icu4x_WindowsParser_D_HPP
