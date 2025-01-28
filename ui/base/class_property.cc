// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/class_property.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <set>

#include "base/notreached.h"

namespace ui {

PropertyHandler::PropertyHandler() = default;

PropertyHandler::PropertyHandler(PropertyHandler&&) = default;
PropertyHandler& PropertyHandler::operator=(PropertyHandler&&) = default;

PropertyHandler::~PropertyHandler() {
  ClearProperties();
}

void PropertyHandler::AcquireAllPropertiesFrom(PropertyHandler&& other) {
  for (auto& prop_pair : other.prop_map_) {
    prop_map_.insert_or_assign(std::move(prop_pair.first),
                               std::move(prop_pair.second));
  }
  other.prop_map_.clear();
}

std::set<const void*> PropertyHandler::GetAllPropertyKeys() const {
  std::set<const void*> keys;
  std::ranges::transform(prop_map_, std::inserter(keys, keys.end()),
                         &PropMap::value_type::first);
  return keys;
}

PropertyHandler* PropertyHandler::GetParentHandler() const {
  // If you plan on using cascading properties, you must override this method
  // to return the "parent" handler. If you want to use cascading properties in
  // scenarios where there isn't a notion of a parent, just override this method
  // and return null.
  NOTREACHED();
}

void PropertyHandler::ClearProperties() {
  static constexpr auto dealloc = [](Value& v) {
    if (v.deallocator) {
      (*v.deallocator)(v.value);
    }
  };
  std::ranges::for_each(prop_map_, dealloc, &PropMap::value_type::second);
  prop_map_.clear();
}

int64_t PropertyHandler::SetPropertyInternal(const void* key,
                                             const char* name,
                                             PropertyDeallocator deallocator,
                                             int64_t value,
                                             int64_t default_value) {
  const auto it = prop_map_.find(key);
  int64_t old = default_value;
  if (it != prop_map_.end()) {
    old = it->second.value;
    if (value == default_value) {
      prop_map_.erase(it);
    } else {
      it->second = {.name = name, .value = value, .deallocator = deallocator};
    }
  } else if (value != default_value) {
    prop_map_.emplace(
        key, Value{.name = name, .value = value, .deallocator = deallocator});
  }
  AfterPropertyChange(key, old);
  return old;
}

int64_t PropertyHandler::GetPropertyInternal(const void* key,
                                             int64_t default_value,
                                             bool search_parent) const {
  for (const PropertyHandler* handler = this; handler;
       handler = search_parent ? handler->GetParentHandler() : nullptr) {
    if (const auto it = handler->prop_map_.find(key);
        it != handler->prop_map_.end()) {
      return it->second.value;
    }
  }
  return default_value;
}

}  // namespace ui
