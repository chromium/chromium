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
  bool is_ptr = Parent() && (Parent()->IsRawPtr() || Parent()->IsRefPtr());
  invalid_fields_.push_back(std::make_pair(
      current_, is_ptr ? kPtrToMemberInUnmanaged : kMemberInUnmanaged));
}

void CheckFieldsVisitor::AtWeakMember(WeakMember*) {
  AtMember(nullptr);
}

void CheckFieldsVisitor::AtIterator(Iterator* edge) {
  if (!managed_host_)
    return;

  if (!stack_allocated_host_ && edge->on_heap())
    invalid_fields_.push_back(std::make_pair(current_, kIteratorToGCManaged));
}

namespace {

CheckFieldsVisitor::Error InvalidSmartPtr(Edge* ptr, bool is_gced) {
  if (ptr->IsRefPtr()) {
    return is_gced ? CheckFieldsVisitor::Error::kRefPtrToGCManaged
                   : CheckFieldsVisitor::Error::kRefPtrToTraceable;
  }
  if (ptr->IsUniquePtr()) {
    return is_gced ? CheckFieldsVisitor::Error::kUniquePtrToGCManaged
                   : CheckFieldsVisitor::Error::kUniquePtrToTraceable;
  }
  llvm_unreachable("Unknown smart pointer kind");
}

}  // namespace

void CheckFieldsVisitor::AtValue(Value* edge) {
  RecordInfo* record = edge->value();

  // TODO: what should we do to check unions?
  if (record->record()->isUnion()) {
    return;
  }

  // Don't allow unmanaged classes to contain traceable part-objects.
  const bool child_is_part_object = record->IsNewDisallowed() && !Parent();
  if (!managed_host_ && child_is_part_object && record->RequiresTraceMethod()) {
    invalid_fields_.push_back(
        std::make_pair(current_, kTraceablePartObjectInUnmanaged));
    return;
  }

  if (!stack_allocated_host_ && record->IsStackAllocated()) {
    invalid_fields_.push_back(std::make_pair(current_, kPtrFromHeapToStack));
    return;
  }

  if (!Parent() && record->IsGCDerived() && !record->IsGCMixin()) {
    invalid_fields_.push_back(std::make_pair(current_, kGCDerivedPartObject));
    return;
  }

  // Members/WeakMembers are prohibited if the host is stack allocated, but
  // heap collections with Members are okay.
  if (stack_allocated_host_ && Parent() &&
      (Parent()->IsMember() || Parent()->IsWeakMember())) {
    if (!GrandParent() ||
        (!GrandParent()->IsCollection() && !GrandParent()->IsRawPtr() &&
         !GrandParent()->IsRefPtr())) {
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

  if (!Parent() || (!edge->value()->IsGCAllocated() &&
                    (!options_.enable_ptrs_to_traceable_check ||
                     !edge->value()
                          ->NeedsTracing(Edge::NeedsTracingOption::kRecursive)
                          .IsNeeded()))) {
    return;
  }

  // Disallow unique_ptr<T>, scoped_refptr<T>
  if (Parent()->IsUniquePtr() ||
      (Parent()->IsRefPtr() && (Parent()->Kind() == Edge::kStrong))) {
    invalid_fields_.push_back(std::make_pair(
        current_, InvalidSmartPtr(Parent(), edge->value()->IsGCAllocated())));
    return;
  }
  if (Parent()->IsRawPtr() && !stack_allocated_host_) {
    RawPtr* rawPtr = static_cast<RawPtr*>(Parent());
    Error error = edge->value()->IsGCAllocated()
                      ? (rawPtr->HasReferenceType() ? kReferencePtrToGCManaged
                                                    : kRawPtrToGCManaged)
                      : (rawPtr->HasReferenceType() ? kReferencePtrToTraceable
                                                    : kRawPtrToTraceable);
    invalid_fields_.push_back(std::make_pair(current_, error));
  }
}

void CheckFieldsVisitor::AtCollection(Collection* edge) {
  if (GrandParent() &&
      (GrandParent()->IsRawPtr() || GrandParent()->IsRefPtr())) {
    // Don't alert on pointers to unique_ptr. Alerting on the pointed unique_ptr
    // should suffice.
    return;
  }
  if (edge->on_heap() && Parent() && Parent()->IsUniquePtr())
    invalid_fields_.push_back(std::make_pair(current_, kUniquePtrToGCManaged));
}
