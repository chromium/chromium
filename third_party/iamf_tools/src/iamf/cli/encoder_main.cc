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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/app/adm_to_user_metadata_main_lib.h"
#include "iamf/cli/encoder_main_lib.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/ia_sequence_header.h"
#include "src/google/protobuf/text_format.h"

// Flags to parse input user metadata.
ABSL_FLAG(
    std::string, user_metadata_filename, "",
    "Filename of the proto containing user metadata. It will be read as "
    "a textproto if the file extension is `.txtpb or `.textproto`. It will "
    "be read as a binary proto if the file extension is `.binpb`.  Exactly one "
    "of --adm_filename or --user_metadata_filename must be provided.");
ABSL_FLAG(std::string, input_wav_directory, "",
          "Directory containing the input wav files. Used only if "
          "--user_metadata_filename is provided.");

// Flags to parse input ADM file.
ABSL_FLAG(std::string, adm_filename, "",
          "Filename of the ADM BW64 file to use. Exactly one of --adm_filename "
          "or --user_metadata_filename must be provided.");
ABSL_FLAG(std::string, adm_profile_version, "base",
          "IAMF version to be used: (base/enhanced). Used only if "
          "--adm_filename is provided.");
ABSL_FLAG(int32_t, adm_importance_threshold, 0,
          "Importance value below which an audioObject is omitted from the "
          "generated IAMF metadata. Clamped to [0, 10]. This does not remove "
          "the object's audio: every channel of the input file is still "
          "rendered into the output. Used only if --adm_filename is "
          "provided.");
ABSL_FLAG(int32_t, adm_frame_duration_ms, 10,
          "Target frame duration in milliseconds. The actual frame duration "
          "may vary slightly. Used only if --adm_filename is provided.");

// Flags to control output directory for either type of input.
ABSL_FLAG(std::string, output_iamf_directory, "",
          "Output directory for iamf files");
// TODO(b/349504599): Add support to write output WAV files.

namespace {

// Reads in a user metadata proto from a binary or textproto file.
absl::StatusOr<iamf_tools_cli_proto::UserMetadata> ReadUserMetadataFromFile(
    const std::filesystem::path& user_metadata_filename) {
  std::ifstream user_metadata_file(user_metadata_filename.string(),
                                   std::ios::binary | std::ios::in);
  if (!user_metadata_file) {
    return absl::FailedPreconditionError(
        absl::StrCat("Error loading user_metadata_filename= ",
                     user_metadata_filename.string()));
  }

  std::ostringstream user_metadata_stream;
  user_metadata_stream << user_metadata_file.rdbuf();

  bool is_parse_successful = false;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  if (user_metadata_filename.extension() == ".binpb") {
    is_parse_successful =
        user_metadata.ParseFromString(user_metadata_stream.str());
  } else if (user_metadata_filename.extension() == ".textproto" ||
             user_metadata_filename.extension() == ".txtpb") {
    is_parse_successful = google::protobuf::TextFormat::ParseFromString(
        user_metadata_stream.str(), &user_metadata);
  }

  if (!is_parse_successful) {
    return absl::InvalidArgumentError(
        absl::StrCat("Error parsing proto with user_metadata_filename= ",
                     user_metadata_filename.string()));
  }

  return user_metadata;
}

// Gets a user metadata proto and directory which the encoder will read wav
// files from. The proto may be read directly from a file or be generated based
// on an input ADM file.
absl::StatusOr<iamf_tools_cli_proto::UserMetadata>
GetUserMetadataAndInputWavDirectory(
    const std::filesystem::path& input_user_metadata_filename,
    const std::filesystem::path& adm_filename,
    std::filesystem::path& encoder_input_wav_directory,
    const iamf_tools::ProfileVersion profile_version) {
  if (input_user_metadata_filename.empty() == adm_filename.empty()) {
    return absl::InvalidArgumentError(
        "Please provide exactly one of --user_metadata_filename or "
        "--adm_filename.");
  } else if (!input_user_metadata_filename.empty()) {
    // The user directly provided a proto. Load it from the input file.
    encoder_input_wav_directory =
        absl::GetFlag(FLAGS_input_wav_directory).empty()
            ? std::filesystem::path("iamf/cli/testdata/")
            : std::filesystem::path(absl::GetFlag(FLAGS_input_wav_directory));
    return ReadUserMetadataFromFile(input_user_metadata_filename);
  } else {
    // Generate user metadata and wav files based on the input ADM file.
    std::ifstream adm_file(adm_filename, std::ios::binary | std::ios::in);

    // Wav files associated with each audio object will be written to a
    // temporary directory. The encoder will read back in the wav files from
    // this temporary directory.
    const auto temp_wav_file_directory = std::filesystem::temp_directory_path();
    encoder_input_wav_directory = temp_wav_file_directory;
    return iamf_tools::adm_to_user_metadata::
        GenerateUserMetadataAndSpliceWavFiles(
            std::filesystem::path(adm_filename).stem().string(),
            absl::GetFlag(FLAGS_adm_frame_duration_ms),
            absl::GetFlag(FLAGS_adm_importance_threshold),
            temp_wav_file_directory, adm_file, profile_version);
  }
}

}  // namespace

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);

  // Log the profile version flag.
  using enum iamf_tools::ProfileVersion;
  std::string iamf_profile = absl::GetFlag(FLAGS_adm_profile_version);
  iamf_tools::ProfileVersion profile_version;
  if (iamf_profile == "base") {
    profile_version = kIamfBaseProfile;
  } else if (iamf_profile == "enhanced") {
    profile_version = kIamfBaseEnhancedProfile;
  } else {
    ABSL_LOG(ERROR) << "Invalid profile version: " << iamf_profile;
    return static_cast<int>(absl::StatusCode::kInvalidArgument);
  }
  ABSL_LOG(INFO) << "Using IAMF" << iamf_profile << "profile version.";

  // Prepare `user_metadata` and `input_wav_directory` depending on the
  // input source.
  std::filesystem::path input_wav_directory;
  const auto& user_metadata = GetUserMetadataAndInputWavDirectory(
      absl::GetFlag(FLAGS_user_metadata_filename),
      absl::GetFlag(FLAGS_adm_filename), input_wav_directory, profile_version);
  if (!user_metadata.ok()) {
    ABSL_LOG(ERROR) << user_metadata.status();
    return static_cast<int>(user_metadata.status().code());
  }

  ABSL_LOG(INFO) << user_metadata;

  // Get the directory for the output .iamf files.
  const auto& output_iamf_directory =
      absl::GetFlag(FLAGS_output_iamf_directory).empty()
          ? std::filesystem::temp_directory_path()
          : std::filesystem::path(absl::GetFlag(FLAGS_output_iamf_directory));

  absl::Status status =
      iamf_tools::TestMain(*user_metadata, input_wav_directory.string(),
                           output_iamf_directory.string());

  // Log success or failure. Success is defined as a valid test vector returning
  // `absl::OkStatus()` or an invalid test vector returning a different status.
  const bool test_vector_is_valid =
      user_metadata->test_vector_metadata().is_valid();
  std::stringstream ss;
  ss << "Test case expected to " << (test_vector_is_valid ? "pass" : "fail")
     << ".\nstatus= " << status;
  if (test_vector_is_valid == status.ok()) {
    ABSL_LOG(INFO) << "Success. " << ss.str();
  } else {
    ABSL_LOG(ERROR) << "Failure. " << ss.str();
  }

  return static_cast<int>(status.code());
}
