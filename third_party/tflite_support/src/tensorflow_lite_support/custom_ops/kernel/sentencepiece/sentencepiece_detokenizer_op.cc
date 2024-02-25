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

#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"
#include "tensorflow_lite_support/custom_ops/kernel/sentencepiece/optimized_decoder.h"
#include "tensorflow_lite_support/custom_ops/kernel/sentencepiece/sentencepiece_detokenizer.h"

namespace tensorflow {
namespace ops {
REGISTER_OP("TFSentencepieceDetokenizeOp")
    .Input("sp_model: uint8")
    .Input("input_values: int32")
    .Input("input_splits: Tsplits")
    .Attr("Tsplits: {int32, int64} = DT_INT64")
    .Output("output: string")
    .SetShapeFn([](tensorflow::shape_inference::InferenceContext* c) {
      shape_inference::ShapeHandle unused;
      TF_TFLITE_RETURN_IF_ERROR(c->WithRank(c->input(1), 1, &unused));
      TF_TFLITE_RETURN_IF_ERROR(c->WithRank(c->input(2), 1, &unused));

      shape_inference::DimensionHandle dim;
      TF_TFLITE_RETURN_IF_ERROR(c->Subtract(c->NumElements(c->input(2)), 1, &dim));
      c->set_output(0, c->Vector(dim));
      return OkStatus();
    });

template <typename Tsplits>
class TFSentencepieceDetokenizerOp : public tensorflow::OpKernel {
 public:
  explicit TFSentencepieceDetokenizerOp(tensorflow::OpKernelConstruction* ctx)
      : OpKernel(ctx) {}
  void Compute(tensorflow::OpKernelContext* ctx) override {
    const auto& model_tensor = ctx->input(kSPModelIndex);
    const auto& input_values_tensor = ctx->input(kInputIndex);
    const auto input_values_flat =
        input_values_tensor.flat<tensorflow::int32>();
    const auto& input_splits_tensor = ctx->input(kInputSplits);
    const auto input_splits_flat = input_splits_tensor.flat<Tsplits>();
    const int num_of_sentences = input_splits_flat.size() - 1;
    Tensor* output_tensor = nullptr;
    OP_REQUIRES_OK(ctx,
                   ctx->allocate_output(0, {num_of_sentences}, &output_tensor));
    auto output_flat = output_tensor->flat<tensorflow::tstring>();
    std::vector<int> codes_for_split;
    int input_offset = 0;
    for (int i = 0; i < num_of_sentences; i++) {
      // Create a vector of int32 from input according to spans.
      const int split_size = input_splits_flat(i + 1) - input_splits_flat(i);
      codes_for_split.clear();
      codes_for_split.reserve(split_size);
      for (int j = 0; j < split_size; ++j) {
        codes_for_split.push_back(input_values_flat(input_offset++));
      }
      const auto res = tflite::ops::custom::sentencepiece::DecodeString(
          codes_for_split, model_tensor.data());
      OP_REQUIRES(
          ctx,
          res.type ==
              tflite::ops::custom::sentencepiece::DecoderResultType::SUCCESS,
          tensorflow::Status(static_cast<tensorflow::errors::Code>(
                                 tensorflow::error::INTERNAL),
                             "Sentencepiece conversion failed"));
      output_flat(i) = res.decoded;
    }
  }
};
}  // namespace ops
}  // namespace tensorflow

REGISTER_KERNEL_BUILDER(
    Name("TFSentencepieceDetokenizeOp")
        .Device(tensorflow::DEVICE_CPU)
        .TypeConstraint<tensorflow::int32>("Tsplits"),
    tensorflow::ops::TFSentencepieceDetokenizerOp<tensorflow::int32>);
REGISTER_KERNEL_BUILDER(
    Name("TFSentencepieceDetokenizeOp")
        .Device(tensorflow::DEVICE_CPU)
        .TypeConstraint<tensorflow::int64>("Tsplits"),
    tensorflow::ops::TFSentencepieceDetokenizerOp<tensorflow::int64>);
