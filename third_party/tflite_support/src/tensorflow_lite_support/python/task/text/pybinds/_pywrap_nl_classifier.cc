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
#include "tensorflow_lite_support/cc/task/processor/proto/class.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/classifications.pb.h"
#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace text {

namespace {
using PythonBaseOptions = ::tflite::python::task::core::BaseOptions;
using CppBaseOptions = ::tflite::task::core::BaseOptions;
using CppClassificationResult = ::tflite::task::processor::ClassificationResult;
using NLClassifier = ::tflite::task::text::nlclassifier::NLClassifier;
}  // namespace

PYBIND11_MODULE(_pywrap_nl_classifier, m) {
  // python wrapper for C++ NLClassifier class which shouldn't be directly used
  // by the users.
  pybind11_protobuf::ImportNativeProtoCasters();

  pybind11::class_<NLClassifier>(m, "NLClassifier")
      .def_static(
          "create_from_options",
          [](const PythonBaseOptions& base_options) {
            NLClassifierOptions options;
            auto cpp_base_options =
                core::convert_to_cpp_base_options(base_options);
            options.set_allocated_base_options(cpp_base_options.release());

            auto classifier = NLClassifier::CreateFromOptions(options);
            return core::get_value(classifier);
          })
      .def("classify",
           [](NLClassifier& self,
              const std::string& text) -> processor::ClassificationResult {
             auto text_result = self.ClassifyText(text);
             auto results = core::get_value(text_result);
             processor::ClassificationResult classification_result;
             auto* classifications =
                 classification_result.add_classifications();
             classifications->set_head_index(0);
             for (int i = 0; i < results.size(); ++i) {
               auto* cl = classifications->add_classes();
               cl->set_class_name(results[i].class_name);
               cl->set_score(results[i].score);
             }
             return classification_result;
           });
}

}  // namespace text
}  // namespace task
}  // namespace tflite
