// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

struct StackAllocatedType {
  using IsStackAllocatedTypeMarker [[maybe_unused]] = int;
  int i;
};

struct InheritsFromStackAllocatedType : public StackAllocatedType {
  char* s;
};

struct StackAllocatedClassWithStackAllocatedMember {
  using IsStackAllocatedTypeMarker [[maybe_unused]] = int;
  // Stack-allocated member in stack-allocated class is OK.
  StackAllocatedType stack_allocated;
  // Stack-allocated member in stack-allocated class is OK.
  InheritsFromStackAllocatedType stack_allocated_subclass;
};

struct StackAllocatedField {
  // Stack-allocated member variable in non-stack-allocated class.
  StackAllocatedType stack_allocated;  // Error 1
  // Stack-allocated member variable in non-stack-allocated class.
  InheritsFromStackAllocatedType stack_allocated_subclass;  // Error 2
};

struct IgnoreStackAllocatedField {
  // Explicitly ignore stack-allocated field in non-stack-allocated class.
  __attribute__((annotate("stack_allocated_ignore")))
  StackAllocatedType stack_allocated;
};

template <typename T>
struct TemplatedClass {};

template <>
struct TemplatedClass<char> {
  using IsStackAllocatedTypeMarker [[maybe_unused]] = int;
  // Stack-allocated member variable in stack-allocated template
  // specialization class is OK.
  StackAllocatedType stack_allocated;
};

template <>
struct TemplatedClass<int> {
  // Stack-allocated member variable in non-stack-allocated template
  // specialization class.
  StackAllocatedType stack_allocated;  // Error 3
};

struct StackAllocatedNestedField {
  struct {
    // Stack-allocated member in anonymous nested struct within
    // non-stack-allocated class.
    StackAllocatedType stack_allocated;  // Error 4
  } nested_stack_allocated;
};

struct StackAllocatedPointerField {
  // Pointer to stack-allocated type member variable in non-stack-allocated
  // class.
  StackAllocatedType* stack_allocated;  // Error 5
};

struct StackAllocatedReferenceField {
  // Reference to stack-allocated type member variable in non-stack-allocated
  // class.
  StackAllocatedType& stack_allocated;  // Error 6
};

struct StackAllocatedUnionField {
  union {
    int foo;
    // Stack-allocated member of union within non-stack-allocated class.
    StackAllocatedType stack_allocated;  // Error 7
  } stack_allocated_union;
};

struct StackAllocatedSharedPointerField {
  // Member variable is a template instantiation with stack-allocated-type
  // as template parameter, within a non-stack-allocated class.
  std::shared_ptr<StackAllocatedType> stack_allocated;  // Error 8
};

struct StackAllocatedPointerVectorField {
  // Member variable is a template instantiation with pointer to
  // stack-allocated-type as template parameter, within a non-stack-allocated
  // class.
  std::vector<StackAllocatedType*> stack_allocated;  // Error 9
};

struct NestedTemplateParameter {
  // Stack-allocated type as nested template parameter for member variable
  // inside non-stack-allocated class.
  std::shared_ptr<std::vector<StackAllocatedType>> stack_allocated;  // Error 10
};

struct StackAllocatedArrayField {
  // Array of stack-allocated type as member variable in non-stack-allocated
  // class.
  StackAllocatedType stack_allocated[2];  // Error 11
};
