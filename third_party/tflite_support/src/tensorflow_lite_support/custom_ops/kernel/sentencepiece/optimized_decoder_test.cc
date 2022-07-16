/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/custom_ops/kernel/sentencepiece/optimized_decoder.h"

#include <fstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/sentencepiece.pb.h"
#include "src/sentencepiece_processor.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow_lite_support/custom_ops/kernel/sentencepiece/model_converter.h"

namespace tflite {
namespace ops {
namespace custom {
namespace sentencepiece {

namespace internal {

tensorflow::Status TFReadFileToString(const std::string& filepath,
                                      std::string* data) {
  return tensorflow::ReadFileToString(tensorflow::Env::Default(),
                                      /*test_path*/ filepath, data);
}

absl::Status StdReadFileToString(const std::string& filepath,
                                 std::string* data) {
  std::ifstream infile(filepath);
  if (!infile.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Error when opening %s", filepath));
  }
  std::string contents((std::istreambuf_iterator<char>(infile)),
                       (std::istreambuf_iterator<char>()));
  data->append(contents);
  infile.close();
  return absl::OkStatus();
}

}  // namespace internal

namespace {
static char kConfigFilePath[] =
    "tensorflow_lite_support/custom_ops/kernel/"
    "sentencepiece/testdata/sentencepiece.model";

TEST(OptimizedEncoder, ConfigConverter) {
  std::string config;
  auto status = internal::StdReadFileToString(kConfigFilePath, &config);

  ASSERT_TRUE(status.ok());

  ::sentencepiece::SentencePieceProcessor processor;
  ASSERT_OK(processor.LoadFromSerializedProto(config));
  const auto converted_model = ConvertSentencepieceModelForDecoder(config);
  const std::string test_string("Hello world!\\xF0\\x9F\\x8D\\x95");
  ::sentencepiece::SentencePieceText reference_encoded;
  CHECK_OK(processor.Encode(test_string, &reference_encoded));

  std::vector<int> encoded_vector;
  encoded_vector.reserve(reference_encoded.pieces_size());
  for (const auto& piece : reference_encoded.pieces()) {
    encoded_vector.push_back(piece.id());
  }
  std::string ref_decoded;
  ASSERT_OK(processor.Decode(encoded_vector, &ref_decoded));
  const auto decoded = DecodeString(encoded_vector, converted_model.data());
  ASSERT_EQ(decoded.type, DecoderResultType::SUCCESS);
  ASSERT_EQ(ref_decoded, decoded.decoded);
}
}  // namespace

}  // namespace sentencepiece
}  // namespace custom
}  // namespace ops
}  // namespace tflite
