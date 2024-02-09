// Copyright 2016 The Chromium Authors
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

bool AddLegacyValuesFromValue(const base::Value::Dict& dict,
                              DisplayLayout* layout) {
  std::optional<int> optional_offset = dict.FindInt(kOffsetKey);
  if (optional_offset) {
    DisplayPlacement::Position position;
    const std::string* position_str = dict.FindString(kPositionKey);
    if (!position_str)
      return false;
    DisplayPlacement::StringToPosition(*position_str, &position);
    layout->placement_list.emplace_back(position, *optional_offset);
  }
  return true;
}

// Returns true if
//     The key is missing - output is left unchanged
//     The key matches the type - output is updated to the dict.
bool UpdateFromDict(const base::Value::Dict& dict,
                    const std::string& field_name,
                    bool* output) {
  const base::Value* field = dict.Find(field_name);
  if (!field) {
    LOG(WARNING) << "Missing field: " << field_name;
    return true;
  }

  std::optional<bool> field_value = field->GetIfBool();
  if (!field_value)
    return false;

  *output = *field_value;
  return true;
}

// Returns true if
//     The key is missing - output is left unchanged
//     The key matches the type - output is updated to the dict.
bool UpdateFromDict(const base::Value::Dict& dict,
                    const std::string& field_name,
                    int* output) {
  const base::Value* field = dict.Find(field_name);
  if (!field) {
    LOG(WARNING) << "Missing field: " << field_name;
    return true;
  }

  std::optional<int> field_value = field->GetIfInt();
  if (!field_value)
    return false;

  *output = *field_value;
  return true;
}

// Returns true if
//     The key is missing - output is left unchanged
//     The key matches the type - output is updated to the dict.
bool UpdateFromDict(const base::Value::Dict& dict,
                    const std::string& field_name,
                    DisplayPlacement::Position* output) {
  const base::Value* field = dict.Find(field_name);
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
//     The key matches the type - output is updated to the dict.
bool UpdateFromDict(const base::Value::Dict& dict,
                    const std::string& field_name,
                    int64_t* output) {
  const base::Value* field = dict.Find(field_name);
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
//     The key matches the type - output is updated to the dict.
bool UpdateFromDict(const base::Value::Dict& dict,
                    const std::string& field_name,
                    std::vector<DisplayPlacement>* output) {
  const base::Value* field = dict.Find(field_name);
  if (!field) {
    LOG(WARNING) << "Missing field: " << field_name;
    return true;
  }

  if (!field->is_list())
    return false;

  const base::Value::List& list = field->GetList();
  output->reserve(list.size());

  for (const base::Value& list_item : list) {
    if (!list_item.is_dict())
      return false;

    DisplayPlacement item;
    const base::Value::Dict& item_dict = list_item.GetDict();
    if (!UpdateFromDict(item_dict, kOffsetKey, &item.offset) ||
        !UpdateFromDict(item_dict, kPositionKey, &item.position) ||
        !UpdateFromDict(item_dict, kDisplayPlacementDisplayIdKey,
                        &item.display_id) ||
        !UpdateFromDict(item_dict, kDisplayPlacementParentDisplayIdKey,
                        &item.parent_display_id)) {
      return false;
    }

    output->push_back(item);
  }
  return true;
}

}  // namespace

bool JsonToDisplayLayout(const base::Value& value, DisplayLayout* layout) {
  if (!value.is_dict())
    return false;
  return JsonToDisplayLayout(value.GetDict(), layout);
}

bool JsonToDisplayLayout(const base::Value::Dict& dict, DisplayLayout* layout) {
  layout->placement_list.clear();

  if (!UpdateFromDict(dict, kDefaultUnifiedKey, &layout->default_unified) ||
      !UpdateFromDict(dict, kPrimaryIdKey, &layout->primary_id)) {
    return false;
  }

  UpdateFromDict(dict, kDisplayPlacementKey, &layout->placement_list);
  if (layout->placement_list.size() != 0u)
    return true;

  // For compatibility with old format.
  return AddLegacyValuesFromValue(dict, layout);
}

void DisplayLayoutToJson(const DisplayLayout& layout, base::Value::Dict& dict) {
  dict.Set(kDefaultUnifiedKey, layout.default_unified);
  dict.Set(kPrimaryIdKey, base::NumberToString(layout.primary_id));

  base::Value::List placement_list;
  for (const auto& placement : layout.placement_list) {
    base::Value::Dict placement_value;
    placement_value.Set(kPositionKey,
                        DisplayPlacement::PositionToString(placement.position));
    placement_value.Set(kOffsetKey, placement.offset);
    placement_value.Set(kDisplayPlacementDisplayIdKey,
                        base::NumberToString(placement.display_id));
    placement_value.Set(kDisplayPlacementParentDisplayIdKey,
                        base::NumberToString(placement.parent_display_id));
    placement_list.Append(std::move(placement_value));
  }
  dict.Set(kDisplayPlacementKey, std::move(placement_list));
}

}  // namespace display
