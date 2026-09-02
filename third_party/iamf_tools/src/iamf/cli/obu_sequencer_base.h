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
#ifndef CLI_OBU_SEQUENCER_BASE_H_
#define CLI_OBU_SEQUENCER_BASE_H_

#include <cstdint>
#include <list>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/metadata_obu.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Abstract base class for serializing and writing out OBUs.
 *
 * This class contains functions to serialize and write an IA Sequence. The
 * concrete classes are responsible for packing and writing the output to some
 * output stream.
 *
 * Usage pattern:
 *   // Create a concrete sequencer. Interface is dependent on the conreate
 *   // sequencer.
 *   std::unique_ptr<ObuSequencerBase> sequencer = ...;
 *
 *  // Call the `PushDescriptorObus` method.
 *  RETURN_IF_NOT_OK(sequencer->PushDescriptorObus(...));
 *
 *  while (more data is available) {
 *    // Call the `PushTemporalUnit` method.
 *    RETURN_IF_NOT_OK(sequencer->PushTemporalUnit(...));
 *   }
 *  // Signal that no more data is coming.
 *  // Depending on the context, choose one of the closing functions. Either
 *  // `UpdateDescriptorObusAndClose` (preferred) or `Close`.
 *  RETURN_IF_NOT_OK(sequencer->UpdateDescriptorObusAndClose(...));
 *  // Or:
 *  RETURN_IF_NOT_OK(sequencer->Close());
 *
 * // Optionally. `Abort` may be called to clean up output. E.g. file-based
 * // sequencers could delete their output file. `Abort` is most useful when
 * // some component outside the class failes; failures in `PushDescriptorObus`,
 * `PushTemporalUnit`, or `UpdateDescriptorObusAndClose` automatically call
 * `Abort`.
 */
class ObuSequencerBase {
 public:
  /*!\brief Constructor.
   *
   * \param leb_generator Leb generator to use when writing OBUs.
   * \param include_temporal_delimiters Whether the serialized data should
   *        include a temporal delimiter.
   * \param delay_descriptors_until_first_untrimmed_sample When `true`,
   *        `PushSerializedDescriptorObus` will be delayed until the first
   *        untrimmed sample is pushed.
   */
  ObuSequencerBase(const LebGenerator& leb_generator,
                   bool include_temporal_delimiters,
                   bool delay_descriptors_until_first_untrimmed_sample);

  // Deleted move and copy operations.
  ObuSequencerBase(ObuSequencerBase&&) = delete;
  ObuSequencerBase& operator=(ObuSequencerBase&&) = delete;
  ObuSequencerBase(const ObuSequencerBase&) = delete;
  ObuSequencerBase& operator=(const ObuSequencerBase&) = delete;

  /*!\brief Destructor.*/
  virtual ~ObuSequencerBase() = 0;

  /*!\brief Gathers statistics on and pushes the OBUs to some output.
   *
   * \param ia_sequence_header_obu IA Sequence Header OBU to write.
   * \param metadata_obus Metadata OBUs to write.
   * \param codec_config_obus Codec Config OBUs to write.
   * \param audio_elements Audio Element OBUs with data to write.
   * \param mix_presentation_obus Mix Presentation OBUs to write.
   * \param arbitrary_obus Arbitrary OBUs to write.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushDescriptorObus(
      const IASequenceHeaderObu& ia_sequence_header_obu,
      const std::list<MetadataObu>& metadata_obus,
      const DescriptorObus::CodecConfigsById& codec_config_obus,
      const DescriptorObus::AudioElementsById& audio_elements,
      const DescriptorObus::MixPresentationObus& mix_presentation_obus,
      const std::list<ArbitraryObu>& arbitrary_obus);

  /*!\brief Gathers statistics on and pushes the temporal unit to some output.
   *
   * \param temporal_unit Temporal unit to push.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushTemporalUnit(const TemporalUnitView& temporal_unit);

  /*!\brief Finalizes the descriptor OBUs and closes the output.
   *
   * \param ia_sequence_header_obu IA Sequence Header OBU to write.
   * \param metadata_obus Metadata OBUs to write.
   * \param codec_config_obus Codec Config OBUs to write.
   * \param audio_elements Audio Element OBUs with data to write.
   * \param mix_presentation_obus Mix Presentation OBUs to write.
   * \param arbitrary_obus Arbitrary OBUs to write.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status UpdateDescriptorObusAndClose(
      const IASequenceHeaderObu& ia_sequence_header_obu,
      const std::list<MetadataObu>& metadata_obus,
      const DescriptorObus::CodecConfigsById& codec_config_obus,
      const DescriptorObus::AudioElementsById& audio_elements,
      const DescriptorObus::MixPresentationObus& mix_presentation_obus,
      const std::list<ArbitraryObu>& arbitrary_obus);

  /*!\brief Signals that no more data is coming, and closes the output.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Close();

  /*!\brief Aborts writing the output.
   *
   * Useful for sequencers which want to clean up their output. Such as to avoid
   * leaving a stray file when encoding fails.
   */
  void Abort();

 protected:
  /*!\brief Pushes the descriptor OBUs and to some output.
   *
   * Various statistics are also signalled to the concrete class. For example,
   * an MP4 sequencer may need the timing information to control the timebase in
   * the output file. Concrete classes may ignore these statistics as they see
   * fit.
   *
   * \param common_samples_per_frame Common number of samples per frame for the
   *        IA Sequence.
   * \param common_sample_rate Common sample rate for the IA Sequence.
   * \param common_bit_depth Common bit depth for the IA Sequence.
   * \param first_untrimmed_timestamp Timestamp for the first untrimmed sample
   *        in the IA Sequence, or
   *        `delay_descriptors_until_first_untrimmed_sample` is `false`. In some
   *        contexts, this is known as the first Presentation Time Stamp (PTS).
   * \param descriptor_obus Serialized descriptor OBUs to push.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status PushSerializedDescriptorObus(
      uint32_t common_samples_per_frame, uint32_t common_sample_rate,
      uint8_t common_bit_depth,
      std::optional<InternalTimestamp> first_untrimmed_timestamp,
      int num_channels, absl::Span<const uint8_t> descriptor_obus) = 0;

  /*!\brief Pushes a single temporal unit to some output.
   *
   * \param timestamp Start timestamp of the temporal unit.
   * \param num_samples Number of samples in the temporal unit.
   * \param is_key_frame Whether the temporal unit is a key frame.
   * \param temporal_unit Temporal unit to push.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status PushSerializedTemporalUnit(
      InternalTimestamp timestamp, int num_samples, bool is_key_frame,
      absl::Span<const uint8_t> temporal_unit) = 0;

  /*!\brief Pushes the finalized descriptor OBUs to some output.
   *
   * \param descriptor_obus Serialized finalized descriptor OBUs to push.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status PushFinalizedDescriptorObus(
      absl::Span<const uint8_t> descriptor_obus) = 0;

  /*!\brief Signals that no more data is coming, and closes the output. */
  virtual void CloseDerived() = 0;

  /*!\brief Aborts writing the output.
   *
   * Useful for sequencers which want to clean up their output. Such as to avoid
   * leaving a stray file when encoding fails.
   */
  virtual void AbortDerived() = 0;

  // The `LebGenerator` to use when writing OBUs.
  const LebGenerator leb_generator_;

 private:
  /*!\brief Handles the initial temporal units.
   *
   * This function manages state to help process the initial temporal units up
   * to and including the first one that has a real sample. In a typical IA
   * Sequence, this would rarely be more few frames.
   *
   * \param temporal_unit Temporal unit to push.
   * \param serialized_temporal_unit Serialized temporla unit.
   * \param is_key_frame Whether the temporal unit is a key frame.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status HandleInitialTemporalUnits(
      const TemporalUnitView& temporal_unit,
      absl::Span<const uint8_t> serialized_temporal_unit, bool is_key_frame);

  enum State {
    // Initial state.
    kInitialized,
    // `PushDescriptorObus` has been called, but it may have been delayed when
    // `delay_descriptors_until_first_untrimmed_sample_` is `true`.
    kPushDescriptorObusCalled,
    // Descriptors have been pushed, in this state temporal units are no longer
    // delayed.
    kPushSerializedDescriptorsCalled,
    // `Close` or `Abort` has been called.
    kClosed
  };
  State state_ = kInitialized;

  const bool delay_descriptors_until_first_untrimmed_sample_;
  const bool include_temporal_delimiters_;

  // Statistics for the current IA Sequence. Convenient to hold, in order to
  // validate that the finalized OBUs are consistent with the initial ones.
  struct DescriptorStatistics {
    uint32_t common_samples_per_frame = 0;
    uint32_t common_sample_rate = 0;
    uint8_t common_bit_depth = 0;
    int num_channels = 0;
    std::optional<InternalTimestamp> first_untrimmed_timestamp;
    std::vector<uint8_t> descriptor_obus;
  };
  std::optional<DescriptorStatistics> descriptor_statistics_;

  // Reusable scratch buffer.
  WriteBitBuffer wb_;

  int64_t num_temporal_units_for_logging_ = 0;
  int64_t cumulative_num_samples_for_logging_ = 0;

  // State for delayed OBUs. `delay_descriptors_until_first_untrimmed_sample_ ==
  // true` implies we must cache and delayed OBUs until the first untrimmed
  // sample is seen. In practical IA Sequences, this is rarely more than a few
  // temporal units.
  struct SerializedTemporalUnit {
    InternalTimestamp start_timestamp;
    uint32_t num_untrimmed_samples;
    bool is_key_frame;
    std::vector<uint8_t> data;
  };
  std::list<SerializedTemporalUnit> delayed_temporal_units_;
};

}  // namespace iamf_tools

#endif  // CLI_OBU_SEQUENCER_BASE_H_
