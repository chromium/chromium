#ifndef ICU4X_DecimalFormatter_HPP
#define ICU4X_DecimalFormatter_HPP

#include "DecimalFormatter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "Decimal.hpp"
#include "DecimalGroupingStrategy.hpp"
#include "Locale.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_DecimalFormatter_create_with_grouping_strategy_mv1_result {union {icu4x::capi::DecimalFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_DecimalFormatter_create_with_grouping_strategy_mv1_result;
    icu4x_DecimalFormatter_create_with_grouping_strategy_mv1_result icu4x_DecimalFormatter_create_with_grouping_strategy_mv1(const icu4x::capi::Locale* locale, icu4x::capi::DecimalGroupingStrategy_option grouping_strategy);

    typedef struct icu4x_DecimalFormatter_create_with_grouping_strategy_and_provider_mv1_result {union {icu4x::capi::DecimalFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_DecimalFormatter_create_with_grouping_strategy_and_provider_mv1_result;
    icu4x_DecimalFormatter_create_with_grouping_strategy_and_provider_mv1_result icu4x_DecimalFormatter_create_with_grouping_strategy_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::DecimalGroupingStrategy_option grouping_strategy);

    typedef struct icu4x_DecimalFormatter_create_with_manual_data_mv1_result {union {icu4x::capi::DecimalFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_DecimalFormatter_create_with_manual_data_mv1_result;
    icu4x_DecimalFormatter_create_with_manual_data_mv1_result icu4x_DecimalFormatter_create_with_manual_data_mv1(icu4x::diplomat::capi::DiplomatStringView plus_sign_prefix, icu4x::diplomat::capi::DiplomatStringView plus_sign_suffix, icu4x::diplomat::capi::DiplomatStringView minus_sign_prefix, icu4x::diplomat::capi::DiplomatStringView minus_sign_suffix, icu4x::diplomat::capi::DiplomatStringView decimal_separator, icu4x::diplomat::capi::DiplomatStringView grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, icu4x::diplomat::capi::DiplomatCharView digits, icu4x::capi::DecimalGroupingStrategy_option grouping_strategy);

    void icu4x_DecimalFormatter_format_mv1(const icu4x::capi::DecimalFormatter* self, const icu4x::capi::Decimal* value, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_DecimalFormatter_destroy_mv1(DecimalFormatter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError> icu4x::DecimalFormatter::create_with_grouping_strategy(const icu4x::Locale& locale, std::optional<icu4x::DecimalGroupingStrategy> grouping_strategy) {
    auto result = icu4x::capi::icu4x_DecimalFormatter_create_with_grouping_strategy_mv1(locale.AsFFI(),
        grouping_strategy.has_value() ? (icu4x::capi::DecimalGroupingStrategy_option{ { grouping_strategy.value().AsFFI() }, true }) : (icu4x::capi::DecimalGroupingStrategy_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DecimalFormatter>>(std::unique_ptr<icu4x::DecimalFormatter>(icu4x::DecimalFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError> icu4x::DecimalFormatter::create_with_grouping_strategy_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, std::optional<icu4x::DecimalGroupingStrategy> grouping_strategy) {
    auto result = icu4x::capi::icu4x_DecimalFormatter_create_with_grouping_strategy_and_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        grouping_strategy.has_value() ? (icu4x::capi::DecimalGroupingStrategy_option{ { grouping_strategy.value().AsFFI() }, true }) : (icu4x::capi::DecimalGroupingStrategy_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DecimalFormatter>>(std::unique_ptr<icu4x::DecimalFormatter>(icu4x::DecimalFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError> icu4x::DecimalFormatter::create_with_manual_data(std::string_view plus_sign_prefix, std::string_view plus_sign_suffix, std::string_view minus_sign_prefix, std::string_view minus_sign_suffix, std::string_view decimal_separator, std::string_view grouping_separator, uint8_t primary_group_size, uint8_t secondary_group_size, uint8_t min_group_size, icu4x::diplomat::span<const char32_t> digits, std::optional<icu4x::DecimalGroupingStrategy> grouping_strategy) {
    auto result = icu4x::capi::icu4x_DecimalFormatter_create_with_manual_data_mv1({plus_sign_prefix.data(), plus_sign_prefix.size()},
        {plus_sign_suffix.data(), plus_sign_suffix.size()},
        {minus_sign_prefix.data(), minus_sign_prefix.size()},
        {minus_sign_suffix.data(), minus_sign_suffix.size()},
        {decimal_separator.data(), decimal_separator.size()},
        {grouping_separator.data(), grouping_separator.size()},
        primary_group_size,
        secondary_group_size,
        min_group_size,
        {digits.data(), digits.size()},
        grouping_strategy.has_value() ? (icu4x::capi::DecimalGroupingStrategy_option{ { grouping_strategy.value().AsFFI() }, true }) : (icu4x::capi::DecimalGroupingStrategy_option{ {}, false }));
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::DecimalFormatter>>(std::unique_ptr<icu4x::DecimalFormatter>(icu4x::DecimalFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::DecimalFormatter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::DecimalFormatter::format(const icu4x::Decimal& value) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_DecimalFormatter_format_mv1(this->AsFFI(),
        value.AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::DecimalFormatter::format_write(const icu4x::Decimal& value, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_DecimalFormatter_format_mv1(this->AsFFI(),
        value.AsFFI(),
        &write);
}

inline const icu4x::capi::DecimalFormatter* icu4x::DecimalFormatter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::DecimalFormatter*>(this);
}

inline icu4x::capi::DecimalFormatter* icu4x::DecimalFormatter::AsFFI() {
    return reinterpret_cast<icu4x::capi::DecimalFormatter*>(this);
}

inline const icu4x::DecimalFormatter* icu4x::DecimalFormatter::FromFFI(const icu4x::capi::DecimalFormatter* ptr) {
    return reinterpret_cast<const icu4x::DecimalFormatter*>(ptr);
}

inline icu4x::DecimalFormatter* icu4x::DecimalFormatter::FromFFI(icu4x::capi::DecimalFormatter* ptr) {
    return reinterpret_cast<icu4x::DecimalFormatter*>(ptr);
}

inline void icu4x::DecimalFormatter::operator delete(void* ptr) {
    icu4x::capi::icu4x_DecimalFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::DecimalFormatter*>(ptr));
}


#endif // ICU4X_DecimalFormatter_HPP
