// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "off-heap-collections-of-members.h"

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
    (void)set_untraced_;
    (void)vector_untraced_;
    (void)map_key_untraced_;
    (void)map_value_untraced_;
    (void)vector_pair_;
    (void)ignored_set_;

    for (int i = 0; i < 4; ++i) {
      v->Trace(array_[i]);
    }

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
  std::set<Member<Base>> set_;
  std::vector<WeakMember<Base>> vector_;
  std::map<Member<Base>, int> map_key_;
  std::unordered_map<int, WeakMember<Base>> map_value_;
  std::unordered_set<Member<Base>*> set_ptr_;
  std::vector<Member<Base>&> vector_ref_;
  std::map<const Member<Base>, int> map_const_;
  std::vector<std::pair<Member<Base>, int>> vector_pair_;
  std::array<Member<Base>, 4> array_;

  // Bad WTF collections:
  WTF::HashSet<Member<Base>> wtf_hash_set_;
  WTF::Deque<WeakMember<Base>> wtf_deque_;
  WTF::Vector<Member<Base>> wtf_vector_;
  WTF::LinkedHashSet<Member<Base>*> wtf_linked_hash_set_;
  WTF::HashCountedSet<WeakMember<Base>&> wtf_hash_counted_set_;
  WTF::HashMap<Member<Base>, bool> wtf_hash_map_key_;
  WTF::HashMap<double, const Member<Base>> wtf_hash_map_value_;

  // Good collections:
  blink::HeapHashSet<Member<Base>> heap_hash_set_;
  blink::HeapDeque<WeakMember<Base>> heap_deque_;
  blink::HeapVector<Member<Base>> heap_vector_;
  blink::HeapLinkedHashSet<WeakMember<Base>> heap_linked_hash_set_;
  blink::HeapHashCountedSet<Member<Base>> heap_hash_counted_set_;
  blink::HeapHashMap<WeakMember<Base>, bool> heap_hash_map_key_;
  blink::HeapHashMap<double, Member<Base>> heap_hash_map_value_;
  std::set<UntracedMember<Base>> set_untraced_;
  std::vector<UntracedMember<Base>> vector_untraced_;
  std::map<UntracedMember<Base>, int> map_key_untraced_;
  std::map<int, UntracedMember<Base>> map_value_untraced_;

  GC_PLUGIN_IGNORE("For testing")
  std::set<Member<Base>> ignored_set_;
};

class StackAllocated {
  STACK_ALLOCATED();

 public:
  StackAllocated() { (void)array_; }

 private:
  std::array<Member<Base>, 4> array_;
};

void DisallowedUseOfCollections() {
  // Bad stl collections:
  std::set<Member<Base>> set;
  (void)set;
  std::vector<WeakMember<Base>> vector;
  (void)vector;
  std::map<Member<Base>, int> map_key;
  (void)map_key;
  std::unordered_map<int, WeakMember<Base>> map_value;
  (void)map_value;
  std::unordered_set<Member<Base>*> set_ptr;
  (void)set_ptr;
  std::vector<Member<Base>&> vector_ref;
  (void)vector_ref;
  std::map<const Member<Base>, int> map_const;
  (void)map_const;
  std::vector<std::pair<Member<Base>, int>> vector_pair;
  (void)vector_pair;

  // Bad WTF collections:
  WTF::HashSet<Member<Base>> wtf_hash_set;
  WTF::Deque<WeakMember<Base>> wtf_deque;
  WTF::Vector<Member<Base>> wtf_vector;
  WTF::LinkedHashSet<Member<Base>*> wtf_linked_hash_set;
  WTF::HashCountedSet<WeakMember<Base>&> wtf_hash_counted_set;
  WTF::HashMap<Member<Base>, bool> wtf_hash_map_key;
  WTF::HashMap<double, const Member<Base>> wtf_hash_map_value;

  // Good collections:
  blink::HeapHashSet<Member<Base>> heap_hash_set;
  blink::HeapDeque<WeakMember<Base>> heap_deque;
  blink::HeapVector<Member<Base>> heap_vector;
  blink::HeapLinkedHashSet<WeakMember<Base>> heap_linked_hash_set;
  blink::HeapHashCountedSet<Member<Base>> heap_hash_counted_set;
  blink::HeapHashMap<WeakMember<Base>, bool> heap_hash_map_key;
  blink::HeapHashMap<double, Member<Base>> heap_hash_map_value;
  std::set<UntracedMember<Base>> set_untraced;
  (void)set_untraced;
  std::vector<UntracedMember<Base>> vector_untraced;
  (void)vector_untraced;
  std::map<UntracedMember<Base>, int> map_key_untraced;
  (void)map_key_untraced;
  std::map<int, UntracedMember<Base>> map_value_untraced;
  (void)map_value_untraced;

  std::array<Member<Base>, 4> array;
  (void)array;

  GC_PLUGIN_IGNORE("For testing")
  std::set<Member<Base>> ignored_set;
  (void)ignored_set;
}

}  // namespace blink
