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

#include "tensorflow_lite_support/custom_ops/kernel/ngrams.h"

#include "flatbuffers/flexbuffers.h"  // from @flatbuffers
#include "tensorflow/lite/context.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/string_util.h"

namespace tflite {
namespace ops {
namespace custom {
namespace ngrams {

// This TFLite op implements the text.ngrams when reduction_type = STRING_JOIN.
//
// Input:
// * data: A string tensor, or a ragged string tensor (a 1D string value tensor
//     and one or more 1D int64 row_split tensors).
//
// Attributes:
// * width:             scalar integer
//     The width of the ngram window.
// * axis:              scalar integer
//     The axis to create ngrams along.  For STRING_JOIN, this must be -1.
// * reduction_type:    scalar string
//     A string corresponding to the name of an enum value of text.Reduction
//     Currently, only STRING_JOIN is supported.
// * string_separator:  scalar string
//     The separator string used to join tokens together.
//
// Output:
// * output: A string tensor that matches the rank of 'data'.  Will be a ragged
//     tensor if 'data' is a ragged tensor.

// Both the input and output tensors use the same indices.
constexpr int kValues = 0;
constexpr int kRowSplitsStart = 1;

// Reduction types.
constexpr char kStringJoin[] = "STRING_JOIN";

struct NgramsAttributes {
  int width;
  int axis;
  std::string reduction_type;
  std::string string_separator;

  explicit NgramsAttributes(const flexbuffers::Map& m)
      : width(m["width"].AsInt32()),
        axis(m["axis"].AsInt32()),
        reduction_type(m["reduction_type"].ToString()),
        string_separator(m["string_separator"].ToString()) {}
};

inline bool OutputIsTensor(TfLiteNode* node) { return NumOutputs(node) == 1; }
inline int NumRowSplits(TfLiteNode* node) {
  return NumInputs(node) - kRowSplitsStart;
}

void* Init(TfLiteContext* context, const char* buffer, size_t length) {
  const uint8_t* buffer_t = reinterpret_cast<const uint8_t*>(buffer);
  return new NgramsAttributes(flexbuffers::GetRoot(buffer_t, length).AsMap());
}

void Free(TfLiteContext* context, void* buffer) {
  delete reinterpret_cast<NgramsAttributes*>(buffer);
}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  const auto& attributes =
      *reinterpret_cast<NgramsAttributes*>(node->user_data);

  TF_LITE_ENSURE(context, attributes.reduction_type == kStringJoin);
  TF_LITE_ENSURE(context, attributes.axis == -1);

  TfLiteTensor* output_values = GetOutput(context, node, kValues);
  if (OutputIsTensor(node)) {
    const TfLiteTensor* input_values = GetInput(context, node, kValues);
    int values_num_dims = NumDimensions(input_values);
    TfLiteIntArray* output_values_shape = TfLiteIntArrayCreate(values_num_dims);
    for (int i = 0; i < values_num_dims; ++i) {
      output_values_shape->data[i] = SizeOfDimension(input_values, i);
    }
    output_values_shape->data[values_num_dims - 1] =
        std::max(0, SizeOfDimension(input_values, values_num_dims - 1) -
                        attributes.width + 1);
    TF_LITE_ENSURE_OK(context, context->ResizeTensor(context, output_values,
                                                     output_values_shape));
    return kTfLiteOk;
  }

  SetTensorToDynamic(output_values);
  // The row_splits tensors maintain their shape, because only the
  // innermost dimension will change.
  for (int i = kRowSplitsStart; i < NumOutputs(node); ++i) {
    const TfLiteTensor* input_row_splits = GetInput(context, node, i);
    TfLiteTensor* output_row_splits = GetOutput(context, node, i);
    TF_LITE_ENSURE_EQ(context, NumDimensions(input_row_splits), 1);
    TfLiteIntArray* output_row_splits_shape = TfLiteIntArrayCreate(1);
    output_row_splits_shape->data[0] = SizeOfDimension(input_row_splits, 0);
    TF_LITE_ENSURE_OK(context, context->ResizeTensor(context, output_row_splits,
                                                     output_row_splits_shape));
  }
  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  const auto& attributes =
      *reinterpret_cast<NgramsAttributes*>(node->user_data);

  // Storage for the dummy input and output row_splits used in the tensor case.
  std::vector<int64_t> tensor_input_row_splits;
  std::vector<int64_t> tensor_output_row_splits;

  const int64_t* input_row_splits;
  int64_t* output_row_splits;
  int n_row_splits = 0;

  const TfLiteTensor* input_values = GetInput(context, node, kValues);

  if (OutputIsTensor(node)) {
    // Generate mock input and output innermost row_splits.
    int64_t total_tokens = NumElements(input_values);
    int64_t tokens_per_element =
        SizeOfDimension(input_values, NumDimensions(input_values) - 1);
    tensor_input_row_splits.reserve(total_tokens / tokens_per_element + 1);
    tensor_output_row_splits.resize(total_tokens / tokens_per_element + 1);
    for (int64_t i = 0; i <= total_tokens; i += tokens_per_element) {
      tensor_input_row_splits.push_back(i);
    }
    input_row_splits = tensor_input_row_splits.data();
    output_row_splits = tensor_output_row_splits.data();
    n_row_splits = tensor_input_row_splits.size();
  } else {
    int index = 0;
    while (index < NumRowSplits(node) - 1) {
      const TfLiteTensor* input_tensor_row_splits =
          GetInput(context, node, kRowSplitsStart + index);
      TfLiteTensor* output_tensor_row_splits =
          GetOutput(context, node, kRowSplitsStart + index);
      memcpy(output_tensor_row_splits->data.raw,
             input_tensor_row_splits->data.raw, input_tensor_row_splits->bytes);
      ++index;
    }

    const TfLiteTensor* input_tensor_row_splits =
        GetInput(context, node, kRowSplitsStart + index);
    TfLiteTensor* output_tensor_row_splits =
        GetOutput(context, node, kRowSplitsStart + index);
    input_row_splits = input_tensor_row_splits->data.i64;
    output_row_splits = output_tensor_row_splits->data.i64;
    n_row_splits = SizeOfDimension(input_tensor_row_splits, 0);
  }

  DynamicBuffer buffer;
  StringRef separator;
  separator.str = attributes.string_separator.c_str();
  separator.len = attributes.string_separator.length();
  int buffer_index = 0;
  for (int i = 0; i < n_row_splits - 1; ++i) {
    output_row_splits[i] = buffer_index;
    std::vector<StringRef> tokens;
    for (int j = input_row_splits[i]; j < input_row_splits[i + 1]; ++j) {
      tokens.emplace_back(GetString(input_values, j));
      if (tokens.size() < attributes.width) continue;
      tokens.erase(tokens.begin(),
                   tokens.begin() + tokens.size() - attributes.width);
      buffer.AddJoinedString(tokens, separator);
      ++buffer_index;
    }
  }
  output_row_splits[n_row_splits - 1] = buffer_index;

  TfLiteTensor* output_values = GetOutput(context, node, kValues);
  if (OutputIsTensor(node)) {
    buffer.WriteToTensor(output_values, /*new_shape=*/nullptr);
  } else {
    buffer.WriteToTensorAsVector(output_values);
  }

  return kTfLiteOk;
}

}  // namespace ngrams

TfLiteRegistration* Register_tftext_Ngrams() {
  static TfLiteRegistration r = {ngrams::Init, ngrams::Free, ngrams::Prepare,
                                 ngrams::Eval};
  return &r;
}

}  // namespace custom
}  // namespace ops
}  // namespace tflite
