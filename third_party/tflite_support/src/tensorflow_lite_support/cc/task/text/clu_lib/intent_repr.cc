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

#include <vector>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "absl/strings/str_split.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/task/text/clu_lib/constants.h"

namespace tflite::task::text::clu {

// IntentRepr

std::string IntentRepr::FullName() const {
  if (domain_.empty()) return name_;
  return absl::StrCat(domain_, kNamespaceDelim, name_);
}

absl::StatusOr<IntentRepr> IntentRepr::CreateFromFullName(
    const absl::string_view full_name) {
  IntentRepr ret;
  std::vector<std::string> splits = absl::StrSplit(full_name, kNamespaceDelim);
  // Ensure we only end up with at most two parts after splitting.
  if (splits.size() > 2) {
    return absl::InternalError(absl::StrCat("invalid argument: ", full_name));
  }
  if (splits.size() == 2) ret.domain_ = splits[0];
  ret.name_ = splits[splits.size() - 1];
  return ret;
}

IntentRepr IntentRepr::Create(absl::string_view name, absl::string_view domain,
                              const bool share_across_domains) {
  IntentRepr ret;
  ret.name_ = std::string(name);
  if (!share_across_domains) ret.domain_ = std::string(domain);
  return ret;
}

}  // namespace tflite::task::text::clu
