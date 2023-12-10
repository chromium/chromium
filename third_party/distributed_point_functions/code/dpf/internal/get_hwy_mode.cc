// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dpf/internal/get_hwy_mode.h"

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "dpf/internal/get_hwy_mode.cc"
#include "absl/strings/string_view.h"
#include "hwy/foreach_target.h"
// clang-format on

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace distributed_point_functions {
namespace dpf_internal {
namespace HWY_NAMESPACE {

const absl::string_view GetHwyModeAsString() {
  return hwy::TargetName(HWY_TARGET);
}

}  // namespace HWY_NAMESPACE

#if HWY_ONCE || HWY_IDE

HWY_EXPORT(GetHwyModeAsString);
const absl::string_view GetHwyModeAsString() {
  return HWY_DYNAMIC_DISPATCH(GetHwyModeAsString)();
}

#endif

}  // namespace dpf_internal
}  // namespace distributed_point_functions
HWY_AFTER_NAMESPACE();
