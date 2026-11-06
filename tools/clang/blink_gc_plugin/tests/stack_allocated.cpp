// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stack_allocated.h"

namespace blink {

// Verify that anon namespaces are checked.
namespace {

class AnonStackObject : public StackObject {
public:
    HeapObject* m_obj;
};

}

void HeapObject::Trace(Visitor* visitor) const {}

class HeapObjectWithCollections
    : public GarbageCollected<HeapObjectWithCollections> {
 public:
  void Trace(Visitor*) const {}

 private:
  std::vector<StackObject> vector_;
  std::array<StackObject, 4> array_;

  GC_PLUGIN_IGNORE("For testing")
  std::set<StackObject> ignored_set_;

  STACK_ALLOCATED_IGNORE("For testing")
  std::set<StackObject> stack_allocated_ignored_set_;
};

class StackObjectWithCollections {
  STACK_ALLOCATED();

 public:
  StackObjectWithCollections() {
    (void)set_;
    (void)vector_;
    (void)map_key_;
    (void)map_value_;
    (void)unordered_set_;
    (void)map_const_;
    (void)vector_pair_;
    (void)ignored_set_;
    (void)stack_allocated_ignored_set_;
    (void)array_;
    (void)vector_ptr_;
  }

 private:
  // Bad STL collections:
  std::set<StackObject> set_;
  std::vector<DerivedStackObject> vector_;
  std::map<StackObject, int> map_key_;
  std::unordered_map<int, DerivedStackObject> map_value_;
  std::unordered_set<StackObject> unordered_set_;
  std::map<const StackObject, int> map_const_;
  std::vector<std::pair<StackObject, int>> vector_pair_;

  // Good collections:
  GC_PLUGIN_IGNORE("For testing")
  std::set<StackObject> ignored_set_;
  STACK_ALLOCATED_IGNORE("For testing")
  std::set<StackObject> stack_allocated_ignored_set_;
  std::array<StackObject, 4> array_;
  std::vector<StackObject*> vector_ptr_;
};

void CollectionsOfStackAllocated() {
  // Bad STL collections:
  std::set<StackObject> set;
  (void)set;
  std::vector<DerivedStackObject> vector;
  (void)vector;
  std::map<StackObject, int> map_key;
  (void)map_key;
  std::unordered_map<int, DerivedStackObject> map_value;
  (void)map_value;
  std::unordered_set<StackObject> unordered_set;
  (void)unordered_set;
  std::map<const StackObject, int> map_const;
  (void)map_const;
  std::vector<std::pair<StackObject, int>> vector_pair;
  (void)vector_pair;
  STACK_ALLOCATED_IGNORE("For testing")
  std::map<StackObject, HeapObject*> stack_allocated_ignored_map_gced;
  (void)stack_allocated_ignored_map_gced;
  new std::vector<StackObject>;

  // Good collections:
  GC_PLUGIN_IGNORE("For testing")
  std::set<StackObject> ignored_set;
  (void)ignored_set;
  STACK_ALLOCATED_IGNORE("For testing")
  std::set<StackObject> stack_allocated_ignored_set;
  (void)stack_allocated_ignored_set;
  std::array<StackObject, 4> array;
  (void)array;
  std::vector<StackObject*> vector_ptr;
  (void)vector_ptr;
}
}
