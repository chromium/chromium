#ifndef icu4x_DecimalFormatter_D_HPP
#define icu4x_DecimalFormatter_D_HPP

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
namespace capi { struct Decimal; }
class Decimal;
namespace capi { struct DecimalFormatter; }
class DecimalFormatter;
namespace capi { struct Locale; }
class Locale;
class DataError;
class DecimalGroupingStrategy;
}


namespace icu4x {
namespace capi {
    struct DecimalFormatter;
} // namespace capi
} // namespace

namespace icu4x {
class DecimalFormatter {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError> create_with_grouping_strategy(const icu4x::Locale& locale, std::optional<icu4x::DecimalGroupingStrategy> grouping_strategy);

  inline static diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError> create_with_grouping_strategy_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DecimalGroupingStrategy> grouping_strategy);

  inline static diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError> create_with_manual_data(std::string_view plus_sign_prefix, std::string_view plus_sign_suffix, std::string_view minus_sign_prefix, std::string_view minus_sign_suffix, std::string_view decimal_separator, std::string_view grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, diplomat::span<const char32_t> digits, std::optional<icu4x::DecimalGroupingStrategy> grouping_strategy);

  inline std::string format(const icu4x::Decimal& value) const;

  inline const icu4x::capi::DecimalFormatter* AsFFI() const;
  inline icu4x::capi::DecimalFormatter* AsFFI();
  inline static const icu4x::DecimalFormatter* FromFFI(const icu4x::capi::DecimalFormatter* ptr);
  inline static icu4x::DecimalFormatter* FromFFI(icu4x::capi::DecimalFormatter* ptr);
  inline static void operator delete(void* ptr);
private:
  DecimalFormatter() = delete;
  DecimalFormatter(const icu4x::DecimalFormatter&) = delete;
  DecimalFormatter(icu4x::DecimalFormatter&&) noexcept = delete;
  DecimalFormatter operator=(const icu4x::DecimalFormatter&) = delete;
  DecimalFormatter operator=(icu4x::DecimalFormatter&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_DecimalFormatter_D_HPP
