// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Config.h"

#include <cassert>

#include "clang/AST/AST.h"

using namespace clang;

const char kNewOperatorName[] = "operator new";
const char kCreateName[] = "Create";
const char kTraceName[] = "Trace";
const char kFinalizeName[] = "FinalizeGarbageCollectedObject";
const char kTraceAfterDispatchName[] = "TraceAfterDispatch";
const char kRegisterWeakMembersName[] = "RegisterWeakMembers";
const char kHeapAllocatorName[] = "HeapAllocator";
const char kTraceIfNeededName[] = "TraceIfNeeded";
const char kVisitorDispatcherName[] = "VisitorDispatcher";
const char kVisitorVarName[] = "visitor";
const char kAdjustAndMarkName[] = "AdjustAndMark";
const char kIsHeapObjectAliveName[] = "IsHeapObjectAlive";
const char kConstIteratorName[] = "const_iterator";
const char kIteratorName[] = "iterator";
const char kConstReverseIteratorName[] = "const_reverse_iterator";
const char kReverseIteratorName[] = "reverse_iterator";

bool Config::IsTemplateInstantiation(CXXRecordDecl* record) {
  ClassTemplateSpecializationDecl* spec =
      dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
  if (!spec)
    return false;
  switch (spec->getTemplateSpecializationKind()) {
    case TSK_ImplicitInstantiation:
    case TSK_ExplicitInstantiationDefinition:
      return true;
    case TSK_Undeclared:
    case TSK_ExplicitSpecialization:
      return false;
    // TODO: unsupported cases.
    case TSK_ExplicitInstantiationDeclaration:
      return false;
  }
  assert(false && "Unknown template specialization kind");
  return false;
}
