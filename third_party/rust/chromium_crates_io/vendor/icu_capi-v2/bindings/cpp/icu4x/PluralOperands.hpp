#ifndef ICU4X_PluralOperands_HPP
#define ICU4X_PluralOperands_HPP

#include "PluralOperands.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Decimal.hpp"
#include "DecimalParseError.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_PluralOperands_from_string_mv1_result {union {icu4x::capi::PluralOperands* ok; icu4x::capi::DecimalParseError err;}; bool is_ok;} icu4x_PluralOperands_from_string_mv1_result;
    icu4x_PluralOperands_from_string_mv1_result icu4x_PluralOperands_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    icu4x::capi::PluralOperands* icu4x_PluralOperands_from_int64_mv1(int64_t i);

    icu4x::capi::PluralOperands* icu4x_PluralOperands_from_fixed_decimal_mv1(const icu4x::capi::Decimal* x);

    void icu4x_PluralOperands_destroy_mv1(PluralOperands* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PluralOperands>, icu4x::DecimalParseError> icu4x::PluralOperands::from_string(std::string_view s) {
    auto result = icu4x::capi::icu4x_PluralOperands_from_string_mv1({s.data(), s.size()});
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PluralOperands>, icu4x::DecimalParseError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PluralOperands>>(std::unique_ptr<icu4x::PluralOperands>(icu4x::PluralOperands::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PluralOperands>, icu4x::DecimalParseError>(icu4x::diplomat::Err<icu4x::DecimalParseError>(icu4x::DecimalParseError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PluralOperands> icu4x::PluralOperands::from(int64_t i) {
    auto result = icu4x::capi::icu4x_PluralOperands_from_int64_mv1(i);
    return std::unique_ptr<icu4x::PluralOperands>(icu4x::PluralOperands::FromFFI(result));
}

inline std::unique_ptr<icu4x::PluralOperands> icu4x::PluralOperands::from_fixed_decimal(const icu4x::Decimal& x) {
    auto result = icu4x::capi::icu4x_PluralOperands_from_fixed_decimal_mv1(x.AsFFI());
    return std::unique_ptr<icu4x::PluralOperands>(icu4x::PluralOperands::FromFFI(result));
}

inline const icu4x::capi::PluralOperands* icu4x::PluralOperands::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::PluralOperands*>(this);
}

inline icu4x::capi::PluralOperands* icu4x::PluralOperands::AsFFI() {
    return reinterpret_cast<icu4x::capi::PluralOperands*>(this);
}

inline const icu4x::PluralOperands* icu4x::PluralOperands::FromFFI(const icu4x::capi::PluralOperands* ptr) {
    return reinterpret_cast<const icu4x::PluralOperands*>(ptr);
}

inline icu4x::PluralOperands* icu4x::PluralOperands::FromFFI(icu4x::capi::PluralOperands* ptr) {
    return reinterpret_cast<icu4x::PluralOperands*>(ptr);
}

inline void icu4x::PluralOperands::operator delete(void* ptr) {
    icu4x::capi::icu4x_PluralOperands_destroy_mv1(reinterpret_cast<icu4x::capi::PluralOperands*>(ptr));
}


#endif // ICU4X_PluralOperands_HPP
