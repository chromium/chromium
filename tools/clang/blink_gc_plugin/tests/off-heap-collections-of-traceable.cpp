// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "off-heap-collections-of-traceable.h"

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
    (void)span_;
    (void)span_ptr_;

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
  std::set<Traceable> set_;
  std::vector<Traceable> vector_;
  std::map<Traceable, int> map_key_;
  std::unordered_map<int, Traceable> map_value_;
  std::unordered_set<Traceable*> set_ptr_;
  std::vector<Traceable&> vector_ref_;
  std::map<const Traceable, int> map_const_;
  std::vector<std::pair<Traceable, int>> vector_pair_;
  std::array<Traceable, 4> array_;
  std::array<Traceable, 4> array_of_vectors_;
  std::span<Traceable, 4> span_;
  std::span<Traceable*, 4> span_ptr_;

  // Bad WTF collections:
  HashSet<Traceable> wtf_hash_set_;
  Deque<Traceable> wtf_deque_;
  Vector<Traceable> wtf_vector_;
  LinkedHashSet<Traceable*> wtf_linked_hash_set_;
  HashCountedSet<Traceable&> wtf_hash_counted_set_;
  HashMap<Traceable, bool> wtf_hash_map_key_;
  HashMap<double, const Traceable> wtf_hash_map_value_;

  GC_PLUGIN_IGNORE("For testing")
  std::set<Traceable> ignored_set_;
};

class StackAllocated {
  STACK_ALLOCATED();

 public:
  StackAllocated() {
    (void)array_;
    (void)span_;
    (void)span_ptr_;
  }

 private:
  std::array<Traceable, 4> array_;
  std::span<Traceable, 4> span_;
  std::span<Traceable*, 4> span_ptr_;
};

void DisallowedUseOfCollections() {
  // Bad stl collections:
  std::set<Traceable> set;
  (void)set;
  std::vector<Traceable> vector;
  (void)vector;
  std::map<Traceable, int> map_key;
  (void)map_key;
  std::unordered_map<int, Traceable> map_value;
  (void)map_value;
  std::unordered_set<Traceable*> set_ptr;
  (void)set_ptr;
  std::vector<Traceable&> vector_ref;
  (void)vector_ref;
  std::map<const Traceable, int> map_const;
  (void)map_const;
  std::vector<std::pair<Traceable, int>> vector_pair;
  (void)vector_pair;
  std::span<Traceable, 4> span_;
  (void)span_;
  std::span<Traceable*, 4> span_ptr_;
  (void)span_ptr_;

  // Bad WTF collections:
  HashSet<Traceable> wtf_hash_set;
  Deque<Traceable> wtf_deque;
  Vector<Traceable> wtf_vector;
  LinkedHashSet<Traceable*> wtf_linked_hash_set;
  HashCountedSet<Traceable&> wtf_hash_counted_set;
  HashMap<Traceable, bool> wtf_hash_map_key;
  HashMap<double, const Traceable> wtf_hash_map_value;

  std::array<Traceable, 4> array;
  (void)array;
  std::array<HeapVector<Traceable>, 4> array_of_vectors;
  (void)array_of_vectors;

  GC_PLUGIN_IGNORE("For testing")
  std::set<Traceable> ignored_set;
  (void)ignored_set;
}

}  // namespace blink
