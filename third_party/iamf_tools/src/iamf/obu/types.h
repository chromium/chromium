/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_TYPES_H_
#define OBU_TYPES_H_

#include <cstdint>

namespace iamf_tools {

/*!\brief IAMF spec requires a ULEB128 or a SLEB128 be encoded in <= 8 bytes.
 */
inline constexpr int kMaxLeb128Size = 8;

/*!\brief The maximum length of an IAMF string in bytes.
 *
 * The spec limits the length of a string to 128 bytes including the
 * null terminator ('\0').
 */
inline constexpr int kIamfMaxStringSize = 128;

/*!\brief IAMF spec requires an entire OBU to be <= 2 MB.
 */
constexpr uint32_t kEntireObuSizeMaxTwoMegabytes = (1 << 21);

/*!\brief Decoded `leb128` in IAMF. */
typedef uint32_t DecodedUleb128;

/*!\brief Decoded `sleb128` in IAMF. */
typedef int32_t DecodedSleb128;

/*!\brief Type of audio samples for internal computation.
 *
 * Typically this should be used as a value in the range of [-1.0, 1.0].
 */
typedef double InternalSampleType;

/*!\brief Timestamp for use in internal computations.
 *
 * Typically this represents a duration of ticks, based on the sample rate used
 * for timing purposes in an IA Sequence. I.e. if the sample rate is 48 kHz,
 * then a timestamp of 1000 represents `1000/48000Hz ~= .02083s`.
 */
typedef int64_t InternalTimestamp;

/*!\brief Internal trimming configuration for the decoder. */
struct TrimmingSettings {
  // If true, the decoder trims start samples as specified in the frames.
  bool trim_beginning = true;
  // If true, the decoder trims trailing samples as specified in the frames.
  bool trim_end = true;
};

}  // namespace iamf_tools

#endif  // OBU_TYPES_H_
