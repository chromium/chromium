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

#include "tensorflow_lite_support/cc/task/core/error_reporter.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "tensorflow/lite/minimal_logging.h"

namespace tflite {
namespace task {
namespace core {

int ErrorReporter::Report(const char* format, va_list args) {
  std::strcpy(second_last_message_, last_message_);  // NOLINT
  last_message_[0] = '\0';
  int num_characters = vsnprintf(last_message_, kBufferSize, format, args);
  // To mimic tflite::StderrReporter.
  tflite::logging_internal::MinimalLogger::Log(TFLITE_LOG_ERROR, "%s",
                                               last_message_);
  return num_characters;
}

std::string ErrorReporter::message() { return last_message_; }

std::string ErrorReporter::previous_message() { return second_last_message_; }

}  // namespace core
}  // namespace task
}  // namespace tflite
