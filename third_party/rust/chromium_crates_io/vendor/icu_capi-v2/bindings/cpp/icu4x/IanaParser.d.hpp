#ifndef icu4x_IanaParser_D_HPP
#define icu4x_IanaParser_D_HPP

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
namespace capi { struct IanaParser; }
class IanaParser;
namespace capi { struct TimeZone; }
class TimeZone;
namespace capi { struct TimeZoneIterator; }
class TimeZoneIterator;
class DataError;
}


namespace icu4x {
namespace capi {
    struct IanaParser;
} // namespace capi
} // namespace

namespace icu4x {
class IanaParser {
public:

  inline static std::unique_ptr<icu4x::IanaParser> create();

  inline static diplomat::result<std::unique_ptr<icu4x::IanaParser>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  inline std::unique_ptr<icu4x::TimeZone> parse(std::string_view value) const;

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
#endif // icu4x_IanaParser_D_HPP
