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

#ifndef TENSORFLOW_LITE_SUPPORT_CC_TASK_CORE_TASK_UTILS_H_
#define TENSORFLOW_LITE_SUPPORT_CC_TASK_CORE_TASK_UTILS_H_

#include <algorithm>
#include <cstring>
#include <numeric>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "flatbuffers/flatbuffers.h"  // from @flatbuffers
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow/lite/kernels/op_macros.h"
#include "tensorflow/lite/string_util.h"
#include "tensorflow/lite/type_to_tflitetype.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace core {

// Checks if data type of tensor is T and returns the pointer casted to T if
// applicable, returns nullptr if tensor type is not T.
// See type_to_tflitetype.h for a mapping from plain C++ type to TfLiteType.
template <typename T>
T* TypedTensor(const TfLiteTensor* tensor_ptr) {
  if (tensor_ptr->type == typeToTfLiteType<T>()) {
    return reinterpret_cast<T*>(tensor_ptr->data.raw);
  }
  return nullptr;
}

// Checks and returns type of a tensor, fails if tensor type is not T.
template <typename T>
T* AssertAndReturnTypedTensor(const TfLiteTensor* tensor) {
  if (T* v = TypedTensor<T>(tensor))
    return v;
  // TODO(b/150903834): throw exceptions instead
  TF_LITE_ASSERT(tensor->data.raw);
  TF_LITE_FATAL(absl::StrCat("Type mismatch for tensor ", tensor->name,
                             ". Requested ",
                             TfLiteTypeGetName(typeToTfLiteType<T>()), ", got ",
                             TfLiteTypeGetName(tensor->type), ".")
                    .c_str());
}

// Populates tensor with array of data, fails if data type doesn't match tensor
// type or has not the same number of elements.
template <typename T>
inline void PopulateTensor(const T* data,
                           int num_elements,
                           TfLiteTensor* tensor) {
  T* v = AssertAndReturnTypedTensor<T>(tensor);
  size_t bytes = num_elements * sizeof(T);
  // TODO(b/150903834): throw exceptions instead
  TF_LITE_ASSERT(tensor->bytes == bytes);
  memcpy(v, data, bytes);
}

// Populates tensor with vector of data, fails if data type doesn't match tensor
// type or has not the same number of elements.
template <typename T>
inline void PopulateTensor(const std::vector<T>& data, TfLiteTensor* tensor) {
  return PopulateTensor<T>(data.data(), data.size(), tensor);
}

template <>
inline void PopulateTensor<std::string>(const std::vector<std::string>& data,
                                        TfLiteTensor* tensor) {
  if (tensor->type != kTfLiteString) {
    TF_LITE_FATAL(absl::StrCat("Type mismatch for tensor ", tensor->name,
                               ". Requested STRING, got ",
                               TfLiteTypeGetName(tensor->type), ".")
                      .c_str());
  }
  tflite::DynamicBuffer input_buf;
  for (const auto& value : data) {
    input_buf.AddString(value.data(), value.length());
  }
  input_buf.WriteToTensorAsVector(tensor);
}

// Populates tensor one data item, fails if data type doesn't match tensor
// type.
template <typename T>
inline void PopulateTensor(const T& data, TfLiteTensor* tensor) {
  T* v = AssertAndReturnTypedTensor<T>(tensor);
  *v = data;
}

template <>
inline void PopulateTensor<std::string>(const std::string& data,
                                        TfLiteTensor* tensor) {
  tflite::DynamicBuffer input_buf;
  input_buf.AddString(data.data(), data.length());
  input_buf.WriteToTensorAsVector(tensor);
}

// Populates a vector from the tensor, fails if data type doesn't match tensor
// type.
template <typename T>
inline void PopulateVector(const TfLiteTensor* tensor, std::vector<T>* data) {
  AssertAndReturnTypedTensor<T>(tensor);
  const T* results = GetTensorData<T>(tensor);
  size_t num = tensor->bytes / sizeof(tensor->type);
  data->reserve(num);
  for (size_t i = 0; i < num; i++) {
    data->emplace_back(results[i]);
  }
}

template <>
inline void PopulateVector<std::string>(const TfLiteTensor* tensor,
                                        std::vector<std::string>* data) {
  AssertAndReturnTypedTensor<std::string>(tensor);
  int num = GetStringCount(tensor);
  data->reserve(num);
  for (int i = 0; i < num; i++) {
    const auto& strref = tflite::GetString(tensor, i);
    data->emplace_back(strref.str, strref.len);
  }
}

// Returns the reversely sorted indices of a vector.
template <typename T>
std::vector<size_t> ReverseSortIndices(const std::vector<T>& v) {
  std::vector<size_t> idx(v.size());
  std::iota(idx.begin(), idx.end(), 0);

  std::stable_sort(idx.begin(), idx.end(),
                   [&v](size_t i1, size_t i2) { return v[i2] < v[i1]; });

  return idx;
}

// Returns the original (dequantized) value of the 'index'-th element of
// 'tensor.
double Dequantize(const TfLiteTensor& tensor, int index);

// Returns the index-th string from the tensor.
std::string GetStringAtIndex(const TfLiteTensor* labels, int index);

// Loads binary content of a file into a string.
std::string LoadBinaryContent(const char* filename);

// Gets the tensor from a vector of tensors with name specified inside metadata.
template <typename TensorType>
static TensorType* FindTensorByName(
    const std::vector<TensorType*>& tensors,
    const flatbuffers::Vector<flatbuffers::Offset<TensorMetadata>>*
        tensor_metadatas,
    const std::string& name) {
  if (tensor_metadatas == nullptr ||
      tensor_metadatas->size() != tensors.size()) {
    return nullptr;
  }
  for (flatbuffers::uoffset_t i = 0; i < tensor_metadatas->size(); i++) {
    if (strcmp(name.data(), tensor_metadatas->Get(i)->name()->c_str()) == 0) {
      return tensors[i];
    }
  }
  return nullptr;
}

}  // namespace core
}  // namespace task
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_TASK_CORE_TASK_UTILS_H_
