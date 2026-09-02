/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/include/iamf_tools/iamf_encoder_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/iamf_components.h"
#include "iamf/cli/iamf_encoder.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/cli/obu_sequencer_iamf.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/include/iamf_tools/iamf_encoder_interface.h"

namespace iamf_tools {
namespace api {

absl::StatusOr<std::unique_ptr<IamfEncoderInterface>>
IamfEncoderFactory::CreateFileGeneratingIamfEncoder(
    absl::string_view serialized_user_metadata,
    absl::string_view output_file_name) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  if (!user_metadata.ParseFromString(serialized_user_metadata)) {
    return absl::InvalidArgumentError(
        "Failed to deserialize a `UserMetadata` protocol buffer.");
  }

  // Create an encoder, which is pre-configured with enough functionality to
  // measure loudness and generate the output file.
  const auto leb_generator =
      CreateLebGenerator(user_metadata.test_vector_metadata().leb_generator());
  if (leb_generator == nullptr) {
    return absl::InvalidArgumentError(
        "Invalid LebGenerator settings in user metadata.");
  }
  const IamfEncoder::ObuSequencerFactory obu_sequencer_factory = [&]() {
    std::vector<std::unique_ptr<ObuSequencerBase>> obu_sequencers;
    obu_sequencers.push_back(std::make_unique<ObuSequencerIamf>(
        std::string(output_file_name),
        user_metadata.temporal_delimiter_metadata()
            .enable_temporal_delimiters(),
        *leb_generator));
    return obu_sequencers;
  };
  return IamfEncoder::Create(
      user_metadata, CreateLayoutRendererFactory().get(),
      CreateLoudnessCalculatorFactory().get(),
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      obu_sequencer_factory);
}

absl::StatusOr<std::unique_ptr<IamfEncoderInterface>>
IamfEncoderFactory::CreateIamfEncoder(
    absl::string_view serialized_user_metadata) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  if (!user_metadata.ParseFromString(serialized_user_metadata)) {
    return absl::InvalidArgumentError(
        "Failed to deserialize a `UserMetadata` protocol buffer.");
  }

  // Create an encoder, which is pre-configured with enough functionality to
  // measure loudness.
  return IamfEncoder::Create(
      user_metadata, CreateLayoutRendererFactory().get(),
      CreateLoudnessCalculatorFactory().get(),
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      IamfEncoder::CreateNoObuSequencers);
}

}  // namespace api
}  // namespace iamf_tools
