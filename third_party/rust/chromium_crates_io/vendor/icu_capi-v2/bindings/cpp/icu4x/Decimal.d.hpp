#ifndef ICU4X_Decimal_D_HPP
#define ICU4X_Decimal_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
namespace capi { struct Decimal; }
class Decimal;
struct DecimalLimitError;
class DecimalParseError;
class DecimalRoundingIncrement;
class DecimalSign;
class DecimalSignDisplay;
class DecimalSignedRoundingMode;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct Decimal;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `Decimal`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html) for more information.
 */
class Decimal {
public:

  /**
   * Construct an {@link Decimal} from an integer.
   *
   * See the [Rust documentation for `Decimal`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html) for more information.
   */
  inline static std::unique_ptr<icu4x::Decimal> from(int32_t v);

  /**
   * Construct an {@link Decimal} from an integer.
   *
   * See the [Rust documentation for `Decimal`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html) for more information.
   */
  inline static std::unique_ptr<icu4x::Decimal> from(uint32_t v);

  /**
   * Construct an {@link Decimal} from an integer.
   *
   * See the [Rust documentation for `Decimal`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html) for more information.
   */
  inline static std::unique_ptr<icu4x::Decimal> from(int64_t v);

  /**
   * Construct an {@link Decimal} from an integer.
   *
   * See the [Rust documentation for `Decimal`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html) for more information.
   */
  inline static std::unique_ptr<icu4x::Decimal> from(uint64_t v);

  /**
   * Construct an {@link Decimal} from an integer-valued float
   *
   * See the [Rust documentation for `try_from_f64`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.try_from_f64) for more information.
   *
   * See the [Rust documentation for `FloatPrecision`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/enum.FloatPrecision.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError> from_double_with_integer_precision(double f);

  /**
   * Construct an {@link Decimal} from an float, with a given power of 10 for the lower magnitude
   *
   * See the [Rust documentation for `try_from_f64`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.try_from_f64) for more information.
   *
   * See the [Rust documentation for `FloatPrecision`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/enum.FloatPrecision.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError> from_double_with_lower_magnitude(double f, int16_t magnitude);

  /**
   * Construct an {@link Decimal} from an float, for a given number of significant digits
   *
   * See the [Rust documentation for `try_from_f64`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.try_from_f64) for more information.
   *
   * See the [Rust documentation for `FloatPrecision`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/enum.FloatPrecision.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError> from_double_with_significant_digits(double f, uint8_t digits);

  /**
   * Construct an {@link Decimal} from an float, with enough digits to recover
   * the original floating point in IEEE 754 without needing trailing zeros
   *
   * See the [Rust documentation for `try_from_f64`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.try_from_f64) for more information.
   *
   * See the [Rust documentation for `FloatPrecision`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/enum.FloatPrecision.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalLimitError> from_double_with_round_trip_precision(double f);

  /**
   * Construct an {@link Decimal} from a string.
   *
   * See the [Rust documentation for `try_from_str`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.try_from_str) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::Decimal>, icu4x::DecimalParseError> from_string(std::string_view v);

  /**
   * See the [Rust documentation for `digit_at`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.digit_at) for more information.
   */
  inline uint8_t digit_at(int16_t magnitude) const;

  /**
   * See the [Rust documentation for `magnitude_range`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.magnitude_range) for more information.
   */
  inline int16_t magnitude_start() const;

  /**
   * See the [Rust documentation for `magnitude_range`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.magnitude_range) for more information.
   */
  inline int16_t magnitude_end() const;

  /**
   * See the [Rust documentation for `nonzero_magnitude_start`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.nonzero_magnitude_start) for more information.
   */
  inline int16_t nonzero_magnitude_start() const;

  /**
   * See the [Rust documentation for `nonzero_magnitude_end`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.nonzero_magnitude_end) for more information.
   */
  inline int16_t nonzero_magnitude_end() const;

  /**
   * See the [Rust documentation for `is_zero`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.is_zero) for more information.
   */
  inline bool is_zero() const;

  /**
   * Multiply the {@link Decimal} by a given power of ten.
   *
   * See the [Rust documentation for `multiply_pow10`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.multiply_pow10) for more information.
   */
  inline void multiply_pow10(int16_t power);

  /**
   * See the [Rust documentation for `sign`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.sign) for more information.
   */
  inline icu4x::DecimalSign sign() const;

  /**
   * Set the sign of the {@link Decimal}.
   *
   * See the [Rust documentation for `set_sign`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.set_sign) for more information.
   */
  inline void set_sign(icu4x::DecimalSign sign);

  /**
   * See the [Rust documentation for `apply_sign_display`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.apply_sign_display) for more information.
   */
  inline void apply_sign_display(icu4x::DecimalSignDisplay sign_display);

  /**
   * See the [Rust documentation for `trim_start`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.trim_start) for more information.
   */
  inline void trim_start();

  /**
   * See the [Rust documentation for `trim_end`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.trim_end) for more information.
   */
  inline void trim_end();

  /**
   * See the [Rust documentation for `trim_end_if_integer`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.trim_end_if_integer) for more information.
   */
  inline void trim_end_if_integer();

  /**
   * Zero-pad the {@link Decimal} on the left to a particular position
   *
   * See the [Rust documentation for `pad_start`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.pad_start) for more information.
   */
  inline void pad_start(int16_t position);

  /**
   * Zero-pad the {@link Decimal} on the right to a particular position
   *
   * See the [Rust documentation for `pad_end`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.pad_end) for more information.
   */
  inline void pad_end(int16_t position);

  /**
   * Truncate the {@link Decimal} on the left to a particular position, deleting digits if necessary. This is useful for, e.g. abbreviating years
   * ("2022" -> "22")
   *
   * See the [Rust documentation for `set_max_position`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.set_max_position) for more information.
   */
  inline void set_max_position(int16_t position);

  /**
   * Round the number at a particular digit position.
   *
   * This uses half to even rounding, which resolves ties by selecting the nearest
   * even integer to the original value.
   *
   * See the [Rust documentation for `round`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.round) for more information.
   */
  inline void round(int16_t position);

  /**
   * See the [Rust documentation for `ceil`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.ceil) for more information.
   */
  inline void ceil(int16_t position);

  /**
   * See the [Rust documentation for `expand`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.expand) for more information.
   */
  inline void expand(int16_t position);

  /**
   * See the [Rust documentation for `floor`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.floor) for more information.
   */
  inline void floor(int16_t position);

  /**
   * See the [Rust documentation for `trunc`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.trunc) for more information.
   */
  inline void trunc(int16_t position);

  /**
   * See the [Rust documentation for `round_with_mode`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.round_with_mode) for more information.
   */
  inline void round_with_mode(int16_t position, icu4x::DecimalSignedRoundingMode mode);

  /**
   * See the [Rust documentation for `round_with_mode_and_increment`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.round_with_mode_and_increment) for more information.
   */
  inline void round_with_mode_and_increment(int16_t position, icu4x::DecimalSignedRoundingMode mode, icu4x::DecimalRoundingIncrement increment);

  /**
   * Concatenates `other` to the end of `self`.
   *
   * If successful, `other` will be set to 0 and a successful status is returned.
   *
   * If not successful, `other` will be unchanged and an error is returned.
   *
   * See the [Rust documentation for `concatenate_end`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.concatenate_end) for more information.
   */
  inline icu4x::diplomat::result<std::monostate, std::monostate> concatenate_end(icu4x::Decimal& other);

  /**
   * Format the {@link Decimal} as a string.
   *
   * See the [Rust documentation for `write_to`](https://docs.rs/fixed_decimal/0.7.2/fixed_decimal/type.Decimal.html#method.write_to) for more information.
   */
  inline std::string to_string() const;
  template<typename W>
  inline void to_string_write(W& writeable_output) const;

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
#endif // ICU4X_Decimal_D_HPP
