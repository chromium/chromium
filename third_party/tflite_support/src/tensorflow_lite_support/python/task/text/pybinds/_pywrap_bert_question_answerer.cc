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
#include "tensorflow_lite_support/cc/task/processor/proto/qa_answers.pb.h"
#include "tensorflow_lite_support/cc/task/text/bert_question_answerer.h"
#include "tensorflow_lite_support/python/task/core/pybinds/task_utils.h"

namespace tflite {
namespace task {
namespace text {

namespace {
namespace py = ::pybind11;
using PythonBaseOptions = ::tflite::python::task::core::BaseOptions;
using CppBaseOptions = ::tflite::task::core::BaseOptions;
}  // namespace

PYBIND11_MODULE(_pywrap_bert_question_answerer, m) {
  // python wrapper for C++ BertQuestionAnswerer class which shouldn't be
  // directly used by the users.
  pybind11_protobuf::ImportNativeProtoCasters();

  pybind11::class_<BertQuestionAnswerer>(m, "BertQuestionAnswerer")
      .def_static(
          "create_from_options",
          [](const PythonBaseOptions& base_options) {
            BertQuestionAnswererOptions options;
            auto cpp_base_options =
                core::convert_to_cpp_base_options(base_options);
            options.set_allocated_base_options(cpp_base_options.release());

            auto question_answerer =
                BertQuestionAnswerer::CreateFromOptions(options);
            return core::get_value(question_answerer);
          })
      .def("answer",
           [](BertQuestionAnswerer& self, const std::string& context,
              const std::string& question)
               -> tflite::task::processor::QuestionAnswererResult {
             auto results = self.Answer(context, question);

             tflite::task::processor::QuestionAnswererResult
                 question_answerer_result;

             for (int i = 0; i < results.size(); ++i) {
               auto* answers = question_answerer_result.add_answers();
               answers->mutable_pos()->set_start(results[i].pos.start);
               answers->mutable_pos()->set_end(results[i].pos.end);
               answers->mutable_pos()->set_logit(results[i].pos.logit);
               answers->set_text(results[i].text);
             }

             return question_answerer_result;
           });
}

}  // namespace text
}  // namespace task
}  // namespace tflite
