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
  const base::DictionaryValue* dict_value = nullptr;
  if (!value.GetAsDictionary(&dict_value))
    return false;
  int offset;
  if (dict_value->GetInteger(kOffsetKey, &offset)) {
    DisplayPlacement::Position position;
    std::string position_str;
    if (!dict_value->GetString(kPositionKey, &position_str))
      return false;
    DisplayPlacement::StringToPosition(position_str, &position);
    layout->placement_list.emplace_back(position, offset);
  }
  return true;
}

// Returns true if
//     The key is missing - output is left unchanged
//     The key matches the type - output is updated to the value.
template <typename Getter, typename Output>
bool UpdateFromDict(const base::DictionaryValue* dict_value,
                    const std::string& field_name,
                    Getter getter,
                    Output* output) {
  const base::Value* field = nullptr;
  if (!dict_value->Get(field_name, &field)) {
    LOG(WARNING) << "Missing field: " << field_name;
    return true;
  }

  return (field->*getter)(output);
}

// No implementation here as specialization is required.
template <typename Output>
bool UpdateFromDict(const base::DictionaryValue* dict_value,
                    const std::string& field_name,
                    Output* output);

template <>
bool UpdateFromDict(const base::DictionaryValue* dict_value,
                    const std::string& field_name,
                    bool* output) {
  return UpdateFromDict(dict_value, field_name, &base::Value::GetAsBoolean,
                        output);
}

template <>
bool UpdateFromDict(const base::DictionaryValue* dict_value,
                    const std::string& field_name,
                    int* output) {
  return UpdateFromDict(dict_value, field_name, &base::Value::GetAsInteger,
                        output);
}

template <>
bool UpdateFromDict(const base::DictionaryValue* dict_value,
                    const std::string& field_name,
                    DisplayPlacement::Position* output) {
  bool (base::Value::*getter)(std::string*) const = &base::Value::GetAsString;
  std::string value;
  if (!UpdateFromDict(dict_value, field_name, getter, &value))
    return false;

  return value.empty() ? true
                       : DisplayPlacement::StringToPosition(value, output);
}

template <>
bool UpdateFromDict(const base::DictionaryValue* dict_value,
                    const std::string& field_name,
                    int64_t* output) {
  bool (base::Value::*getter)(std::string*) const = &base::Value::GetAsString;
  std::string value;
  if (!UpdateFromDict(dict_value, field_name, getter, &value))
    return false;

  return value.empty() ? true : base::StringToInt64(value, output);
}

template <>
bool UpdateFromDict(const base::DictionaryValue* dict_value,
                    const std::string& field_name,
                    std::vector<DisplayPlacement>* output) {
  bool (base::Value::*getter)(const base::ListValue**) const =
      &base::Value::GetAsList;
  const base::ListValue* list = nullptr;
  if (!UpdateFromDict(dict_value, field_name, getter, &list))
    return false;

  if (list == nullptr)
    return true;

  output->reserve(list->GetSize());
  for (const auto& list_item : *list) {
    const base::DictionaryValue* item_values = nullptr;
    if (!list_item.GetAsDictionary(&item_values))
      return false;

    DisplayPlacement item;
    if (!UpdateFromDict(item_values, kOffsetKey, &item.offset) ||
        !UpdateFromDict(item_values, kPositionKey, &item.position) ||
        !UpdateFromDict(item_values, kDisplayPlacementDisplayIdKey,
                        &item.display_id) ||
        !UpdateFromDict(item_values, kDisplayPlacementParentDisplayIdKey,
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
  const base::DictionaryValue* dict_value = nullptr;
  if (!value.GetAsDictionary(&dict_value))
    return false;

  if (!UpdateFromDict(dict_value, kDefaultUnifiedKey,
                      &layout->default_unified) ||
      !UpdateFromDict(dict_value, kPrimaryIdKey, &layout->primary_id)) {
    return false;
  }

  UpdateFromDict(dict_value, kDisplayPlacementKey, &layout->placement_list);

  if (layout->placement_list.size() != 0u)
    return true;

  // For compatibility with old format.
  return AddLegacyValuesFromValue(value, layout);
}

bool DisplayLayoutToJson(const DisplayLayout& layout, base::Value* value) {
  base::DictionaryValue* dict_value = nullptr;
  if (!value->GetAsDictionary(&dict_value))
    return false;

  dict_value->SetBoolean(kDefaultUnifiedKey, layout.default_unified);
  dict_value->SetString(kPrimaryIdKey, base::NumberToString(layout.primary_id));

  std::unique_ptr<base::ListValue> placement_list(new base::ListValue);
  for (const auto& placement : layout.placement_list) {
    std::unique_ptr<base::DictionaryValue> placement_value(
        new base::DictionaryValue);
    placement_value->SetString(
        kPositionKey, DisplayPlacement::PositionToString(placement.position));
    placement_value->SetInteger(kOffsetKey, placement.offset);
    placement_value->SetString(kDisplayPlacementDisplayIdKey,
                               base::NumberToString(placement.display_id));
    placement_value->SetString(
        kDisplayPlacementParentDisplayIdKey,
        base::NumberToString(placement.parent_display_id));
    placement_list->Append(std::move(placement_value));
  }
  dict_value->Set(kDisplayPlacementKey, std::move(placement_list));
  return true;
}

}  // namespace display
