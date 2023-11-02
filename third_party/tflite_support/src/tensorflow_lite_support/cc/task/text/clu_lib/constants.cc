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

#include "tensorflow_lite_support/cc/task/text/clu_lib/constants.h"

namespace tflite::task::text::clu {

// Static member defs
constexpr char CLUFeature::kDialogId[];
constexpr char CLUFeature::kTurnIndex[];
constexpr char CLUFeature::kUtteranceSeq[];
constexpr char CLUFeature::kUtteranceTokenIdSeq[];
constexpr char CLUFeature::kUtteranceCharSeq[];
constexpr char CLUFeature::kUtteranceEngineeredSeq[];
constexpr char CLUFeature::kSeqLength[];
constexpr char CLUFeature::kWord[];
constexpr char CLUFeature::kRawUtterance[];
constexpr char CLUFeature::kTokenAlignmentSeq[];
constexpr char CLUFeature::kSlotTagSeq[];
constexpr char CLUFeature::kIntents[];
constexpr char CLUFeature::kDomain[];
constexpr char CLUFeature::kAnnotatedSpanSeq[];
constexpr char CLUFeature::kAnnotatedSpanScoreSeq[];
constexpr char CLUFeature::kNumHistoryTurns[];
constexpr char CLUFeature::kHistorySeq[];
constexpr char CLUFeature::kHistorySeqLength[];
constexpr char CLUFeature::kSurfaceType[];
constexpr char CLUFeature::kPosTagSeq[];
constexpr char CLUFeature::kHasDomain[];
constexpr char CLUFeature::kHasIntents[];
constexpr char CLUFeature::kHasSlots[];

}  // namespace tflite::task::text::clu
