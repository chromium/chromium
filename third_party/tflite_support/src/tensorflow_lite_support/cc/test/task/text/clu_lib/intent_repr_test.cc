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

#include "tensorflow_lite_support/cc/task/text/clu_lib/intent_repr.h"

#include "tensorflow_lite_support/cc/port/default/status_matchers.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite::task::text::clu {

TEST(IntentClassification, IntentRepr) {
  const auto intent_repr = IntentRepr::Create("open_intent", "media", false);
  EXPECT_EQ(intent_repr.FullName(), "media~~open_intent");
}

TEST(IntentClassification, IntentRepr2) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(const auto intent_repr,
                       IntentRepr::CreateFromFullName("REQUEST"));
  EXPECT_EQ(intent_repr.Name(), "REQUEST");
  EXPECT_EQ(intent_repr.Domain(), "");
}

TEST(IntentClassification, IntentRepr3) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      const auto intent_repr,
      IntentRepr::CreateFromFullName("nlp_semantic_parsing.models.planning."
                                     "Planning.planning~~show_attribute="
                                     "SHOW_COUNT"));
  EXPECT_EQ(intent_repr.Name(), "show_attribute=SHOW_COUNT");
  EXPECT_EQ(intent_repr.Domain(),
            "nlp_semantic_parsing.models.planning.Planning.planning");
}

TEST(IntentClassification, IntentReprSharing) {
  const auto intent_repr = IntentRepr::Create("open_intent", "media", true);
  EXPECT_EQ(intent_repr.FullName(), "open_intent");
}

}  // namespace tflite::task::text::clu
