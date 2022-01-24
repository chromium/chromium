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

#include "tensorflow_lite_support/custom_ops/kernel/whitespace_tokenizer.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "tensorflow/lite/kernels/test_util.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/string_util.h"

namespace tflite {
namespace ops {
namespace custom {
namespace whitespace_tokenizer {
namespace test {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

}  // namespace

enum OutputType { PADDED, RAGGED };

class WhitespaceTokenizerModel : public SingleOpModel {
 public:
  WhitespaceTokenizerModel(OutputType output_type,
                           const std::vector<std::string>& input_values,
                           const std::vector<int>& input_shape)
      : input_shape_(input_shape) {
    input_ = AddInput(TensorType_STRING);
    output_values_ = AddOutput(TensorType_STRING);
    if (output_type == RAGGED) {
      for (int i = 0; i < input_shape_.size(); ++i) {
        output_row_splits_.push_back(AddOutput(TensorType_INT64));
      }
    }
    SetCustomOp("WhitespaceTokenizer", {}, Register_tftext_WhitespaceTokenizer);

    BuildInterpreter({input_shape});
    PopulateStringTensor(input_, input_values);
    Invoke();
  }

  std::vector<int> GetValuesTensorShape() {
    return GetTensorShape(output_values_);
  }

  std::vector<std::string> ExtractValuesTensorVector() {
    std::vector<std::string> r;
    TfLiteTensor* tensor = interpreter_->tensor(output_values_);
    int n = GetStringCount(tensor);
    for (int i = 0; i < n; ++i) {
      StringRef ref = GetString(tensor, i);
      r.emplace_back(ref.str, ref.len);
    }
    return r;
  }

  void CheckRowSplits(const std::vector<int>& token_counts) {
    int size = 1;
    for (int i = 0; i < input_shape_.size(); ++i) {
      size *= input_shape_[i];
      EXPECT_THAT(GetTensorShape(output_row_splits_[i]), ElementsAre(size + 1))
          << "row_splits " << i << " has the wrong shape";

      std::vector<int64_t> expected_values(size + 1);
      if (i == input_shape_.size() - 1) {
        ASSERT_EQ(token_counts.size(), size);

        int index = 0;
        expected_values[0] = index;
        for (int j = 0; j < size; ++j) {
          index += token_counts[j];
          expected_values[j + 1] = index;
        }
      } else {
        for (int j = 0; j <= size; ++j) {
          expected_values[j] = j * input_shape_[i + 1];
        }
      }
      EXPECT_THAT(ExtractVector<int64_t>(output_row_splits_[i]),
                  ElementsAreArray(expected_values))
          << "row_splits " << i << " has an incorrect value/index";
    }
  }

 private:
  int input_;
  std::vector<int> input_shape_;
  int output_values_;
  std::vector<int> output_row_splits_;
};  // namespace test

TEST(WhitespaceTokenizerTest, SingleStringPaddedOutput) {
  WhitespaceTokenizerModel m(PADDED, {"this is a test"}, {1});
  EXPECT_THAT(m.GetValuesTensorShape(), ElementsAre(1, 4));
  EXPECT_THAT(m.ExtractValuesTensorVector(),
              ElementsAre("this", "is", "a", "test"));
}

TEST(WhitespaceTokenizerTest, SingleStringRaggedOutput) {
  WhitespaceTokenizerModel m(RAGGED, {"this is a test"}, {1});
  m.CheckRowSplits({4});
  EXPECT_THAT(m.ExtractValuesTensorVector(),
              ElementsAre("this", "is", "a", "test"));
}

TEST(WhitespaceTokenizerTest, VectorPaddedOutput) {
  WhitespaceTokenizerModel m(PADDED,
                             {"this is a test",        //
                              "three token sentence",  //
                              "many more tokens than that sentence"},
                             {3});
  EXPECT_THAT(m.GetValuesTensorShape(), ElementsAre(3, 6));
  EXPECT_THAT(
      m.ExtractValuesTensorVector(),
      ElementsAre("this", "is", "a", "test", "", "",         //
                  "three", "token", "sentence", "", "", "",  //
                  "many", "more", "tokens", "than", "that", "sentence"));
}

TEST(WhitespaceTokenizerTest, VectorRaggedOutput) {
  WhitespaceTokenizerModel m(RAGGED,
                             {"this is a test",        //
                              "three token sentence",  //
                              "many more tokens than that sentence"},
                             {3});
  m.CheckRowSplits({4, 3, 6});
  EXPECT_THAT(
      m.ExtractValuesTensorVector(),
      ElementsAre("this", "is", "a", "test",     //
                  "three", "token", "sentence",  //
                  "many", "more", "tokens", "than", "that", "sentence"));
}

TEST(WhitespaceTokenizerTest, MatrixPaddedOutput) {
  WhitespaceTokenizerModel m(PADDED,
                             {"a b c", "d e f",  //
                              "g h", "i j k l",  //
                              "m", "n o p q r"},
                             {3, 2});
  EXPECT_THAT(m.GetValuesTensorShape(), ElementsAre(3, 2, 5));
  EXPECT_THAT(m.ExtractValuesTensorVector(),
              ElementsAre("a", "b", "c", "", "",   //
                          "d", "e", "f", "", "",   //
                          "g", "h", "", "", "",    //
                          "i", "j", "k", "l", "",  //
                          "m", "", "", "", "",     //
                          "n", "o", "p", "q", "r"));
}

TEST(WhitespaceTokenizerTest, MatrixRAGGEDOutput) {
  WhitespaceTokenizerModel m(RAGGED,
                             {"a b c", "d e f",  //
                              "g h", "i j k l",  //
                              "m", "n o p q r"},
                             {3, 2});
  m.CheckRowSplits({3, 3, 2, 4, 1, 5});
  EXPECT_THAT(m.ExtractValuesTensorVector(),
              ElementsAre("a", "b", "c",       //
                          "d", "e", "f",       //
                          "g", "h",            //
                          "i", "j", "k", "l",  //
                          "m",                 //
                          "n", "o", "p", "q", "r"));
}

}  // namespace test
}  // namespace whitespace_tokenizer
}  // namespace custom
}  // namespace ops
}  // namespace tflite
