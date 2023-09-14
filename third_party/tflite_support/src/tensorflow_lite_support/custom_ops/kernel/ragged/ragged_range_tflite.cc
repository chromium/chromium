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
#include <algorithm>
#include <functional>

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/context.h"
#include "tensorflow/lite/kernels/internal/tensor.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow/lite/kernels/internal/types.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/model.h"

namespace tflite {
namespace ops {
namespace custom {
namespace ragged {
namespace ragged_range {
namespace {
constexpr int kInputStarts = 0;
constexpr int kInputLimits = 1;
constexpr int kInputDeltas = 2;

constexpr int kOutputNestedSplits = 0;
constexpr int kOutputDenseValues = 1;

TfLiteIntArray* IntArrayFromInt(int x) {
  TfLiteIntArray* result = TfLiteIntArrayCreate(1);
  result->data[0] = x;
  return result;
}

// Returns the number of elements in the specified range.
template <typename T, typename SPLITS_TYPE>
SPLITS_TYPE RangeSize(T start, T limit, T delta) {
  if (((delta > 0) && (limit < start)) || ((delta < 0) && (limit > start))) {
    return 0;
  }
  // The following is copied from tensorflow::RangeOp::Compute().
  return (
      std::is_integral<T>::value
          ? ((std::abs(limit - start) + std::abs(delta) - 1) / std::abs(delta))
          : std::ceil(std::abs((limit - start) / delta)));
}

template <typename T, typename SPLITS_TYPE>
TfLiteStatus EvalT(TfLiteContext* context, TfLiteNode* node) {
  TfLiteTensor& input_starts =
      context->tensors[node->inputs->data[kInputStarts]];
  TfLiteTensor& input_limits =
      context->tensors[node->inputs->data[kInputLimits]];
  TfLiteTensor& input_deltas =
      context->tensors[node->inputs->data[kInputDeltas]];
  // Determine which tensors we need to broadcast.
  const bool broadcast_starts = NumElements(&input_starts) == 1;
  const bool broadcast_limits = NumElements(&input_limits) == 1;
  const bool broadcast_deltas = NumElements(&input_deltas) == 1;

  // nrows (number of output rows) is the size of the non-broadcast inputs,
  // or 1 if all inputs are scalars.
  std::vector<int> in_sizes;
  if (!broadcast_starts) in_sizes.push_back(input_starts.dims->data[0]);
  if (!broadcast_limits) in_sizes.push_back(input_limits.dims->data[0]);
  if (!broadcast_deltas) in_sizes.push_back(input_deltas.dims->data[0]);
  if (std::adjacent_find(std::begin(in_sizes), std::end(in_sizes),
                         std::not_equal_to<>()) != std::end(in_sizes)) {
    context->ReportError(
        context,
        "Invalid argument: starts, limits, and deltas must have the "
        "same shape");
    return kTfLiteError;
  }

  const SPLITS_TYPE nrows = in_sizes.empty() ? 1 : in_sizes.front();

  const T* starts = GetTensorData<T>(&input_starts);
  const T* limits = GetTensorData<T>(&input_limits);
  const T* deltas = GetTensorData<T>(&input_deltas);

  TfLiteTensor& rt_nested_splits_out =
      context->tensors[node->outputs->data[kOutputNestedSplits]];
  TF_LITE_ENSURE_OK(context,
                    context->ResizeTensor(context, &rt_nested_splits_out,
                                          IntArrayFromInt(nrows + 1)));
  SPLITS_TYPE* rt_nested_splits =
      GetTensorData<SPLITS_TYPE>(&rt_nested_splits_out);
  rt_nested_splits[0] = 0;

  for (int row = 0; row < nrows; ++row) {
    const T start = broadcast_starts ? starts[0] : starts[row];
    const T limit = broadcast_limits ? limits[0] : limits[row];
    const T delta = broadcast_deltas ? deltas[0] : deltas[row];
    if (delta == 0) {
      context->ReportError(context, "Invalid argument: Requires delta != 0");
      return kTfLiteError;
    }
    rt_nested_splits[row + 1] =
        rt_nested_splits[row] + RangeSize<T, SPLITS_TYPE>(start, limit, delta);
  }
  const SPLITS_TYPE nvals = rt_nested_splits[nrows];

  TfLiteTensor& rt_dense_values_out =
      context->tensors[node->outputs->data[kOutputDenseValues]];
  TF_LITE_ENSURE_OK(context,
                    context->ResizeTensor(context, &rt_dense_values_out,
                                          IntArrayFromInt(nvals)));
  T* rt_dense_values = GetTensorData<T>(&rt_dense_values_out);
  int value_index = 0;
  for (int row = 0; row < nrows; ++row) {
    const SPLITS_TYPE row_size =
        rt_nested_splits[row + 1] - rt_nested_splits[row];
    T value = broadcast_starts ? starts[0] : starts[row];
    const T delta = broadcast_deltas ? deltas[0] : deltas[row];
    for (SPLITS_TYPE i = 0; i < row_size; ++i) {
      rt_dense_values[value_index++] = value;
      value += delta;
    }
  }
  return kTfLiteOk;
}

template <typename SPLITS_TYPE>
TfLiteStatus EvalSplitsT(TfLiteContext* context, TfLiteNode* node) {
  TfLiteTensor& rt_dense_values_out =
      context->tensors[node->outputs->data[kOutputDenseValues]];
  switch (rt_dense_values_out.type) {
    case kTfLiteInt32:
      return EvalT<int32_t, SPLITS_TYPE>(context, node);
    case kTfLiteInt64:
      return EvalT<int64_t, SPLITS_TYPE>(context, node);
    case kTfLiteFloat32:
      return EvalT<float, SPLITS_TYPE>(context, node);
    case kTfLiteFloat64:
      return EvalT<double, SPLITS_TYPE>(context, node);
    default:
      context->ReportError(context,
                           "Invalid argument: Not supported VALUES type");
      return kTfLiteError;
  }
}
}  // namespace

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  // Set outputs dynamic.
  TfLiteTensor& nested_splits =
      context->tensors[node->outputs->data[kOutputNestedSplits]];
  SetTensorToDynamic(&nested_splits);
  TfLiteTensor& dense_values =
      context->tensors[node->outputs->data[kOutputDenseValues]];
  SetTensorToDynamic(&dense_values);
  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  TfLiteTensor& rt_nested_splits_out =
      context->tensors[node->outputs->data[kOutputNestedSplits]];
  switch (rt_nested_splits_out.type) {
    case kTfLiteInt32:
      return EvalSplitsT<int32_t>(context, node);
    case kTfLiteInt64:
      return EvalSplitsT<int64_t>(context, node);
    default:
      context->ReportError(context,
                           "Invalid argument: Not supported ROW_SPLITS type");
      return kTfLiteError;
  }
}

}  // namespace ragged_range
}  // namespace ragged
TfLiteRegistration* Register_RAGGED_RANGE() {
  static TfLiteRegistration r = {nullptr /*Initialize*/, nullptr /*Free*/,
                                 ragged::ragged_range::Prepare,
                                 ragged::ragged_range::Eval};
  return &r;
}

}  // namespace custom
}  // namespace ops
}  // namespace tflite
