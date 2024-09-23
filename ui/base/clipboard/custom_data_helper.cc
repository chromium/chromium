// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(dcheng): For efficiency reasons, consider passing custom data around
// as a vector instead. It allows us to append a
// std::pair<std::u16string, std::u16string> and swap the deserialized values.

#include "ui/base/clipboard/custom_data_helper.h"

#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/pickle.h"

namespace ui {

namespace {

bool SkipString16(base::PickleIterator* iter) {
  DCHECK(iter);

  size_t len;
  if (!iter->ReadLength(&len))
    return false;
  return iter->SkipBytes(len * sizeof(char16_t));
}

}  // namespace

void ReadCustomDataTypes(base::span<const uint8_t> data,
                         std::vector<std::u16string>* types) {
  base::Pickle pickle = base::Pickle::WithData(data);
  base::PickleIterator iter(pickle);

  uint32_t size = 0;
  if (!iter.ReadUInt32(&size))
    return;

  // Keep track of the original elements in the types vector. On failure, we
  // truncate the vector to the original size since we want to ignore corrupt
  // custom data pickles.
  size_t original_size = types->size();

  for (uint32_t i = 0; i < size; ++i) {
    types->push_back(std::u16string());
    if (!iter.ReadString16(&types->back()) || !SkipString16(&iter)) {
      types->resize(original_size);
      return;
    }
  }
}

std::optional<std::u16string> ReadCustomDataForType(
    base::span<const uint8_t> data,
    std::u16string_view type) {
  base::Pickle pickle = base::Pickle::WithData(data);
  base::PickleIterator iter(pickle);

  uint32_t size = 0;
  if (!iter.ReadUInt32(&size))
    return std::nullopt;

  for (uint32_t i = 0; i < size; ++i) {
    std::u16string deserialized_type;
    if (!iter.ReadString16(&deserialized_type)) {
      return std::nullopt;
    }
    if (deserialized_type == type) {
      std::u16string result;
      if (iter.ReadString16(&result)) {
        return std::move(result);
      }
      return std::nullopt;
    }
    if (!SkipString16(&iter)) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

std::optional<std::unordered_map<std::u16string, std::u16string>>
ReadCustomDataIntoMap(base::span<const uint8_t> data) {
  base::Pickle pickle = base::Pickle::WithData(data);
  base::PickleIterator iter(pickle);

  uint32_t size = 0;
  if (!iter.ReadUInt32(&size)) {
    return std::nullopt;
  }

  std::unordered_map<std::u16string, std::u16string> result;
  for (uint32_t i = 0; i < size; ++i) {
    std::u16string custom_type;
    if (!iter.ReadString16(&custom_type)) {
      return std::nullopt;
    }
    std::u16string custom_data;
    if (!iter.ReadString16(&custom_data)) {
      return std::nullopt;
    }
    result.insert({std::move(custom_type), std::move(custom_data)});
  }
  return std::move(result);
}

void WriteCustomDataToPickle(
    const std::unordered_map<std::u16string, std::u16string>& data,
    base::Pickle* pickle) {
  pickle->WriteUInt32(data.size());
  for (const auto& it : data) {
    pickle->WriteString16(it.first);
    pickle->WriteString16(it.second);
  }
}

void WriteCustomDataToPickle(
    const base::flat_map<std::u16string, std::u16string>& data,
    base::Pickle* pickle) {
  pickle->WriteUInt32(data.size());
  for (const auto& it : data) {
    pickle->WriteString16(it.first);
    pickle->WriteString16(it.second);
  }
}

}  // namespace ui
