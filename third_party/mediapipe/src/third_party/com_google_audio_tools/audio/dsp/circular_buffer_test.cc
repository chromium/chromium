/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/circular_buffer.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/types/span.h"

namespace audio_dsp {

using ::std::vector;

namespace {

TEST(CircularBufferTest, CircularBufferTest) {
  constexpr int kNumChannels = 3;
  constexpr int kBufferLength = 9;
  CircularBuffer<int> buffer;
  buffer.Init(kBufferLength * kNumChannels);
  EXPECT_EQ(buffer.capacity(), kBufferLength * kNumChannels);
  for (int i = 0; i < 3; ++i) {
    // Write 3 samples in per channel.
    buffer.Write({1, 3, 6, 2, 4, 7, 3, 5, 8});
    ASSERT_EQ(buffer.NumReadableEntries(), 9);
    ASSERT_FALSE(buffer.IsFull());
    // Write 2 more samples, a total of 5.
    buffer.Write({8, 8, 8, 8, 8, 8});
    ASSERT_EQ(buffer.NumReadableEntries(), 15);
    ASSERT_FALSE(buffer.IsFull());
    // Write 4 more samples, a total of 5.
    buffer.Write({-1, 9, 8, -1, 9, 8, -1, 9, 8, -1, 9, 8});
    ASSERT_EQ(buffer.NumReadableEntries(), 27);
    ASSERT_TRUE(buffer.IsFull());
    vector<int> read_data;
    {
      // Peek at the first sample in the buffer.
      vector<int> expected = {1, 3, 6};
      buffer.Peek(3, &read_data);
      ASSERT_THAT(read_data, testing::ElementsAreArray(expected));
      ASSERT_EQ(buffer.NumReadableEntries(), 27);
      ASSERT_TRUE(buffer.IsFull());
    }
    {
      // Read two samples back out of the buffer.
      vector<int> expected = {1, 3, 6, 2, 4, 7};
      buffer.Read(6, &read_data);
      ASSERT_THAT(read_data, testing::ElementsAreArray(expected));
      ASSERT_EQ(buffer.NumReadableEntries(), 21);
      ASSERT_FALSE(buffer.IsFull());
    }
    {
      // Peek at the first two samples in the buffer.
      vector<int> expected = {3, 5, 8, 8, 8, 8};
      buffer.Peek(6, &read_data);
      ASSERT_THAT(read_data, testing::ElementsAreArray(expected));
      ASSERT_EQ(buffer.NumReadableEntries(), 21);
      ASSERT_FALSE(buffer.IsFull());
    }
    {
      // Advance the buffer.
      buffer.Advance(4);
      ASSERT_EQ(buffer.NumReadableEntries(), 17);
      // Read nine more samples.
      vector<int> expected = {8, 8, 8, 8, 8};
      buffer.Read(5, &read_data);
      ASSERT_THAT(read_data, testing::ElementsAreArray(expected));
      ASSERT_EQ(buffer.NumReadableEntries(), 12);
      ASSERT_FALSE(buffer.IsFull());
    }
    // Write 5 more samples.
    buffer.Write({0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4});
    ASSERT_EQ(buffer.NumReadableEntries(), 27);
    ASSERT_TRUE(buffer.IsFull());
    {
      // Read all contents of the buffer.
      vector<int> expected = {-1, 9, 8, -1, 9, 8, -1, 9, 8, -1, 9, 8, 0, 0,
                              0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4};
      read_data.resize(expected.size());
      absl::Span<int> data_slice(read_data.data(), expected.size());
      buffer.Read(data_slice);
      ASSERT_THAT(read_data, testing::ElementsAreArray(expected));
      ASSERT_EQ(buffer.NumReadableEntries(), 0);
      ASSERT_FALSE(buffer.IsFull());
    }
  }
}

TEST(CircularBufferTest, CircularBufferTestEmpty) {
  constexpr int kNumChannels = 3;
  constexpr int kBufferLength = 9;

  CircularBuffer<int> buffer;
  buffer.Init(kBufferLength * kNumChannels);
  // Write 3 samples in per channel.
  buffer.Write({1, 2, 3, 4, 5, 6, 7, 8, 9});
  // Clear the buffer.
  buffer.Clear();

  int num_entries = buffer.NumReadableEntries();

  ASSERT_EQ(num_entries, 0);
}

TEST(CircularBufferTest, PlanarCircularBufferTest) {
  constexpr int kNumChannels = 3;
  constexpr int kBufferLength = 9;
  PlanarCircularBuffer<int> buffer;
  buffer.Init(kNumChannels, kBufferLength);
  EXPECT_EQ(buffer.GetNumChannels(), kNumChannels);
  EXPECT_EQ(buffer.capacity(), kBufferLength);
  // The buffer is empty at the end of each pass through this loop, but
  // the internal state is different (different values of the read and write
  // pointer).
  for (int i = 0; i < 3; ++i) {
    // Write 3 samples in per channel.
    vector<vector<int>> samples1 = {{1, 2, 3}, {3, 4, 5}, {6, 7, 8}};
    buffer.Write(absl::MakeSpan(samples1));
    ASSERT_EQ(buffer.NumReadableEntries(), 3);
    ASSERT_FALSE(buffer.IsFull());
    // Write 2 more samples, a total of 5.
    vector<vector<int>> samples2 = {{8, 8}, {8, 8}, {8, 8}};
    buffer.Write(absl::MakeSpan(samples2));
    ASSERT_EQ(buffer.NumReadableEntries(), 5);
    ASSERT_FALSE(buffer.IsFull());
    // Write 4 more samples, a total of 5.
    vector<vector<int>> samples3 =
        {{-1, -1, -1, -1}, {9, 9, 9, 9}, {8, 8, 8, 8}};
    buffer.Write(absl::MakeSpan(samples3));
    ASSERT_EQ(buffer.NumReadableEntries(), 9);
    ASSERT_TRUE(buffer.IsFull());
    vector<vector<int>> read_data;
    {
      // Read two samples back out of the buffer.
      vector<vector<int>> expected = {{1, 2}, {3, 4}, {6, 7}};
      buffer.Read(2, &read_data);
      ASSERT_THAT(read_data, testing::ElementsAreArray(expected));
      ASSERT_EQ(buffer.NumReadableEntries(), 7);
      ASSERT_FALSE(buffer.IsFull());
    }
    {
      // Read two more samples.
      vector<vector<int>> expected = {{3, 8, 8},
                                      {5, 8, 8},
                                      {8, 8, 8}};
      buffer.Read(3, &read_data);
      ASSERT_THAT(read_data, testing::ElementsAreArray(expected));
      ASSERT_EQ(buffer.NumReadableEntries(), 4);
      ASSERT_FALSE(buffer.IsFull());
    }
    // Write 5 more samples.
    vector<vector<int>> samples =
        {{0, 1, 2, 3, 4}, {0, 1, 2, 3, 4}, {0, 1, 2, 3, 4}};
    buffer.Write(absl::MakeSpan(samples));
    ASSERT_EQ(buffer.NumReadableEntries(), 9);
    ASSERT_TRUE(buffer.IsFull());
    {
      // Read all contents of the buffer.
      vector<vector<int>> expected = {{-1, -1, -1, -1, 0, 1, 2, 3, 4},
                                      { 9,  9,  9,  9, 0, 1, 2, 3, 4},
                                      { 8,  8,  8,  8, 0, 1, 2, 3, 4}
                                     };
      buffer.Read(9, &read_data);
      ASSERT_THAT(read_data, testing::ElementsAreArray(expected));
      ASSERT_EQ(buffer.NumReadableEntries(), 0);
      ASSERT_FALSE(buffer.IsFull());
    }
  }
}

}  // namespace
}  // namespace audio_dsp
