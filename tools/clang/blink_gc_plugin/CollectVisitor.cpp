// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CollectVisitor.h"

#include "Config.h"

using namespace clang;

CollectVisitor::CollectVisitor() {
}

CollectVisitor::RecordVector& CollectVisitor::record_decls() {
  return record_decls_;
}

CollectVisitor::MethodVector& CollectVisitor::trace_decls() {
  return trace_decls_;
}

bool CollectVisitor::VisitCXXRecordDecl(CXXRecordDecl* record) {
  if (record->hasDefinition() && record->isCompleteDefinition())
    record_decls_.push_back(record);
  return true;
}

bool CollectVisitor::VisitCXXMethodDecl(CXXMethodDecl* method) {
  if (method->isThisDeclarationADefinition()) {
    if (Config::IsTraceMethod(method)) {
      trace_decls_.push_back(method);
    }
  }
  return true;
}
