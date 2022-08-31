/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#include "pybind11/pybind11.h"
#include "pybind11_protobuf/native_proto_caster.h"  // from @pybind11_protobuf
#include "tensorflow_lite_support/cc/task/text/text_embedder.h"
#include "tensorflow_lite_support/cc/task/text/utils/text_op_resolver.h"
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace text {

namespace {
using PythonBaseOptions = ::tflite::python::task::core::BaseOptions;
using CppBaseOptions = ::tflite::task::core::BaseOptions;
}  // namespace

PYBIND11_MODULE(_pywrap_text_embedder, m) {
  // python wrapper for C++ TextEmbeder class which shouldn't be directly used
  // by the users.
  pybind11_protobuf::ImportNativeProtoCasters();

  pybind11::class_<TextEmbedder>(m, "TextEmbedder")
      .def_static(
          "create_from_options",
          [](const PythonBaseOptions& base_options,
             const processor::EmbeddingOptions& embedding_options) {
            TextEmbedderOptions options;
            auto cpp_base_options =
                core::convert_to_cpp_base_options(base_options);

            options.set_allocated_base_options(cpp_base_options.release());
            options.add_embedding_options()->CopyFrom(embedding_options);
            auto embedder = TextEmbedder::CreateFromOptions(
                options, CreateTextOpResolver());
            return core::get_value(embedder);
          })
      .def("embed",
           [](TextEmbedder& self,
              const std::string& text) -> processor::EmbeddingResult {
             auto embedding_result = self.Embed(text);
             return core::get_value(embedding_result);
           })
      .def("get_embedding_dimension", &TextEmbedder::GetEmbeddingDimension)
      .def("get_number_of_output_layers",
           &TextEmbedder::GetNumberOfOutputLayers)
      .def_static("cosine_similarity",
                  [](const processor::FeatureVector& u,
                     const processor::FeatureVector& v) -> double {
                    auto similarity = TextEmbedder::CosineSimilarity(u, v);
                    return core::get_value(similarity);
                  });
}

}  // namespace text
}  // namespace task
}  // namespace tflite
