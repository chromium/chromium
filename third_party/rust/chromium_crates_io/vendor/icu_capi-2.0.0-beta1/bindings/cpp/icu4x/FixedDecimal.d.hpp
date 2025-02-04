#ifndef icu4x_FixedDecimal_D_HPP
#define icu4x_FixedDecimal_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct FixedDecimal; }
class FixedDecimal;
struct FixedDecimalLimitError;
class FixedDecimalParseError;
class FixedDecimalRoundingIncrement;
class FixedDecimalRoundingMode;
class FixedDecimalSign;
class FixedDecimalSignDisplay;
}


namespace icu4x {
namespace capi {
    struct FixedDecimal;
} // namespace capi
} // namespace

namespace icu4x {
class FixedDecimal {
public:

  inline static std::unique_ptr<icu4x::FixedDecimal> from(int32_t v);

  inline static std::unique_ptr<icu4x::FixedDecimal> from(uint32_t v);

  inline static std::unique_ptr<icu4x::FixedDecimal> from(int64_t v);

  inline static std::unique_ptr<icu4x::FixedDecimal> from(uint64_t v);

  inline static diplomat::result<std::unique_ptr<icu4x::FixedDecimal>, icu4x::FixedDecimalLimitError> from_double_with_integer_precision(double f);

  inline static diplomat::result<std::unique_ptr<icu4x::FixedDecimal>, icu4x::FixedDecimalLimitError> from_double_with_lower_magnitude(double f, int16_t magnitude);

  inline static diplomat::result<std::unique_ptr<icu4x::FixedDecimal>, icu4x::FixedDecimalLimitError> from_double_with_significant_digits(double f, uint8_t digits);

  inline static diplomat::result<std::unique_ptr<icu4x::FixedDecimal>, icu4x::FixedDecimalLimitError> from_double_with_round_trip_precision(double f);

  inline static diplomat::result<std::unique_ptr<icu4x::FixedDecimal>, icu4x::FixedDecimalParseError> from_string(std::string_view v);

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

  inline void pad_start(int16_t position);

  inline void pad_end(int16_t position);

  inline void set_max_position(int16_t position);

  inline void round(int16_t position);

  inline void ceil(int16_t position);

  inline void expand(int16_t position);

  inline void floor(int16_t position);

  inline void trunc(int16_t position);

  inline void round_with_mode(int16_t position, icu4x::FixedDecimalRoundingMode mode);

  inline void round_with_mode_and_increment(int16_t position, icu4x::FixedDecimalRoundingMode mode, icu4x::FixedDecimalRoundingIncrement increment);

  inline diplomat::result<std::monostate, std::monostate> concatenate_end(icu4x::FixedDecimal& other);

  inline std::string to_string() const;

  inline const icu4x::capi::FixedDecimal* AsFFI() const;
  inline icu4x::capi::FixedDecimal* AsFFI();
  inline static const icu4x::FixedDecimal* FromFFI(const icu4x::capi::FixedDecimal* ptr);
  inline static icu4x::FixedDecimal* FromFFI(icu4x::capi::FixedDecimal* ptr);
  inline static void operator delete(void* ptr);
private:
  FixedDecimal() = delete;
  FixedDecimal(const icu4x::FixedDecimal&) = delete;
  FixedDecimal(icu4x::FixedDecimal&&) noexcept = delete;
  FixedDecimal operator=(const icu4x::FixedDecimal&) = delete;
  FixedDecimal operator=(icu4x::FixedDecimal&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_FixedDecimal_D_HPP
