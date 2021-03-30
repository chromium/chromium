// Copyright 2021 TF.Text Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {
namespace text {

Status SentenceFragmentV2ShapeFn(
    ::tensorflow::shape_inference::InferenceContext* c) {
  shape_inference::ShapeHandle unused;
  TF_RETURN_IF_ERROR(c->WithRank(c->input(0), 1, &unused));

  for (int i = 0; i < c->num_outputs(); ++i) {
    c->set_output(i, c->UnknownShapeOfRank(1));
  }

  return Status::OK();
}

REGISTER_OP("SentenceFragmentsV2")
    .Input("doc: string")
    .Output("fragment_start: int64")
    .Output("fragment_end: int64")
    .Output("fragment_properties: int64")
    .Output("terminal_punc_token: int64")
    .Output("output_row_lengths: int64")
    .SetShapeFn(SentenceFragmentV2ShapeFn);

}  // namespace text
}  // namespace tensorflow
