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
#ifndef OBU_AMBISONICS_CONFIG_H_
#define OBU_AMBISONICS_CONFIG_H_

#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Configuration for mono-coded Ambisonics.
 *
 * Invariant: this class and its utility functions always are consistent with
 * the constraints in the IAMF spec.
 */
class AmbisonicsMonoConfig {
 public:
  // RFC 8486 reserves 255 to signal an inactive ACN (ambisonics channel
  // number).
  static constexpr uint8_t kInactiveAmbisonicsChannelNumber = 255;

  friend bool operator==(const AmbisonicsMonoConfig& lhs,
                         const AmbisonicsMonoConfig& rhs) = default;

  /*!\brief Default constructor for help when held in an `std::variant`.
   *
   * Initializes in a valid state for zeroth order Ambisonics.
   */
  [[deprecated("Use Create() instead.")]]
  AmbisonicsMonoConfig()
      : AmbisonicsMonoConfig(1, {0}) {}

  /*!\brief Creates an `AmbisonicsMonoConfig` and validates its settings.
   *
   * \param substream_count `substream_count` (N).
   * \param channel_mapping `channel_mapping`. Size implies
   *        `output_channel_count` (C).
   * \return Config or a specific status on failure.
   */
  static absl::StatusOr<AmbisonicsMonoConfig> Create(
      uint8_t substream_count, absl::Span<const uint8_t> channel_mapping);

  /*!\brief Creates an `AmbisonicsMonoConfig` from a buffer and validates its
   * settings.
   *
   * \param rb Buffer from which to read variables.
   * \return Config or a specific status on failure.
   */
  static absl::StatusOr<AmbisonicsMonoConfig> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Gets the number of output channels in the configuration.
   *
   * \return Number of output channels.
   */
  uint8_t GetOutputChannelCount() const {
    return static_cast<uint8_t>(channel_mapping_.size());
  }

  /*!\brief Gets the number of substreams in the configuration.
   *
   * \return Number of substreams.
   */
  uint8_t GetSubstreamCount() const { return substream_count_; }

  /*!\brief Gets a read-only view of the channel mapping.
   *
   * The index in the returned span is the Ambisonics Channel Number (ACN). The
   * value represents the substream indices. A value of
   * `kInactiveAmbisonicsChannelNumber` implies that the ACN is dropped.
   *
   * \return Span of channel mapping indices.
   */
  absl::Span<const uint8_t> GetChannelMappingView() const {
    return absl::MakeConstSpan(channel_mapping_);
  }

 private:
  /*!\brief Constructor for use by `Create()`.
   *
   * \param substream_count `substream_count` (N).
   * \param channel_mapping `channel_mapping`. Size implies
   *        `output_channel_count` (C).
   */
  AmbisonicsMonoConfig(uint8_t substream_count,
                       absl::Span<const uint8_t> channel_mapping);

  // `output_channel_count` is inferred from the size of the `channel_mapping`.
  uint8_t substream_count_;  // (N).

  // Vector of length (C).
  std::vector<uint8_t> channel_mapping_;
};

/*!\brief Configuration for projection-coded Ambisonics.
 *
 * Invariant: this class and its utility functions always are consistent with
 * the constraints in the IAMF spec.
 *
 */
class AmbisonicsProjectionConfig {
 public:
  friend bool operator==(const AmbisonicsProjectionConfig& lhs,
                         const AmbisonicsProjectionConfig& rhs) = default;

  /*!\brief Creates an `AmbisonicsProjectionConfig` and validates its settings.
   *
   * \param output_channel_count `output_channel_count` (C).
   * \param substream_count `substream_count` (N).
   * \param coupled_substream_count `coupled_substream_count` (M).
   * \param demixing_matrix Demixing matrix of size (N + M) * C.
   * \return `AmbisonicsProjectionConfig` config or a specific status on
   *         failure.
   */
  static absl::StatusOr<AmbisonicsProjectionConfig> Create(
      uint8_t output_channel_count, uint8_t substream_count,
      uint8_t coupled_substream_count,
      absl::Span<const int16_t> demixing_matrix);

  /*!\brief Creates an `AmbisonicsProjectionConfig` from a buffer and validates
   * its settings.
   *
   * \param rb Buffer from which to read variables.
   * \return `AmbisonicsProjectionConfig` config or a specific status on
   *         failure.
   */
  static absl::StatusOr<AmbisonicsProjectionConfig> CreateFromBuffer(
      ReadBitBuffer& rb);

  uint8_t GetOutputChannelCount() const { return output_channel_count_; }
  uint8_t GetSubstreamCount() const { return substream_count_; }
  uint8_t GetCoupledSubstreamCount() const { return coupled_substream_count_; }

  /*!\brief Gets a view of the demixing matrix.
   *
   * \return Span of demixing matrix elements.
   */
  absl::Span<const int16_t> GetDemixingMatrixView() const {
    return absl::MakeConstSpan(demixing_matrix_);
  }

 private:
  /*!\brief Constructor for use by `Create()`.
   *
   * \param output_channel_count `output_channel_count` (C).
   * \param substream_count `substream_count` (N).
   * \param coupled_substream_count `coupled_substream_count` (M).
   * \param demixing_matrix Demixing matrix of size (N + M) * C.
   */
  AmbisonicsProjectionConfig(uint8_t output_channel_count,
                             uint8_t substream_count,
                             uint8_t coupled_substream_count,
                             absl::Span<const int16_t> demixing_matrix);

  uint8_t output_channel_count_;     // (C).
  uint8_t substream_count_;          // (N).
  uint8_t coupled_substream_count_;  // (M).

  std::vector<int16_t> demixing_matrix_;
};

/*!\brief Config to reconstruct an Audio Element OBU using Ambisonics layout.
 *
 * The metadata required for combining the substreams identified here in order
 * to reconstruct an Ambisonics layout.
 */
struct AmbisonicsConfig {
  /*!\brief A `DecodedUleb128` enum for the method of coding Ambisonics. */
  enum AmbisonicsMode : DecodedUleb128 {
    kAmbisonicsModeMono = 0,
    kAmbisonicsModeProjection = 1,
    kAmbisonicsModeReservedStart = 2,
    kAmbisonicsModeReservedEnd = std::numeric_limits<DecodedUleb128>::max(),
  };
  friend bool operator==(const AmbisonicsConfig& lhs,
                         const AmbisonicsConfig& rhs) = default;

  /*!\brief Gets the next valid number of output channels.
   *
   * \param requested_output_channel_count Requested number of channels.
   * \param next_valid_output_channel_count Minimum valid `output_channel_count`
   *        that has at least the required number of channels.
   * \return `absl::OkStatus()` if successful. `kIamfInvalid` argument if
   *         the input is too large.
   */
  static absl::Status GetNextValidOutputChannelCount(
      uint8_t requested_output_channel_count,
      uint8_t& next_valid_output_channel_count);

  /*!\brief Validates and writes the configuration.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Reads and validates the configuration from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb);

  /*!\brief Prints logging information about the configuration. */
  void Print() const;

  /*!\brief Gets the number of output channels in the configuration.
   *
   * \return Number of output channels.
   */
  uint8_t GetOutputChannelCount() const;

  /*!\brief Gets the number of substreams in the configuration.
   *
   * \return Number of substreams.
   */
  uint8_t GetNumSubstreams() const;

  AmbisonicsMode GetAmbisonicsMode() const;

  /*!\brief Gets a view of the demixing matrix.
   *
   * \return Span of demixing matrix elements stored in a 1D array in
   *         column-major order for ambisonics projection mode. `std::nullopt`
   *         for ambisonics mono mode.
   */
  std::optional<absl::Span<const int16_t>> GetDemixingMatrix() const;

  // `ambisonics_mode` is inferred from the contents of the `ambisonics_config`
  // variant.
  std::variant<AmbisonicsMonoConfig, AmbisonicsProjectionConfig>
      ambisonics_config;
};

}  // namespace iamf_tools

#endif  // OBU_AMBISONICS_CONFIG_H_
