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

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "tensorflow_lite_support/custom_ops/kernel/sentencepiece/model_converter.h"

namespace tflite {
namespace ops {
namespace custom {
namespace sentencepiece {

namespace py = pybind11;

PYBIND11_MODULE(pywrap_model_converter, m) {
  m.def("convert_sentencepiece_model", [](py::bytes model_string) {
    return py::bytes(ConvertSentencepieceModel(std::string(model_string)));
  });

  m.def("convert_sentencepiece_model_for_decoder", [](py::bytes model_string) {
    return py::bytes(
        ConvertSentencepieceModelForDecoder(std::string(model_string)));
  });

  m.def("get_vocabulary_size", [](py::bytes model_string) {
    return GetVocabularySize(std::string(model_string));
  });
}

}  // namespace sentencepiece
}  // namespace custom
}  // namespace ops
}  // namespace tflite
