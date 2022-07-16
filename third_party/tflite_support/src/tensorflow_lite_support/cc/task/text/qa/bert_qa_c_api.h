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
#ifndef TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_QA_BERT_QA_C_API_H_
#define TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_QA_BERT_QA_C_API_H_

// --------------------------------------------------------------------------
/// C API for BertQuestionAnswerer.
///
/// The API leans towards simplicity and uniformity instead of convenience, as
/// most usage will be by language-specific wrappers. It provides largely the
/// same set of functionality as that of the C++ TensorFlow Lite
/// `BertQuestionAnswerer` API, but is useful for shared libraries where having
/// a stable ABI boundary is important.
///
/// Usage:
/// <pre><code>
/// // Create the model and interpreter options.
/// BertQuestionAnswerer* qa_answerer =
///   BertQuestionAnswererFromFile("/path/to/model.tflite");
///
/// // answer a question.
/// QaAnswers* answers = Answer(qa_answerer, context, question);
///
/// // Dispose of the API and QaAnswers objects.
/// BertQuestionAnswererDelete(qa_answerer);
/// BertQuestionAnswererQaAnswersDelete(answers);

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct BertQuestionAnswerer BertQuestionAnswerer;

struct QaAnswer {
  int start;
  int end;
  float logit;
  char* text;
};

struct QaAnswers {
  int size;
  struct QaAnswer* answers;
};

// Creates BertQuestionAnswerer from model path, returns nullptr if the file
// doesn't exist or is not a well formatted TFLite model path.
extern BertQuestionAnswerer* BertQuestionAnswererFromFile(
    const char* model_path);

// Invokes the encapsulated TFLite model and answers a question based on
// context.
extern struct QaAnswers* BertQuestionAnswererAnswer(
    const BertQuestionAnswerer* question_answerer,
    const char* context,
    const char* question);

extern void BertQuestionAnswererDelete(
    BertQuestionAnswerer* bert_question_answerer);

extern void BertQuestionAnswererQaAnswersDelete(struct QaAnswers* qa_answers);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_QA_BERT_QA_C_API_H_
