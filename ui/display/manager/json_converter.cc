// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/json_converter.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "ui/display/display_layout.h"

namespace display {

namespace {

// Persistent key names
const char kDefaultUnifiedKey[] = "default_unified";
const char kPrimaryIdKey[] = "primary-id";
const char kDisplayPlacementKey[] = "display_placement";

// DisplayPlacement key names
const char kPositionKey[] = "position";
const char kOffsetKey[] = "offset";
const char kDisplayPlacementDisplayIdKey[] = "display_id";
const char kDisplayPlacementParentDisplayIdKey[] = "parent_display_id";

bool AddLegacyValuesFromValue(const base::Value& value, DisplayLayout* layout) {
  if (!value.is_dict())
    return false;

  absl::optional<int> optional_offset = value.FindIntKey(kOffsetKey);
  if (optional_offset) {
    DisplayPlacement::Position position;
    const std::string* position_str = value.FindStringKey(kPositionKey);
    if (!position_str)
      return false;
    DisplayPlacement::StringToPosition(*position_str, &position);
    layout->placement_list.emplace_back(position, *optional_offset);
  }
  return true;
}

// Returns true if
//     The key is missing - output is left unchanged
//     The key matches the type - output is updated to the value.
bool UpdateFromDict(const base::Value& value,
                    const std::string& field_name,
                    bool* output) {
  const base::Value* field = value.FindKey(field_name);
  if (!field) {
    LOG(WARNING) << "Missing field: " << field_name;
    return true;
  }

  absl::optional<bool> field_value = field->GetIfBool();
  if (!field_value)
    return false;

  *output = *field_value;
  return true;
}

// Returns true if
//     The key is missing - output is left unchanged
//     The key matches the type - output is updated to the value.
bool UpdateFromDict(const base::Value& value,
                    const std::string& field_name,
                    int* output) {
  const base::Value* field = value.FindKey(field_name);
  if (!field) {
    LOG(WARNING) << "Missing field: " << field_name;
    return true;
  }

  absl::optional<int> field_value = field->GetIfInt();
  if (!field_value)
    return false;

  *output = *field_value;
  return true;
}

// Returns true if
//     The key is missing - output is left unchanged
//     The key matches the type - output is updated to the value.
bool UpdateFromDict(const base::Value& value,
                    const std::string& field_name,
                    DisplayPlacement::Position* output) {
  const base::Value* field = value.FindKey(field_name);
  if (!field) {
    LOG(WARNING) << "Missing field: " << field_name;
    return true;
  }

  const std::string* field_value = field->GetIfString();
  if (!field_value)
    return false;

  return field_value->empty()
             ? true
             : DisplayPlacement::StringToPosition(*field_value, output);
}

// Returns true if
//     The key is missing - output is left unchanged
//     The key matches the type - output is updated to the value.
bool UpdateFromDict(const base::Value& value,
                    const std::string& field_name,
                    int64_t* output) {
  const base::Value* field = value.FindKey(field_name);
  if (!field) {
    LOG(WARNING) << "Missing field: " << field_name;
    return true;
  }

  const std::string* field_value = field->GetIfString();
  if (!field_value)
    return false;

  return field_value->empty() ? true
                              : base::StringToInt64(*field_value, output);
}

// Returns true if
//     The key is missing - output is left unchanged
//     The key matches the type - output is updated to the value.
bool UpdateFromDict(const base::Value& value,
                    const std::string& field_name,
                    std::vector<DisplayPlacement>* output) {
  const base::Value* field = value.FindKey(field_name);
  if (!field) {
    LOG(WARNING) << "Missing field: " << field_name;
    return true;
  }

  if (!field->is_list())
    return false;

  const base::Value::ConstListView list = field->GetListDeprecated();
  output->reserve(list.size());

  for (const base::Value& list_item : list) {
    if (!list_item.is_dict())
      return false;

    DisplayPlacement item;
    if (!UpdateFromDict(list_item, kOffsetKey, &item.offset) ||
        !UpdateFromDict(list_item, kPositionKey, &item.position) ||
        !UpdateFromDict(list_item, kDisplayPlacementDisplayIdKey,
                        &item.display_id) ||
        !UpdateFromDict(list_item, kDisplayPlacementParentDisplayIdKey,
                        &item.parent_display_id)) {
      return false;
    }

    output->push_back(item);
  }
  return true;
}

}  // namespace

bool JsonToDisplayLayout(const base::Value& value, DisplayLayout* layout) {
  layout->placement_list.clear();
  if (!value.is_dict())
    return false;

  if (!UpdateFromDict(value, kDefaultUnifiedKey, &layout->default_unified) ||
      !UpdateFromDict(value, kPrimaryIdKey, &layout->primary_id)) {
    return false;
  }

  UpdateFromDict(value, kDisplayPlacementKey, &layout->placement_list);
  if (layout->placement_list.size() != 0u)
    return true;

  // For compatibility with old format.
  return AddLegacyValuesFromValue(value, layout);
}

bool DisplayLayoutToJson(const DisplayLayout& layout, base::Value* value) {
  if (!value->is_dict())
    return false;

  value->SetBoolKey(kDefaultUnifiedKey, layout.default_unified);
  value->SetStringKey(kPrimaryIdKey, base::NumberToString(layout.primary_id));

  base::Value::ListStorage placement_list;
  for (const auto& placement : layout.placement_list) {
    base::Value placement_value(base::Value::Type::DICTIONARY);
    placement_value.SetStringKey(
        kPositionKey, DisplayPlacement::PositionToString(placement.position));
    placement_value.SetIntKey(kOffsetKey, placement.offset);
    placement_value.SetStringKey(kDisplayPlacementDisplayIdKey,
                                 base::NumberToString(placement.display_id));
    placement_value.SetStringKey(
        kDisplayPlacementParentDisplayIdKey,
        base::NumberToString(placement.parent_display_id));
    placement_list.push_back(std::move(placement_value));
  }
  value->SetKey(kDisplayPlacementKey, base::Value(std::move(placement_list)));

  return true;
}

}  // namespace display
