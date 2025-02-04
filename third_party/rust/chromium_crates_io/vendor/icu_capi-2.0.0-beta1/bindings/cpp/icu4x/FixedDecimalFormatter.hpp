#ifndef icu4x_FixedDecimalFormatter_HPP
#define icu4x_FixedDecimalFormatter_HPP

#include "FixedDecimalFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "FixedDecimal.hpp"
#include "FixedDecimalGroupingStrategy.hpp"
#include "Locale.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_FixedDecimalFormatter_create_with_grouping_strategy_mv1_result {union {icu4x::capi::FixedDecimalFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_FixedDecimalFormatter_create_with_grouping_strategy_mv1_result;
    icu4x_FixedDecimalFormatter_create_with_grouping_strategy_mv1_result icu4x_FixedDecimalFormatter_create_with_grouping_strategy_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::FixedDecimalGroupingStrategy_option grouping_strategy);
    
    typedef struct icu4x_FixedDecimalFormatter_create_with_manual_data_mv1_result {union {icu4x::capi::FixedDecimalFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_FixedDecimalFormatter_create_with_manual_data_mv1_result;
    icu4x_FixedDecimalFormatter_create_with_manual_data_mv1_result icu4x_FixedDecimalFormatter_create_with_manual_data_mv1(diplomat::capi::DiplomatStringView plus_sign_prefix, diplomat::capi::DiplomatStringView plus_sign_suffix, diplomat::capi::DiplomatStringView minus_sign_prefix, diplomat::capi::DiplomatStringView minus_sign_suffix, diplomat::capi::DiplomatStringView decimal_separator, diplomat::capi::DiplomatStringView grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, diplomat::capi::DiplomatCharView digits, icu4x::capi::FixedDecimalGroupingStrategy_option grouping_strategy);
    
    void icu4x_FixedDecimalFormatter_format_mv1(const icu4x::capi::FixedDecimalFormatter* self, const icu4x::capi::FixedDecimal* value, diplomat::capi::DiplomatWrite* write);
    
    
    void icu4x_FixedDecimalFormatter_destroy_mv1(FixedDecimalFormatter* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::FixedDecimalFormatter>, icu4x::DataError> icu4x::FixedDecimalFormatter::create_with_grouping_strategy(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::FixedDecimalGroupingStrategy> grouping_strategy) {
  auto result = icu4x::capi::icu4x_FixedDecimalFormatter_create_with_grouping_strategy_mv1(provider.AsFFI(),
    locale.AsFFI(),
    grouping_strategy.has_value() ? (icu4x::capi::FixedDecimalGroupingStrategy_option{ { grouping_strategy.value().AsFFI() }, true }) : (icu4x::capi::FixedDecimalGroupingStrategy_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::FixedDecimalFormatter>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::FixedDecimalFormatter>>(std::unique_ptr<icu4x::FixedDecimalFormatter>(icu4x::FixedDecimalFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::FixedDecimalFormatter>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::FixedDecimalFormatter>, icu4x::DataError> icu4x::FixedDecimalFormatter::create_with_manual_data(std::string_view plus_sign_prefix, std::string_view plus_sign_suffix, std::string_view minus_sign_prefix, std::string_view minus_sign_suffix, std::string_view decimal_separator, std::string_view grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, diplomat::span<const char32_t> digits, std::optional<icu4x::FixedDecimalGroupingStrategy> grouping_strategy) {
  auto result = icu4x::capi::icu4x_FixedDecimalFormatter_create_with_manual_data_mv1({plus_sign_prefix.data(), plus_sign_prefix.size()},
    {plus_sign_suffix.data(), plus_sign_suffix.size()},
    {minus_sign_prefix.data(), minus_sign_prefix.size()},
    {minus_sign_suffix.data(), minus_sign_suffix.size()},
    {decimal_separator.data(), decimal_separator.size()},
    {grouping_separator.data(), grouping_separator.size()},
    primary_group_size,
    secondary_group_size,
    min_group_size,
    {digits.data(), digits.size()},
    grouping_strategy.has_value() ? (icu4x::capi::FixedDecimalGroupingStrategy_option{ { grouping_strategy.value().AsFFI() }, true }) : (icu4x::capi::FixedDecimalGroupingStrategy_option{ {}, false }));
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::FixedDecimalFormatter>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::FixedDecimalFormatter>>(std::unique_ptr<icu4x::FixedDecimalFormatter>(icu4x::FixedDecimalFormatter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::FixedDecimalFormatter>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::FixedDecimalFormatter::format(const icu4x::FixedDecimal& value) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_FixedDecimalFormatter_format_mv1(this->AsFFI(),
    value.AsFFI(),
    &write);
  return output;
}

inline const icu4x::capi::FixedDecimalFormatter* icu4x::FixedDecimalFormatter::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::FixedDecimalFormatter*>(this);
}

inline icu4x::capi::FixedDecimalFormatter* icu4x::FixedDecimalFormatter::AsFFI() {
  return reinterpret_cast<icu4x::capi::FixedDecimalFormatter*>(this);
}

inline const icu4x::FixedDecimalFormatter* icu4x::FixedDecimalFormatter::FromFFI(const icu4x::capi::FixedDecimalFormatter* ptr) {
  return reinterpret_cast<const icu4x::FixedDecimalFormatter*>(ptr);
}

inline icu4x::FixedDecimalFormatter* icu4x::FixedDecimalFormatter::FromFFI(icu4x::capi::FixedDecimalFormatter* ptr) {
  return reinterpret_cast<icu4x::FixedDecimalFormatter*>(ptr);
}

inline void icu4x::FixedDecimalFormatter::operator delete(void* ptr) {
  icu4x::capi::icu4x_FixedDecimalFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::FixedDecimalFormatter*>(ptr));
}


#endif // icu4x_FixedDecimalFormatter_HPP
