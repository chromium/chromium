/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_WAV_READER_H_
#define CLI_WAV_READER_H_

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "src/dsp/read_wav_info.h"

namespace iamf_tools {

class WavReader {
 public:
  /*!\brief Factory function.
   *
   * \param wav_filename Filename of file to read.
   * \param num_samples_per_frame Maximum number of samples per frame to read.
   * \param `WavReader` on success. A specific error code if the file could not
   *        be opened or was not detected to be a valid WAV file.
   */
  static absl::StatusOr<WavReader> CreateFromFile(
      const std::string& wav_filename, size_t num_samples_per_frame);

  /*!\brief Moves the `WavReader` without closing the underlying file.*/
  WavReader(WavReader&& original);

  /*!\brief Destructor. */
  ~WavReader();

  /*!\brief Gets the number of channels of the reader.
   *
   * \return Number of channels.
   */
  int num_channels() const { return info_.num_channels; }

  /*!\brief Gets the sample rate of the reader.
   *
   * \return Sample rate.
   */
  int sample_rate_hz() const { return info_.sample_rate_hz; }

  /*!\brief Gets the bit-depth of the reader.
   *
   * Reports the bit-depth of the source samples in the file. For IEEE float
   * sources this is the container width (32 or 64), not the justification of
   * the decoded samples in `buffers_`.
   *
   * \return Bit-depth.
   */
  int bit_depth() const { return info_.bit_depth; }

  /*!\brief Gets the number of remaining samples in the file.
   *
   * \return Number of samples remaining to be read.
   */
  int remaining_samples() const { return info_.remaining_samples; }

  /*!\brief Read up to one frame worth of samples.
   *
   * Typically this function reads up to `(num_channels() *
   * num_samples_per_frame_)` samples. It may read fewer samples when the end
   * of the wav file is reached.
   *
   * \return Number of samples read.
   */
  size_t ReadFrame();

  /*!\brief Buffers storing samples in (channel, time) axes.
   *
   * For integer sources the samples are left-justified; the upper
   * `bit_depth()` bits represent the sample, with the remaining lower bits set
   * to 0. For IEEE float sources the normalized samples are scaled to the full
   * `int32_t` range regardless of `bit_depth()`.
   */
  std::vector<std::vector<int32_t>> buffers_;

  const size_t num_samples_per_frame_;

 private:
  /*!\brief Private constructor.
   *
   * Used by factory function.
   */
  WavReader(size_t num_samples_per_frame, FILE* file, const ReadWavInfo& info);

  FILE* file_;
  ReadWavInfo info_;
};
}  // namespace iamf_tools

#endif  // CLI_WAV_READER_H_
