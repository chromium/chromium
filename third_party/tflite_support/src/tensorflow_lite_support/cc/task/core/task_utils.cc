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

#include "tensorflow_lite_support/cc/task/core/task_utils.h"

#include <fstream>

#include "absl/strings/str_cat.h"  // from @com_google_absl

namespace tflite {
namespace task {
namespace core {

double Dequantize(const TfLiteTensor& tensor, int index) {
  int32_t quantized_value = 0;
  switch (tensor.type) {
    case kTfLiteUInt8:
      quantized_value = GetTensorData<uint8_t>(&tensor)[index];
      break;
    case kTfLiteInt8:
      quantized_value = GetTensorData<int8_t>(&tensor)[index];
      break;
    case kTfLiteInt16:
      quantized_value = GetTensorData<int16_t>(&tensor)[index];
      break;
    default:
      TF_LITE_FATAL(
          absl::StrCat(
              "Invalid tensor type for dequantization ", tensor.name,
              ". Requested kTfLiteUInt8, kTfLiteInt8 or kTfLiteInt16, got ",
              TfLiteTypeGetName(tensor.type), ".")
              .c_str());
  }
  return tensor.params.scale * (quantized_value - tensor.params.zero_point);
}

std::string GetStringAtIndex(const TfLiteTensor* labels, int index) {
  const auto& strref = tflite::GetString(labels, index);
  return std::string(strref.str, strref.len);
}

std::string LoadBinaryContent(const char* filename) {
  std::ifstream input_file(filename, std::ios::binary | std::ios::ate);
  // Find buffer size from input file, and load the buffer.
  size_t buffer_size = input_file.tellg();
  std::string buffer(buffer_size, '\0');
  input_file.seekg(0, std::ios::beg);
  input_file.read(const_cast<char*>(buffer.c_str()), buffer_size);
  return buffer;
}

int FindTensorIndexByMetadataName(
    const flatbuffers::Vector<flatbuffers::Offset<TensorMetadata>>*
        tensor_metadatas,
    absl::string_view name) {
  if (tensor_metadatas == nullptr) {
    return -1;
  }
  for (int i = 0; i < tensor_metadatas->size(); i++) {
    if (name == tensor_metadatas->Get(i)->name()->c_str()) {
      return i;
    }
  }
  // Returns -1 if not found.
  return -1;
}

}  // namespace core
}  // namespace task
}  // namespace tflite
