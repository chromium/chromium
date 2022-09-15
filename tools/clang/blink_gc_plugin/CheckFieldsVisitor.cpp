// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CheckFieldsVisitor.h"

#include <cassert>

#include "RecordInfo.h"
#include "llvm/Support/ErrorHandling.h"

CheckFieldsVisitor::CheckFieldsVisitor(const BlinkGCPluginOptions& options)
    : options_(options), current_(0), stack_allocated_host_(false) {}

CheckFieldsVisitor::Errors& CheckFieldsVisitor::invalid_fields() {
  return invalid_fields_;
}

bool CheckFieldsVisitor::ContainsInvalidFields(RecordInfo* info) {
  stack_allocated_host_ = info->IsStackAllocated();
  managed_host_ =
      stack_allocated_host_ || info->IsGCAllocated() || info->IsNewDisallowed();
  for (RecordInfo::Fields::iterator it = info->GetFields().begin();
       it != info->GetFields().end();
       ++it) {
    context().clear();
    current_ = &it->second;
    current_->edge()->Accept(this);
  }
  return !invalid_fields_.empty();
}

void CheckFieldsVisitor::AtMember(Member*) {
  if (managed_host_)
    return;
  // A member is allowed to appear in the context of a root.
  for (Context::iterator it = context().begin();
       it != context().end();
       ++it) {
    if ((*it)->Kind() == Edge::kRoot)
      return;
  }
  invalid_fields_.push_back(std::make_pair(current_, kMemberInUnmanaged));
}

void CheckFieldsVisitor::AtWeakMember(WeakMember*) {
  // TODO(sof): remove this once crbug.com/724418's change
  // has safely been rolled out.
  if (options_.enable_weak_members_in_unmanaged_classes)
    return;
  AtMember(nullptr);
}

void CheckFieldsVisitor::AtIterator(Iterator* edge) {
  if (!managed_host_)
    return;

  if (!stack_allocated_host_ && edge->on_heap())
    invalid_fields_.push_back(std::make_pair(current_, kIteratorToGCManaged));
}

void CheckFieldsVisitor::AtValue(Value* edge) {
  // TODO: what should we do to check unions?
  if (edge->value()->record()->isUnion())
    return;

  if (!stack_allocated_host_ && edge->value()->IsStackAllocated()) {
    invalid_fields_.push_back(std::make_pair(current_, kPtrFromHeapToStack));
    return;
  }

  if (!Parent() &&
      edge->value()->IsGCDerived() &&
      !edge->value()->IsGCMixin()) {
    invalid_fields_.push_back(std::make_pair(current_, kGCDerivedPartObject));
    return;
  }

  // Members/WeakMembers are prohibited if the host is stack allocated, but
  // heap collections with Members are okay.
  if (stack_allocated_host_ && Parent() &&
      (Parent()->IsMember() || Parent()->IsWeakMember())) {
    if (!GrandParent() || !GrandParent()->IsCollection()) {
      invalid_fields_.push_back(
          std::make_pair(current_, kMemberInStackAllocated));
      return;
    }
  }

  // If in a stack allocated context, be fairly insistent that T in Member<T>
  // is GC allocated, as stack allocated objects do not have a trace()
  // that separately verifies the validity of Member<T>.
  //
  // Notice that an error is only reported if T's definition is in scope;
  // we do not require that it must be brought into scope as that would
  // prevent declarations of mutually dependent class types.
  //
  // (Note: Member<>'s constructor will at run-time verify that the
  // pointer it wraps is indeed heap allocated.)
  if (stack_allocated_host_ && Parent() &&
      (Parent()->IsMember() || Parent()->IsWeakMember()) &&
      edge->value()->HasDefinition() && !edge->value()->IsGCAllocated()) {
    invalid_fields_.push_back(std::make_pair(current_, kMemberToGCUnmanaged));
    return;
  }

  if (!Parent() || !edge->value()->IsGCAllocated())
    return;

  // Disallow unique_ptr<T>, scoped_refptr<T>, WeakPtr<T>.
  if (Parent()->IsUniquePtr() || Parent()->IsRefPtr()) {
    invalid_fields_.push_back(std::make_pair(
        current_, InvalidSmartPtr(Parent())));
    return;
  }
  if (Parent()->IsRawPtr() && !stack_allocated_host_) {
    RawPtr* rawPtr = static_cast<RawPtr*>(Parent());
    Error error = rawPtr->HasReferenceType() ?
        kReferencePtrToGCManaged : kRawPtrToGCManaged;
    invalid_fields_.push_back(std::make_pair(current_, error));
  }
}

void CheckFieldsVisitor::AtCollection(Collection* edge) {
  if (edge->on_heap() && Parent() && Parent()->IsUniquePtr())
    invalid_fields_.push_back(std::make_pair(current_, kUniquePtrToGCManaged));
}

CheckFieldsVisitor::Error CheckFieldsVisitor::InvalidSmartPtr(Edge* ptr) {
  if (ptr->IsRefPtr())
    return ptr->Kind() == Edge::kStrong ? kRefPtrToGCManaged
                                        : kWeakPtrToGCManaged;
  if (ptr->IsUniquePtr())
    return kUniquePtrToGCManaged;
  llvm_unreachable("Unknown smart pointer kind");
}
