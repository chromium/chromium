#ifndef icu4x_FixedDecimalFormatter_D_HPP
#define icu4x_FixedDecimalFormatter_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct FixedDecimal; }
class FixedDecimal;
namespace capi { struct FixedDecimalFormatter; }
class FixedDecimalFormatter;
namespace capi { struct Locale; }
class Locale;
class DataError;
class FixedDecimalGroupingStrategy;
}


namespace icu4x {
namespace capi {
    struct FixedDecimalFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class FixedDecimalFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::FixedDecimalFormatter>, icu4x::DataError> create_with_grouping_strategy(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::FixedDecimalGroupingStrategy> grouping_strategy);

  inline static diplomat::result<std::unique_ptr<icu4x::FixedDecimalFormatter>, icu4x::DataError> create_with_manual_data(std::string_view plus_sign_prefix, std::string_view plus_sign_suffix, std::string_view minus_sign_prefix, std::string_view minus_sign_suffix, std::string_view decimal_separator, std::string_view grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, diplomat::span<const char32_t> digits, std::optional<icu4x::FixedDecimalGroupingStrategy> grouping_strategy);

  inline std::string format(const icu4x::FixedDecimal& value) const;

  inline const icu4x::capi::FixedDecimalFormatter* AsFFI() const;
  inline icu4x::capi::FixedDecimalFormatter* AsFFI();
  inline static const icu4x::FixedDecimalFormatter* FromFFI(const icu4x::capi::FixedDecimalFormatter* ptr);
  inline static icu4x::FixedDecimalFormatter* FromFFI(icu4x::capi::FixedDecimalFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  FixedDecimalFormatter() = delete;
  FixedDecimalFormatter(const icu4x::FixedDecimalFormatter&) = delete;
  FixedDecimalFormatter(icu4x::FixedDecimalFormatter&&) noexcept = delete;
  FixedDecimalFormatter operator=(const icu4x::FixedDecimalFormatter&) = delete;
  FixedDecimalFormatter operator=(icu4x::FixedDecimalFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_FixedDecimalFormatter_D_HPP
