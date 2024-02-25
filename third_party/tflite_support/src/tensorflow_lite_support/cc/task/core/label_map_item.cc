/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow_lite_support/cc/task/core/label_map_item.h"

#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/str_split.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"

namespace tflite {
namespace task {
namespace core {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;

StatusOr<std::vector<LabelMapItem>> BuildLabelMapFromFiles(
    absl::string_view labels_file, absl::string_view display_names_file) {
  if (labels_file.empty()) {
    return CreateStatusWithPayload(StatusCode::kInvalidArgument,
                                   "Expected non-empty labels file.",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  std::vector<absl::string_view> labels = absl::StrSplit(labels_file, '\n');
  // In most cases, there is an empty line (i.e. newline character) at the end
  // of the file that needs to be ignored. In such a situation, StrSplit() will
  // produce a vector with an empty string as final element. Also note that in
  // case `labels_file` is entirely empty, StrSplit() will produce a vector with
  // one single empty substring, so there's no out-of-range risk here.
  if (labels[labels.size() - 1].empty()) {
    labels.pop_back();
  }

  std::vector<LabelMapItem> label_map_items;
  label_map_items.reserve(labels.size());
  for (int i = 0; i < labels.size(); ++i) {
    label_map_items.emplace_back(LabelMapItem{.name = std::string(labels[i])});
  }

  if (!display_names_file.empty()) {
    std::vector<std::string> display_names =
        absl::StrSplit(display_names_file, '\n');
    // In most cases, there is an empty line (i.e. newline character) at the end
    // of the file that needs to be ignored. See above.
    if (display_names[display_names.size() - 1].empty()) {
      display_names.pop_back();
    }
    if (display_names.size() != labels.size()) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat(
              "Mismatch between number of labels (%d) and display names (%d).",
              labels.size(), display_names.size()),
          TfLiteSupportStatus::kMetadataNumLabelsMismatchError);
    }
    for (int i = 0; i < display_names.size(); ++i) {
      label_map_items[i].display_name = display_names[i];
    }
  }
  return label_map_items;
}

absl::Status LabelHierarchy::InitializeFromLabelMap(
    std::vector<LabelMapItem> label_map_items) {
  parents_map_.clear();
  for (const LabelMapItem& label : label_map_items) {
    for (const std::string& child_name : label.child_name) {
      parents_map_[child_name].insert(label.name);
    }
  }
  if (parents_map_.empty()) {
    return CreateStatusWithPayload(StatusCode::kInvalidArgument,
                                   "Input labelmap is not hierarchical: there "
                                   "is no parent-child relationship.",
                                   TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

bool LabelHierarchy::HaveAncestorDescendantRelationship(
    const std::string& ancestor_name,
    const std::string& descendant_name) const {
  absl::flat_hash_set<std::string> ancestors;
  GetAncestors(descendant_name, &ancestors);
  return ancestors.contains(ancestor_name);
}

absl::flat_hash_set<std::string> LabelHierarchy::GetParents(
    const std::string& name) const {
  absl::flat_hash_set<std::string> parents;
  auto it = parents_map_.find(name);
  if (it != parents_map_.end()) {
    for (const std::string& parent_name : it->second) {
      parents.insert(parent_name);
    }
  }
  return parents;
}

void LabelHierarchy::GetAncestors(
    const std::string& name,
    absl::flat_hash_set<std::string>* ancestors) const {
  const absl::flat_hash_set<std::string> parents = GetParents(name);
  for (const std::string& parent_name : parents) {
    auto it = ancestors->insert(parent_name);
    if (it.second) {
      GetAncestors(parent_name, ancestors);
    }
  }
}

}  // namespace core
}  // namespace task
}  // namespace tflite
