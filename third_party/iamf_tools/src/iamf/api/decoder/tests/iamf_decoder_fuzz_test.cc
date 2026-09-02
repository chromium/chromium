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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "absl/strings/string_view.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"
#include "iamf/api/decoder/iamf_decoder.h"
#include "iamf/cli/tests/portable/get_test_path.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"

namespace iamf_tools {
namespace {

using api::OutputLayout;
using ::fuzztest::Arbitrary;
using ::fuzztest::ElementOf;
using ::fuzztest::OptionalOf;

constexpr absl::string_view kIamfFileTestPath = "iamf/cli/testdata/iamf/";

constexpr OutputLayout kStereoLayout =
    OutputLayout::kItu2051_SoundSystemA_0_2_0;

int GetBytesPerSample(api::OutputSampleType output_sample_type) {
  switch (output_sample_type) {
    case api::OutputSampleType::kInt16LittleEndian:
      return 2;
    case api::OutputSampleType::kInt32LittleEndian:
      return 4;
    default:
      return 0;
  }
}

void OutputAllTemporalUnits(api::IamfDecoder& iamf_decoder) {
  if (!iamf_decoder.IsDescriptorProcessingComplete()) {
    // Under fuzz testing, some streams are too corrupt to meaningfully decode
    // further.
    return;
  }

  // Compute the maximum size of the output audio buffer.
  const int bytes_per_sample =
      GetBytesPerSample(iamf_decoder.GetOutputSampleType());
  uint32_t frame_size;
  if (!iamf_decoder.GetFrameSize(frame_size).ok()) {
    return;
  }
  int num_output_channels;
  if (!iamf_decoder.GetNumberOfOutputChannels(num_output_channels).ok()) {
    return;
  }
  // Extract and throw away all temporal units.
  std::vector<uint8_t> output_buffer(bytes_per_sample * frame_size *
                                     num_output_channels);
  size_t unused_bytes_written;
  while (iamf_decoder.IsTemporalUnitAvailable()) {
    auto unused_get_status = iamf_decoder.GetOutputTemporalUnit(
        output_buffer.data(), output_buffer.size(), unused_bytes_written);
  }
}

void DoesNotDieWithBasicDecode(const std::string& data) {
  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  const api::IamfDecoder::Settings kStereoLayoutSettings = {
      .requested_mix = {.output_layout = kStereoLayout}};
  ASSERT_TRUE(
      api::IamfDecoder::Create(kStereoLayoutSettings, iamf_decoder).ok());

  std::vector<uint8_t> bitstream(data.begin(), data.end());
  auto decode_status = iamf_decoder->Decode(bitstream.data(), bitstream.size());

  OutputAllTemporalUnits(*iamf_decoder);
}

FUZZ_TEST(IamfDecoderFuzzTest_ArbitraryBytes, DoesNotDieWithBasicDecode);

FUZZ_TEST(IamfDecoderFuzzTest_Seeded, DoesNotDieWithBasicDecode)
    .WithSeeds(
        fuzztest::ReadFilesFromDirectory(GetRunfilesPath(kIamfFileTestPath)));

void DoesNotDieCreateFromDescriptors(const std::string& descriptor_data,
                                     const std::string& temporal_unit_data) {
  std::vector<uint8_t> descriptors(descriptor_data.begin(),
                                   descriptor_data.end());

  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  // Intentionally check that defaulted settings are safe to use.
  const api::IamfDecoder::Settings kDefaultSettings;
  api::IamfStatus status = api::IamfDecoder::CreateFromDescriptors(
      kDefaultSettings, descriptors.data(), descriptors.size(), iamf_decoder);
  if (!status.ok()) {
    return;
  }

  std::vector<uint8_t> temporal_unit(temporal_unit_data.begin(),
                                     temporal_unit_data.end());
  auto unused_decode_status =
      iamf_decoder->Decode(temporal_unit.data(), temporal_unit.size());
  OutputAllTemporalUnits(*iamf_decoder);
}

FUZZ_TEST(IamfDecoderFuzzTest_ArbitraryBytesToDescriptors,
          DoesNotDieCreateFromDescriptors);

void DoesNotDieReset(const std::string& descriptor_data,
                     const std::string& temporal_unit_data) {
  std::vector<uint8_t> descriptors(descriptor_data.begin(),
                                   descriptor_data.end());

  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  const api::IamfDecoder::Settings kDefaultSettings;
  api::IamfStatus status = api::IamfDecoder::CreateFromDescriptors(
      kDefaultSettings, descriptors.data(), descriptors.size(), iamf_decoder);
  if (!status.ok()) {
    return;
  }

  std::vector<uint8_t> temporal_unit(temporal_unit_data.begin(),
                                     temporal_unit_data.end());
  auto unused_decode_status =
      iamf_decoder->Decode(temporal_unit.data(), temporal_unit.size());
  OutputAllTemporalUnits(*iamf_decoder);

  if (iamf_decoder->Reset().ok()) {
    auto unused_decode_status_after_reset =
        iamf_decoder->Decode(temporal_unit.data(), temporal_unit.size());
    OutputAllTemporalUnits(*iamf_decoder);
  }
}

FUZZ_TEST(IamfDecoderFuzzTest_Reset, DoesNotDieReset);

void DoesNotDieAllParams(
    api::RequestedMix requested_mix, api::ChannelOrdering channel_ordering,
    std::unordered_set<api::ProfileVersion> requested_profile_versions,
    api::OutputSampleType output_sample_type,
    api::TrimmingSettings trimming_settings, std::string data) {
  std::vector<uint8_t> bitstream(data.begin(), data.end());
  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  const api::IamfDecoder::Settings kSettings = {
      .requested_mix = requested_mix,
      .channel_ordering = channel_ordering,
      .requested_profile_versions = requested_profile_versions,
      .requested_output_sample_type = output_sample_type,
      .trimming_settings = trimming_settings,
  };
  ASSERT_TRUE(api::IamfDecoder::Create(kSettings, iamf_decoder).ok());

  auto unused_decode_status =
      iamf_decoder->Decode(bitstream.data(), bitstream.size());
  OutputAllTemporalUnits(*iamf_decoder);
}

auto AnyOutputLayout() {
  return ElementOf<api::OutputLayout>({
      api::OutputLayout::kItu2051_SoundSystemA_0_2_0,
      api::OutputLayout::kItu2051_SoundSystemB_0_5_0,
      api::OutputLayout::kItu2051_SoundSystemC_2_5_0,
      api::OutputLayout::kItu2051_SoundSystemD_4_5_0,
      api::OutputLayout::kItu2051_SoundSystemE_4_5_1,
      api::OutputLayout::kItu2051_SoundSystemF_3_7_0,
      api::OutputLayout::kItu2051_SoundSystemG_4_9_0,
      api::OutputLayout::kItu2051_SoundSystemH_9_10_3,
      api::OutputLayout::kItu2051_SoundSystemI_0_7_0,
      api::OutputLayout::kItu2051_SoundSystemJ_4_7_0,
      api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0,
      api::OutputLayout::kIAMF_SoundSystemExtension_2_3_0,
      api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0,
      api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0,
      api::OutputLayout::kIAMF_Binaural,
  });
}

auto AnyRequestedMix() {
  return fuzztest::StructOf<api::RequestedMix>(
      fuzztest::OptionalOf(Arbitrary<uint32_t>()),  // mix_presentation_id,
      fuzztest::OptionalOf(AnyOutputLayout())       // output_layout,
  );
}

auto AnyChannelOrdering() {
  return ElementOf<api::ChannelOrdering>({
      api::ChannelOrdering::kIamfOrdering,
      api::ChannelOrdering::kOrderingForAndroid,
  });
}

// The ProfileVersion should be any combination of values from the
// ProfileVersion enum (from empty to all of them).
auto AnyProfileVersion() {
  return fuzztest::UnorderedSetOf(ElementOf<api::ProfileVersion>({
      api::ProfileVersion::kIamfSimpleProfile,
      api::ProfileVersion::kIamfBaseProfile,
      api::ProfileVersion::kIamfBaseEnhancedProfile,
  }));
}

auto AnyOutputSampleType() {
  return ElementOf<api::OutputSampleType>({
      api::OutputSampleType::kInt16LittleEndian,
      api::OutputSampleType::kInt32LittleEndian,
  });
}

auto AnyTrimmingSettings() {
  return fuzztest::StructOf<api::TrimmingSettings>(
      fuzztest::Arbitrary<bool>(),  // trim_beginning
      fuzztest::Arbitrary<bool>()   // trim_end
  );
}

enum class OperationType {
  kDecode,
  kGetOutputTemporalUnit,
  kIsTemporalUnitAvailable,
  kIsDescriptorProcessingComplete,
  kGetOutputMix,
  kGetNumberOfOutputChannels,
  kGetOutputSampleType,
  kGetSampleRate,
  kGetFrameSize,
  kReset,
  kResetWithNewMix,
  kSignalEndOfDecoding
};

struct Operation {
  OperationType type;
  // Input data for methods that consume data.
  std::string data;
  int32_t chunk_size;
  std::optional<api::OutputLayout> output_layout;
  std::optional<uint32_t> mix_presentation_id;
};

void RunOperation(api::IamfDecoder& iamf_decoder, const Operation& op,
                  size_t& seed_offset, const std::string& seed_data) {
  switch (op.type) {
    case OperationType::kDecode: {
      size_t chunk_size = op.chunk_size;
      if (seed_offset < seed_data.size()) {
        chunk_size = std::min(chunk_size, seed_data.size() - seed_offset);
        auto unused_status = iamf_decoder.Decode(
            reinterpret_cast<const uint8_t*>(seed_data.data()) + seed_offset,
            chunk_size);
        seed_offset += chunk_size;
      } else {
        auto unused_status = iamf_decoder.Decode(nullptr, 0);
      }
      break;
    }
    case OperationType::kGetOutputTemporalUnit: {
      OutputAllTemporalUnits(iamf_decoder);
      break;
    }
    case OperationType::kIsTemporalUnitAvailable:
      iamf_decoder.IsTemporalUnitAvailable();
      break;
    case OperationType::kIsDescriptorProcessingComplete:
      iamf_decoder.IsDescriptorProcessingComplete();
      break;
    case OperationType::kGetOutputMix: {
      api::SelectedMix selected_mix;
      auto unused_status = iamf_decoder.GetOutputMix(selected_mix);
      break;
    }
    case OperationType::kGetNumberOfOutputChannels: {
      int num_channels = 0;
      auto unused_status = iamf_decoder.GetNumberOfOutputChannels(num_channels);
      break;
    }
    case OperationType::kGetOutputSampleType:
      iamf_decoder.GetOutputSampleType();
      break;
    case OperationType::kGetSampleRate: {
      uint32_t sample_rate = 0;
      auto unused_status = iamf_decoder.GetSampleRate(sample_rate);
      break;
    }
    case OperationType::kGetFrameSize: {
      uint32_t frame_size = 0;
      auto unused_status = iamf_decoder.GetFrameSize(frame_size);
      break;
    }
    case OperationType::kReset: {
      auto unused_status = iamf_decoder.Reset();
      break;
    }
    case OperationType::kResetWithNewMix: {
      api::RequestedMix requested_mix = {
          .mix_presentation_id = op.mix_presentation_id,
          .output_layout = op.output_layout};
      api::SelectedMix selected_mix;
      auto unused_status =
          iamf_decoder.ResetWithNewMix(requested_mix, selected_mix);
      break;
    }
    case OperationType::kSignalEndOfDecoding: {
      auto unused_status = iamf_decoder.SignalEndOfDecoding();
      break;
    }
  }
}

void DoesNotDieWithArbitrarySequenceOfOperations(
    const std::string& seed_data, const std::vector<Operation>& operations) {
  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  const api::IamfDecoder::Settings kDefaultSettings;

  api::IamfStatus status =
      api::IamfDecoder::Create(kDefaultSettings, iamf_decoder);
  if (!status.ok()) {
    return;
  }

  size_t seed_offset = 0;
  for (const auto& op : operations) {
    RunOperation(*iamf_decoder, op, seed_offset, seed_data);
  }
}

auto AnyOperationType() {
  return ElementOf<OperationType>(
      {OperationType::kDecode, OperationType::kGetOutputTemporalUnit,
       OperationType::kIsTemporalUnitAvailable,
       OperationType::kIsDescriptorProcessingComplete,
       OperationType::kGetOutputMix, OperationType::kGetNumberOfOutputChannels,
       OperationType::kGetOutputSampleType, OperationType::kGetSampleRate,
       OperationType::kGetFrameSize, OperationType::kReset,
       OperationType::kResetWithNewMix, OperationType::kSignalEndOfDecoding});
}

auto AnyOperation() {
  return fuzztest::StructOf<Operation>(
      AnyOperationType(), Arbitrary<std::string>(), Arbitrary<int32_t>(),
      OptionalOf(AnyOutputLayout()), OptionalOf(Arbitrary<uint32_t>()));
}

std::vector<std::tuple<std::string, std::vector<Operation>>> GetSeeds() {
  auto files =
      fuzztest::ReadFilesFromDirectory(GetRunfilesPath(kIamfFileTestPath));
  // We have to return a tuple of string and vector of operations since that is
  // the argument of DoesNotDieWithArbitrarySequenceOfOperations. But we don't
  // have a specific set of operations to initially seed it with, so we leave it
  // as an empty vector. The fuzzer will start there and proceed to add
  // different sequences of operations.
  std::vector<std::tuple<std::string, std::vector<Operation>>> seeds;
  for (const auto& seed_file : files) {
    seeds.push_back({std::get<0>(seed_file), {}});
  }
  return seeds;
}

FUZZ_TEST(IamfDecoderFuzzTest_RandomSequence,
          DoesNotDieWithArbitrarySequenceOfOperations)
    .WithDomains(Arbitrary<std::string>(), fuzztest::VectorOf(AnyOperation()))
    .WithSeeds(GetSeeds);

FUZZ_TEST(IamfDecoderFuzzTest_AllArbitraryParams, DoesNotDieAllParams)
    .WithDomains(AnyRequestedMix(), AnyChannelOrdering(), AnyProfileVersion(),
                 AnyOutputSampleType(), AnyTrimmingSettings(),
                 Arbitrary<std::string>());

}  // namespace
}  // namespace iamf_tools
