// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace base {
template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
struct flat_tree {};

struct identity {
  template <class T>
  constexpr T&& operator()(T&& t) const noexcept {
    return t;
  }
};

template <class Key,
          class Compare = std::less<>,
          class Container = std::vector<Key>>
using flat_set = flat_tree<Key, identity, Compare, Container>;

template <class T, class Container = std::list<T>>
using queue = std::queue<T, Container>;

template <class T, class Container = std::list<T>>
using stack = std::stack<T, Container>;
}  // namespace base

class SomeClass {};

using LIST = std::list<SomeClass*>;

class MyClass {
 public:
  using VECTOR = std::vector<SomeClass*>;
  // Errors expected. All these containers are included in test config.
  VECTOR aliased_type_member;
  LIST aliased_list_member;
  std::vector<SomeClass*> vector_member;
  std::list<SomeClass*> list_member;
  std::set<SomeClass*> set_member;
  std::stack<SomeClass*> stack_member;
  std::queue<SomeClass*> queue_member;
  std::deque<SomeClass*> dequeue_member;
  std::unordered_set<SomeClass*> unordered_set_member;
  base::flat_set<SomeClass*> flat_set_member;
  base::queue<SomeClass*> base_queue_member;
  base::stack<SomeClass*> base_stack_member;

  // No error expected, maps are not enforced for now.
  std::map<SomeClass*, SomeClass*> map_member;
  // No error expected, maps are not enforced for now.
  std::unordered_map<SomeClass*, SomeClass*> unordered_map_member;

  // No error expected, const char* pointers are excluded from the rewrite.
  std::vector<const char*> const_char_member;
};

// The field below won't compile without the |typename| keyword (because
// at this point we don't know if MaybeProvidesType<T>::Type is a type,
// value or something else).
template <typename T>
struct MaybeProvidesType;
template <typename T>
struct DependentNameTest {
  using LIST = std::list<typename MaybeProvidesType<T>::Type*>;

  // Error expected for all of the following fields.  Even though
  // MaybeProvidesType<T>::Type is an unknown type,
  // MaybeProvidesType<T>::Type* is a pointer so an error is expected.
  std::vector<typename MaybeProvidesType<T>::Type*> vector_field;
  base::stack<typename MaybeProvidesType<T>::Type*> base_stack_field;
  base::flat_set<typename MaybeProvidesType<T>::Type*> flat_set_field;
  LIST list_field;
};

struct StackAllocatedType {
  using IsStackAllocatedTypeMarker [[maybe_unused]] = int;
};
struct StackAllocatedSubType : public StackAllocatedType {};
struct NonStackAllocatedType {};

struct S {
  // No error expected.
  // Containers with pointers to stack-allocated types shouldn't warn
  std::list<StackAllocatedSubType*> member1;
  // Error expected.
  // Enforced container with a non-stack-allocated pointer should be flagged.
  std::list<NonStackAllocatedType*> member2;
};
