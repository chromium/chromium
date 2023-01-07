// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gc_api.h"

SafepointTable spt = GenSafepointTable();
Heap* heap = nullptr;

HeapAddress Heap::AllocRaw(long value) {
  assert(heap_ptr < kHeapSize && "Allocation failed: Heap full");

  HeapAddress raw_ptr = &fromspace()[heap_ptr];
  *raw_ptr = value;
  heap_ptr++;
  return raw_ptr;
}

void Heap::MoveObjects() {
  for (int i = 0; i < kHeapSize; i++) {
    auto tmp = a_frag_[i];
    a_frag_[i] = b_frag_[i];
    b_frag_[i] = tmp;
  }
  alloc_on_a_ = !alloc_on_a_;
}

HeapAddress Heap::UpdatePointer(HeapAddress ptr) {
  int offset =
      reinterpret_cast<char*>(ptr) - reinterpret_cast<char*>(fromspace());
  auto* new_ptr = reinterpret_cast<char*>(tospace()) + offset;
  return reinterpret_cast<HeapAddress>(new_ptr);
}

void FrameRoots::Print() const {
  printf("\tRegister Roots: NYI\n");
  if (!stack_roots_.size()) {
    printf("\tStack Roots: []\n");
    return;
  }

  printf("\tStack Roots: [");
  for (auto SR : stack_roots_) {
    printf("RBP - %d, ", SR);
  }
  printf("\b\b]\n");
}

void SafepointTable::Print() const {
  printf("Safepoint Table\n");
  for (auto const& pair : roots_) {
    printf("Frame %p\n", reinterpret_cast<void*>(pair.first));
    pair.second.Print();
  }
}

extern "C" void StackWalkAndMoveObjects(FramePtr fp) {
  while (true) {
    // The caller's return address is always 1 machine word above the recorded
    // RBP value in the current frame
    auto ra = reinterpret_cast<ReturnAddress>(*(fp + 1));

    // Step up into the caller's frame or bail if we're at the top of stack
    fp = reinterpret_cast<FramePtr>(*fp);
    if (reinterpret_cast<uintptr_t>(fp) == TopOfStack)
      break;

    printf("==== Frame %p ====\n", reinterpret_cast<void*>(ra));

    auto it = spt.roots()->find(ra);
    if (it != spt.roots()->end()) {
      auto fr_roots = it->second;
      for (auto root : *fr_roots.stack_roots()) {
        auto offset = root / sizeof(uintptr_t);
        auto stack_address = reinterpret_cast<uintptr_t*>((fp - offset));

        printf("\tRoot: [RBP - %d]\n", root);
        printf("\tAddress: %p\n", reinterpret_cast<void*>(*stack_address));

        // We know that all HeapObjects are wrappers around a single long
        // integer, so for debugging purposes we can cast it as such and print
        // the value to see if it looks correct.
        printf("\tValue: %ld\n",
               reinterpret_cast<HeapObject*>(*stack_address)->data);

        // We are in a collection, so we know that the underlying objects will
        // be moved before we return to the mutator. We update the on-stack
        // pointers here to point to the object's new location in the heap.
        HeapAddress new_ptr =
            heap->UpdatePointer(reinterpret_cast<HeapAddress>(*stack_address));
        *stack_address = reinterpret_cast<uintptr_t>(new_ptr);

        printf("\tAddress after Relocation: %p\n",
               reinterpret_cast<void*>(*stack_address));
      }
    }
  }
  heap->MoveObjects();
}

Handle<HeapObject> AllocateHeapObject(long data) {
  HeapAddress ptr = heap->AllocRaw(data);
  return Handle<HeapObject>::New(reinterpret_cast<HeapObject*>(ptr));
}

void InitGC() {
  InitTopOfStack();
  heap = new Heap();
}

void TeardownGC() {
  delete heap;
}

void PrintSafepointTable() {
  spt.Print();
}
