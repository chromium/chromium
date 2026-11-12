/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBARTBASE_BASE_HASH_MAP_H_
#define ART_LIBARTBASE_BASE_HASH_MAP_H_

#include <utility>

#include "hash_set.h"

namespace art {

template <typename Key, typename Value, typename HashFn>
class HashMapHashWrapper {
 public:
  size_t operator()(const Key& key) const {
    return hash_fn_(key);
  }

  size_t operator()(const std::pair<Key, Value>& pair) const {
    return hash_fn_(pair.first);
  }

 private:
  HashFn hash_fn_;
};

template <typename Key, typename Value, typename PredFn>
class HashMapPredWrapper {
 public:
  bool operator()(const std::pair<Key, Value>& a, const std::pair<Key, Value>& b) const {
    return pred_fn_(a.first, b.first);
  }

  template <typename Element>
  bool operator()(const std::pair<Key, Value>& a, const Element& element) const {
    return pred_fn_(a.first, element);
  }

 private:
  PredFn pred_fn_;
};

template <typename Key, typename Value>
class DefaultMapEmptyFn {
 public:
  void MakeEmpty(std::pair<Key, Value>& item) const {
    item = std::pair<Key, Value>();
  }
  bool IsEmpty(const std::pair<Key, Value>& item) const {
    return item.first == Key();
  }
};

template <class Key,
          class Value,
          class EmptyFn = DefaultMapEmptyFn<Key, Value>,
          class HashFn = DefaultHashFn<Key>,
          class Pred = DefaultPred<Key>,
          class Alloc = std::allocator<std::pair<Key, Value>>>
class HashMap : public HashSet<std::pair<Key, Value>,
                               EmptyFn,
                               HashMapHashWrapper<Key, Value, HashFn>,
                               HashMapPredWrapper<Key, Value, Pred>,
                               Alloc> {
 private:
  using Base = HashSet<std::pair<Key, Value>,
                       EmptyFn,
                       HashMapHashWrapper<Key, Value, HashFn>,
                       HashMapPredWrapper<Key, Value, Pred>,
                       Alloc>;

 public:
  // Inherit constructors.
  using Base::Base;

  // Used to insert a new mapping.
  typename Base::iterator Overwrite(const Key& k, const Value& v) {
    auto res = Base::insert({ k, v }).first;
    *res = { k, v };
    return res;
  }
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_HASH_MAP_H_
