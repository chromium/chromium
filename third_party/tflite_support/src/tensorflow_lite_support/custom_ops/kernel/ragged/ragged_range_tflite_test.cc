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
#include <initializer_list>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "flatbuffers/flexbuffers.h"  // from @flatbuffers
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/internal/tensor.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow/lite/kernels/test_util.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace tflite {
namespace ops {
namespace custom {
TfLiteRegistration* Register_RAGGED_RANGE();
}  // namespace custom
}  // namespace ops

namespace {

template <typename T>
class RaggedRangeOpModel : public SingleOpModel {
 public:
  static TensorType GetType();

  RaggedRangeOpModel(const std::vector<T>& start, const std::vector<T>& limits,
                     const std::vector<T>& deltas) {
    const TensorType value_type = GetType();
    std::vector<std::vector<int>> shapes;
    input_start_ = AddInput(value_type);
    shapes.push_back({static_cast<int>(start.size())});
    input_limits_ = AddInput(value_type);
    shapes.push_back({static_cast<int>(limits.size())});
    input_deltas_ = AddInput(value_type);
    shapes.push_back({static_cast<int>(deltas.size())});

    output_splits_ = AddOutput(TensorType_INT32);
    output_values_ = AddOutput(value_type);

    SetCustomOp("RaggedRange", {}, ops::custom::Register_RAGGED_RANGE);
    BuildInterpreter(shapes);

    PopulateTensor(input_start_, start);
    PopulateTensor(input_limits_, limits);
    PopulateTensor(input_deltas_, deltas);
  }

  std::vector<int32> GetSplits() {
    return ExtractVector<int32>(output_splits_);
  }
  std::vector<T> GetValues() const { return ExtractVector<T>(output_values_); }

 protected:
  int input_start_ = -1;
  int input_limits_ = -1;
  int input_deltas_ = -1;

  int output_splits_ = -1;
  int output_values_ = -1;
};

template <>
TensorType RaggedRangeOpModel<int32>::GetType() {
  return TensorType_INT32;
}

template <>
TensorType RaggedRangeOpModel<float>::GetType() {
  return TensorType_FLOAT32;
}

TEST(RaggedRangeOpTest, IntValues) {
  RaggedRangeOpModel<int32> model({0, 5, 8, 5},    // Starts.
                                  {8, 7, 8, 1},    // Limits.
                                  {2, 1, 1, -1});  // Deltas.
  ASSERT_EQ(model.Invoke(), kTfLiteOk);

  EXPECT_THAT(model.GetSplits(),
              testing::UnorderedElementsAreArray({0, 4, 6, 6, 10}));
  EXPECT_THAT(model.GetValues(), testing::UnorderedElementsAreArray(
                                     {0, 2, 4, 6, 5, 6, 5, 4, 3, 2}));
}

TEST(RaggedRangeOpTest, FloatValues) {
  RaggedRangeOpModel<float> model({0, 5, 8, 5},    // Starts.
                                  {8, 7, 8, 1},    // Limits.
                                  {2, 1, 1, -1});  // Deltas.
  ASSERT_EQ(model.Invoke(), kTfLiteOk);

  EXPECT_THAT(model.GetSplits(),
              testing::UnorderedElementsAreArray({0, 4, 6, 6, 10}));
  EXPECT_THAT(model.GetValues(), testing::UnorderedElementsAreArray(
                                     {0, 2, 4, 6, 5, 6, 5, 4, 3, 2}));
}

TEST(RaggedRangeOpTest, BroadcastDelta) {
  RaggedRangeOpModel<int32> model({0, 5, 8},  // Starts.
                                  {8, 7, 8},  // Limits.
                                  {1});       // Deltas.
  ASSERT_EQ(model.Invoke(), kTfLiteOk);

  EXPECT_THAT(model.GetSplits(),
              testing::UnorderedElementsAreArray({0, 8, 10, 10}));
  EXPECT_THAT(model.GetValues(), testing::UnorderedElementsAreArray(
                                     {0, 1, 2, 3, 4, 5, 6, 7, 5, 6}));
}

TEST(RaggedRangeOpTest, BroadcastStartDeltas) {
  RaggedRangeOpModel<int32> model({0},      // Starts.
                                  {10},     // Limits.
                                  {2, 1});  // Deltas.
  ASSERT_EQ(model.Invoke(), kTfLiteOk);

  EXPECT_THAT(model.GetSplits(),
              testing::UnorderedElementsAreArray({0, 5, 15}));
  EXPECT_THAT(model.GetValues(),
              testing::UnorderedElementsAreArray(
                  {0, 2, 4, 6, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(RaggedRangeOpTest, BadDeltas) {
  RaggedRangeOpModel<int32> model({0, 5, 8, 5},   // Starts.
                                  {8, 7, 7, 9},   // Limits.
                                  {0, 1, 1, 1});  // Deltas.
  EXPECT_EQ(model.Invoke(), kTfLiteError);
}

TEST(RaggedRangeOpTest, ZeroRange) {
  RaggedRangeOpModel<int32> model({0, 7},   // Starts.
                                  {8, 5},   // Limits.
                                  {1, 1});  // Deltas.
  ASSERT_EQ(model.Invoke(), kTfLiteOk);
  EXPECT_THAT(model.GetSplits(), testing::UnorderedElementsAreArray({0, 8, 8}));
  EXPECT_THAT(model.GetValues(),
              testing::UnorderedElementsAreArray({0, 1, 2, 3, 4, 5, 6, 7}));
}

}  // namespace
}  // namespace tflite
