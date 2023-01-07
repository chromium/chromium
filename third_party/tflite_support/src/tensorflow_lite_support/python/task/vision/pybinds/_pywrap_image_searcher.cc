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
#include "tensorflow_lite_support/cc/task/processor/proto/bounding_box.pb.h"
#include "tensorflow_lite_support/cc/task/vision/image_searcher.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace vision {

namespace {
namespace py = ::pybind11;
using PythonBaseOptions = ::tflite::python::task::core::BaseOptions;
using CppBaseOptions = ::tflite::task::core::BaseOptions;
using CppEmbeddingOptions = ::tflite::task::processor::EmbeddingOptions;
using CppSearchOptions = ::tflite::task::processor::SearchOptions;
}  // namespace

PYBIND11_MODULE(_pywrap_image_searcher, m) {
  // python wrapper for C++ ImageSearcher class which shouldn't be directly used
  // by the users.
  pybind11_protobuf::ImportNativeProtoCasters();

  pybind11::class_<ImageSearcher>(m, "ImageSearcher")
      .def_static(
          "create_from_options",
          [](const PythonBaseOptions& base_options,
             const processor::EmbeddingOptions& embedding_options,
             const processor::SearchOptions& search_options) {
            ImageSearcherOptions options;
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

            auto searcher = ImageSearcher::CreateFromOptions(options);
            return core::get_value(searcher);
          })
      .def("search",
           [](ImageSearcher& self,
              const ImageData& image_data) -> processor::SearchResult {
             auto frame_buffer = CreateFrameBufferFromImageData(image_data);
             auto search_result = self.Search(*core::get_value(frame_buffer));
             return core::get_value(search_result);
           })
      .def("search",
           [](ImageSearcher& self, const ImageData& image_data,
              const processor::BoundingBox& bounding_box)
               -> processor::SearchResult {
             // Convert from processor::BoundingBox to vision::BoundingBox as
             // the latter is used in the C++ layer.
             BoundingBox vision_bounding_box;
             vision_bounding_box.ParseFromString(
                 bounding_box.SerializeAsString());

             auto frame_buffer = CreateFrameBufferFromImageData(image_data);
             auto search_result = self.Search(*core::get_value(frame_buffer),
                                              vision_bounding_box);
             return core::get_value(search_result);
           })
      .def("get_user_info", [](ImageSearcher& self) -> py::str {
        return py::str(self.GetUserInfo()->data());
      });
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
