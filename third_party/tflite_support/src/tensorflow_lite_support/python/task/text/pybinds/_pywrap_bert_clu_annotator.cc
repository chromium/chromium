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
#include "tensorflow_lite_support/cc/task/processor/proto/clu.pb.h"
#include "tensorflow_lite_support/cc/task/processor/proto/clu_annotation_options.pb.h"
#include "tensorflow_lite_support/cc/task/text/bert_clu_annotator.h"
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace text {

namespace {
using PythonBaseOptions = ::tflite::python::task::core::BaseOptions;
using CppBaseOptions = ::tflite::task::core::BaseOptions;
using BertCluAnnotator = ::tflite::task::text::clu::BertCluAnnotator;
}  // namespace

PYBIND11_MODULE(_pywrap_bert_clu_annotator, m) {
  // python wrapper for C++ BertCLUAnnotator class which shouldn't be directly
  // used by the users.
  pybind11_protobuf::ImportNativeProtoCasters();

  pybind11::class_<BertCluAnnotator>(m, "BertCluAnnotator")
      .def_static(
          "create_from_options",
          [](const PythonBaseOptions& base_options,
             const processor::BertCluAnnotationOptions& annotation_options) {
            BertCluAnnotatorOptions options;
            auto cpp_base_options =
                core::convert_to_cpp_base_options(base_options);

            options.set_allocated_base_options(cpp_base_options.release());

            if (annotation_options.has_max_history_turns()) {
              options.set_max_history_turns(
                  annotation_options.max_history_turns());
            }
            if (annotation_options.has_domain_threshold()) {
              options.set_domain_threshold(
                  annotation_options.domain_threshold());
            }
            if (annotation_options.has_intent_threshold()) {
              options.set_intent_threshold(
                  annotation_options.intent_threshold());
            }
            if (annotation_options.has_categorical_slot_threshold()) {
              options.set_categorical_slot_threshold(
                  annotation_options.categorical_slot_threshold());
            }
            if (annotation_options.has_mentioned_slot_threshold()) {
              options.set_mentioned_slot_threshold(
                  annotation_options.mentioned_slot_threshold());
            }

            auto annotator = BertCluAnnotator::CreateFromOptions(options);
            return core::get_value(annotator);
          })
      .def("annotate",
           [](BertCluAnnotator& self,
              const processor::CluRequest& request) -> processor::CluResponse {
             // Convert from processor::CluRequest to text::CluRequest as
             // required by the C++ layer.
             tflite::task::text::CluRequest text_clu_request;
             text_clu_request.ParseFromString(request.SerializeAsString());
             auto text_clu_response = self.Annotate(text_clu_request);
             // Convert from text::CluResponse to
             // processor::CluResponse as required by the Python layer.
             processor::CluResponse clu_response;
             clu_response.ParseFromString(
                 core::get_value(text_clu_response).SerializeAsString());
             return clu_response;
           });
}

}  // namespace text
}  // namespace task
}  // namespace tflite
