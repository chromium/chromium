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

#include "absl/memory/memory.h"
#include "knowledge/hobbes/chat/tensorflow/tflite/tflite-all-lingua-ops-resolver.h"
#include "tensorflow/lite/op_resolver.h"

namespace tflite {
namespace task {
// Provides custom OpResolver for test NLClassifier models.
std::unique_ptr<OpResolver> CreateOpResolver() {  // NOLINT
  MutableOpResolver resolver;
  RegisterAllLinguaOps(&resolver);
  return absl::make_unique<MutableOpResolver>(resolver);
}

}  // namespace task
}  // namespace tflite
