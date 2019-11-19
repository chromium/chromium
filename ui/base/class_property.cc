// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/class_property.h"

#include <algorithm>
#include <utility>

namespace ui {

PropertyHandler::PropertyHandler() = default;

PropertyHandler::PropertyHandler(PropertyHandler&& other) = default;

PropertyHandler::~PropertyHandler() {
  ClearProperties();
}

void PropertyHandler::AcquireAllPropertiesFrom(PropertyHandler&& other) {
  for (auto& prop_pair : other.prop_map_)
    prop_map_[std::move(prop_pair.first)] = std::move(prop_pair.second);
  other.prop_map_.clear();
}

int64_t PropertyHandler::SetPropertyInternal(const void* key,
                                             const char* name,
                                             PropertyDeallocator deallocator,
                                             int64_t value,
                                             int64_t default_value) {
  int64_t old = GetPropertyInternal(key, default_value);
  if (value == default_value) {
    prop_map_.erase(key);
  } else {
    Value prop_value;
    prop_value.name = name;
    prop_value.value = value;
    prop_value.deallocator = deallocator;
    prop_map_[key] = prop_value;
  }
  AfterPropertyChange(key, old);
  return old;
}

void PropertyHandler::ClearProperties() {
  // Clear properties.
  for (std::map<const void*, Value>::const_iterator iter = prop_map_.begin();
       iter != prop_map_.end();
       ++iter) {
    if (iter->second.deallocator)
      (*iter->second.deallocator)(iter->second.value);
  }
  prop_map_.clear();
}

int64_t PropertyHandler::GetPropertyInternal(const void* key,
                                             int64_t default_value) const {
  auto iter = prop_map_.find(key);
  if (iter == prop_map_.end())
    return default_value;
  return iter->second.value;
}

std::set<const void*> PropertyHandler::GetAllPropertyKeys() const {
  std::set<const void*> keys;
  for (auto& pair : prop_map_)
    keys.insert(pair.first);
  return keys;
}

}  // namespace ui
