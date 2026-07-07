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

#include "iamf/api/decoder/iamf_decoder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/api/conversion/channel_reorderer.h"
#include "iamf/api/conversion/mix_presentation_conversion.h"
#include "iamf/api/conversion/profile_conversion.h"
#include "iamf/cli/obu_processor.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace api {

enum class DecoderStatus { kAcceptingData, kEndOfStream };

namespace {
iamf_tools::TrimmingSettings TranslateTrimmingSettings(
    api::TrimmingSettings api_settings) {
  return iamf_tools::TrimmingSettings{
      .trim_beginning = api_settings.trim_beginning,
      .trim_end = api_settings.trim_end,
  };
}
}  // namespace

// Holds the internal state of the decoder to hide it and necessary includes
// from API users.
struct IamfDecoder::DecoderState {
  /*!\brief Constructor.
   *
   * \param read_bit_buffer Buffer to decode from.
   * \param requested_mix User-requested mix, the actual mix used may be
   *        different depending on what is found in the mix presentations.
   * \param requested_profile_versions User-requested profile versions, the
   *        actual profile version may be different depending on the mix
   *        presentations.
   */
  DecoderState(std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer,
               const RequestedMix& requested_mix,
               const absl::flat_hash_set<::iamf_tools::ProfileVersion>&
                   requested_profile_versions,
               api::TrimmingSettings api_trimming_settings)
      : read_bit_buffer(std::move(read_bit_buffer)),
        requested_mix(requested_mix),
        desired_profile_versions(requested_profile_versions),
        trimming_settings(TranslateTrimmingSettings(api_trimming_settings)) {}

  /*!\brief Creates an ObuProcessor and maintains related bookkeeping. */
  absl::Status CreateObuProcessor();

  // Current status of the decoder.
  DecoderStatus status = DecoderStatus::kAcceptingData;

  // Used to process descriptor OBUs and temporal units. Is only created after
  // the descriptor OBUs have been parsed.
  std::unique_ptr<ObuProcessor> obu_processor;

  // Buffer that is filled with data from Decode().
  std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer;

  // Rendered samples. Corresponds to one temporal unit.
  std::vector<absl::Span<const InternalSampleType>> rendered_samples;

  // The optionally set parameters to request a particular mix.
  RequestedMix requested_mix;

  // The actually selected Mix Presentation ID, as reported by ObuProcessor.
  DecodedUleb128 actual_mix_presentation_id;

  // The actually selected Layout, as reported by ObuProcessor.
  Layout actual_layout;

  // TODO(b/379122580):  Use the bit depth of the underlying content.
  // Defaulting to int32 for now.
  OutputSampleType output_sample_type = OutputSampleType::kInt32LittleEndian;

  // True iff the decoder was created via CreateFromDescriptors().
  bool created_from_descriptors = false;

  // Cache the profile versions that the user is interested in, we use them to
  // select an appropriate mix presentation.
  const absl::flat_hash_set<::iamf_tools::ProfileVersion>
      desired_profile_versions;

  const iamf_tools::TrimmingSettings trimming_settings;

  // Once descriptors have been processed, they are stored here. This is useful
  // for Reset() purposes, in which we can recreate the ObuProcessor with the
  // original descriptors in order to ensure that the state of the processor is
  // clean.
  std::vector<uint8_t> descriptor_obus;

  ChannelReorderer::RearrangementScheme channel_rearrangement_scheme =
      ChannelReorderer::RearrangementScheme::kDefaultNoOp;
  // Created after DescriptorObus are processed and final Layout is known.
  std::optional<ChannelReorderer> channel_reorderer = std::nullopt;
};

// Creates an ObuProcessor; an ObuProcessor is only created once all descriptor
// OBUs have been processed. Contracted to only return a resource exhausted
// error if there is not enough data to process the descriptor OBUs.
absl::Status IamfDecoder::DecoderState::CreateObuProcessor() {
  // Happens only in the pure streaming case.
  const auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;
  auto temp_obu_processor = ObuProcessor::CreateForRendering(
      desired_profile_versions, requested_mix.mix_presentation_id,
      ApiToInternalType(requested_mix.output_layout), created_from_descriptors,
      read_bit_buffer.get(), trimming_settings, insufficient_data);
  if (temp_obu_processor == nullptr) {
    // `insufficient_data` is true iff everything so far is valid but more data
    // is needed.
    if (insufficient_data && !created_from_descriptors) {
      return absl::ResourceExhaustedError(
          "Have not received enough data yet to process descriptor "
          "OBUs. Please call Decode() again with more data.");
    }
    return absl::InvalidArgumentError("Failed to create OBU processor.");
  }
  const auto num_bytes_read = (read_bit_buffer->Tell() - start_position) / 8;

  // Seek back to the beginning of the data that was processed so that we can
  // read and store the binary IAMF descriptor OBUs.
  RETURN_IF_NOT_OK(read_bit_buffer->Seek(start_position));
  descriptor_obus.resize(num_bytes_read);
  RETURN_IF_NOT_OK(
      read_bit_buffer->ReadUint8Span(absl::MakeSpan(descriptor_obus)));
  RETURN_IF_NOT_OK(read_bit_buffer->Flush(num_bytes_read));

  auto new_mix_presentation_id =
      temp_obu_processor->GetOutputMixPresentationId();
  if (!new_mix_presentation_id.ok()) {
    return new_mix_presentation_id.status();
  }
  actual_mix_presentation_id = *new_mix_presentation_id;

  auto new_layout = temp_obu_processor->GetOutputLayout();
  if (!new_layout.ok()) {
    return new_layout.status();
  }
  actual_layout = *new_layout;

  // Copy over fields at the end, now that everything is successful.
  obu_processor = std::move(temp_obu_processor);

  return absl::OkStatus();
}

namespace {
constexpr int kInitialBufferSize = 1024;

IamfStatus AbslToIamfStatus(const absl::Status& absl_status) {
  if (absl_status.ok()) {
    return IamfStatus::OkStatus();
  } else {
    return IamfStatus::ErrorStatus(
        absl_status.ToString(absl::StatusToStringMode::kDefault));
  }
}

ChannelReorderer::RearrangementScheme ChannelOrderingApiToInternalType(
    ChannelOrdering channel_ordering) {
  switch (channel_ordering) {
    case ChannelOrdering::kOrderingForAndroid:
      return ChannelReorderer::RearrangementScheme::kReorderForAndroid;
    case ChannelOrdering::kIamfOrdering:
    default:
      return ChannelReorderer::RearrangementScheme::kDefaultNoOp;
  }
}

IamfStatus DecodeOneTemporalUnit(
    StreamBasedReadBitBuffer* read_bit_buffer, ObuProcessor* obu_processor,
    bool eos_is_end_of_sequence,
    std::vector<absl::Span<const InternalSampleType>>& rendered_samples,
    std::optional<ChannelReorderer> channel_reorderer) {
  if (read_bit_buffer == nullptr) {
    return IamfStatus::ErrorStatus("Internal Error: Read bit buffer is null.");
  }
  if (obu_processor == nullptr) {
    return IamfStatus::ErrorStatus("Internal Error: Obu processor is null.");
  }
  const auto start_position_bits = read_bit_buffer->Tell();
  std::optional<ObuProcessor::OutputTemporalUnit> output_temporal_unit;
  bool unused_continue_processing = true;
  absl::Status absl_status = obu_processor->ProcessTemporalUnit(
      eos_is_end_of_sequence, output_temporal_unit, unused_continue_processing);
  if (!absl_status.ok()) {
    return AbslToIamfStatus(absl_status);
  }
  // We may have processed bytes but not a full temporal unit.
  if (output_temporal_unit.has_value()) {
    absl::StatusOr<absl::Span<const absl::Span<const InternalSampleType>>>
        rendered_samples_for_temporal_unit =
            obu_processor->RenderTemporalUnitAndMeasureLoudness(
                output_temporal_unit->output_timestamp,
                output_temporal_unit->output_parameter_blocks,
                output_temporal_unit->output_audio_frames);
    if (!rendered_samples_for_temporal_unit.ok()) {
      return AbslToIamfStatus(rendered_samples_for_temporal_unit.status());
    }
    rendered_samples = std::vector(rendered_samples_for_temporal_unit->begin(),
                                   rendered_samples_for_temporal_unit->end());
    if (channel_reorderer.has_value()) {
      ABSL_CHECK_OK(channel_reorderer->Reorder(rendered_samples));
    }
  }
  // Empty the buffer of the data that was processed thus far.
  const auto num_bits_read = read_bit_buffer->Tell() - start_position_bits;
  absl::Status flush_status = read_bit_buffer->Flush(num_bits_read / 8);
  if (!flush_status.ok()) {
    return AbslToIamfStatus(flush_status);
  }
  return IamfStatus::OkStatus();
}

size_t BytesPerSample(OutputSampleType sample_type) {
  switch (sample_type) {
    case OutputSampleType::kInt16LittleEndian:
      return 2;
    case OutputSampleType::kInt32LittleEndian:
      return 4;
    default:
      return 0;
  }
}

IamfStatus WriteFrameToSpan(
    const std::vector<absl::Span<const InternalSampleType>>& frame,
    OutputSampleType sample_type, absl::Span<uint8_t> output_bytes,
    size_t& bytes_written) {
  const size_t bytes_per_sample = BytesPerSample(sample_type);
  const size_t bits_per_sample = bytes_per_sample * 8;
  const size_t required_size =
      frame.size() * frame[0].size() * bytes_per_sample;
  if (output_bytes.size() < required_size) {
    return IamfStatus::ErrorStatus(
        "Invalid Argument: Span does not have enough space to write output "
        "bytes.");
  }
  const bool big_endian = false;
  size_t write_position = 0;
  uint8_t* data = output_bytes.data();
  for (size_t t = 0; t < frame[0].size(); t++) {
    for (size_t c = 0; c < frame.size(); ++c) {
      int32_t sample;
      absl::Status absl_status =
          NormalizedFloatingPointToInt32(frame[c][t], sample);
      if (!absl_status.ok()) {
        return AbslToIamfStatus(absl_status);
      }

      absl_status.Update(WritePcmSample(static_cast<uint32_t>(sample),
                                        bits_per_sample, big_endian, data,
                                        write_position));
      if (!absl_status.ok()) {
        return AbslToIamfStatus(absl_status);
      }
    }
  }
  bytes_written = write_position;
  return IamfStatus::OkStatus();
}

}  // namespace

IamfDecoder::IamfDecoder(std::unique_ptr<DecoderState> state)
    : state_(std::move(state)) {}

// While these are all `= default`, they must be here in the source file because
// the unique_ptr of the partial class, DecoderState, prevents them from being
// inline.
IamfDecoder::~IamfDecoder() = default;
IamfDecoder::IamfDecoder(IamfDecoder&&) = default;
IamfDecoder& IamfDecoder::operator=(IamfDecoder&&) = default;

IamfStatus IamfDecoder::Create(const Settings& settings,
                               std::unique_ptr<IamfDecoder>& output_decoder) {
  output_decoder = nullptr;

  std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer =
      StreamBasedReadBitBuffer::Create(kInitialBufferSize);
  if (read_bit_buffer == nullptr) {
    return IamfStatus::ErrorStatus(
        "Internal Error: Failed to create read bit buffer.");
  }

  // Cache the internal representation of the profile versions. Depending on
  // creation mode, we may not have all the descriptors yet.
  absl::flat_hash_set<::iamf_tools::ProfileVersion> desired_profile_versions;
  for (const auto& profile_version : settings.requested_profile_versions) {
    desired_profile_versions.insert(ApiToInternalType(profile_version));
  }

  std::unique_ptr<DecoderState> state = std::make_unique<DecoderState>(
      std::move(read_bit_buffer), settings.requested_mix,
      desired_profile_versions, settings.trimming_settings);
  state->channel_rearrangement_scheme =
      ChannelOrderingApiToInternalType(settings.channel_ordering);
  state->output_sample_type = settings.requested_output_sample_type;
  output_decoder = absl::WrapUnique(new IamfDecoder(std::move(state)));
  return IamfStatus::OkStatus();
}

IamfStatus IamfDecoder::CreateFromDescriptors(
    const Settings& settings, const uint8_t* input_buffer,
    size_t input_buffer_size, std::unique_ptr<IamfDecoder>& output_decoder) {
  output_decoder = nullptr;
  absl::Span<const uint8_t> descriptor_obus(input_buffer, input_buffer_size);

  IamfStatus status = Create(settings, output_decoder);
  if (!status.ok()) {
    return status;
  }
  if (output_decoder == nullptr) {
    return IamfStatus::ErrorStatus("Internal Error: Unexpected null decoder");
  }
  absl::Status absl_status =
      output_decoder->state_->read_bit_buffer->PushBytes(descriptor_obus);
  if (!absl_status.ok()) {
    return AbslToIamfStatus(absl_status);
  }

  output_decoder->state_->created_from_descriptors = true;
  return AbslToIamfStatus(output_decoder->state_->CreateObuProcessor());
}

IamfStatus IamfDecoder::Decode(const uint8_t* input_buffer,
                               size_t input_buffer_size) {
  if (state_->status == DecoderStatus::kEndOfStream) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: Decode() cannot be called after "
        "SignalEndOfStream() has been called.");
  }
  auto bitstream = absl::MakeConstSpan(input_buffer, input_buffer_size);
  absl::Status push_bytes_status =
      state_->read_bit_buffer->PushBytes(bitstream);
  if (!push_bytes_status.ok()) {
    return AbslToIamfStatus(push_bytes_status);
  }
  if (!IsDescriptorProcessingComplete()) {
    const auto created_obu_processor_status = state_->CreateObuProcessor();

    if (created_obu_processor_status.ok()) {
      return IamfStatus::OkStatus();
    } else if (absl::IsResourceExhausted(created_obu_processor_status)) {
      // Don't have enough data to process the descriptor OBUs yet, but no
      // errors have occurred.
      return IamfStatus::OkStatus();
    } else {
      // Corrupted data or other errors.
      return AbslToIamfStatus(created_obu_processor_status);
    }
  }

  // At this stage, we know that we've processed all descriptor OBUs.
  if (std::holds_alternative<LoudspeakersSsConventionLayout>(
          state_->actual_layout.specific_layout)) {
    auto sound_system = std::get<LoudspeakersSsConventionLayout>(
                            state_->actual_layout.specific_layout)
                            .sound_system;
    state_->channel_reorderer = ChannelReorderer::Create(
        sound_system, state_->channel_rearrangement_scheme);
  }
  if (state_->rendered_samples.empty()) {
    // We only try to actually decode a temporal unit if we have no currently
    // decoded temporal units. If we do, we'll decode the next temporal unit in
    // `GetOutputTemporalUnit()`.
    return DecodeOneTemporalUnit(
        state_->read_bit_buffer.get(), state_->obu_processor.get(),
        state_->created_from_descriptors, state_->rendered_samples,
        state_->channel_reorderer);
  }
  return IamfStatus::OkStatus();
}

IamfStatus IamfDecoder::GetOutputTemporalUnit(uint8_t* output_buffer,
                                              size_t output_buffer_size,
                                              size_t& bytes_written) {
  bytes_written = 0;
  if (state_->rendered_samples.empty()) {
    return IamfStatus::OkStatus();
  }
  // Write decoded temporal unit to output buffer.
  OutputSampleType output_sample_type = GetOutputSampleType();
  IamfStatus status = WriteFrameToSpan(
      state_->rendered_samples, output_sample_type,
      absl::MakeSpan(output_buffer, output_buffer_size), bytes_written);
  if (status.ok()) {
    state_->rendered_samples.clear();
  }

  // Refill the rendered samples with the next temporal unit.
  auto decode_status = DecodeOneTemporalUnit(
      state_->read_bit_buffer.get(), state_->obu_processor.get(),
      state_->created_from_descriptors ||
          state_->status == DecoderStatus::kEndOfStream,
      state_->rendered_samples, state_->channel_reorderer);
  if (!decode_status.ok()) {
    return decode_status;
  }
  return status;
}

bool IamfDecoder::IsTemporalUnitAvailable() const {
  return !state_->rendered_samples.empty();
}

bool IamfDecoder::IsDescriptorProcessingComplete() const {
  return state_->obu_processor != nullptr;
}

IamfStatus IamfDecoder::GetOutputMix(SelectedMix& output_selected_mix) const {
  if (!IsDescriptorProcessingComplete()) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: GetOutputMix() cannot be called before "
        "descriptor processing is complete.");
  }
  absl::StatusOr<OutputLayout> conversion =
      InternalToApiType(state_->actual_layout);
  if (!conversion.ok()) {
    return AbslToIamfStatus(conversion.status());
  }
  output_selected_mix.output_layout = *conversion;
  output_selected_mix.mix_presentation_id = state_->actual_mix_presentation_id;
  return IamfStatus::OkStatus();
}

IamfStatus IamfDecoder::GetNumberOfOutputChannels(
    int& output_num_channels) const {
  if (!IsDescriptorProcessingComplete()) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: GetNumberOfOutputChannels() cannot be called "
        "before descriptor processing is complete.");
  }
  return AbslToIamfStatus(MixPresentationObu::GetNumChannelsFromLayout(
      state_->actual_layout, output_num_channels));
}

OutputSampleType IamfDecoder::GetOutputSampleType() const {
  return state_->output_sample_type;
}

IamfStatus IamfDecoder::GetSampleRate(uint32_t& output_sample_rate) const {
  if (!IsDescriptorProcessingComplete()) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: GetSampleRate() cannot be called before "
        "descriptor processing is complete.");
  }
  absl::StatusOr<uint32_t> sample_rate =
      state_->obu_processor->GetOutputSampleRate();
  if (sample_rate.ok()) {
    output_sample_rate = *sample_rate;
    return IamfStatus::OkStatus();
  }
  return AbslToIamfStatus(sample_rate.status());
}

IamfStatus IamfDecoder::GetFrameSize(uint32_t& output_frame_size) const {
  if (!IsDescriptorProcessingComplete()) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: GetFrameSize() cannot be called before "
        "descriptor processing is complete.");
  }

  absl::StatusOr<uint32_t> frame_size =
      state_->obu_processor->GetOutputFrameSize();
  if (frame_size.ok()) {
    output_frame_size = *frame_size;
    return IamfStatus::OkStatus();
  }
  return AbslToIamfStatus(frame_size.status());
}

IamfStatus IamfDecoder::Reset() {
  if (!state_->created_from_descriptors) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: Reset() cannot be called in standalone decoding "
        "mode.");
  }

  // Clear the rendered samples.
  state_->rendered_samples = {};

  // Clear the channel reorderer.
  state_->channel_reorderer = std::nullopt;

  // Set state.
  state_->status = DecoderStatus::kAcceptingData;
  // Create a new read bit buffer.
  std::unique_ptr<StreamBasedReadBitBuffer> read_bit_buffer =
      StreamBasedReadBitBuffer::Create(kInitialBufferSize);
  if (read_bit_buffer == nullptr) {
    return IamfStatus::ErrorStatus(
        "Internal Error: Failed to create read bit buffer.");
  }
  state_->read_bit_buffer = std::move(read_bit_buffer);

  // Create a new ObuProcessor with the original descriptor OBUs.
  absl::Status absl_status =
      state_->read_bit_buffer->PushBytes(state_->descriptor_obus);
  if (!absl_status.ok()) {
    return AbslToIamfStatus(absl_status);
  }
  return AbslToIamfStatus(state_->CreateObuProcessor());
}

IamfStatus IamfDecoder::ResetWithNewMix(const RequestedMix& requested_mix,
                                        SelectedMix& selected_mix) {
  if (!state_->created_from_descriptors) {
    return IamfStatus::ErrorStatus(
        "Failed Precondition: ResetWithNewMix() cannot be called in "
        "standalone decoding mode.");
  }
  state_->requested_mix = requested_mix;
  IamfStatus status = Reset();
  if (!status.ok()) {
    return status;
  }
  auto output_layout = InternalToApiType(state_->actual_layout);
  if (!output_layout.ok()) {
    return AbslToIamfStatus(output_layout.status());
  }
  selected_mix.mix_presentation_id = state_->actual_mix_presentation_id;
  selected_mix.output_layout = *output_layout;
  return IamfStatus::OkStatus();
}

IamfStatus IamfDecoder::SignalEndOfDecoding() {
  state_->status = DecoderStatus::kEndOfStream;
  if (!state_->created_from_descriptors && state_->rendered_samples.empty() &&
      state_->obu_processor != nullptr) {
    // If we're in standalone decoding mode, we need to decode any remaining
    // temporal units with the signal that we've reached the end of the stream
    // so that we know to end the last temporal unit.
    auto decode_status = DecodeOneTemporalUnit(
        state_->read_bit_buffer.get(), state_->obu_processor.get(),
        /*eos_is_end_of_sequence=*/true, state_->rendered_samples,
        state_->channel_reorderer);
    if (!decode_status.ok()) {
      return decode_status;
    }
  }
  return IamfStatus::OkStatus();
}

}  // namespace api
}  // namespace iamf_tools
