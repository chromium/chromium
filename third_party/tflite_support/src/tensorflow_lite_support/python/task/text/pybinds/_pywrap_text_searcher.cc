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
#include "tensorflow_lite_support/cc/task/text/text_searcher.h"
#include "tensorflow_lite_support/cc/task/text/utils/text_op_resolver.h"
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace text {

namespace {
namespace py = ::pybind11;
using PythonBaseOptions = ::tflite::python::task::core::BaseOptions;
using CppBaseOptions = ::tflite::task::core::BaseOptions;
using CppEmbeddingOptions = ::tflite::task::processor::EmbeddingOptions;
using CppSearchOptions = ::tflite::task::processor::SearchOptions;
}  // namespace

PYBIND11_MODULE(_pywrap_text_searcher, m) {
  // python wrapper for C++ TextSearcher class which shouldn't be directly used
  // by the users.
  pybind11_protobuf::ImportNativeProtoCasters();

  pybind11::class_<TextSearcher>(m, "TextSearcher")
      .def_static(
          "create_from_options",
          [](const PythonBaseOptions& base_options,
             const processor::EmbeddingOptions& embedding_options,
             const processor::SearchOptions& search_options) {
            TextSearcherOptions options;
            auto cpp_base_options =
                core::convert_to_cpp_base_options(base_options);
            options.set_allocated_base_options(cpp_base_options.release());

            std::unique_ptr<CppEmbeddingOptions> cpp_embedding_options =
                std::make_unique<CppEmbeddingOptions>();
            cpp_embedding_options->CopyFrom(embedding_options);
            options.set_allocated_embedding_options(
                cpp_embedding_options.release());

            std::unique_ptr<CppSearchOptions> cpp_search_options =
                std::make_unique<CppSearchOptions>();
            cpp_search_options->CopyFrom(search_options);
            options.set_allocated_search_options(cpp_search_options.release());

            auto searcher = TextSearcher::CreateFromOptions(
                options, CreateTextOpResolver());
            return core::get_value(searcher);
          })
      .def("search",
           [](TextSearcher& self,
              const std::string& text) -> processor::SearchResult {
             auto search_result = self.Search(text);
             return core::get_value(search_result);
           })
      .def("get_user_info", [](TextSearcher& self) -> py::str {
        return py::str(self.GetUserInfo()->data());
      });
}

}  // namespace text
}  // namespace task
}  // namespace tflite
