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

#include "audio/dsp/portable/write_wav_string.h"

#include <cstdint>
#include <string>

#include "audio/util/wavfile.h"
#include "gtest/gtest.h"

namespace audio_dsp {
namespace {

// Make sure we get the same result as we do with the really well-tested
// libraries.
TEST(TestWriteStringWav, IntTest) {
  std::vector<int16_t> data(2000);
  for (int i = 0; i < data.size(); ++i) {
    data[i] = i;
  }
  std::string result = "It doesn't matter what is in the string initially.";
  WriteWavToString(&data[0], data.size(), 16000, 2, &result);
  std::string expected;
  audio_util::WriteWavToString(2, 16000, data, &expected);
  EXPECT_EQ(result, expected);
}

TEST(TestWriteStringWav, FloatTest) {
  std::vector<float> data(2000);
  for (int i = 0; i < data.size(); ++i) {
    data[i] = i / 5000.0;  // Keep samples less than 1.
    data[i] /= (1 << 15) - 1;
  }
  std::string result = "It doesn't matter what is in the string initially.";
  WriteWavToString(&data[0], data.size(), 16000, 2, &result);
  std::string expected;
  audio_util::WriteWavToString(2, 16000, data, &expected);
  EXPECT_EQ(result, expected);
}

}  // namespace
}  // namespace audio_dsp
