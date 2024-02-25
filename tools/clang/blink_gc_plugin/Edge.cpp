// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Config.h"
#include "Edge.h"
#include "RecordInfo.h"

TracingStatus Value::NeedsTracing(NeedsTracingOption option) {
  return value_->NeedsTracing(option);
}

bool Value::NeedsFinalization() { return value_->NeedsFinalization(); }
bool Collection::NeedsFinalization() { return info_->NeedsFinalization(); }
bool Collection::IsSTDCollection() {
  return Config::IsSTDCollection(info_->name());
}
std::string Collection::GetCollectionName() const {
  return info_->name();
}

TracingStatus Collection::NeedsTracing(NeedsTracingOption) {
  if (on_heap_) {
    return TracingStatus::Needed();
  }

  // This will be handled by matchers.
  if (IsSTDCollection()) {
    if ((GetCollectionName() == "array") && !members_.empty()) {
      Edge* type = members_.at(0);
      if (type->IsMember() || type->IsWeakMember() ||
          type->IsTraceWrapperV8Reference()) {
        return TracingStatus::Needed();
      }
    }
    return TracingStatus::Unknown();
  }

  // For off-heap collections, determine tracing status of members.
  TracingStatus status = TracingStatus::Unneeded();
  for (Members::iterator it = members_.begin(); it != members_.end(); ++it) {
    // Do a non-recursive test here since members could equal the holder.
    status = status.LUB((*it)->NeedsTracing(kNonRecursive));
  }
  return status;
}

void RecursiveEdgeVisitor::AtValue(Value*) {}
void RecursiveEdgeVisitor::AtRawPtr(RawPtr*) {}
void RecursiveEdgeVisitor::AtRefPtr(RefPtr*) {}
void RecursiveEdgeVisitor::AtUniquePtr(UniquePtr*) {}
void RecursiveEdgeVisitor::AtMember(Member*) {}
void RecursiveEdgeVisitor::AtWeakMember(WeakMember*) {}
void RecursiveEdgeVisitor::AtPersistent(Persistent*) {}
void RecursiveEdgeVisitor::AtCrossThreadPersistent(CrossThreadPersistent*) {}
void RecursiveEdgeVisitor::AtCollection(Collection*) {}
void RecursiveEdgeVisitor::AtIterator(Iterator*) {}
void RecursiveEdgeVisitor::AtTraceWrapperV8Reference(TraceWrapperV8Reference*) {
}
void RecursiveEdgeVisitor::AtArrayEdge(ArrayEdge*) {}

void RecursiveEdgeVisitor::VisitValue(Value* e) {
  AtValue(e);
}

void RecursiveEdgeVisitor::VisitRawPtr(RawPtr* e) {
  AtRawPtr(e);
  Enter(e);
  e->ptr()->Accept(this);
  Leave();
}

void RecursiveEdgeVisitor::VisitRefPtr(RefPtr* e) {
  AtRefPtr(e);
  Enter(e);
  e->ptr()->Accept(this);
  Leave();
}

void RecursiveEdgeVisitor::VisitUniquePtr(UniquePtr* e) {
  AtUniquePtr(e);
  Enter(e);
  e->ptr()->Accept(this);
  Leave();
}

void RecursiveEdgeVisitor::VisitMember(Member* e) {
  AtMember(e);
  Enter(e);
  e->ptr()->Accept(this);
  Leave();
}

void RecursiveEdgeVisitor::VisitWeakMember(WeakMember* e) {
  AtWeakMember(e);
  Enter(e);
  e->ptr()->Accept(this);
  Leave();
}

void RecursiveEdgeVisitor::VisitPersistent(Persistent* e) {
  AtPersistent(e);
  Enter(e);
  e->ptr()->Accept(this);
  Leave();
}

void RecursiveEdgeVisitor::VisitCrossThreadPersistent(
    CrossThreadPersistent* e) {
  AtCrossThreadPersistent(e);
  Enter(e);
  e->ptr()->Accept(this);
  Leave();
}

void RecursiveEdgeVisitor::VisitCollection(Collection* e) {
  AtCollection(e);
  Enter(e);
  e->AcceptMembers(this);
  Leave();
}

void RecursiveEdgeVisitor::VisitIterator(Iterator* e) {
  AtIterator(e);
}

void RecursiveEdgeVisitor::VisitTraceWrapperV8Reference(
    TraceWrapperV8Reference* e) {
  AtTraceWrapperV8Reference(e);
  Enter(e);
  e->ptr()->Accept(this);
  Leave();
}

void RecursiveEdgeVisitor::VisitArrayEdge(ArrayEdge* e) {
  AtArrayEdge(e);
}
