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

#include <iterator>
#include <vector>

#include "tensorflow_lite_support/custom_ops/kernel/sentencepiece/optimized_encoder.h"
#include "tensorflow_lite_support/custom_ops/kernel/sentencepiece/sentencepiece_tokenizer.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"

namespace tensorflow {
namespace ops{

// copied from third_party/tensorflow_text/core/ops/sentencepiece_ops.cc
REGISTER_OP("TFSentencepieceTokenizeOp")
    .Input("sp_model: uint8")
    .Input("input: string")
    .Input("nbest_size: int32")
    .Input("alpha: float")
    .Input("add_bos: bool")
    .Input("add_eos: bool")
    .Input("reverse: bool")
    .Attr("out_type: {int32, string} = DT_INT32")
    .Attr("Tsplits: {int32, int64} = DT_INT32")
    .Output("output_values: out_type")
    .Output("output_splits: Tsplits")
    .SetShapeFn([](tensorflow::shape_inference::InferenceContext* c) {
      tensorflow::shape_inference::ShapeHandle unused;
      TF_TFLITE_RETURN_IF_ERROR(c->WithRank(c->input(1), 1, &unused));
      TF_TFLITE_RETURN_IF_ERROR(c->WithRank(c->input(2), 0, &unused));
      TF_TFLITE_RETURN_IF_ERROR(c->WithRank(c->input(3), 0, &unused));
      TF_TFLITE_RETURN_IF_ERROR(c->WithRank(c->input(4), 0, &unused));
      TF_TFLITE_RETURN_IF_ERROR(c->WithRank(c->input(5), 0, &unused));
      TF_TFLITE_RETURN_IF_ERROR(c->WithRank(c->input(6), 0, &unused));

      c->set_output(
          0, c->Vector(
                 tensorflow::shape_inference::InferenceContext::kUnknownDim));

      tensorflow::shape_inference::DimensionHandle num_splits;
      TF_TFLITE_RETURN_IF_ERROR(c->Add(c->NumElements(c->input(1)), 1, &num_splits));
      c->set_output(1, c->Vector(num_splits));
      return tensorflow::OkStatus();
    });

class TFSentencepieceOp : public tensorflow::OpKernel {
 public:
  explicit TFSentencepieceOp(tensorflow::OpKernelConstruction* ctx)
      : OpKernel(ctx) {}
  void Compute(tensorflow::OpKernelContext* ctx) override {
    const auto& model_tensor = ctx->input(kSPModelIndex);
    const auto& input_values_tensor = ctx->input(kInputIndex);
    const auto input_values_flat =
        input_values_tensor.flat<tensorflow::tstring>();
    const int num_of_input_values = input_values_flat.size();

    const auto& add_bos_tensor = ctx->input(kAddBOSInput);
    const bool add_bos = add_bos_tensor.scalar<bool>()();
    const auto& add_eos_tensor = ctx->input(kAddEOSInput);
    const bool add_eos = add_eos_tensor.scalar<bool>()();
    const auto& reverse_tensor = ctx->input(kReverseInput);
    const bool reverse = reverse_tensor.scalar<bool>()();

    std::vector<int32> encoded;
    std::vector<int32> splits;
    for (int i = 0; i < num_of_input_values; ++i) {
      const auto res = ::tflite::ops::custom::sentencepiece::EncodeString(
          input_values_flat(i), model_tensor.data(), add_bos, add_eos, reverse);
      OP_REQUIRES(
          ctx,
          res.type ==
              ::tflite::ops::custom::sentencepiece::EncoderResultType::SUCCESS,
          tensorflow::Status(static_cast<tensorflow::errors::Code>(
                                 tensorflow::error::INTERNAL),
                             "Sentencepiece conversion failed"));
      std::copy(res.codes.begin(), res.codes.end(),
                std::back_inserter(encoded));
      splits.emplace_back(encoded.size());
    }
    tensorflow::Tensor* output_values_tensor = nullptr;
    tensorflow::Tensor* output_splits_tensor = nullptr;

    OP_REQUIRES_OK(
        ctx, ctx->allocate_output(0, {encoded.size()}, &output_values_tensor));
    OP_REQUIRES_OK(ctx, ctx->allocate_output(1, {splits.size() + 1},
                                             &output_splits_tensor));

    auto values_tensor_flat = output_values_tensor->vec<int32>();
    auto splits_tensor_flat = output_splits_tensor->vec<int32>();
    for (int i = 0; i < encoded.size(); ++i) {
      values_tensor_flat(i) = encoded[i];
    }
    splits_tensor_flat(0) = 0;
    for (int i = 0; i < splits.size(); ++i) {
      splits_tensor_flat(i + 1) = splits[i];
    }
  }
};

}  // namespace ops
}  // namespace tensorflow
REGISTER_KERNEL_BUILDER(
    Name("TFSentencepieceTokenizeOp").Device(tensorflow::DEVICE_CPU),
    tensorflow::ops::TFSentencepieceOp);
