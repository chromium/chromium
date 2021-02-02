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

#ifndef TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_QA_BERT_QUESTION_ANSWERER_H_
#define TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_QA_BERT_QUESTION_ANSWERER_H_

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/text/qa/question_answerer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/bert_tokenizer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/sentencepiece_tokenizer.h"

namespace tflite {
namespace task {
namespace text {
namespace qa {

// BertQA task API, performs tokenization for models (BERT, Albert, etc.) in
// preprocess and returns most possible answers.
//
// In particular, the branch of BERT models use WordPiece tokenizer, and the
// branch of Albert models use SentencePiece tokenizer, respectively.
//
// Factory methods:
//   CreateFromFile(path_to_model_with_metadata)
//   CreateFromBuffer(model_with_metadata_buffer_data,
//                                model_with_metadata_buffer_size)
//   CreateFromFd(file_descriptor_to_model_with_metadata)
//     Generic API to create the QuestionAnswerer for bert models with metadata
//     populated. The API expects a Bert based TFLite model with metadata
//     containing the following information:
//       - input_process_units for Wordpiece/Sentencepiece Tokenizer. Wordpiece
//         Tokenizer can be used for a MobileBert[0] model, Sentencepiece
//         Tokenizer Tokenizer can be used for an Albert[1] model
//       - 3 input tensors with names "ids", "mask" and "segment_ids"
//       - 2 output tensors with names "end_logits" and "start_logits"
//      [0]: https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1
//      [1]: https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/1
//
//   CreateBertQuestionAnswererFromFile(path_to_model, path_to_vocab)
//     Creates a BertQuestionAnswerer from TFLite model file and vocab file for
//     WordPiece tokenizer. Used in C++ environment.
//     One suitable model is:
//       https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1
//
//   CreateBertQuestionAnswererFromBuffer(model_buffer_data, model_buffer_size,
//                                        vocab_buffer_data, vocab_buffer_size)
//     Creates a BertQuestionAnswerer from TFLite model buffer and vocab file
//     buffer for WordPiece tokenizer. Used in Jave (JNI) environment.
//
//   CreateAlbertQuestionAnswererFromFile(path_to_model, path_to_spmodel)
//     Creates an AlbertQuestionAnswerer from TFLite model file and
//     SentencePiece model file. Used in C++ environment.
//     One suitable model is:
//       https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/1
//
//   CreateAlbertQuestionAnswererFromBuffer(model_buffer_data,
//                                          model_buffer_size,
//                                          spmodel_buffer_data,
//                                          spmodel_buffer_size)
//     Creates an AlbertQuestionAnswerer from TFLite model file buffer and
//     SentencePiece model file buffer. Used in Jave (JNI) environment.
//

class BertQuestionAnswerer : public QuestionAnswerer {
 public:
  // TODO(b/150904655): add support to parameterize.
  static constexpr int kMaxQueryLen = 64;
  static constexpr int kMaxSeqLen = 384;
  static constexpr int kPredictAnsNum = 5;
  static constexpr int kMaxAnsLen = 32;
  // TODO(b/151954803): clarify the offset usage
  static constexpr int kOutputOffset = 1;
  static constexpr int kNumLiteThreads = 4;
  static constexpr bool kUseLowerCase = true;

  static tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>>
  CreateFromFile(const std::string& path_to_model_with_metadata);

  static tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>>
  CreateFromBuffer(const char* model_with_metadata_buffer_data,
                   size_t model_with_metadata_buffer_size);

  static tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>>
  CreateFromFd(int fd);

  static tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>>
  CreateBertQuestionAnswererFromFile(const std::string& path_to_model,
                                     const std::string& path_to_vocab);

  static tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>>
  CreateBertQuestionAnswererFromBuffer(const char* model_buffer_data,
                                       size_t model_buffer_size,
                                       const char* vocab_buffer_data,
                                       size_t vocab_buffer_size);

  static tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>>
  CreateAlbertQuestionAnswererFromFile(const std::string& path_to_model,
                                       const std::string& path_to_spmodel);

  static tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>>
  CreateAlbertQuestionAnswererFromBuffer(const char* model_buffer_data,
                                         size_t model_buffer_size,
                                         const char* spmodel_buffer_data,
                                         size_t spmodel_buffer_size);

  explicit BertQuestionAnswerer(std::unique_ptr<core::TfLiteEngine> engine)
      : QuestionAnswerer(std::move(engine)) {}

  // Answers question based on the context. Could be empty if no answer was
  // found from the given context.
  std::vector<QaAnswer> Answer(const std::string& context,
                               const std::string& question) override;

 private:
  absl::Status Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                          const std::string& lowercased_context,
                          const std::string& lowercased_query) override;

  tflite::support::StatusOr<std::vector<QaAnswer>> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors,
      const std::string& lowercased_context,
      const std::string& lowercased_query) override;

  // Initialize API with a BertTokenizer from the vocabulary file.
  void InitializeBertTokenizer(const std::string& path_to_vocab);
  // Initialize API with a BertTokenizer from the vocabulary buffer.
  void InitializeBertTokenizerFromBinary(const char* vocab_buffer_data,
                                         size_t vocab_buffer_size);

  // Initialize API with a SentencepieceTokenizer from the model file.
  void InitializeSentencepieceTokenizer(const std::string& path_to_spmodel);
  // Initialize API with a SentencepieceTokenizer from the model buffer.
  void InitializeSentencepieceTokenizerFromBinary(
      const char* spmodel_buffer_data,
      size_t spmodel_buffer_size);

  // Initialize the API with the tokenizer set in the metadata.
  absl::Status InitializeFromMetadata();

  std::string ConvertIndexToString(int start, int end);

  std::unique_ptr<tflite::support::text::tokenizer::Tokenizer> tokenizer_;
  // Maps index of input token to index of untokenized word from original input.
  absl::flat_hash_map<size_t, size_t> token_to_orig_map_;
  // Original tokens of context.
  std::vector<std::string> orig_tokens_;
};

}  // namespace qa
}  // namespace text
}  // namespace task
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_QA_BERT_QUESTION_ANSWERER_H_
