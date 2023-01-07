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

#include "tensorflow_lite_support/cc/task/text/clu_lib/tflite_modules.h"

#include <memory>
#include <string>

#include "absl/strings/str_split.h"  // from @com_google_absl
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/string_util.h"
#include "tensorflow_lite_support/cc/port/default/status_matchers.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/task/text/clu_lib/tflite_test_utils.h"
#include "tensorflow_lite_support/cc/task/text/proto/bert_clu_annotator_options_proto_inc.h"
#include "tensorflow_lite_support/cc/test/message_matchers.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"
#include "tensorflow_lite_support/cc/text/tokenizers/bert_tokenizer.h"

namespace tflite::task::text::clu {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::tflite::support::EqualsProto;
using ::tflite::support::proto::TextFormat;
using ::tflite::task::ParseTextProtoOrDie;

class UtteranceSeqModuleBERTTest : public ::testing::Test {
 protected:
  void SetUp() override {
    interpreter_.AddTensors(3);
    utterance_seq_id_feat_t_.reset(interpreter_.tensor(0));
    utterance_mask_feat_t_.reset(interpreter_.tensor(1));
    utterance_segment_id_feat_t_.reset(interpreter_.tensor(2));
    interpreter_.SetInputs({0, 1, 2});
    interpreter_.AllocateTensors();
    max_seq_len_ = 4;
    ReallocDynamicTensor<int64_t>({/*batch*/ 1, max_seq_len_},
                                  utterance_seq_id_feat_t_.get());
    ReallocDynamicTensor<int32_t>({/*batch*/ 1, max_seq_len_},
                                  utterance_mask_feat_t_.get());
    ReallocDynamicTensor<int64_t>({/*batch*/ 1, max_seq_len_},
                                  utterance_segment_id_feat_t_.get());

    const std::vector<std::string> wordpiece_vocab =
        absl::StrSplit("[PAD]\n[UNK]\n[CLS]\n[SEP]\nfoo\nbar\nbaz", '\n');
    tokenizer_ =
        std::make_unique<tflite::support::text::tokenizer::BertTokenizer>(
            wordpiece_vocab);

    options_ = std::make_unique<BertCluAnnotatorOptions>();
    tensor_index_map_ = std::make_unique<TensorIndexMap>();
    tensor_index_map_->token_id_idx = 0;
    tensor_index_map_->token_mask_idx = 1;
    tensor_index_map_->token_type_id_idx = 2;

    SUPPORT_ASSERT_OK_AND_ASSIGN(
        utterance_seq_module_,
        UtteranceSeqModule::Create(&interpreter_, tensor_index_map_.get(),
                                   options_.get(), tokenizer_.get()));
  }

  tflite::Interpreter interpreter_;
  std::unique_ptr<BertCluAnnotatorOptions> options_;
  int max_seq_len_;
  UniqueTfLiteTensor utterance_seq_id_feat_t_;
  UniqueTfLiteTensor utterance_mask_feat_t_;
  UniqueTfLiteTensor utterance_segment_id_feat_t_;
  std::unique_ptr<AbstractModule> utterance_seq_module_;
  std::unique_ptr<tflite::support::text::tokenizer::BertTokenizer> tokenizer_;
  std::unique_ptr<TensorIndexMap> tensor_index_map_;
};

TEST_F(UtteranceSeqModuleBERTTest, Truncation) {
  // Setup.
  // In this test, the last token should be truncated.
  CluRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb(
                                            utterances: "foo bar baz"
                                          )pb",
                                          &request));
  Artifacts artifacts;
  // Run Test.
  SUPPORT_ASSERT_OK(utterance_seq_module_->Preprocess(request, &artifacts));
  // Assertions.
  absl::Span<int64_t> output_seq_id(interpreter_.typed_input_tensor<int64_t>(0),
                                    1 * max_seq_len_);
  EXPECT_THAT(output_seq_id, ElementsAre(2, 4, 5, 3));
  absl::Span<int32_t> output_mask(interpreter_.typed_input_tensor<int32_t>(1),
                                  1 * max_seq_len_);
  EXPECT_THAT(output_mask, ElementsAre(1, 1, 1, 1));
  EXPECT_THAT(artifacts.token_alignments,
              ElementsAre(Pair(-1, -1), Pair(0, 3), Pair(4, 7), Pair(7, 7)));
  EXPECT_THAT(artifacts.token_turn_ids, ElementsAre(0, 0, 0, 0));
  EXPECT_THAT(artifacts.first_subword_indicators, ElementsAre(0, 1, 1, 0));
}

TEST_F(UtteranceSeqModuleBERTTest, Padding) {
  // Setup.
  // In this test, it should be padded with one [PAD] token.
  CluRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb(
                                            utterances: "foo"
                                          )pb",
                                          &request));
  Artifacts artifacts;
  // Run Test.
  SUPPORT_ASSERT_OK(utterance_seq_module_->Preprocess(request, &artifacts));
  // Assertions.
  absl::Span<int64_t> output_seq_id(interpreter_.typed_input_tensor<int64_t>(0),
                                    1 * max_seq_len_);
  EXPECT_THAT(output_seq_id, ElementsAre(2, 4, 3, 0));
  absl::Span<int32_t> output_mask(interpreter_.typed_input_tensor<int32_t>(1),
                                  1 * max_seq_len_);
  EXPECT_THAT(output_mask, ElementsAre(1, 1, 1, 0));
  EXPECT_THAT(artifacts.token_alignments,
              ElementsAre(Pair(-1, -1), Pair(0, 3), Pair(3, 3)));
  EXPECT_THAT(artifacts.token_turn_ids, ElementsAre(0, 0, 0));
  EXPECT_THAT(artifacts.first_subword_indicators, ElementsAre(0, 1, 0));
}

class UtteranceSeqModuleBERTWithHistoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    interpreter_.AddTensors(3);
    utterance_seq_id_feat_t_.reset(interpreter_.tensor(0));
    utterance_mask_feat_t_.reset(interpreter_.tensor(1));
    utterance_segment_id_feat_t_.reset(interpreter_.tensor(2));
    interpreter_.SetInputs({0, 1, 2});
    interpreter_.AllocateTensors();
    max_seq_len_ = 10;
    ReallocDynamicTensor<int64_t>({/*batch*/ 1, max_seq_len_},
                                  utterance_seq_id_feat_t_.get());
    ReallocDynamicTensor<int32_t>({/*batch*/ 1, max_seq_len_},
                                  utterance_mask_feat_t_.get());
    ReallocDynamicTensor<int64_t>({/*batch*/ 1, max_seq_len_},
                                  utterance_segment_id_feat_t_.get());

    options_ = std::make_unique<BertCluAnnotatorOptions>();
    options_->set_max_history_turns(2);

    const std::vector<std::string> wordpiece_vocab =
        absl::StrSplit("[PAD]\n[UNK]\n[CLS]\n[SEP]\nfoo\nbar\nbaz", '\n');
    tokenizer_ =
        std::make_unique<tflite::support::text::tokenizer::BertTokenizer>(
            wordpiece_vocab);
    tensor_index_map_ = std::make_unique<TensorIndexMap>();
    tensor_index_map_->token_id_idx = 0;
    tensor_index_map_->token_mask_idx = 1;
    tensor_index_map_->token_type_id_idx = 2;
    SUPPORT_ASSERT_OK_AND_ASSIGN(
        utterance_seq_module_,
        UtteranceSeqModule::Create(&interpreter_, tensor_index_map_.get(),
                                   options_.get(), tokenizer_.get()));
  }

  tflite::Interpreter interpreter_;
  std::unique_ptr<BertCluAnnotatorOptions> options_;
  int max_seq_len_;
  UniqueTfLiteTensor utterance_seq_id_feat_t_;
  UniqueTfLiteTensor utterance_mask_feat_t_;
  UniqueTfLiteTensor utterance_segment_id_feat_t_;
  std::unique_ptr<AbstractModule> utterance_seq_module_;
  std::unique_ptr<tflite::support::text::tokenizer::BertTokenizer> tokenizer_;
  std::unique_ptr<TensorIndexMap> tensor_index_map_;
};

TEST_F(UtteranceSeqModuleBERTWithHistoryTest, Truncation) {
  // Setup.
  // In this test, the last token should be truncated.
  CluRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb(
                                            utterances: "foo foo"
                                            utterances: "baz baz"
                                            utterances: "bar bar"
                                            utterances: "foo bar baz"
                                          )pb",
                                          &request));
  Artifacts artifacts;
  // Run Test.
  SUPPORT_ASSERT_OK(utterance_seq_module_->Preprocess(request, &artifacts));
  // Assertions.
  absl::Span<int64_t> output_seq_id(interpreter_.typed_input_tensor<int64_t>(0),
                                    1 * max_seq_len_);
  EXPECT_THAT(output_seq_id, ElementsAre(2, 4, 5, 6, 3, 5, 5, 3, 6, 3));
  absl::Span<int32_t> output_mask(interpreter_.typed_input_tensor<int32_t>(1),
                                  1 * max_seq_len_);
  EXPECT_THAT(output_mask, ElementsAre(1, 1, 1, 1, 1, 1, 1, 1, 1, 1));
  absl::Span<int64_t> output_segment_id(
      interpreter_.typed_input_tensor<int64_t>(2), 1 * max_seq_len_);
  EXPECT_THAT(output_segment_id, ElementsAre(0, 0, 0, 0, 0, 1, 1, 1, 1, 1));
  EXPECT_THAT(artifacts.reverse_utterance_list_to_encode.size(), 3);
  EXPECT_THAT(artifacts.token_turn_ids,
              ElementsAre(0, 0, 0, 0, 0, 1, 1, 1, 2, 2));
  EXPECT_THAT(artifacts.token_alignments,
              ElementsAre(Pair(-1, -1), Pair(0, 3), Pair(4, 7), Pair(8, 11),
                          Pair(11, 11), Pair(0, 3), Pair(4, 7), Pair(7, 7),
                          Pair(0, 3), Pair(3, 3)));
  EXPECT_THAT(artifacts.first_subword_indicators,
              ElementsAre(0, 1, 1, 1, 0, 1, 1, 0, 1, 0));
}

TEST_F(UtteranceSeqModuleBERTWithHistoryTest, Padding) {
  // Setup.
  // In this test, it should be padded with [PAD] tokens.
  CluRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb(
                                            utterances: "foo foo"
                                            utterances: "baz"
                                            utterances: "bar"
                                            utterances: "foo"
                                          )pb",
                                          &request));
  Artifacts artifacts;
  // Run Test.
  SUPPORT_ASSERT_OK(utterance_seq_module_->Preprocess(request, &artifacts));
  // Assertions.
  absl::Span<int64_t> output_seq_id(interpreter_.typed_input_tensor<int64_t>(0),
                                    1 * max_seq_len_);
  EXPECT_THAT(output_seq_id, ElementsAre(2, 4, 3, 5, 3, 6, 3, 0, 0, 0));
  absl::Span<int32_t> output_mask(interpreter_.typed_input_tensor<int32_t>(1),
                                  1 * max_seq_len_);
  EXPECT_THAT(output_mask, ElementsAre(1, 1, 1, 1, 1, 1, 1, 0, 0, 0));
  absl::Span<int64_t> output_segment_id(
      interpreter_.typed_input_tensor<int64_t>(2), 1 * max_seq_len_);
  EXPECT_THAT(output_segment_id, ElementsAre(0, 0, 0, 1, 1, 1, 1, 0, 0, 0));
  EXPECT_THAT(artifacts.reverse_utterance_list_to_encode.size(), 3);
  EXPECT_THAT(artifacts.token_turn_ids, ElementsAre(0, 0, 0, 1, 1, 2, 2));
  EXPECT_THAT(artifacts.token_alignments,
              ElementsAre(Pair(-1, -1), Pair(0, 3), Pair(3, 3), Pair(0, 3),
                          Pair(3, 3), Pair(0, 3), Pair(3, 3)));
  EXPECT_THAT(artifacts.first_subword_indicators,
              ElementsAre(0, 1, 0, 1, 0, 1, 0));
}

TEST(DomainModuleTest, PostProcess) {
  // Set up the module and tensor outputs.
  tflite::Interpreter interpreter;
  UniqueTfLiteTensor domain_tags_t;
  UniqueTfLiteTensor domain_confidences_t;
  std::unique_ptr<AbstractModule> domain_module;
  interpreter.AddTensors(2);
  domain_tags_t.reset(interpreter.tensor(0));
  domain_confidences_t.reset(interpreter.tensor(1));
  interpreter.SetOutputs({0, 1});
  interpreter.AllocateTensors();
  const int num_of_domains = 4;
  ReallocDynamicTensor<std::string>(
      {/*batch*/ 1, /*num_of_domains*/ num_of_domains}, domain_tags_t.get());
  ReallocDynamicTensor<float>({/*batch*/ 1, /*num_of_domains*/ num_of_domains},
                              domain_confidences_t.get());

  tflite::DynamicBuffer buf;
  buf.AddString("movies", 6);
  buf.AddString("restaurants", 11);
  buf.AddString("flights", 7);
  buf.AddString("other", 5);

  buf.WriteToTensor(domain_tags_t.get(), nullptr);
  std::vector<float> confidences{
      0.2,  // movies
      0.6,  // restaurants
      0.1,  // flights
      0.1,  // other
  };
  memcpy(domain_confidences_t->data.raw, confidences.data(),
         confidences.size() * sizeof(float));

  auto options = std::make_unique<BertCluAnnotatorOptions>();
  options->set_domain_threshold(0.5);
  auto tensor_index_map = std::make_unique<TensorIndexMap>();
  tensor_index_map->domain_names_idx = 0;
  tensor_index_map->domain_scores_idx = 1;
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      domain_module, DomainModule::Create(&interpreter, tensor_index_map.get(),
                                          options.get()));

  Artifacts artifacts;
  CluResponse response;
  // Run test.
  SUPPORT_ASSERT_OK(domain_module->Postprocess(&artifacts, &response));
  // Assertions.
  const auto& expected = ParseTextProtoOrDie<CluResponse>(R"pb(
    domains { display_name: "restaurants" score: 0.6 }
  )pb");
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST(IntentModuleTest, PostProcess) {
  // Set up the module and tensor outputs.
  tflite::Interpreter interpreter;
  UniqueTfLiteTensor intent_tags_t;
  UniqueTfLiteTensor intent_confidences_t;
  std::unique_ptr<AbstractModule> intent_module;
  interpreter.AddTensors(2);
  intent_tags_t.reset(interpreter.tensor(0));
  intent_confidences_t.reset(interpreter.tensor(1));
  interpreter.SetOutputs({0, 1});
  interpreter.AllocateTensors();
  const int num_of_intents = 4;
  ReallocDynamicTensor<std::string>(
      {/*batch*/ 1, /*num_of_intents*/ num_of_intents}, intent_tags_t.get());
  ReallocDynamicTensor<float>({/*batch*/ 1, /*num_of_intents*/ num_of_intents},
                              intent_confidences_t.get());

  tflite::DynamicBuffer buf;
  buf.AddString("request(show_date)", 18);
  buf.AddString("number_of_seats=2", 17);
  buf.AddString("book_restaurant", 15);
  buf.AddString("other", 5);

  buf.WriteToTensor(intent_tags_t.get(), nullptr);
  std::vector<float> confidences{
      0.5,  // request(show_date)
      0.5,  // number_of_seats=2
      0.7,  // book_restaurant
      0.1,  // other
  };
  memcpy(intent_confidences_t->data.raw, confidences.data(),
         confidences.size() * sizeof(float));

  auto options = std::make_unique<BertCluAnnotatorOptions>();

  options->set_intent_threshold(0.6);
  options->set_categorical_slot_threshold(0.5);
  auto tensor_index_map = std::make_unique<TensorIndexMap>();
  tensor_index_map->intent_names_idx = 0;
  tensor_index_map->intent_scores_idx = 1;
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      intent_module, IntentModule::Create(&interpreter, tensor_index_map.get(),
                                          options.get()));

  Artifacts artifacts;
  CluResponse response;
  // Run test.
  SUPPORT_ASSERT_OK(intent_module->Postprocess(&artifacts, &response));
  // Assertions.
  const auto& expected = ParseTextProtoOrDie<CluResponse>(R"pb(
    intents { display_name: "book_restaurant" score: 0.7 }
    categorical_slots {
      slot: "number_of_seats"
      prediction: { display_name: "2" score: 0.5 }
    }
  )pb");
  EXPECT_THAT(response, EqualsProto(expected));
}

TEST(SlotModuleTest, PostProcess) {
  // Set up the module and tensor outputs.
  tflite::Interpreter interpreter;
  UniqueTfLiteTensor slot_tags_t;
  UniqueTfLiteTensor slot_confidences_t;
  std::unique_ptr<AbstractModule> slot_module;
  interpreter.AddTensors(2);
  slot_tags_t.reset(interpreter.tensor(0));
  slot_confidences_t.reset(interpreter.tensor(1));
  interpreter.SetOutputs({0, 1});
  interpreter.AllocateTensors();
  // Assume that the current turn with two history turns (in the reverse
  // chronological order):
  //  current utterance: "[CLS] book a ##t 4 pm [SEP]"
  //  the last turn: "Sun ##day ? [SEP]"
  //  the turn before the last: "book for 2 peo ##ple [SEP]"
  //
  // Note that "at", "Sunday", and "people" are broken into multiple subwords.
  // Also assume that max_seq_length of BERT is 18 (with one padding).
  const int max_seq_len = 18;
  ReallocDynamicTensor<std::string>({/*batch*/ 1, /*max_seq_len*/ max_seq_len},
                                    slot_tags_t.get());
  ReallocDynamicTensor<float>({/*batch*/ 1, /*max_seq_len*/ max_seq_len},
                              slot_confidences_t.get());

  tflite::DynamicBuffer buf;
  buf.AddString("O", 1);         // [CLS]
  buf.AddString("O", 1);         // "book"
  buf.AddString("O", 1);         // "a"
  buf.AddString("B-ok", 4);      // "##t"
  buf.AddString("B-time", 6);    // "4"
  buf.AddString("I-time", 6);    // "pm"
  buf.AddString("O", 1);         // [SEP]
  buf.AddString("B-date", 6);    // "Sun"
  buf.AddString("O", 1);         // "##day"
  buf.AddString("O", 1);         // "?"
  buf.AddString("O", 1);         // [SEP]
  buf.AddString("O", 1);         // "book"
  buf.AddString("O", 1);         // "for"
  buf.AddString("B-people", 8);  // "2"
  buf.AddString("I-people", 8);  // "peo"
  buf.AddString("O", 1);         // "##ple"
  buf.AddString("O", 1);         // "[SEP]"
  buf.AddString("O", 1);         // "[PAD]"

  buf.WriteToTensor(slot_tags_t.get(), nullptr);
  std::vector<float> confidences{
      1.0,  // [CLS]
      0.9,  // "book"
      0.8,  // "a"
      0.9,  // "##t"
      0.7,  // "4"
      0.9,  // "pm"
      1.0,  // [SEP]
      0.8,  // "Sun"
      0.9,  // "##day"
      0.9,  // "?"
      0.7,  // [SEP]
      1.0,  // "book"
      0.9,  // "for"
      0.8,  // "2"
      0.5,  // "peo"
      0.9,  // "##ple"
      0.8,  // [SEP]
      1.0   // [PAD]
  };
  memcpy(slot_confidences_t->data.raw, confidences.data(),
         confidences.size() * sizeof(float));

  auto options = std::make_unique<BertCluAnnotatorOptions>();
  options->set_mentioned_slot_threshold(0.5);
  options->set_max_history_turns(2);
  auto tensor_index_map = std::make_unique<TensorIndexMap>();
  tensor_index_map->slot_names_idx = 0;
  tensor_index_map->slot_scores_idx = 1;
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      slot_module,
      SlotModule::Create(&interpreter, tensor_index_map.get(), options.get()));

  // Set up the artifacts.
  Artifacts artifacts;
  artifacts.reverse_utterance_list_to_encode = std::vector<absl::string_view>{
      "book at 4 pm",
      "Sunday?",
      "book for 2 people",
  };
  artifacts.token_turn_ids = {
      0,  // [CLS]
      0,  // "book"
      0,  // "a"
      0,  // "##t"
      0,  // "4"
      0,  // "pm"
      0,  // [SEP]
      1,  // "Sun"
      1,  // "##day"
      1,  // "?"
      1,  // [SEP]
      2,  // "book"
      2,  // "for"
      2,  // "2"
      2,  // "peo"
      2,  // "##ple"
      2,  // [SEP]
  };
  artifacts.token_alignments = {
      {-1, -1},  // [CLS]
      {0, 4},    // "book"
      {5, 6},    // "a"
      {6, 7},    // "##t"
      {8, 9},    // "4"
      {10, 12},  // "pm"
      {12, 12},  // [SEP]
      {0, 3},    // "Sun"
      {3, 6},    // "##day"
      {7, 8},    // "?"
      {8, 8},    // [SEP]
      {0, 4},    // "book"
      {5, 8},    // "for"
      {9, 11},   // "2"
      {12, 15},  // "peo"
      {15, 18},  // "##ple"
      {18, 18}   // [SEP]
  };
  artifacts.first_subword_indicators = {
      0,  // [CLS]
      1,  // "book"
      1,  // "a"
      0,  // "##t"
      1,  // "4"
      1,  // "pm"
      0,  // [SEP]
      1,  // "Sun"
      0,  // "##day"
      1,  // "?"
      0,  // [SEP]
      1,  // "book"
      1,  // "for"
      1,  // "2"
      1,  // "peo"
      0,  // "##ple"
      0,  // [SEP]
  };
  CluResponse response;
  // Run test.
  SUPPORT_ASSERT_OK(slot_module->Postprocess(&artifacts, &response));
  // Assertions.
  const auto& expected = ParseTextProtoOrDie<CluResponse>(R"pb(
    mentioned_slots {
      slot: "time"
      mention: { value: "4 pm" start: 8 end: 12 score: 0.7 }
    }
  )pb");
  EXPECT_THAT(response, EqualsProto(expected));
}

}  // namespace
}  // namespace tflite::task::text::clu
