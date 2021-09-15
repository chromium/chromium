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

#include "tensorflow_lite_support/cc/task/text/qa/bert_qa_c_api.h"

#include <memory>

#include "tensorflow_lite_support/cc/task/text/qa/bert_question_answerer.h"
#include "tensorflow_lite_support/cc/task/text/qa/question_answerer.h"

using BertQuestionAnswererCPP = ::tflite::task::text::qa::BertQuestionAnswerer;
using QaAnswerCPP = ::tflite::task::text::qa::QaAnswer;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct BertQuestionAnswerer {
  std::unique_ptr<BertQuestionAnswererCPP> impl;
};

BertQuestionAnswerer* BertQuestionAnswererFromFile(const char* model_path) {
  auto bert_qa_status =
      BertQuestionAnswererCPP::CreateFromFile(std::string(model_path));
  if (bert_qa_status.ok()) {
    return new BertQuestionAnswerer{
        .impl = std::unique_ptr<BertQuestionAnswererCPP>(
            dynamic_cast<BertQuestionAnswererCPP*>(
                bert_qa_status.value().release()))};
  } else {
    return nullptr;
  }
}

QaAnswers* BertQuestionAnswererAnswer(
    const BertQuestionAnswerer* question_answerer,
    const char* context,
    const char* question) {
  std::vector<QaAnswerCPP> answers = question_answerer->impl->Answer(
      absl::string_view(context).data(), absl::string_view(question).data());
  size_t size = answers.size();
  auto* qa_answers = new QaAnswer[size];

  for (size_t i = 0; i < size; ++i) {
    qa_answers[i].start = answers[i].pos.start;
    qa_answers[i].end = answers[i].pos.end;
    qa_answers[i].logit = answers[i].pos.logit;
    qa_answers[i].text = strdup(answers[i].text.c_str());
  }

  auto* c_answers = new QaAnswers;
  c_answers->size = size;
  c_answers->answers = qa_answers;
  return c_answers;
}

void BertQuestionAnswererDelete(BertQuestionAnswerer* bert_question_answerer) {
  delete bert_question_answerer;
}

void BertQuestionAnswererQaAnswersDelete(QaAnswers* qa_answers) {
  delete[] qa_answers->answers;
  delete qa_answers;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
