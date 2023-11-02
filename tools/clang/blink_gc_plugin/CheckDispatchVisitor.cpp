// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CheckDispatchVisitor.h"

#include "Config.h"
#include "RecordInfo.h"

using namespace clang;

CheckDispatchVisitor::CheckDispatchVisitor(RecordInfo* receiver)
    : receiver_(receiver),
      dispatched_to_receiver_(false) {
}

bool CheckDispatchVisitor::dispatched_to_receiver() {
  return dispatched_to_receiver_;
}

bool CheckDispatchVisitor::VisitMemberExpr(MemberExpr* member) {
  if (CXXMethodDecl* fn = dyn_cast<CXXMethodDecl>(member->getMemberDecl())) {
    if (fn->getParent() == receiver_->record())
      dispatched_to_receiver_ = true;
  }
  return true;
}

bool CheckDispatchVisitor::VisitUnresolvedMemberExpr(
    UnresolvedMemberExpr* member) {
  for (Decl* decl : member->decls()) {
    if (CXXMethodDecl* method = dyn_cast<CXXMethodDecl>(decl)) {
      if (method->getParent() == receiver_->record() &&
          Config::GetTraceMethodType(method) ==
          Config::TRACE_AFTER_DISPATCH_METHOD) {
        dispatched_to_receiver_ = true;
        return true;
      }
    }
  }
  return true;
}
