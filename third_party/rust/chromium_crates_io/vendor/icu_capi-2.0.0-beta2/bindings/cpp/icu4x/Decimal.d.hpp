#ifndef icu4x_Decimal_D_HPP
#define icu4x_Decimal_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Decimal; }
class Decimal;
struct FixedDecimalLimitError;
class FixedDecimalParseError;
class FixedDecimalRoundingIncrement;
class FixedDecimalSign;
class FixedDecimalSignDisplay;
class FixedDecimalSignedRoundingMode;
}


namespace icu4x {
namespace capi {
    struct Decimal;
} // namespace capi
} // namespace

namespace icu4x {
class Decimal {
public:

  inline static std::unique_ptr<icu4x::Decimal> from(int32_t v);

  inline static std::unique_ptr<icu4x::Decimal> from(uint32_t v);

  inline static std::unique_ptr<icu4x::Decimal> from(int64_t v);

  inline static std::unique_ptr<icu4x::Decimal> from(uint64_t v);

  inline static diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::FixedDecimalLimitError> from_double_with_integer_precision(double f);

  inline static diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::FixedDecimalLimitError> from_double_with_lower_magnitude(double f, int16_t magnitude);

  inline static diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::FixedDecimalLimitError> from_double_with_significant_digits(double f, uint8_t digits);

  inline static diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::FixedDecimalLimitError> from_double_with_round_trip_precision(double f);

  inline static diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::FixedDecimalParseError> from_string(std::string_view v);

  inline uint8_t digit_at(int16_t magnitude) const;

  inline int16_t magnitude_start() const;

  inline int16_t magnitude_end() const;

  inline int16_t nonzero_magnitude_start() const;

  inline int16_t nonzero_magnitude_end() const;

  inline bool is_zero() const;

  inline void multiply_pow10(int16_t power);

  inline icu4x::FixedDecimalSign sign() const;

  inline void set_sign(icu4x::FixedDecimalSign sign);

  inline void apply_sign_display(icu4x::FixedDecimalSignDisplay sign_display);

  inline void trim_start();

  inline void trim_end();

  inline void trim_end_if_integer();

  inline void pad_start(int16_t position);

  inline void pad_end(int16_t position);

  inline void set_max_position(int16_t position);

  inline void round(int16_t position);

  inline void ceil(int16_t position);

  inline void expand(int16_t position);

  inline void floor(int16_t position);

  inline void trunc(int16_t position);

  inline void round_with_mode(int16_t position, icu4x::FixedDecimalSignedRoundingMode mode);

  inline void round_with_mode_and_increment(int16_t position, icu4x::FixedDecimalSignedRoundingMode mode, icu4x::FixedDecimalRoundingIncrement increment);

  inline diplomat::result<std::monostate, std::monostate> concatenate_end(icu4x::Decimal& other);

  inline std::string to_string() const;

  inline const icu4x::capi::Decimal* AsFFI() const;
  inline icu4x::capi::Decimal* AsFFI();
  inline static const icu4x::Decimal* FromFFI(const icu4x::capi::Decimal* ptr);
  inline static icu4x::Decimal* FromFFI(icu4x::capi::Decimal* ptr);
  inline static void operator delete(void* ptr);
private:
  Decimal() = delete;
  Decimal(const icu4x::Decimal&) = delete;
  Decimal(icu4x::Decimal&&) noexcept = delete;
  Decimal operator=(const icu4x::Decimal&) = delete;
  Decimal operator=(icu4x::Decimal&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_Decimal_D_HPP
