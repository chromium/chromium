/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef COMMON_UTILS_NUMERIC_UTILS_H_
#define COMMON_UTILS_NUMERIC_UTILS_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/common/utils/validation_utils.h"

namespace iamf_tools {

/*!\brief Sums the input values and checks for overflow.
 *
 * \param x_1 First summand.
 * \param x_2 Second summand.
 * \param result Sum of the inputs on success.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` when
 *         the sum would cause an overflow in a `uint32_t`.
 */
absl::Status AddUint32CheckOverflow(uint32_t x_1, uint32_t x_2,
                                    uint32_t& result);

/*!\brief Converts float input to Q7.8 format.
 *
 * \param value Value to convert.
 * \param result Converted value if successful. The result is floored to the
 *        nearest Q7.8 value.
 * \return `absl::OkStatus()` if successful. `absl::UnknownError()` if the input
 *         is not valid in Q7.8 format.
 */
absl::Status FloatToQ7_8(float value, int16_t& result);

/*!\brief Converts Q7.8 input to float output.
 *
 * \param value Value to convert.
 * \return Converted value.
 */
float Q7_8ToFloat(int16_t value);

// TODO(b/283281856): Consider removing `FloatToQ0_8()` if it is still an unused
//                    function after the encoder supports resampling parameter
//                    blocks.
/*!\brief Converts float input to Q0.8 format.
 *
 * \param value Value to convert.
 * \param result Converted value if successful. The result is floored to the
 *        nearest Q0.8 value.
 * \return `absl::OkStatus()` if successful. `absl::UnknownError()` if the input
 *         is not valid in Q0.8 format.
 */
absl::Status FloatToQ0_8(float value, uint8_t& result);

/*!\brief Converts Q0.8 input to float output.
 *
 * \param value Value to convert.
 * \return Converted value.
 */
float Q0_8ToFloat(uint8_t value);

/*!\brief Typecasts the input value and writes to the output argument if valid.
 *
 * The custom `field_name` is used to create a more descriptive error message.
 * This is inserted surrounded by backticks. When this refers to a user facing
 * field (i.e. related to `UserMetadata` protos) this should refer to the
 * user-facing field name.
 *
 * \param field_name Field name to insert into the error message.
 * \param input Value to convert.
 * \param output Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the input is outside the expected range.
 */
template <typename InputType, typename OutputType>
absl::Status StaticCastIfInRange(absl::string_view field_name, InputType input,
                                 OutputType& output) {
  // As a special case, we don't care if the `char` is signed or unsigned.
  // Always cast it to the `uint8_t`, which is typically used in the codebase
  // as the "raw bytes" type.
  constexpr bool is_char_to_raw_bytes =
      std::is_same_v<InputType, char> && std::is_same_v<OutputType, uint8_t>;
  constexpr OutputType kMinOutput = std::numeric_limits<OutputType>::min();
  constexpr OutputType kMaxOutput = std::numeric_limits<OutputType>::max();
  bool is_in_range = kMinOutput <= input && input <= kMaxOutput;

  if (is_in_range || is_char_to_raw_bytes) [[likely]] {
    output = static_cast<OutputType>(input);
    return absl::OkStatus();
  }

  std::string message =
      absl::StrCat(field_name, " is outside the expected range of ");
  if constexpr (std::is_same_v<OutputType, char> ||
                std::is_same_v<OutputType, unsigned char>) {
    absl::StrAppend(&message, "[0, 255]");
  } else {
    absl::StrAppend(&message, "[", kMinOutput, ", ", kMaxOutput, "]");
  }
  return absl::InvalidArgumentError(message);
}

/*!\brief Creates a 32-bit signed integer from the [1, 4] input `bytes`.
 *
 * \param bytes Bytes to convert.
 * \param output Converted value if successful. The result is left-justified;
 *        the upper `bytes.size()` bytes are set based on the input and the
 *        remaining lower bytes are 0.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the number of bytes is not in the range of [1, 4].
 */
absl::Status LittleEndianBytesToInt32(absl::Span<const uint8_t> bytes,
                                      int32_t& output);

/*!\brief Creates a 32-bit signed integer from the [1, 4] input `bytes`.
 *
 * \param bytes Bytes to convert.
 * \param output Converted value if successful. The result is left-justified;
 *        the upper `bytes.size()` bytes are set based on the input and the
 *        remaining lower bytes are 0.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the number of bytes is not in the range of [1, 4].
 */
absl::Status BigEndianBytesToInt32(absl::Span<const uint8_t> bytes,
                                   int32_t& output);

/*!\brief Clips and typecasts the input value and writes to the output argument.
 *
 * \param input Value to convert.
 * \param output Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the input is NaN.
 */
absl::Status ClipDoubleToInt32(double input, int32_t& output);

namespace obu_util_internal {

constexpr double kMaxInt32PlusOneAsDouble =
    static_cast<double>(std::numeric_limits<int32_t>::max()) + 1.0;

}  // namespace obu_util_internal

/*!\brief Normalizes the input value to a floating point in the range [-1, +1].
 *
 * Normalizes the input from [std::numeric_limits<int32_t>::min(),
 * std::numeric_limits<int32_t>::max() + 1] to [-1, +1].
 *
 * \param value Value to normalize.
 * \return Normalized value.
 */
template <typename T>
constexpr T Int32ToNormalizedFloatingPoint(int32_t value) {
  using obu_util_internal::kMaxInt32PlusOneAsDouble;
  static_assert(std::is_floating_point_v<T>);

  // Perform calculations in double. The final cast to the output type, e.g.
  // `float` could result in loss of precision. Note that casting `int32_t` to
  // `double` is lossless; every `int32_t` can be exactly represented.
  return static_cast<T>(static_cast<double>(value) / kMaxInt32PlusOneAsDouble);
}

/*!\brief Converts normalized floating point input to an `int32_t`.
 *
 * Transforms the input from the range of [-1, +1] to the range of
 * [std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() +
 * 1].
 *
 * Input is clamped to [-1, +1] before processing. Output is clamped to the
 * full range of an `int32_t`.
 *
 * \param value Normalized floating point value to convert.
 * \param result Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the input is any type of NaN or infinity.
 */
template <typename T>
absl::Status NormalizedFloatingPointToInt32(T value, int32_t& result) {
  using obu_util_internal::kMaxInt32PlusOneAsDouble;
  static_assert(std::is_floating_point_v<T>);
  if (std::isnan(value) || std::isinf(value)) {
    return absl::InvalidArgumentError("Input is NaN or infinity.");
  }

  const double clamped_input =
      std::clamp(static_cast<double>(value), -1.0, 1.0);
  // Clip the result to be safe. Although only values near
  // `std::numeric_limits<int32_t>::max() + 1` will be out of range.
  return ClipDoubleToInt32(clamped_input * kMaxInt32PlusOneAsDouble, result);
}

/*!\brief Gets the native byte order of the runtime system.
 *
 * \return `true` if the runtime system natively uses big endian, `false`
 *         otherwise.
 */
bool IsNativeBigEndian();

/*!\brief Casts and copies the input span to the output span.
 *
 * \param field_name Field name of the vector to insert into the error message.
 * \param vector_size Size of the vector.
 * \param reported_size Size reported by associated fields (e.g. "*_size" fields
 *        in the OBU).
 * \return `absl::OkStatus()` if the size arguments are equivalent.
 *         `absl::InvalidArgumentError()` otherwise.
 */
template <typename T, typename U>
absl::Status StaticCastSpanIfInRange(absl::string_view field_name,
                                     absl::Span<const T> input_data,
                                     absl::Span<U> output_data) {
  if (const auto status = ValidateContainerSizeEqual(field_name, input_data,
                                                     output_data.size());
      !status.ok()) [[unlikely]] {
    return status;
  }

  for (int i = 0; i < input_data.size(); ++i) {
    const auto status =
        StaticCastIfInRange(field_name, input_data[i], output_data[i]);
    if (!status.ok()) [[unlikely]] {
      return status;
    }
  }
  return absl::OkStatus();
}
}  // namespace iamf_tools

#endif  // COMMON_UTILS_NUMERIC_UTILS_H_
