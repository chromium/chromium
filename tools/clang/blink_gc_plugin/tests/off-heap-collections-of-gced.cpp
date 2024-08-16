// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "off-heap-collections-of-gced.h"

namespace blink {

class WithCollections : public GarbageCollected<WithCollections> {
 public:
  virtual void Trace(Visitor* v) const {
    (void)set_;
    (void)vector_;
    (void)map_key_;
    (void)map_value_;
    (void)set_ptr_;
    (void)vector_ref_;
    (void)map_const_;
    (void)vector_pair_;
    (void)ignored_set_;
    (void)array_;
    (void)array_of_vectors_;

    v->Trace(heap_hash_set_);
    v->Trace(heap_deque_);
    v->Trace(heap_vector_);
    v->Trace(heap_linked_hash_set_);
    v->Trace(heap_hash_counted_set_);
    v->Trace(heap_hash_map_key_);
    v->Trace(heap_hash_map_value_);

    // The following calls are not needed and are added merely to silence false
    // untraced field errors.
    v->Trace(wtf_hash_set_);
    v->Trace(wtf_deque_);
    v->Trace(wtf_vector_);
    v->Trace(wtf_hash_map_key_);
    v->Trace(wtf_hash_map_value_);
  }

 private:
  // Bad stl collections:
  std::set<Base> set_;
  std::vector<Derived> vector_;
  std::map<Mixin, int> map_key_;
  std::unordered_map<int, Base> map_value_;
  std::unordered_set<Base*> set_ptr_;
  std::vector<Derived&> vector_ref_;
  std::map<const Mixin, int> map_const_;
  std::vector<std::pair<Base, int>> vector_pair_;
  std::array<Base, 4> array_;
  std::array<HeapVector<Base>, 4> array_of_vectors_;

  // Bad WTF collections:
  WTF::HashSet<Base> wtf_hash_set_;
  WTF::Deque<Derived> wtf_deque_;
  WTF::Vector<Mixin> wtf_vector_;
  WTF::LinkedHashSet<Base*> wtf_linked_hash_set_;
  WTF::HashCountedSet<Derived&> wtf_hash_counted_set_;
  WTF::HashMap<Mixin, bool> wtf_hash_map_key_;
  WTF::HashMap<double, const Base> wtf_hash_map_value_;

  // Good collections:
  blink::HeapHashSet<Base> heap_hash_set_;
  blink::HeapDeque<Derived> heap_deque_;
  blink::HeapVector<Mixin> heap_vector_;
  blink::HeapLinkedHashSet<Base> heap_linked_hash_set_;
  blink::HeapHashCountedSet<Derived> heap_hash_counted_set_;
  blink::HeapHashMap<Mixin, bool> heap_hash_map_key_;
  blink::HeapHashMap<double, Base> heap_hash_map_value_;

  GC_PLUGIN_IGNORE("For testing")
  std::set<Base> ignored_set_;
};

class StackAllocated {
  STACK_ALLOCATED();

 public:
  StackAllocated() {
    (void)array_;
    (void)array_of_vectors_;
  }

 private:
  std::array<Base, 4> array_;
  std::array<HeapVector<Base>, 4> array_of_vectors_;
};

void DisallowedUseOfCollections() {
  // Bad stl collections:
  std::set<Base> set;
  (void)set;
  std::vector<Derived> vector;
  (void)vector;
  std::map<Mixin, int> map_key;
  (void)map_key;
  std::unordered_map<int, Base> map_value;
  (void)map_value;
  std::unordered_set<Base*> set_ptr;
  (void)set_ptr;
  std::vector<Derived&> vector_ref;
  (void)vector_ref;
  std::map<const Mixin, int> map_const;
  (void)map_const;
  std::vector<std::pair<Base, int>> vector_pair;
  (void)vector_pair;

  // Bad WTF collections:
  WTF::HashSet<Base> wtf_hash_set;
  WTF::Deque<Derived> wtf_deque;
  WTF::Vector<Mixin> wtf_vector;
  WTF::LinkedHashSet<Base*> wtf_linked_hash_set;
  WTF::HashCountedSet<Derived&> wtf_hash_counted_set;
  WTF::HashMap<Mixin, bool> wtf_hash_map_key;
  WTF::HashMap<double, const Base> wtf_hash_map_value;

  // Good collections:
  blink::HeapHashSet<Base> heap_hash_set;
  blink::HeapDeque<Derived> heap_deque;
  blink::HeapVector<Mixin> heap_vector;
  blink::HeapLinkedHashSet<Base> heap_linked_hash_set;
  blink::HeapHashCountedSet<Derived> heap_hash_counted_set;
  blink::HeapHashMap<Mixin, bool> heap_hash_map_key;
  blink::HeapHashMap<double, Base> heap_hash_map_value;

  std::array<Base, 4> array;
  (void)array;
  std::array<HeapVector<Base>, 4> array_of_vectors;
  (void)array_of_vectors;

  GC_PLUGIN_IGNORE("For testing")
  std::set<Base> ignored_set;
  (void)ignored_set;
}

}  // namespace blink
