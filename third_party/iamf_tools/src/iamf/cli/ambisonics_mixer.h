/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_AMBISONICS_MIXER_H_
#define CLI_AMBISONICS_MIXER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Represents an Ambisonics mixer. */
class AmbisonicsMixer : public SampleProcessorBase {
 public:
  /*!\brief Preset configuration for Ambisonics.
   *
   * The "best practice" series of preset configurations are intended to provide
   * reasonable default settings for a given use case. The best practices may
   * evolve over time, and the final settings may vary on other IA Sequence
   * settings (e.g. codec).
   */
  enum class Preset {
    kBestPracticeForOrder0 = 0,
    kBestPracticeForOrder1 = 1,
    kBestPracticeForOrder2 = 2,
    kBestPracticeForOrder3 = 3,
  };

  /*!\brief Makes an Ambisonics mixer from a preset.
   *
   * \param codec_id Codec ID.
   * \param preset Preset configuration.
   * \param max_input_samples_per_frame Maximum number of samples per frame.
   * \return AmbisonicsMixer.
   */
  static AmbisonicsMixer MakeFromPreset(CodecConfig::CodecId codec_id,
                                        Preset preset,
                                        size_t max_input_samples_per_frame);

  /*!\brief Makes an Ambisonics mixer from an existing Ambisonics configuration.
   *
   * \param config Ambisonics configuration.
   * \param max_input_samples_per_frame Maximum number of samples per frame.
   * \return AmbisonicsMixer.
   */
  static AmbisonicsMixer MakeFromAmbisonicsConfig(
      const AmbisonicsConfig& config, size_t max_input_samples_per_frame);

  /*!\brief Returns the Ambisonics configuration.
   *
   * \return Ambisonics configuration.
   */
  AmbisonicsConfig GetAmbisonicsConfig() const;

  /*!\brief Returns the expected input channel labels in order.
   *
   * \return Vector of channel labels.
   */
  std::vector<ChannelLabel::Label> GetInputLabels() const;

 private:
  /*!\brief Private constructor.
   *
   * \param config Ambisonics configuration.
   * \param mixing_matrix Optional mixing matrix to apply.
   * \param max_input_samples_per_frame Maximum number of samples per frame.
   */
  AmbisonicsMixer(AmbisonicsConfig config,
                  std::optional<absl::Span<const int16_t>> mixing_matrix,
                  size_t max_input_samples_per_frame);

  /*!\brief Pushes a frame of samples to the mixer.
   *
   * When no mixer is available, it transparently copies the input to the
   * output.
   *
   * \param channel_time_samples Samples to push arranged in (channel, time).
   * \return `absl::OkStatus()` on success.
   */
  absl::Status PushFrameDerived(
      absl::Span<const absl::Span<const InternalSampleType>>
          channel_time_samples) override;

  /*!\brief Signals to close the mixer and flush any remaining samples.
   *
   * \return `absl::OkStatus()` on success.
   */
  absl::Status FlushDerived() override { return absl::OkStatus(); }

  AmbisonicsConfig ambisonics_config_;
  std::vector<double> mixing_matrix_;  // Empty when no mixing is needed.
};

}  // namespace iamf_tools

#endif  // CLI_AMBISONICS_MIXER_H_
