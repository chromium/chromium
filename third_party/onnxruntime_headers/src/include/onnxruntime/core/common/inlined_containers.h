// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cmath>

#include "core/common/inlined_containers_fwd.h"

#ifndef DISABLE_ABSEIL

#ifdef _MSC_VER
#pragma warning(push)
// C4127: conditional expression is constant
#pragma warning(disable : 4127)
// C4324: structure was padded due to alignment specifier
// Usage of alignas causes some internal padding in places.
#pragma warning(disable : 4324)
#endif  // _MSC_VER

#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>

#include <absl/container/node_hash_set.h>
#include <absl/container/node_hash_map.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER

#else  // DISABLE_ABSEIL

#include <unordered_set>
#include <unordered_map>
#include <set>
#include <map>

#endif  // DISABLE_ABSEIL

namespace onnxruntime {

#ifndef DISABLE_ABSEIL
// InlinedHashSet and InlinedHashMap are preferred
// hash based containers. They store their values in the
// buckets array that is allocated in one shot. It eliminates
// per-node new/delete calls. Always call reserve() on any hash set/map
// when the number of items is known in advance.
// This does not allocate a dummy 'end' node on default construction.
template <typename T, typename Allocator>
class InlinedHashSet : public absl::flat_hash_set<T,
                                                  absl::container_internal::hash_default_hash<T>,
                                                  absl::container_internal::hash_default_eq<T>,
                                                  Allocator> {
  using Base = absl::flat_hash_set<T,
                                   absl::container_internal::hash_default_hash<T>,
                                   absl::container_internal::hash_default_eq<T>,
                                   Allocator>;

 public:
  using Base::Base;
};

template <typename Key, typename Value,
          typename Allocator>
class InlinedHashMap : public absl::flat_hash_map<Key, Value,
                                                  absl::container_internal::hash_default_hash<Key>,
                                                  absl::container_internal::hash_default_eq<Key>,
                                                  Allocator> {
  using Base = absl::flat_hash_map<Key, Value,
                                   absl::container_internal::hash_default_hash<Key>,
                                   absl::container_internal::hash_default_eq<Key>,
                                   Allocator>;

 public:
  using Base::Base;
};

// Use this hash set/map where pointer stability is required, otherwise use
// InlinedHashSet and InlinedHashMap
// This does not allocate a dummy 'end' node on default construction.
// Use reserve() when the number of elements is known.
template <typename T, typename Allocator>
class NodeHashSet : public absl::node_hash_set<T,
                                               absl::container_internal::hash_default_hash<T>,
                                               absl::container_internal::hash_default_eq<T>,
                                               Allocator> {
  using Base = absl::node_hash_set<T,
                                   absl::container_internal::hash_default_hash<T>,
                                   absl::container_internal::hash_default_eq<T>,
                                   Allocator>;

 public:
  using Base::Base;
};

template <typename Key, typename Value, typename Allocator>
class NodeHashMap : public absl::node_hash_map<Key, Value,
                                               absl::container_internal::hash_default_hash<Key>,
                                               absl::container_internal::hash_default_eq<Key>,
                                               Allocator> {
  using Base = absl::node_hash_map<Key, Value,
                                   absl::container_internal::hash_default_hash<Key>,
                                   absl::container_internal::hash_default_eq<Key>,
                                   Allocator>;

 public:
  using Base::Base;
};

#else  // DISABLE_ABSEIL

template <typename T, typename Allocator>
class InlinedHashSet : public std::unordered_set<T,
                                                 std::hash<T>,
                                                 std::equal_to<T>,
                                                 Allocator> {
  using Base = std::unordered_set<T,
                                  std::hash<T>,
                                  std::equal_to<T>,
                                  Allocator>;

 public:
  using Base::Base;
};

template <typename Key, typename Value,
          typename Allocator>
class InlinedHashMap : public std::unordered_map<Key, Value,
                                                 std::hash<Key>,
                                                 std::equal_to<Key>,
                                                 Allocator> {
  using Base = std::unordered_map<Key, Value,
                                  std::hash<Key>,
                                  std::equal_to<Key>,
                                  Allocator>;

 public:
  using Base::Base;
};

// Use this hash set/map where pointer stability is required, otherwise use
// InlinedHashSet and InlinedHashMap
// This does not allocate a dummy 'end' node on default construction.
// Use reserve() when the number of elements is known.
template <typename T, typename Allocator>
class NodeHashSet : public std::unordered_set<T,
                                              std::hash<T>,
                                              std::equal_to<T>,
                                              Allocator> {
  using Base = std::unordered_set<T,
                                  std::hash<T>,
                                  std::equal_to<T>,
                                  Allocator>;

 public:
  using Base::Base;
};

template <typename Key, typename Value, typename Allocator>
class NodeHashMap : public std::unordered_map<Key, Value,
                                              std::hash<Key>,
                                              std::equal_to<Key>,
                                              Allocator> {
  using Base = std::unordered_map<Key, Value,
                                  std::hash<Key>,
                                  std::equal_to<Key>,
                                  Allocator>;

 public:
  using Base::Base;
};

#endif  // DISABLE_ABSEIL

}  // namespace onnxruntime
