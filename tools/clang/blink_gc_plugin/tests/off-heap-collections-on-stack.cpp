// Copyright 2026 The Chromium Authors
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
    (void)map_const_;
    (void)vector_pair_;
    (void)array_;
    (void)array_of_vectors_;
    (void)member_set_;
    (void)member_vector_;
    (void)member_map_key_;
    (void)member_map_value_;
    (void)member_set_ptr_;
    (void)member_vector_ref_;
    (void)member_map_const_;
    (void)member_vector_pair_;

    for (int i = 0; i < 4; ++i) {
      v->Trace(member_array_[i]);
    }

    // The following calls are not needed and are added merely to silence false
    // untraced field errors.
    v->Trace(wtf_hash_set_);
    v->Trace(wtf_deque_);
    v->Trace(wtf_vector_);
    v->Trace(wtf_hash_map_key_);
    v->Trace(wtf_hash_map_value_);
    v->Trace(wtf_member_hash_set_);
    v->Trace(wtf_member_deque_);
    v->Trace(wtf_member_vector_);
    v->Trace(wtf_member_hash_map_key_);
    v->Trace(wtf_member_hash_map_value_);
  }

 private:
  // Non-stack collections are allowed when only on-stack collections are
  // checked:
  std::set<Base> set_;
  std::vector<Derived> vector_;
  std::map<Mixin, int> map_key_;
  std::unordered_map<int, Base> map_value_;
  std::map<const Mixin, int> map_const_;
  std::vector<std::pair<Base, int>> vector_pair_;
  std::array<Base, 4> array_;
  std::array<HeapVector<Base>, 4> array_of_vectors_;

  std::set<Member<Base>> member_set_;
  std::vector<WeakMember<Base>> member_vector_;
  std::map<Member<Base>, int> member_map_key_;
  std::unordered_map<int, WeakMember<Base>> member_map_value_;
  std::unordered_set<Member<Base>*> member_set_ptr_;
  std::vector<Member<Base>&> member_vector_ref_;
  std::map<const Member<Base>, int> member_map_const_;
  std::vector<std::pair<Member<Base>, int>> member_vector_pair_;
  std::array<Member<Base>, 4> member_array_;

  HashSet<Base> wtf_hash_set_;
  Deque<Derived> wtf_deque_;
  Vector<Mixin> wtf_vector_;
  HashMap<Mixin, bool> wtf_hash_map_key_;
  HashMap<double, const Base> wtf_hash_map_value_;

  HashSet<Member<Base>> wtf_member_hash_set_;
  Deque<WeakMember<Base>> wtf_member_deque_;
  Vector<Member<Base>> wtf_member_vector_;
  LinkedHashSet<Member<Base>*> wtf_member_linked_hash_set_;
  HashCountedSet<WeakMember<Base>&> wtf_member_hash_counted_set_;
  HashMap<Member<Base>, bool> wtf_member_hash_map_key_;
  HashMap<double, const Member<Base>> wtf_member_hash_map_value_;
};

class StackAllocated {
  STACK_ALLOCATED();

 public:
  StackAllocated() {
    (void)vector_;
    (void)wtf_vector_;
    (void)member_vector_;
    (void)wtf_member_vector_;
    (void)array_;
    (void)array_ptr_;
    (void)member_array_;
    (void)array_of_vectors_;
  }

 private:
  // Bad collections on stack:
  std::vector<Base> vector_;
  Vector<Derived*> wtf_vector_;
  std::vector<Member<Base>> member_vector_;
  Vector<WeakMember<Base>> wtf_member_vector_;
  std::array<Base, 4> array_;

  // Good collections on stack:
  std::array<Base*, 4> array_ptr_;
  std::array<Member<Base>, 4> member_array_;
  std::array<HeapVector<Base>, 4> array_of_vectors_;
};

void DisallowedUseOfCollections() {
  // Bad stl collections of GCed:
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

  // Bad WTF collections of GCed:
  HashSet<Base> wtf_hash_set;
  Deque<Derived> wtf_deque;
  Vector<Mixin> wtf_vector;
  LinkedHashSet<Base*> wtf_linked_hash_set;
  HashCountedSet<Derived&> wtf_hash_counted_set;
  HashMap<Mixin, bool> wtf_hash_map_key;
  HashMap<double, const Base> wtf_hash_map_value;

  // Bad stl collections of Members:
  std::set<Member<Base>> member_set;
  (void)member_set;
  std::vector<WeakMember<Base>> member_vector;
  (void)member_vector;
  std::map<Member<Base>, int> member_map_key;
  (void)member_map_key;
  std::unordered_map<int, WeakMember<Base>> member_map_value;
  (void)member_map_value;
  std::unordered_set<Member<Base>*> member_set_ptr;
  (void)member_set_ptr;
  std::vector<Member<Base>&> member_vector_ref;
  (void)member_vector_ref;
  std::map<const Member<Base>, int> member_map_const;
  (void)member_map_const;
  std::vector<std::pair<Member<Base>, int>> member_vector_pair;
  (void)member_vector_pair;

  // Bad WTF collections of Members:
  HashSet<Member<Base>> wtf_member_hash_set;
  Deque<WeakMember<Base>> wtf_member_deque;
  Vector<Member<Base>> wtf_member_vector;
  LinkedHashSet<Member<Base>*> wtf_member_linked_hash_set;
  HashCountedSet<WeakMember<Base>&> wtf_member_hash_counted_set;
  HashMap<Member<Base>, bool> wtf_member_hash_map_key;
  HashMap<double, const Member<Base>> wtf_member_hash_map_value;

  // Good collections:
  blink::HeapHashSet<Base> heap_hash_set;
  blink::HeapDeque<Derived> heap_deque;
  blink::HeapVector<Mixin> heap_vector;
  blink::HeapLinkedHashSet<Base> heap_linked_hash_set;
  blink::HeapHashCountedSet<Derived> heap_hash_counted_set;
  blink::HeapHashMap<Mixin, bool> heap_hash_map_key;
  blink::HeapHashMap<double, Base> heap_hash_map_value;
  blink::HeapVector<Member<Base>> heap_member_vector;
  std::set<UntracedMember<Base>> set_untraced;
  (void)set_untraced;

  std::array<Base, 4> array;
  (void)array;
  std::array<Base*, 4> array_ptr;
  (void)array_ptr;
  std::array<Member<Base>, 4> member_array;
  (void)member_array;
  std::array<HeapVector<Base>, 4> array_of_vectors;
  (void)array_of_vectors;

  GC_PLUGIN_IGNORE("For testing")
  std::set<Base> ignored_set;
  (void)ignored_set;

  // New-expressions are not on stack and should be allowed when only on-stack
  // collections are checked:
  (void)new std::vector<Base>;
  (void)new std::vector<Base*>;
  (void)new std::vector<Member<Base>>;
  (void)new Vector<Base>;
  (void)new Vector<Member<Base>>;
}

}  // namespace blink
