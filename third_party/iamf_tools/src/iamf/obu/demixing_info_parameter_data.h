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
#ifndef OBU_DEMIXING_INFO_PARAMETER_DATA_H_
#define OBU_DEMIXING_INFO_PARAMETER_DATA_H_

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct DownMixingParams {
  friend bool operator==(const DownMixingParams& lhs,
                         const DownMixingParams& rhs) = default;

  double alpha;
  double beta;
  double gamma;
  double delta;
  int w_idx_offset;
  int w_idx_used;
  double w;
  bool in_bitstream;
};

/*!\brief A 3-bit enum for the demixing info parameter. */
struct DemixingInfoParameterData : public ParameterData {
  enum DMixPMode : uint8_t {
    kDMixPMode1 = 0,
    kDMixPMode2 = 1,
    kDMixPMode3 = 2,
    kDMixPModeReserved1 = 3,
    kDMixPMode1_n = 4,
    kDMixPMode2_n = 5,
    kDMixPMode3_n = 6,
    kDMixPModeReserved2 = 7,
  };

  enum WIdxUpdateRule {
    kNormal = 0,
    kFirstFrame = 1,
    kDefault = 2,
  };

  /*!\brief Constructor.
   *
   * \param input_dmixp_mode Input demixing mode.
   * \param input_reserved Input reserved 5 bits packed in a byte.
   */
  DemixingInfoParameterData(DMixPMode input_dmixp_mode, uint8_t input_reserved)
      : ParameterData(),
        dmixp_mode(input_dmixp_mode),
        reserved(input_reserved) {}
  DemixingInfoParameterData() = default;

  /*!\brief Overridden destructor.
   */
  ~DemixingInfoParameterData() override = default;

  /*!\brief Fill in the input `DownMixingParams` based on the `DMixPMode`.
   *
   * \param dmixp_mode Input demixing mode.
   * \param previous_w_idx Used to determine the value of `w`. Must be in the
   *        range [0, 10]. Pass in `default_w` when `w_idx_update_rule ==
   *        kDefault`.
   * \param w_idx_update_rule Rule to update `w_idx`. According to the Spec,
   *        there are two special rules: when the frame index == 0 and when
   *        the `default_w` should be used.
   * \param down_mixing_params Output demixing parameters.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
   *         the `dmixp_mode` is unknown or `w_idx` is out of range.
   */
  static absl::Status DMixPModeToDownMixingParams(
      DMixPMode dmixp_mode, int previous_w_idx,
      WIdxUpdateRule w_idx_update_rule, DownMixingParams& down_mixing_params);

  bool friend operator==(const DemixingInfoParameterData& lhs,
                         const DemixingInfoParameterData& rhs) = default;

  /*!\brief Creates a `DemixingInfoParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return Deserialized `DemixingInfoParameterData` or error.
   */
  static absl::StatusOr<std::unique_ptr<DemixingInfoParameterData>>
  CreateFromBuffer(ReadBitBuffer& rb);

  /*!\brief Creates a `DemixingInfoParameterData`.
   *
   * \param input_dmixp_mode Demixing mode.
   * \param input_reserved Reserved 5 bits.
   * \return Validated `DemixingInfoParameterData` object or error.
   */
  static absl::StatusOr<std::unique_ptr<DemixingInfoParameterData>> Create(
      DMixPMode input_dmixp_mode, uint8_t input_reserved);

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the demixing info parameter data.
   */
  void Print() const override;

  DMixPMode dmixp_mode = kDMixPMode1;  // 3 bits
  uint8_t reserved = 0;                // 5 bits
};

struct DefaultDemixingInfoParameterData : public DemixingInfoParameterData {
  /*!\brief Constructor.
   *
   * \param input_dmixp_mode Input demixing mode.
   * \param input_reserved Input reserved 5 bits packed in a byte.
   * \param input_default_w Input default weight value.
   * \param input_reserved_for_future_use Input reserved bits for future
   *        use (4 bits packed in a byte).
   */
  DefaultDemixingInfoParameterData(DMixPMode input_dmixp_mode,
                                   uint8_t input_reserved,
                                   uint8_t input_default_w,
                                   uint8_t input_reserved_for_future_use)
      : DemixingInfoParameterData(input_dmixp_mode, input_reserved),
        default_w(input_default_w),
        reserved_for_future_use(input_reserved_for_future_use) {}
  DefaultDemixingInfoParameterData() = default;

  /*!\brief Overridden destructor.*/
  ~DefaultDemixingInfoParameterData() override = default;

  /*!\brief Creates a `DefaultDemixingInfoParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return Deserialized `DefaultDemixingInfoParameterData` or error.
   */
  static absl::StatusOr<std::unique_ptr<DefaultDemixingInfoParameterData>>
  CreateFromBuffer(ReadBitBuffer& rb);

  /*!\brief Creates a `DefaultDemixingInfoParameterData` with validation.
   *
   * \param input_dmixp_mode Demixing mode.
   * \param input_reserved Reserved 5 bits.
   * \param input_default_w Default weight.
   * \param input_reserved_for_future_use Reserved 4 bits.
   * \return Validated `DefaultDemixingInfoParameterData` object or error.
   */
  static absl::StatusOr<std::unique_ptr<DefaultDemixingInfoParameterData>>
  Create(DMixPMode input_dmixp_mode, uint8_t input_reserved,
         uint8_t input_default_w, uint8_t input_reserved_for_future_use);

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the default demixing info parameter data.
   */
  void Print() const override;

  bool friend operator==(const DefaultDemixingInfoParameterData& lhs,
                         const DefaultDemixingInfoParameterData& rhs) = default;

  uint8_t default_w = 0;                // 4 bits.
  uint8_t reserved_for_future_use = 0;  // 4 bits.
};

}  // namespace iamf_tools

#endif  // OBU_DEMIXING_INFO_PARAMETER_DATA_H_
