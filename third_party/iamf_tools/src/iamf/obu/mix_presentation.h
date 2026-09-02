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
#ifndef OBU_MIX_PRESENTATION_H_
#define OBU_MIX_PRESENTATION_H_

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief One of the audio elements within a sub-mix. */
struct SubMixAudioElement {
  friend bool operator==(const SubMixAudioElement& lhs,
                         const SubMixAudioElement& rhs) = default;

  /*!\brief Reads and validates the SubMixAudioElement from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the layout is valid. A specific status if the
   *         write fails.
   */
  absl::Status ReadAndValidate(const int32_t count_label, ReadBitBuffer& rb);

  // The ID of the associated Audio Element OBU.
  DecodedUleb128 audio_element_id;
  // Length `count_labels`.
  std::vector<std::string> localized_element_annotations;
  RenderingConfig rendering_config;
  // The gain value to be applied to the rendered audio element signal.
  MixGainParamDefinition element_mix_gain =
      MixGainParamDefinition(ParamDefinition::BaseArgs{});
};

struct AnchoredLoudnessElement {
  /*!\brief A 8-bit enum for the associated loudness measurement.
   *
   * As defined in ISO-CICP.
   */
  enum AnchorElement : uint8_t {
    kAnchorElementUnknown = 0,
    kAnchorElementDialogue = 1,
    kAnchorElementAlbum = 2,
  };

  friend bool operator==(const AnchoredLoudnessElement& lhs,
                         const AnchoredLoudnessElement& rhs) = default;

  AnchorElement anchor_element;  // 8 bits.
  int16_t anchored_loudness;     // Q7.8 format.
};

struct AnchoredLoudness {
  friend bool operator==(const AnchoredLoudness& lhs,
                         const AnchoredLoudness& rhs) = default;

  // `num_anchored_loudness` is implicit based on the size of
  // `anchor_elements`.
  std::vector<AnchoredLoudnessElement> anchor_elements = {};
};

struct LayoutExtension {
  friend bool operator==(const LayoutExtension& lhs,
                         const LayoutExtension& rhs) = default;

  // `info_type_size` is implicit based on the size of `info_type_bytes`
  std::vector<uint8_t> info_type_bytes;
};

/*!\brief The loudness information for a given audio signal. */
struct LoudnessInfo {
  /*!\brief A 8-bit bitmask to determine the included optional loudness types.
   */
  enum InfoTypeBitmask : uint8_t {
    kTruePeak = 0x01,
    kAnchoredLoudness = 0x02,
    kLive = 0x04,
    kInfoTypeBitMask8 = 0x08,
    kInfoTypeBitMask16 = 0x10,
    kInfoTypeBitMask32 = 0x20,
    kInfoTypeBitMask64 = 0x40,
    kInfoTypeBitMask128 = 0x80,
    // For backwards compatibility several info types signal the need
    // for a `layout_extension`.
    kAnyLayoutExtension = 0xfc,
  };

  friend bool operator==(const LoudnessInfo& lhs,
                         const LoudnessInfo& rhs) = default;

  uint8_t info_type;  // Apply `LoudnessInfoTypeBitmask` to identify what types
                      // of loudness information are included.
  int16_t integrated_loudness = 0;  // Q7.8 format.
  int16_t digital_peak = 0;         // Q7.8 format.

  // Present if `(info_type & kTruePeak) != 0`.
  int16_t true_peak = 0;  // Q7.8 format.

  // Present if `(info_type & kAnchoredLoudness) != 0`.
  AnchoredLoudness anchored_loudness;

  // Present if `(info_type & kAnyLayoutExtension) != 0`.
  LayoutExtension layout_extension;
};

/*!\brief Layout is defined using the sound system convention of ITU2051-3.
 *
 * Implements syntax and utility functions when the `Layout` defined in
 * https://aomediacodec.github.io/iamf/v1.1.0.html#syntax-layout is
 * `LOUDSPEAKERS_SS_CONVENTION`.
 */
struct LoudspeakersSsConventionLayout {
  /*!\brief A 4-bit enum for loudspeaker layout.
   *
   * Sound systems A through J refer to ITU2051-3.
   *
   * Typically named in "A+B+C" convention, where A is the number of height
   * speakers, B is the number of surround speakers, and C is the number of
   * bottom speakers.
   */
  enum SoundSystem : uint8_t {
    kSoundSystemA_0_2_0 = 0,
    kSoundSystemB_0_5_0 = 1,
    kSoundSystemC_2_5_0 = 2,
    kSoundSystemD_4_5_0 = 3,
    kSoundSystemE_4_5_1 = 4,
    kSoundSystemF_3_7_0 = 5,
    kSoundSystemG_4_9_0 = 6,
    kSoundSystemH_9_10_3 = 7,
    kSoundSystemI_0_7_0 = 8,
    kSoundSystemJ_4_7_0 = 9,
    kSoundSystem10_2_7_0 = 10,  // IAMF's 7.1.2.
    kSoundSystem11_2_3_0 = 11,  // IAMF's 3.1.2.
    kSoundSystem12_0_1_0 = 12,  // IAMF's Mono.
    kSoundSystem13_6_9_0 = 13,  // IAMF's 9.1.6.
    kSoundSystem14_5_7_4 = 14,  // IAMF's 7.1.5.4.
    kSoundSystemEndReserved = 15,
  };

  friend bool operator==(const LoudspeakersSsConventionLayout& lhs,
                         const LoudspeakersSsConventionLayout& rhs) = default;

  /*!\brief Writes the layout to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the layout is valid. A specific status if the
   *         write fails.
   */
  absl::Status Write(WriteBitBuffer& wb) const;

  /*!\brief Reads the layout from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the layout is valid. A specific status if the
   *         read fails.
   */
  absl::Status Read(ReadBitBuffer& rb);

  /*!\brief Prints logging information about the layout. */
  void Print() const;

  SoundSystem sound_system;
  uint8_t reserved;  // 2 bits.
};

/*!\brief Layout is binaural or reserved.
 *
 * Implements syntax and utility functions when the `Layout` defined in
 * https://aomediacodec.github.io/iamf/v1.1.0.html#syntax-layout is
 * `BINAURAL` or `RESERVED`.
 */
struct LoudspeakersReservedOrBinauralLayout {
  friend bool operator==(const LoudspeakersReservedOrBinauralLayout& lhs,
                         const LoudspeakersReservedOrBinauralLayout& rhs) =
      default;

  /*!\brief Writes the layout to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the layout is valid. A specific status if the
   *         write fails.
   */
  absl::Status Write(WriteBitBuffer& wb) const;

  /*!\brief Reads the layout from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the layout is valid. A specific status if the
   *         read fails.
   */
  absl::Status Read(ReadBitBuffer& rb);

  /*!\brief Prints logging information about the layout. */
  void Print() const;

  uint8_t reserved;  // 6 bits.
};

/*!\brief Specifies either a binaural system or physical loudspeaker positions.
 *
 * Implements syntax and utility functions related to the `Layout` defined in
 * https://aomediacodec.github.io/iamf/v1.1.0.html#syntax-layout.
 */
struct Layout {
  /*!\brief A 2-bit enum for the type of layout. */
  enum LayoutType : uint8_t {
    kLayoutTypeReserved0 = 0,
    kLayoutTypeReserved1 = 1,
    kLayoutTypeLoudspeakersSsConvention = 2,  // Using convention of ITU2051-3.
    kLayoutTypeBinaural = 3,                  // Layout is binaural.
  };

  friend bool operator==(const Layout& lhs, const Layout& rhs) = default;

  /*!\brief Returns a stereo layout. */
  static Layout MakeStereo();

  /*!\brief Reads and validates the Layout from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the layout is valid. A specific status if the
   *         read fails.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb);

  LayoutType layout_type;  // 2 bits.

  // The active field depends on `layout_type`.
  std::variant<LoudspeakersSsConventionLayout,
               LoudspeakersReservedOrBinauralLayout>
      specific_layout;
};

/*!\brief Identifies measured loudness information according to layout. */
struct MixPresentationLayout {
  friend bool operator==(const MixPresentationLayout& lhs,
                         const MixPresentationLayout& rhs) = default;

  /*!\brief Reads and validates the MixPresentationLayout from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the layout is valid. A specific status if the
   *         read fails.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb);

  Layout loudness_layout;
  LoudnessInfo loudness;
};

/*!\brief One of the sub-mixes within a Mix Presentation Obu. */
struct MixPresentationSubMix {
  friend bool operator==(const MixPresentationSubMix& lhs,
                         const MixPresentationSubMix& rhs) = default;

  /*!\brief Reads and validates the MixPresentationSubMix from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the sub-mix is valid. A specific status if
   *         the read fails.
   */
  absl::Status ReadAndValidate(const int32_t count_label, ReadBitBuffer& rb);

  // `num_audio_elements` is implicit based on the size of `audio_elements`.
  std::vector<SubMixAudioElement> audio_elements;

  // The gain value to be applied in post-processing the mixed audio signal to
  // generate the audio signal for playback.
  MixGainParamDefinition output_mix_gain =
      MixGainParamDefinition(ParamDefinition::BaseArgs{});

  // `num_layouts` is implicit based on the size of `layouts`.
  std::vector<MixPresentationLayout> layouts;
};

struct MixPresentationTags {
  struct Tag {
    friend bool operator==(const Tag& lhs, const Tag& rhs) = default;

    std::string tag_name;
    std::string tag_value;
  };

  /*!\brief Creates `MixPresentationTags` from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the `MixPresentationTags` is valid. A
   *         specific status if the read fails.
   */
  static absl::StatusOr<MixPresentationTags> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Writes the MixPresentationTags to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the MixPresentationTags is valid. A specific
   *         status if the write fails.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  friend bool operator==(const MixPresentationTags& lhs,
                         const MixPresentationTags& rhs) = default;

  // `num_tags` is implicit based on the size of `tags`.
  std::vector<Tag> tags;
};

/*!\brief Enum for preferred loudspeaker renderer. */
enum class PreferredLoudspeakerRenderer : uint8_t {
  kNone = 0,

  // Values in the range of [1, (1 << 8) - 1] are reserved.
  kReservedStart = 1,
  kReservedEnd = std::numeric_limits<uint8_t>::max(),
};

/*!\brief Enum for preferred binaural renderer. */
enum class PreferredBinauralRenderer : uint8_t {
  kNone = 0,

  // Values in the range of [1, (1 << 8) - 1] are reserved.
  kReservedStart = 1,
  kReservedEnd = std::numeric_limits<uint8_t>::max(),
};

/*!\brief Optional fields in a Mix Presentation OBU. */
struct MixPresentationOptionalFields {
  /*!\brief Creates `MixPresentationOptionalFields` from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the `MixPresentationOptionalFields` is valid.
   *         A specific status if the read fails.
   */
  static absl::StatusOr<MixPresentationOptionalFields> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Writes the `MixPresentationOptionalFields` to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the `MixPresentationOptionalFields` is valid.
   *         A specific status if the write fails.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  friend bool operator==(const MixPresentationOptionalFields& lhs,
                         const MixPresentationOptionalFields& rhs) = default;

  DecodedUleb128 optional_fields_size;
  PreferredLoudspeakerRenderer preferred_loudspeaker_renderer;
  PreferredBinauralRenderer preferred_binaural_renderer;
  std::vector<uint8_t> optional_fields_remaining_bytes;
};

/*!\brief Metadata required for post-processing the mixed audio signal.
 *
 * The metadata specifies how to render, process and mix one or more audio
 * elements.
 *
 * A Mix Presentation MAY contain one or more sub-mixes. Common use cases MAY
 * specify only one sub-mix, which includes all rendered and processed Audio
 * Elements used in the Mix Presentation. The use-case for specifying more than
 * one sub-mix arises if an IA multiplexer is merging two or more IA Sequences.
 * In this case, it MAY choose to capture the loudness information from the
 * original IA Sequences in multiple sub-mixes, instead of recomputing the
 * loudness information for the final mix.
 *
 * This class has looser limits than the specification:
 *   - When reading sub-mixes withoput a stereo layout, one will be
 *     automatically added.
 */
class MixPresentationObu : public ObuBase {
 public:
  /*!\brief Writes the number of channels for a `Layout` to the output argument.
   *
   * \param loudness_layout `Layout` to process.
   * \param num_channels Number of channels for this layout if successful.
   * \return `absl::OkStatus()` if successful.  `absl::InvalidArgumentError()`
   *         if the `layout_type` enum is a reserved or unknown value.
   */
  static absl::Status GetNumChannelsFromLayout(const Layout& loudness_layout,
                                               int32_t& num_channels);

  /*!\brief Constructor.
   *
   * This class takes ownership of any allocated memory nested within
   * `MixGainParamDefinition`s.
   *
   * \param header `ObuHeader` of the OBU.
   * \param mix_presentation_id `mix_presentation_id` in the OBU.
   * \param count_label `count_label` in the OBU.
   * \param annotations_language Vector representing all of the
   *        `annotations_language`s in the OBU.
   * \param localized_presentation_annotations Vector representing all of the
   *        `localized_presentation_annotations`s in the OBU.
   * \param sub_mixes Vector representing all of the sub-mixes in the OBU.
   */
  MixPresentationObu(
      const ObuHeader& header, DecodedUleb128 mix_presentation_id,
      DecodedUleb128 count_label,
      const std::vector<std::string>& annotations_language,
      const std::vector<std::string>& localized_presentation_annotations,
      std::vector<MixPresentationSubMix>& sub_mixes)
      : ObuBase(header, kObuIaMixPresentation),
        sub_mixes_(std::move(sub_mixes)),
        mix_presentation_id_(mix_presentation_id),
        count_label_(count_label),
        annotations_language_(annotations_language),
        localized_presentation_annotations_(
            localized_presentation_annotations) {}

  /*!\brief Creates a `MixPresentationObu` from a `ReadBitBuffer`.
   *
   * This function is designed to be used from the perspective of the decoder.
   * It will call `ReadAndValidatePayload` in order to read from the buffer;
   * therefore it can fail.
   *
   * \param header `ObuHeader` of the OBU.
   * \param payload_size Size of the obu payload in bytes.
   * \param rb `ReadBitBuffer` where the `MixPresentationObu` data is stored.
   *        Data read from the buffer is consumed.
   * \return A `MixPresentationObu` on success. A specific status on failure.
   */
  static absl::StatusOr<MixPresentationObu> CreateFromBuffer(
      const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb);

  /*!\brief Destructor. */
  ~MixPresentationObu() override = default;

  friend bool operator==(const MixPresentationObu& lhs,
                         const MixPresentationObu& rhs) = default;

  /*!\brief Prints logging information about the OBU. */
  void PrintObu() const override;

  DecodedUleb128 GetMixPresentationId() const { return mix_presentation_id_; }

  /*!\brief Gets a copy of the `annotations_language`.
   *
   * \return A copy of the `annotations_language` member variable.
   */
  std::vector<std::string> GetAnnotationsLanguage() const {
    return annotations_language_;
  }

  /*!\brief Gets a copy of the `localized_presentation_annotations`.
   *
   * \return A copy of the `localized_presentation_annotations` member variable.
   */
  std::vector<std::string> GetLocalizedPresentationAnnotations() const {
    return localized_presentation_annotations_;
  }

  DecodedUleb128 GetNumSubMixes() const { return sub_mixes_.size(); }

  std::vector<MixPresentationSubMix> sub_mixes_;

  // Implicitly included based on `obu_size` after writing the IAMF v1.1.0
  // payload. Must be present if `optional_fields_flag` in the OBU header
  // is true.
  std::optional<MixPresentationTags> mix_presentation_tags_;

  // Included if `optional_fields_flag` is true.
  std::optional<MixPresentationOptionalFields> optional_fields_;

 private:
  // Used only by the factory create function.
  explicit MixPresentationObu(const ObuHeader& header)
      : ObuBase(header, kObuIaMixPresentation),
        sub_mixes_({}),
        mix_presentation_id_(0),
        count_label_(0),
        annotations_language_({}),
        localized_presentation_annotations_({}) {}
  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if OBU is valid. A specific status on failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*!\brief Reads the OBU payload from the buffer.
   *
   * \param payload_size Size of the obu payload in bytes.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status ReadAndValidatePayloadDerived(int64_t payload_size,
                                             ReadBitBuffer& rb) override;

  DecodedUleb128 mix_presentation_id_;
  DecodedUleb128 count_label_;
  // Length `count_label`.
  std::vector<std::string> annotations_language_;
  // Length `count_label`.
  std::vector<std::string> localized_presentation_annotations_;

  // `num_sub_mixes_` is implicit based on the size of `sub_mixes_`.
};

}  // namespace iamf_tools

#endif  // OBU_MIX_PRESENTATION_H_
