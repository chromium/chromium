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

#include "tensorflow_lite_support/custom_ops/kernel/whitespace_tokenizer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "libutf/utf.h"
#include "tensorflow/lite/context.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/string_util.h"

constexpr int kInput = 0;
constexpr int kOutputValues = 0;
constexpr int kOutputRowSplitsStart = 1;

namespace tflite {
namespace ops {
namespace custom {
namespace whitespace_tokenizer {

// This TFLite op implements a whitespace tokenizer, and can output the
// tokens as either a padded tensor or a ragged tensor.
//
// If we're outputting a padded tensor, our outputs are:
// * A string tensor
//
// If we're outputting a ragged tensor, our outputs are:
// * A string tensor (the innermost values of the ragged tensor)
// * N int64 tensors (the row_splits of the ragged tensor, where N is the
//   rank of the input tensor)

inline bool OutputIsPaddedTensor(TfLiteNode* node) {
  return NumOutputs(node) == 1;
}

inline int charntorune(Rune* r, const char* s, int n) {
  const int bytes_read = chartorune(r, const_cast<char*>(s));
  if (bytes_read > n) {
    *r = Runeerror;
    return 0;
  }
  return bytes_read;
}

std::vector<std::pair<const char*, int>> Tokenize(StringRef str) {
  const char* p = str.str;
  int n = str.len;

  std::vector<std::pair<const char*, int>> tokens;
  const char* start = nullptr;
  while (n > 0) {
    Rune r;
    int c = charntorune(&r, p, n);
    if (r == Runeerror)
      break;

    if (isspacerune(r)) {
      if (start != nullptr) {
        tokens.push_back({start, p - start});
      }
      start = nullptr;
    } else {
      if (start == nullptr) {
        start = p;
      }
    }

    p += c;
    n -= c;
  }
  if (start != nullptr) {
    tokens.push_back({start, p - start});
  }

  return tokens;
}

TfLiteStatus WritePaddedOutput(
    const std::vector<std::vector<std::pair<const char*, int>>>& list_of_tokens,
    const TfLiteTensor* input,
    TfLiteTensor* output_values) {
  TfLiteIntArray* output_shape = TfLiteIntArrayCreate(NumDimensions(input) + 1);
  for (int i = 0; i < NumDimensions(input); ++i) {
    output_shape->data[i] = SizeOfDimension(input, i);
  }

  size_t max_tokens = 0;
  for (const auto& tokens : list_of_tokens) {
    max_tokens = std::max(max_tokens, tokens.size());
  }

  output_shape->data[NumDimensions(input)] = max_tokens;
  DynamicBuffer buffer;
  for (const auto& tokens : list_of_tokens) {
    for (const auto& token : tokens) {
      buffer.AddString(token.first, token.second);
    }
    for (int i = tokens.size(); i < max_tokens; ++i) {
      buffer.AddString(nullptr, 0);
    }
  }
  buffer.WriteToTensor(output_values, output_shape);
  return kTfLiteOk;
}

TfLiteStatus WriteRaggedOutput(
    const std::vector<std::vector<std::pair<const char*, int>>>& list_of_tokens,
    const TfLiteTensor* input,
    TfLiteTensor* output_values,
    std::vector<TfLiteTensor*> nested_row_splits) {
  // The outer dimensions of the ragged tensor are all non-ragged.
  for (int i = 0; i < nested_row_splits.size() - 1; ++i) {
    int row_splits_step = SizeOfDimension(input, i + 1);
    TfLiteTensor* row_splits = nested_row_splits[i];
    for (int j = 0; j < SizeOfDimension(row_splits, 0); ++j) {
      row_splits->data.i64[j] = j * row_splits_step;
    }
  }

  // Generate the innermost row_splits and values tensors.
  TfLiteTensor* row_splits = nested_row_splits.back();
  TfLiteIntArray* output_shape = TfLiteIntArrayCreate(1);
  DynamicBuffer buffer;
  int token_index = 0;
  int row_splits_index = 0;
  for (const auto& tokens : list_of_tokens) {
    row_splits->data.i64[row_splits_index] = token_index;
    for (const auto& token : tokens) {
      buffer.AddString(token.first, token.second);
      ++token_index;
    }
    ++row_splits_index;
  }
  row_splits->data.i64[row_splits_index] = token_index;
  output_shape->data[0] = token_index;
  buffer.WriteToTensor(output_values, output_shape);
  return kTfLiteOk;
}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  TfLiteTensor* output_values = GetOutput(context, node, kOutputValues);
  SetTensorToDynamic(output_values);

  if (OutputIsPaddedTensor(node)) {
    return kTfLiteOk;
  }

  const TfLiteTensor* input = GetInput(context, node, kInput);
  TF_LITE_ENSURE(context, NumDimensions(input) ==
                              (NumOutputs(node) - kOutputRowSplitsStart));

  // Resize the row_splits tensors.  We're just adding a ragged inner
  // dimension to the shape of the input tensor, so the size of the
  // row_splits tensors can be calculated using the input tensor's shape.
  int input_size = 1;
  for (int i = 0; i < NumDimensions(input); ++i) {
    input_size *= SizeOfDimension(input, i);

    TfLiteIntArray* row_splits_shape = TfLiteIntArrayCreate(1);
    row_splits_shape->data[0] = input_size + 1;
    TfLiteTensor* row_splits =
        GetOutput(context, node, kOutputRowSplitsStart + i);
    TF_LITE_ENSURE_STATUS(
        context->ResizeTensor(context, row_splits, row_splits_shape));
  }

  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor* input = GetInput(context, node, kInput);
  int input_size = 1;
  for (int i = 0; i < NumDimensions(input); ++i) {
    input_size *= SizeOfDimension(input, i);
  }

  std::vector<std::vector<std::pair<const char*, int>>> list_of_tokens;
  list_of_tokens.reserve(input_size);
  for (int i = 0; i < input_size; ++i) {
    list_of_tokens.emplace_back(Tokenize(GetString(input, i)));
  }

  TfLiteTensor* output_values = GetOutput(context, node, kOutputValues);
  TF_LITE_ENSURE(context, IsDynamicTensor(output_values));

  if (OutputIsPaddedTensor(node)) {
    return WritePaddedOutput(list_of_tokens, input, output_values);
  }

  std::vector<TfLiteTensor*> nested_row_splits;
  nested_row_splits.reserve(NumDimensions(input));
  for (int i = 0; i < NumDimensions(input); ++i) {
    TfLiteTensor* output_row_splits =
        GetOutput(context, node, kOutputRowSplitsStart + i);
    nested_row_splits.push_back(output_row_splits);
  }
  return WriteRaggedOutput(list_of_tokens, input, output_values,
                           nested_row_splits);
}

}  // namespace whitespace_tokenizer

TfLiteRegistration* Register_tftext_WhitespaceTokenizer() {
  static TfLiteRegistration r = {nullptr, nullptr,
                                 whitespace_tokenizer::Prepare,
                                 whitespace_tokenizer::Eval};
  return &r;
}

}  // namespace custom
}  // namespace ops
}  // namespace tflite
