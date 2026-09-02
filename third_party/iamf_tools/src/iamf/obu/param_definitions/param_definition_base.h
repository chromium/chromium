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
#ifndef OBU_PARAM_DEFINITIONS_PARAM_DEFINITION_BASE_H_
#define OBU_PARAM_DEFINITIONS_PARAM_DEFINITION_BASE_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/subblock_schedule.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Common part of the parameter definitions.
 *
 * Extended by `MixGainParamDefinition`, `DemixingParamDefinition`, and
 * `ReconGainParamDefinition`, and various position-based parameter
 * definitions.
 */
class ParamDefinition {
 public:
  /*!\brief Static limit on num_subblocks prevents OOMs from implausible values.
   *
   * The maximum sample rate is 192000 Hz and maximum duration is 1 second.
   * Therefore the theoretical maximum number of subblocks is 192000.
   */
  static constexpr DecodedUleb128 kMaxNumSubblocks = 192000;

  /*!\brief A `DecodedUleb128` enum for the type of parameter. */
  enum ParameterDefinitionType : DecodedUleb128 {
    kParameterDefinitionMixGain = 0,
    kParameterDefinitionDemixing = 1,
    kParameterDefinitionReconGain = 2,
    kParameterDefinitionPolar = 3,
    kParameterDefinitionCart8 = 4,
    kParameterDefinitionCart16 = 5,
    kParameterDefinitionDualPolar = 6,
    kParameterDefinitionDualCart8 = 7,
    kParameterDefinitionDualCart16 = 8,
    // Values in the range of [9, (1 << 32) - 1] are reserved.
    kParameterDefinitionReservedStart = 9,
    kParameterDefinitionReservedEnd = std::numeric_limits<DecodedUleb128>::max()
  };

  /*!\brief Descriptive names for `parameter_definition_mode_`.
   *
   * The spec just calls these "mode 0" and "mode 1", but this results in poor
   * readability, and it can be easy to confuse the two modes.
   */
  enum ParamDefinitionMode : uint8_t {
    kModeScheduleInParamDefinition = 0,
    kModeScheduleInParameterBlock = 1,
  };

  /*!\brief Arguments for the `ParamDefinitionBase` constructor. */
  struct BaseArgs {
    DecodedUleb128 parameter_id = 0;
    DecodedUleb128 parameter_rate = 0;
    uint8_t reserved = 0;
    std::optional<SubblockSchedule> schedule = std::nullopt;
  };

  /*!\brief Default destructor.
   */
  virtual ~ParamDefinition() = default;

  /*!\brief Gets the number of represented subblocks.
   *
   * When a schedule is absent, the number of subblocks is 0.
   *
   * When a schedule is present and the number of subblocks is explicitly
   * encoded, the returned value is the encoded value.
   *
   * When a schedule is present and the number of subblocks is not explicitly
   * encoded, the returned value is computed based on the implied number of
   * subblocks in the schedule.
   *
   * \return Number of subblocks represented by this parameter definition.
   */
  DecodedUleb128 GetNumSubblocks() const;

  /*!\brief Validates the parameter definition called by `ValidateAndWrite()`.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Validate() const;

  /*!\brief Validates and writes the parameter definition.
   *
   * This function defines the validating and writing of the common parts,
   * and the sub-classes's overridden ones shall define their specific parts.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  virtual absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Reads and validates the parameter definition.
   *
   * This function defines the validating and reading of the common parts,
   * and the sub-classes's overridden ones shall define their specific parts.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  virtual absl::Status ReadAndValidate(ReadBitBuffer& rb);

  /*!\brief Gets the `ParameterDefinitionType`.
   *
   * \return Type of this parameter definition.
   */
  ParameterDefinitionType GetType() const { return type_; }

  /*!\brief Gets the parameter ID.
   *
   * \return Parameter ID.
   */
  DecodedUleb128 GetParameterId() const;

  /*!\brief Gets the parameter rate.
   *
   * \return Parameter rate.
   */
  DecodedUleb128 GetParameterRate() const;

  /*!\brief Gets the parameter definition mode.
   *
   * \return Parameter definition mode.
   */
  ParamDefinitionMode GetParamDefinitionMode() const;

  /*!\brief Gets the reserved field.
   *
   * \return Reserved field.
   */
  uint8_t GetReserved() const;

  /*!\brief Gets the duration.
   *
   * \return Duration.
   */
  DecodedUleb128 GetDuration() const;

  /*!\brief Gets the constant subblock duration.
   *
   * \return Constant subblock duration.
   */
  DecodedUleb128 GetConstantSubblockDuration() const;

  /*!\brief Gets the schedule.
   *
   * \return Schedule.
   */
  const std::optional<SubblockSchedule>& GetSchedule() const;

  /*!\brief Creates parameter data from a buffer.
   *
   * The created instance will be one of the subclasses of `ParameterData`,
   * depending on the specific subclass implementing this function.
   *
   * \param rb Buffer to read from.
   * \return Unique pointer to created parameter data, or specific error
   *         on failure.
   */
  virtual absl::StatusOr<std::unique_ptr<ParameterData>>
  CreateParameterDataFromBuffer(ReadBitBuffer& rb) const = 0;

  /*!\brief Prints the parameter definition.
   */
  virtual void Print() const;

  friend bool operator==(const ParamDefinition& lhs,
                         const ParamDefinition& rhs) = default;

 protected:
  /*!\brief Constructor with a passed-in type used by sub-classes.
   *
   * \param type Type of the specific parameter definition.
   * \param base_args Arguments for `ParamDefinitionBase`.
   */
  ParamDefinition(ParameterDefinitionType type, const BaseArgs& base_args)
      : type_(type),
        parameter_id_(base_args.parameter_id),
        parameter_rate_(base_args.parameter_rate),
        reserved_(base_args.reserved),
        schedule_(base_args.schedule) {}

 private:
  // Type of this parameter definition.
  ParameterDefinitionType type_;

  DecodedUleb128 parameter_id_ = 0;
  DecodedUleb128 parameter_rate_ = 0;
  // `param_definition_mode_` is implied by the presence of `schedule_`.
  uint8_t reserved_ = 0;  // 7 bits.

  std::optional<SubblockSchedule> schedule_ = std::nullopt;
};

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_PARAM_DEFINITION_BASE_H_
