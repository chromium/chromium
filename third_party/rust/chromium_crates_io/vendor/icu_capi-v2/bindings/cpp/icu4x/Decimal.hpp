#ifndef ICU4X_Decimal_HPP
#define ICU4X_Decimal_HPP

#include "Decimal.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DecimalLimitError.hpp"
#include "DecimalParseError.hpp"
#include "DecimalRoundingIncrement.hpp"
#include "DecimalSign.hpp"
#include "DecimalSignDisplay.hpp"
#include "DecimalSignedRoundingMode.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::Decimal* icu4x_Decimal_from_int32_mv1(int32_t v);

    icu4x::capi::Decimal* icu4x_Decimal_from_uint32_mv1(uint32_t v);

    icu4x::capi::Decimal* icu4x_Decimal_from_int64_mv1(int64_t v);

    icu4x::capi::Decimal* icu4x_Decimal_from_uint64_mv1(uint64_t v);

    typedef struct icu4x_Decimal_from_double_with_integer_precision_mv1_result {union {icu4x::capi::Decimal* ok; }; bool is_ok;} icu4x_Decimal_from_double_with_integer_precision_mv1_result;
    icu4x_Decimal_from_double_with_integer_precision_mv1_result icu4x_Decimal_from_double_with_integer_precision_mv1(double f);

    typedef struct icu4x_Decimal_from_double_with_lower_magnitude_mv1_result {union {icu4x::capi::Decimal* ok; }; bool is_ok;} icu4x_Decimal_from_double_with_lower_magnitude_mv1_result;
    icu4x_Decimal_from_double_with_lower_magnitude_mv1_result icu4x_Decimal_from_double_with_lower_magnitude_mv1(double f, int16_t magnitude);

    typedef struct icu4x_Decimal_from_double_with_significant_digits_mv1_result {union {icu4x::capi::Decimal* ok; }; bool is_ok;} icu4x_Decimal_from_double_with_significant_digits_mv1_result;
    icu4x_Decimal_from_double_with_significant_digits_mv1_result icu4x_Decimal_from_double_with_significant_digits_mv1(double f, uint8_t digits);

    typedef struct icu4x_Decimal_from_double_with_round_trip_precision_mv1_result {union {icu4x::capi::Decimal* ok; }; bool is_ok;} icu4x_Decimal_from_double_with_round_trip_precision_mv1_result;
    icu4x_Decimal_from_double_with_round_trip_precision_mv1_result icu4x_Decimal_from_double_with_round_trip_precision_mv1(double f);

    typedef struct icu4x_Decimal_from_string_mv1_result {union {icu4x::capi::Decimal* ok; icu4x::capi::DecimalParseError err;}; bool is_ok;} icu4x_Decimal_from_string_mv1_result;
    icu4x_Decimal_from_string_mv1_result icu4x_Decimal_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v);

    uint8_t icu4x_Decimal_digit_at_mv1(const icu4x::capi::Decimal* self, int16_t magnitude);

    int16_t icu4x_Decimal_magnitude_start_mv1(const icu4x::capi::Decimal* self);

    int16_t icu4x_Decimal_magnitude_end_mv1(const icu4x::capi::Decimal* self);

    int16_t icu4x_Decimal_nonzero_magnitude_start_mv1(const icu4x::capi::Decimal* self);

    int16_t icu4x_Decimal_nonzero_magnitude_end_mv1(const icu4x::capi::Decimal* self);

    bool icu4x_Decimal_is_zero_mv1(const icu4x::capi::Decimal* self);

    void icu4x_Decimal_multiply_pow10_mv1(icu4x::capi::Decimal* self, int16_t power);

    icu4x::capi::DecimalSign icu4x_Decimal_sign_mv1(const icu4x::capi::Decimal* self);

    void icu4x_Decimal_set_sign_mv1(icu4x::capi::Decimal* self, icu4x::capi::DecimalSign sign);

    void icu4x_Decimal_apply_sign_display_mv1(icu4x::capi::Decimal* self, icu4x::capi::DecimalSignDisplay sign_display);

    void icu4x_Decimal_trim_start_mv1(icu4x::capi::Decimal* self);

    void icu4x_Decimal_trim_end_mv1(icu4x::capi::Decimal* self);

    void icu4x_Decimal_trim_end_if_integer_mv1(icu4x::capi::Decimal* self);

    void icu4x_Decimal_pad_start_mv1(icu4x::capi::Decimal* self, int16_t position);

    void icu4x_Decimal_pad_end_mv1(icu4x::capi::Decimal* self, int16_t position);

    void icu4x_Decimal_set_max_position_mv1(icu4x::capi::Decimal* self, int16_t position);

    void icu4x_Decimal_round_mv1(icu4x::capi::Decimal* self, int16_t position);

    void icu4x_Decimal_ceil_mv1(icu4x::capi::Decimal* self, int16_t position);

    void icu4x_Decimal_expand_mv1(icu4x::capi::Decimal* self, int16_t position);

    void icu4x_Decimal_floor_mv1(icu4x::capi::Decimal* self, int16_t position);

    void icu4x_Decimal_trunc_mv1(icu4x::capi::Decimal* self, int16_t position);

    void icu4x_Decimal_round_with_mode_mv1(icu4x::capi::Decimal* self, int16_t position, icu4x::capi::DecimalSignedRoundingMode mode);

    void icu4x_Decimal_round_with_mode_and_increment_mv1(icu4x::capi::Decimal* self, int16_t position, icu4x::capi::DecimalSignedRoundingMode mode, icu4x::capi::DecimalRoundingIncrement increment);

    typedef struct icu4x_Decimal_concatenate_end_mv1_result { bool is_ok;} icu4x_Decimal_concatenate_end_mv1_result;
    icu4x_Decimal_concatenate_end_mv1_result icu4x_Decimal_concatenate_end_mv1(icu4x::capi::Decimal* self, icu4x::capi::Decimal* other);

    void icu4x_Decimal_to_string_mv1(const icu4x::capi::Decimal* self, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_Decimal_destroy_mv1(Decimal* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::Decimal> icu4x::Decimal::from(int32_t v) {
    auto result = icu4x::capi::icu4x_Decimal_from_int32_mv1(v);
    return std::unique_ptr<icu4x::Decimal>(icu4x::Decimal::FromFFI(result));
}

inline std::unique_ptr<icu4x::Decimal> icu4x::Decimal::from(uint32_t v) {
    auto result = icu4x::capi::icu4x_Decimal_from_uint32_mv1(v);
    return std::unique_ptr<icu4x::Decimal>(icu4x::Decimal::FromFFI(result));
}

inline std::unique_ptr<icu4x::Decimal> icu4x::Decimal::from(int64_t v) {
    auto result = icu4x::capi::icu4x_Decimal_from_int64_mv1(v);
    return std::unique_ptr<icu4x::Decimal>(icu4x::Decimal::FromFFI(result));
}

inline std::unique_ptr<icu4x::Decimal> icu4x::Decimal::from(uint64_t v) {
    auto result = icu4x::capi::icu4x_Decimal_from_uint64_mv1(v);
    return std::unique_ptr<icu4x::Decimal>(icu4x::Decimal::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError> icu4x::Decimal::from_double_with_integer_precision(double f) {
    auto result = icu4x::capi::icu4x_Decimal_from_double_with_integer_precision_mv1(f);
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Decimal>>(std::unique_ptr<icu4x::Decimal>(icu4x::Decimal::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError>(icu4x::diplomat::Err<icu4x::DecimalLimitError>(icu4x::DecimalLimitError {}));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError> icu4x::Decimal::from_double_with_lower_magnitude(double f, int16_t magnitude) {
    auto result = icu4x::capi::icu4x_Decimal_from_double_with_lower_magnitude_mv1(f,
        magnitude);
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Decimal>>(std::unique_ptr<icu4x::Decimal>(icu4x::Decimal::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError>(icu4x::diplomat::Err<icu4x::DecimalLimitError>(icu4x::DecimalLimitError {}));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError> icu4x::Decimal::from_double_with_significant_digits(double f, uint8_t digits) {
    auto result = icu4x::capi::icu4x_Decimal_from_double_with_significant_digits_mv1(f,
        digits);
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Decimal>>(std::unique_ptr<icu4x::Decimal>(icu4x::Decimal::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError>(icu4x::diplomat::Err<icu4x::DecimalLimitError>(icu4x::DecimalLimitError {}));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError> icu4x::Decimal::from_double_with_round_trip_precision(double f) {
    auto result = icu4x::capi::icu4x_Decimal_from_double_with_round_trip_precision_mv1(f);
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Decimal>>(std::unique_ptr<icu4x::Decimal>(icu4x::Decimal::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError>(icu4x::diplomat::Err<icu4x::DecimalLimitError>(icu4x::DecimalLimitError {}));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalParseError> icu4x::Decimal::from_string(std::string_view v) {
    auto result = icu4x::capi::icu4x_Decimal_from_string_mv1({v.data(), v.size()});
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalParseError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Decimal>>(std::unique_ptr<icu4x::Decimal>(icu4x::Decimal::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalParseError>(icu4x::diplomat::Err<icu4x::DecimalParseError>(icu4x::DecimalParseError::FromFFI(result.err)));
}

inline uint8_t icu4x::Decimal::digit_at(int16_t magnitude) const {
    auto result = icu4x::capi::icu4x_Decimal_digit_at_mv1(this->AsFFI(),
        magnitude);
    return result;
}

inline int16_t icu4x::Decimal::magnitude_start() const {
    auto result = icu4x::capi::icu4x_Decimal_magnitude_start_mv1(this->AsFFI());
    return result;
}

inline int16_t icu4x::Decimal::magnitude_end() const {
    auto result = icu4x::capi::icu4x_Decimal_magnitude_end_mv1(this->AsFFI());
    return result;
}

inline int16_t icu4x::Decimal::nonzero_magnitude_start() const {
    auto result = icu4x::capi::icu4x_Decimal_nonzero_magnitude_start_mv1(this->AsFFI());
    return result;
}

inline int16_t icu4x::Decimal::nonzero_magnitude_end() const {
    auto result = icu4x::capi::icu4x_Decimal_nonzero_magnitude_end_mv1(this->AsFFI());
    return result;
}

inline bool icu4x::Decimal::is_zero() const {
    auto result = icu4x::capi::icu4x_Decimal_is_zero_mv1(this->AsFFI());
    return result;
}

inline void icu4x::Decimal::multiply_pow10(int16_t power) {
    icu4x::capi::icu4x_Decimal_multiply_pow10_mv1(this->AsFFI(),
        power);
}

inline icu4x::DecimalSign icu4x::Decimal::sign() const {
    auto result = icu4x::capi::icu4x_Decimal_sign_mv1(this->AsFFI());
    return icu4x::DecimalSign::FromFFI(result);
}

inline void icu4x::Decimal::set_sign(icu4x::DecimalSign sign) {
    icu4x::capi::icu4x_Decimal_set_sign_mv1(this->AsFFI(),
        sign.AsFFI());
}

inline void icu4x::Decimal::apply_sign_display(icu4x::DecimalSignDisplay sign_display) {
    icu4x::capi::icu4x_Decimal_apply_sign_display_mv1(this->AsFFI(),
        sign_display.AsFFI());
}

inline void icu4x::Decimal::trim_start() {
    icu4x::capi::icu4x_Decimal_trim_start_mv1(this->AsFFI());
}

inline void icu4x::Decimal::trim_end() {
    icu4x::capi::icu4x_Decimal_trim_end_mv1(this->AsFFI());
}

inline void icu4x::Decimal::trim_end_if_integer() {
    icu4x::capi::icu4x_Decimal_trim_end_if_integer_mv1(this->AsFFI());
}

inline void icu4x::Decimal::pad_start(int16_t position) {
    icu4x::capi::icu4x_Decimal_pad_start_mv1(this->AsFFI(),
        position);
}

inline void icu4x::Decimal::pad_end(int16_t position) {
    icu4x::capi::icu4x_Decimal_pad_end_mv1(this->AsFFI(),
        position);
}

inline void icu4x::Decimal::set_max_position(int16_t position) {
    icu4x::capi::icu4x_Decimal_set_max_position_mv1(this->AsFFI(),
        position);
}

inline void icu4x::Decimal::round(int16_t position) {
    icu4x::capi::icu4x_Decimal_round_mv1(this->AsFFI(),
        position);
}

inline void icu4x::Decimal::ceil(int16_t position) {
    icu4x::capi::icu4x_Decimal_ceil_mv1(this->AsFFI(),
        position);
}

inline void icu4x::Decimal::expand(int16_t position) {
    icu4x::capi::icu4x_Decimal_expand_mv1(this->AsFFI(),
        position);
}

inline void icu4x::Decimal::floor(int16_t position) {
    icu4x::capi::icu4x_Decimal_floor_mv1(this->AsFFI(),
        position);
}

inline void icu4x::Decimal::trunc(int16_t position) {
    icu4x::capi::icu4x_Decimal_trunc_mv1(this->AsFFI(),
        position);
}

inline void icu4x::Decimal::round_with_mode(int16_t position, icu4x::DecimalSignedRoundingMode mode) {
    icu4x::capi::icu4x_Decimal_round_with_mode_mv1(this->AsFFI(),
        position,
        mode.AsFFI());
}

inline void icu4x::Decimal::round_with_mode_and_increment(int16_t position, icu4x::DecimalSignedRoundingMode mode, icu4x::DecimalRoundingIncrement increment) {
    icu4x::capi::icu4x_Decimal_round_with_mode_and_increment_mv1(this->AsFFI(),
        position,
        mode.AsFFI(),
        increment.AsFFI());
}

inline icu4x::diplomat::result<std::monostate, std::monostate> icu4x::Decimal::concatenate_end(icu4x::Decimal& other) {
    auto result = icu4x::capi::icu4x_Decimal_concatenate_end_mv1(this->AsFFI(),
        other.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::monostate, std::monostate>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, std::monostate>(icu4x::diplomat::Err<std::monostate>());
}

inline std::string icu4x::Decimal::to_string() const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_Decimal_to_string_mv1(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::Decimal::to_string_write(W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_Decimal_to_string_mv1(this->AsFFI(),
        &write);
}

inline const icu4x::capi::Decimal* icu4x::Decimal::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::Decimal*>(this);
}

inline icu4x::capi::Decimal* icu4x::Decimal::AsFFI() {
    return reinterpret_cast<icu4x::capi::Decimal*>(this);
}

inline const icu4x::Decimal* icu4x::Decimal::FromFFI(const icu4x::capi::Decimal* ptr) {
    return reinterpret_cast<const icu4x::Decimal*>(ptr);
}

inline icu4x::Decimal* icu4x::Decimal::FromFFI(icu4x::capi::Decimal* ptr) {
    return reinterpret_cast<icu4x::Decimal*>(ptr);
}

inline void icu4x::Decimal::operator delete(void* ptr) {
    icu4x::capi::icu4x_Decimal_destroy_mv1(reinterpret_cast<icu4x::capi::Decimal*>(ptr));
}


#endif // ICU4X_Decimal_HPP
