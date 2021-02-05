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

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/tensor_types.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow_text/core/kernels/sentence_fragmenter_v2.h"

using ::tensorflow::tstring;

namespace tensorflow {
namespace text {

class SentenceFragmentsOpV2 : public OpKernel {
 public:
  explicit SentenceFragmentsOpV2(OpKernelConstruction* context)
      : OpKernel(context) {}

  void Compute(::tensorflow::OpKernelContext* context) override {
    const Tensor* document_tensor;
    OP_REQUIRES_OK(context, context->input("doc", &document_tensor));
    const auto& document = document_tensor->vec<tstring>();

    std::vector<int64> fragment_start;
    std::vector<int64> fragment_end;
    std::vector<int64> fragment_properties;
    std::vector<int64> terminal_punc_token;
    std::vector<int64> output_row_lengths;

    // Iterate through all the documents and find fragments.
    for (int i = 0; i < document.size(); ++i) {
      // Find fragments.
      SentenceFragmenterV2 fragmenter(document(i));
      std::vector<SentenceFragment> frags;

      OP_REQUIRES_OK(context, fragmenter.FindFragments(&frags));

      for (const auto& f : frags) {
        fragment_start.push_back(f.start);
        fragment_end.push_back(f.limit);
        fragment_properties.push_back(f.properties);
        terminal_punc_token.push_back(f.terminal_punc_token);
      }
      output_row_lengths.push_back(frags.size());
    }

#define DECLARE_ALLOCATE_AND_FILL_OUTPUT_TENSOR(name, dtype)                 \
  int64 name##_size = name.size();                                           \
  Tensor* name##_tensor = nullptr;                                           \
  OP_REQUIRES_OK(context,                                                    \
                 context->allocate_output(#name, TensorShape({name##_size}), \
                                          &name##_tensor));                  \
  auto name##_data = name##_tensor->flat<dtype>().data();                    \
  memcpy(name##_data, name.data(), name##_size * sizeof(dtype));

    DECLARE_ALLOCATE_AND_FILL_OUTPUT_TENSOR(fragment_start, int64);
    DECLARE_ALLOCATE_AND_FILL_OUTPUT_TENSOR(fragment_end, int64);
    DECLARE_ALLOCATE_AND_FILL_OUTPUT_TENSOR(fragment_properties, int64);
    DECLARE_ALLOCATE_AND_FILL_OUTPUT_TENSOR(terminal_punc_token, int64);
    DECLARE_ALLOCATE_AND_FILL_OUTPUT_TENSOR(output_row_lengths, int64);

#undef DECLARE_ALLOCATE_AND_FILL_OUTPUT_TENSOR
  }
};

REGISTER_KERNEL_BUILDER(Name("SentenceFragmentsV2").Device(DEVICE_CPU),
                        SentenceFragmentsOpV2);

}  // namespace text
}  // namespace tensorflow
