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

#include "tensorflow_lite_support/c/task/text/bert_question_answerer.h"

#include <memory>

#include "tensorflow_lite_support/cc/task/text/bert_question_answerer.h"
#include "tensorflow_lite_support/cc/task/text/question_answerer.h"

namespace {
using BertQuestionAnswererCpp = ::tflite::task::text::BertQuestionAnswerer;
using QaAnswerCpp = ::tflite::task::text::QaAnswer;
}  // namespace

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct TfLiteBertQuestionAnswerer {
  std::unique_ptr<BertQuestionAnswererCpp> impl;
};

TfLiteBertQuestionAnswerer* TfLiteBertQuestionAnswererCreate(
    const char* model_path) {
  auto bert_qa_status =
      BertQuestionAnswererCpp::CreateFromFile(std::string(model_path));
  if (bert_qa_status.ok()) {
    return new TfLiteBertQuestionAnswerer{
        .impl = std::unique_ptr<BertQuestionAnswererCpp>(
            dynamic_cast<BertQuestionAnswererCpp*>(
                bert_qa_status.value().release()))};
  } else {
    return nullptr;
  }
}

TfLiteQaAnswers* TfLiteBertQuestionAnswererAnswer(
    const TfLiteBertQuestionAnswerer* question_answerer, const char* context,
    const char* question) {
  std::vector<QaAnswerCpp> answers = question_answerer->impl->Answer(
      absl::string_view(context).data(), absl::string_view(question).data());
  size_t size = answers.size();
  auto* qa_answers = new TfLiteQaAnswer[size];

  for (size_t i = 0; i < size; ++i) {
    qa_answers[i].start = answers[i].pos.start;
    qa_answers[i].end = answers[i].pos.end;
    qa_answers[i].logit = answers[i].pos.logit;
    qa_answers[i].text = strdup(answers[i].text.c_str());
  }

  auto* c_answers = new TfLiteQaAnswers;
  c_answers->size = size;
  c_answers->answers = qa_answers;
  return c_answers;
}

void TfLiteBertQuestionAnswererDelete(
    TfLiteBertQuestionAnswerer* bert_question_answerer) {
  delete bert_question_answerer;
}

void TfLiteQaAnswersDelete(TfLiteQaAnswers* qa_answers) {
  for (int i = 0; i < qa_answers->size; i++) {
    // `strdup` obtains memory using `malloc` and the memory needs to be
    // released using `free`.
    free(qa_answers->answers[i].text);
  }
  delete[] qa_answers->answers;
  delete qa_answers;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
