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
#include "tensorflow_lite_support/cc/task/processor/proto/classification_options.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/classifications.pb.h"
#include "tensorflow_lite_support/cc/task/vision/image_classifier.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace vision {

namespace {
namespace py = ::pybind11;
using PythonBaseOptions = ::tflite::python::task::core::BaseOptions;
using CppBaseOptions = ::tflite::task::core::BaseOptions;
}  // namespace

PYBIND11_MODULE(_pywrap_image_classifier, m) {
  // python wrapper for C++ ImageClassifier class which shouldn't be directly
  // used by the users.
  pybind11_protobuf::ImportNativeProtoCasters();

  py::class_<ImageClassifier>(m, "ImageClassifier")
      .def_static(
          "create_from_options",
          [](const PythonBaseOptions& base_options,
             const processor::ClassificationOptions& classification_options) {
            ImageClassifierOptions options;
            auto cpp_base_options =
                core::convert_to_cpp_base_options(base_options);
            options.set_allocated_base_options(cpp_base_options.release());

            if (classification_options.has_display_names_locale()) {
              options.set_display_names_locale(
                  classification_options.display_names_locale());
            }
            if (classification_options.has_max_results()) {
              options.set_max_results(classification_options.max_results());
            }
            if (classification_options.has_score_threshold()) {
              options.set_score_threshold(
                  classification_options.score_threshold());
            }
            options.mutable_class_name_whitelist()->CopyFrom(
                classification_options.class_name_allowlist());
            options.mutable_class_name_blacklist()->CopyFrom(
                classification_options.class_name_denylist());

            auto classifier = ImageClassifier::CreateFromOptions(options);
            return core::get_value(classifier);
          })
      .def("classify",
           [](ImageClassifier& self, const ImageData& image_data)
               -> processor::ClassificationResult {
             auto frame_buffer = CreateFrameBufferFromImageData(image_data);
             auto vision_classification_result = self.Classify(
                     *core::get_value(frame_buffer));
             // Convert from vision::ClassificationResult to
             // processor::ClassificationResult as required by the Python layer.
             processor::ClassificationResult classification_result;
               classification_result.ParseFromString(
                 core::get_value(vision_classification_result)
                 .SerializeAsString());
             return classification_result;
           })
      .def("classify",
           [](ImageClassifier& self, const ImageData& image_data,
              const processor::BoundingBox& bounding_box)
               -> processor::ClassificationResult {
             // Convert from processor::BoundingBox to vision::BoundingBox as
             // the latter is used in the C++ layer.
             BoundingBox vision_bounding_box;
             vision_bounding_box.ParseFromString(
                 bounding_box.SerializeAsString());

             auto frame_buffer = CreateFrameBufferFromImageData(image_data);
             auto vision_classification_result = self.Classify(
                 *core::get_value(frame_buffer), vision_bounding_box);
             // Convert from vision::ClassificationResult to
             // processor::ClassificationResult as required by the Python layer.
             processor::ClassificationResult classification_result;
               classification_result.ParseFromString(
                 core::get_value(vision_classification_result)
                 .SerializeAsString());
             return classification_result;
           });
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
