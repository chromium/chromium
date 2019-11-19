// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CheckTraceVisitor.h"

#include <vector>

#include "Config.h"

using namespace clang;

CheckTraceVisitor::CheckTraceVisitor(CXXMethodDecl* trace,
                                     RecordInfo* info,
                                     RecordCache* cache)
    : trace_(trace), info_(info), cache_(cache) {}

bool CheckTraceVisitor::VisitMemberExpr(MemberExpr* member) {
  // In weak callbacks, consider any occurrence as a correct usage.
  // TODO: We really want to require that isAlive is checked on manually
  // processed weak fields.
  if (IsWeakCallback()) {
    if (FieldDecl* field = dyn_cast<FieldDecl>(member->getMemberDecl()))
      FoundField(field);
  }
  return true;
}

bool CheckTraceVisitor::VisitCallExpr(CallExpr* call) {
  // In weak callbacks we don't check calls (see VisitMemberExpr).
  if (IsWeakCallback())
    return true;

  Expr* callee = call->getCallee();

  // Trace calls from a templated derived class result in a
  // DependentScopeMemberExpr because the concrete trace call depends on the
  // instantiation of any shared template parameters. In this case the call is
  // "unresolved" and we resort to comparing the syntactic type names.
  if (CXXDependentScopeMemberExpr* expr =
      dyn_cast<CXXDependentScopeMemberExpr>(callee)) {
    CheckCXXDependentScopeMemberExpr(call, expr);
    return true;
  }

  // A tracing call will have either a |visitor| or a |m_field| argument.
  // A registerWeakMembers call will have a |this| argument.
  if (call->getNumArgs() != 1)
    return true;
  Expr* arg = call->getArg(0);

  if (UnresolvedMemberExpr* expr = dyn_cast<UnresolvedMemberExpr>(callee)) {
    // This could be a trace call of a base class, as explained in the
    // comments of CheckTraceBaseCall().
    if (CheckTraceBaseCall(call))
      return true;

    if (expr->getMemberName().getAsString() == kRegisterWeakMembersName)
      MarkAllWeakMembersTraced();

    QualType base = expr->getBaseType();
    if (!base->isPointerType())
      return true;
    CXXRecordDecl* decl = base->getPointeeType()->getAsCXXRecordDecl();
    if (decl)
      CheckTraceFieldCall(expr->getMemberName().getAsString(), decl, arg);
    return true;
  }

  if (CXXMemberCallExpr* expr = dyn_cast<CXXMemberCallExpr>(call)) {
    if (CheckTraceFieldMemberCall(expr) || CheckRegisterWeakMembers(expr))
      return true;

  }

  CheckTraceBaseCall(call);
  return true;
}

bool CheckTraceVisitor::IsTraceCallName(const std::string& name) {
  // Currently, a manually dispatched class cannot have mixin bases (having
  // one would add a vtable which we explicitly check against). This means
  // that we can only make calls to a trace method of the same name. Revisit
  // this if our mixin/vtable assumption changes.
  return name == trace_->getName();
}

CXXRecordDecl* CheckTraceVisitor::GetDependentTemplatedDecl(
    CXXDependentScopeMemberExpr* expr) {
  NestedNameSpecifier* qual = expr->getQualifier();
  if (!qual)
    return 0;

  const Type* type = qual->getAsType();
  if (!type)
    return 0;

  return RecordInfo::GetDependentTemplatedDecl(*type);
}

namespace {

class FindFieldVisitor : public RecursiveASTVisitor<FindFieldVisitor> {
 public:
  FindFieldVisitor();
  FieldDecl* field() const;
  bool TraverseMemberExpr(MemberExpr* member);

 private:
  FieldDecl* field_;
};

FindFieldVisitor::FindFieldVisitor() : field_(0) {}

FieldDecl* FindFieldVisitor::field() const {
  return field_;
}

bool FindFieldVisitor::TraverseMemberExpr(MemberExpr* member) {
  if (FieldDecl* field = dyn_cast<FieldDecl>(member->getMemberDecl())) {
    field_ = field;
    return false;
  }
  return true;
}

}  // namespace

void CheckTraceVisitor::CheckCXXDependentScopeMemberExpr(
    CallExpr* call,
    CXXDependentScopeMemberExpr* expr) {
  std::string fn_name = expr->getMember().getAsString();

  // Check for VisitorDispatcher::trace(field) and
  // VisitorDispatcher::registerWeakMembers.
  if (!expr->isImplicitAccess()) {
    if (DeclRefExpr* base_decl = dyn_cast<DeclRefExpr>(expr->getBase())) {
      if (Config::IsVisitorDispatcherType(base_decl->getType())) {
        if (call->getNumArgs() == 1 && fn_name == kTraceName) {
          FindFieldVisitor finder;
          finder.TraverseStmt(call->getArg(0));
          if (finder.field())
            FoundField(finder.field());

          return;
        } else if (call->getNumArgs() == 1 &&
                   fn_name == kRegisterWeakMembersName) {
          MarkAllWeakMembersTraced();
        }
      }
    }
  }

  CXXRecordDecl* tmpl = GetDependentTemplatedDecl(expr);
  if (!tmpl)
    return;

  // Check for Super<T>::trace(visitor)
  if (call->getNumArgs() == 1 && IsTraceCallName(fn_name)) {
    RecordInfo::Bases::iterator it = info_->GetBases().begin();
    for (; it != info_->GetBases().end(); ++it) {
      if (it->first->getName() == tmpl->getName())
        it->second.MarkTraced();
    }
  }

  // Check for TraceIfNeeded<T>::trace(visitor, &field)
  if (call->getNumArgs() == 2 && fn_name == kTraceName &&
      tmpl->getName() == kTraceIfNeededName) {
    FindFieldVisitor finder;
    finder.TraverseStmt(call->getArg(1));
    if (finder.field())
      FoundField(finder.field());
  }
}

bool CheckTraceVisitor::CheckTraceBaseCall(CallExpr* call) {
  // Checks for "Base::trace(visitor)"-like calls.

  // Checking code for these two variables is shared among MemberExpr* case
  // and UnresolvedMemberCase* case below.
  //
  // For example, if we've got "Base::trace(visitor)" as |call|,
  // callee_record will be "Base", and func_name will be "trace".
  CXXRecordDecl* callee_record = nullptr;
  std::string func_name;

  if (MemberExpr* callee = dyn_cast<MemberExpr>(call->getCallee())) {
    if (!callee->hasQualifier())
      return false;

    FunctionDecl* trace_decl =
        dyn_cast<FunctionDecl>(callee->getMemberDecl());
    if (!trace_decl || !Config::IsTraceMethod(trace_decl))
      return false;

    const Type* type = callee->getQualifier()->getAsType();
    if (!type)
      return false;

    callee_record = type->getAsCXXRecordDecl();
    func_name = trace_decl->getName();
  } else if (UnresolvedMemberExpr* callee =
             dyn_cast<UnresolvedMemberExpr>(call->getCallee())) {
    // Callee part may become unresolved if the type of the argument
    // ("visitor") is a template parameter and the called function is
    // overloaded.
    //
    // Here, we try to find a function that looks like trace() from the
    // candidate overloaded functions, and if we find one, we assume it is
    // called here.

    CXXMethodDecl* trace_decl = nullptr;
    for (NamedDecl* named_decl : callee->decls()) {
      if (CXXMethodDecl* method_decl = dyn_cast<CXXMethodDecl>(named_decl)) {
        if (Config::IsTraceMethod(method_decl)) {
          trace_decl = method_decl;
          break;
        }
      }
    }
    if (!trace_decl)
      return false;

    // Check if the passed argument is named "visitor".
    if (call->getNumArgs() != 1)
      return false;
    DeclRefExpr* arg = dyn_cast<DeclRefExpr>(call->getArg(0));
    if (!arg || arg->getNameInfo().getAsString() != kVisitorVarName)
      return false;

    callee_record = trace_decl->getParent();
    func_name = callee->getMemberName().getAsString();
  }

  if (!callee_record)
    return false;

  if (!IsTraceCallName(func_name))
    return false;

  for (auto& base : info_->GetBases()) {
    // We want to deal with omitted trace() function in an intermediary
    // class in the class hierarchy, e.g.:
    //     class A : public GarbageCollected<A> { trace() { ... } };
    //     class B : public A { /* No trace(); have nothing to trace. */ };
    //     class C : public B { trace() { B::trace(visitor); } }
    // where, B::trace() is actually A::trace(), and in some cases we get
    // A as |callee_record| instead of B. We somehow need to mark B as
    // traced if we find A::trace() call.
    //
    // To solve this, here we keep going up the class hierarchy as long as
    // they are not required to have a trace method. The implementation is
    // a simple DFS, where |base_records| represents the set of base classes
    // we need to visit.

    std::vector<CXXRecordDecl*> base_records;
    base_records.push_back(base.first);

    while (!base_records.empty()) {
      CXXRecordDecl* base_record = base_records.back();
      base_records.pop_back();

      if (base_record == callee_record) {
        // If we find a matching trace method, pretend the user has written
        // a correct trace() method of the base; in the example above, we
        // find A::trace() here and mark B as correctly traced.
        base.second.MarkTraced();
        return true;
      }

      if (RecordInfo* base_info = cache_->Lookup(base_record)) {
        if (!base_info->RequiresTraceMethod()) {
          // If this base class is not required to have a trace method, then
          // the actual trace method may be defined in an ancestor.
          for (auto& inner_base : base_info->GetBases())
            base_records.push_back(inner_base.first);
        }
      }
    }
  }

  return false;
}

bool CheckTraceVisitor::CheckTraceFieldMemberCall(CXXMemberCallExpr* call) {
  return CheckTraceFieldCall(call->getMethodDecl()->getNameAsString(),
                             call->getRecordDecl(),
                             call->getArg(0));
}

bool CheckTraceVisitor::CheckTraceFieldCall(
    const std::string& name,
    CXXRecordDecl* callee,
    Expr* arg) {
  if (name != kTraceName || !Config::IsVisitor(callee->getName()))
    return false;

  FindFieldVisitor finder;
  finder.TraverseStmt(arg);
  if (finder.field())
    FoundField(finder.field());

  return true;
}

bool CheckTraceVisitor::CheckRegisterWeakMembers(CXXMemberCallExpr* call) {
  CXXMethodDecl* fn = call->getMethodDecl();
  if (fn->getName() != kRegisterWeakMembersName)
    return false;

  if (fn->isTemplateInstantiation()) {
    const TemplateArgumentList& args =
        *fn->getTemplateSpecializationInfo()->TemplateArguments;
    // The second template argument is the callback method.
    if (args.size() > 1 &&
        args[1].getKind() == TemplateArgument::Declaration) {
      if (FunctionDecl* callback =
          dyn_cast<FunctionDecl>(args[1].getAsDecl())) {
        if (callback->hasBody()) {
          CheckTraceVisitor nested_visitor(nullptr, info_, nullptr);
          nested_visitor.TraverseStmt(callback->getBody());
        }
      }
      // TODO: mark all WeakMember<>s as traced even if
      // the body isn't available?
    }
  }
  return true;
}

bool CheckTraceVisitor::IsWeakCallback() const {
  return !trace_;
}

void CheckTraceVisitor::MarkTraced(RecordInfo::Fields::iterator it) {
  // In a weak callback we can't mark strong fields as traced.
  if (IsWeakCallback() && !it->second.edge()->IsWeakMember())
    return;
  it->second.MarkTraced();
}

void CheckTraceVisitor::FoundField(FieldDecl* field) {
  if (Config::IsTemplateInstantiation(info_->record())) {
    // Pointer equality on fields does not work for template instantiations.
    // The trace method refers to fields of the template definition which
    // are different from the instantiated fields that need to be traced.
    const std::string& name = field->getNameAsString();
    for (RecordInfo::Fields::iterator it = info_->GetFields().begin();
         it != info_->GetFields().end();
         ++it) {
      if (it->first->getNameAsString() == name) {
        MarkTraced(it);
        break;
      }
    }
  } else {
    RecordInfo::Fields::iterator it = info_->GetFields().find(field);
    if (it != info_->GetFields().end())
      MarkTraced(it);
  }
}

void CheckTraceVisitor::MarkAllWeakMembersTraced() {
  // If we find a call to registerWeakMembers which is unresolved we
  // unsoundly consider all weak members as traced.
  // TODO: Find out how to validate weak member tracing for unresolved call.
  for (auto& field : info_->GetFields()) {
    if (field.second.edge()->IsWeakMember())
      field.second.MarkTraced();
  }
}
