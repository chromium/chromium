// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CheckIPCVisitor.h"

using namespace clang;

namespace chrome_checker {

namespace {

const char kWriteParamBadType[] =
    "[chromium-ipc] IPC::WriteParam() is called on blocklisted type '%0'%1.";

const char kTupleBadType[] =
    "[chromium-ipc] IPC tuple references banned type '%0'%1.";

const char kWriteParamBadSignature[] =
    "[chromium-ipc] IPC::WriteParam() is expected to have two arguments.";

const char kNoteSeeHere[] =
    "see here";

}  // namespace

CheckIPCVisitor::CheckIPCVisitor(CompilerInstance& compiler)
  : compiler_(compiler), context_(nullptr) {
  auto& diagnostics = compiler_.getDiagnostics();
  error_write_param_bad_type_ = diagnostics.getCustomDiagID(
      DiagnosticsEngine::Error, kWriteParamBadType);
  error_tuple_bad_type_ = diagnostics.getCustomDiagID(
      DiagnosticsEngine::Error, kTupleBadType);
  error_write_param_bad_signature_ = diagnostics.getCustomDiagID(
      DiagnosticsEngine::Error, kWriteParamBadSignature);
  note_see_here_ = diagnostics.getCustomDiagID(
      DiagnosticsEngine::Note, kNoteSeeHere);

  blocklisted_typedefs_ = llvm::StringSet<>({
      "intmax_t",
      "uintmax_t",
      "intptr_t",
      "uintptr_t",
      "wint_t",
      "size_t",
      "rsize_t",
      "ssize_t",
      "ptrdiff_t",
      "dev_t",
      "off_t",
      "clock_t",
      "time_t",
      "suseconds_t"
  });
}

void CheckIPCVisitor::BeginDecl(Decl* decl) {
  decl_stack_.push_back(decl);
}

void CheckIPCVisitor::EndDecl() {
  decl_stack_.pop_back();
}

void CheckIPCVisitor::VisitTemplateSpecializationType(
    TemplateSpecializationType* spec) {
  ValidateCheckedTuple(spec);
}

void CheckIPCVisitor::VisitCallExpr(CallExpr* call_expr) {
  ValidateWriteParam(call_expr);
}

bool CheckIPCVisitor::ValidateWriteParam(const CallExpr* call_expr) {
  const FunctionDecl* callee_decl = call_expr->getDirectCallee();
  if (!callee_decl ||
      callee_decl->getQualifiedNameAsString() != "IPC::WriteParam") {
    return true;
  }

  return ValidateWriteParamSignature(call_expr) &&
      ValidateWriteParamArgument(call_expr->getArg(1));
}

// Checks that IPC::WriteParam() has expected signature.
bool CheckIPCVisitor::ValidateWriteParamSignature(
    const CallExpr* call_expr) {
  if (call_expr->getNumArgs() != 2) {
    compiler_.getDiagnostics().Report(
        call_expr->getExprLoc(), error_write_param_bad_signature_);
    return false;
  }
  return true;
}

// Checks that IPC::WriteParam() argument type is allowed.
// See CheckType() for specifics.
bool CheckIPCVisitor::ValidateWriteParamArgument(const Expr* arg_expr) {
  if (auto* parent_fn_decl = GetParentDecl<FunctionDecl>()) {
    auto template_kind = parent_fn_decl->getTemplatedKind();
    if (template_kind != FunctionDecl::TK_NonTemplate &&
        template_kind != FunctionDecl::TK_FunctionTemplate) {
      // Skip all specializations - we don't check WriteParam() on dependent
      // types (typedef info gets lost), and we checked all non-dependent uses
      // earlier (when we checked the template itself).
      return true;
    }
  }

  QualType arg_type;

  arg_expr = arg_expr->IgnoreImplicit();
  if (auto* cast_expr = dyn_cast<ExplicitCastExpr>(arg_expr)) {
    arg_type = cast_expr->getTypeAsWritten();
  } else {
    arg_type = arg_expr->getType();
  }

  CheckDetails details;
  if (CheckType(arg_type, &details)) {
    return true;
  }

  ReportCheckError(details,
                   arg_expr->getExprLoc(),
                   error_write_param_bad_type_);

  return false;
}

// Checks that IPC::CheckedTuple<> is specialized with allowed types.
// See CheckType() above for specifics.
bool CheckIPCVisitor::ValidateCheckedTuple(
    const TemplateSpecializationType* spec) {
  TemplateDecl* decl = spec->getTemplateName().getAsTemplateDecl();
  if (!decl || decl->getQualifiedNameAsString() != "IPC::CheckedTuple") {
    return true;
  }

  bool valid = true;
  for (const TemplateArgument& arg : spec->template_arguments()) {
    CheckDetails details;
    if (CheckTemplateArgument(arg, &details)) {
      continue;
    }

    valid = false;

    auto* parent_decl = GetParentDecl<Decl>();
    ReportCheckError(
        details, parent_decl ? parent_decl->getBeginLoc() : SourceLocation(),
        error_tuple_bad_type_);
  }

  return valid;
}

template <typename T>
const T* CheckIPCVisitor::GetParentDecl() const {
  for (auto i = decl_stack_.rbegin(); i != decl_stack_.rend(); ++i) {
    if (auto* parent = dyn_cast_or_null<T>(*i)) {
      return parent;
    }
  }
  return nullptr;
}


bool CheckIPCVisitor::IsBlacklistedType(QualType type) const {
  return context_->hasSameUnqualifiedType(type, context_->LongTy) ||
      context_->hasSameUnqualifiedType(type, context_->UnsignedLongTy);
}

bool CheckIPCVisitor::IsBlacklistedTypedef(const TypedefNameDecl* tdef) const {
  return blocklisted_typedefs_.find(tdef->getName()) !=
      blocklisted_typedefs_.end();
}

// Checks that integer type is allowed (not blocklisted).
bool CheckIPCVisitor::CheckIntegerType(QualType type,
                                       CheckDetails* details) const {
  bool seen_typedef = false;
  while (true) {
    details->exit_type = type;

    if (auto* tdef = dyn_cast<TypedefType>(type)) {
      if (IsBlacklistedTypedef(tdef->getDecl())) {
        return false;
      }
      details->typedefs.push_back(tdef);
      seen_typedef = true;
    }

    QualType desugared_type =
        type->getLocallyUnqualifiedSingleStepDesugaredType();
    if (desugared_type == type) {
      break;
    }

    type = desugared_type;
  }

  return seen_typedef || !IsBlacklistedType(type);
}

// Checks that |type| is allowed (not blocklisted), recursively visiting
// template specializations.
bool CheckIPCVisitor::CheckType(QualType type, CheckDetails* details) const {
  if (type->isReferenceType()) {
    type = type->getPointeeType();
  }
  type = type.getLocalUnqualifiedType();

  if (details->entry_type.isNull()) {
    details->entry_type = type;
  }

  if (type->isIntegerType()) {
    return CheckIntegerType(type, details);
  }

  while (true) {
    if (auto* spec = dyn_cast<TemplateSpecializationType>(type)) {
      for (const TemplateArgument& arg : spec->template_arguments()) {
        if (!CheckTemplateArgument(arg, details)) {
          return false;
        }
      }
      return true;
    }

    if (auto* record = dyn_cast<RecordType>(type)) {
      if (auto* spec = dyn_cast<ClassTemplateSpecializationDecl>(
              record->getDecl())) {
        const TemplateArgumentList& args = spec->getTemplateArgs();
        for (unsigned i = 0; i != args.size(); ++i) {
          if (!CheckTemplateArgument(args[i], details)) {
            return false;
          }
        }
      }
      return true;
    }

    if (auto* tdef = dyn_cast<TypedefType>(type)) {
      details->typedefs.push_back(tdef);
    }

    QualType desugared_type =
        type->getLocallyUnqualifiedSingleStepDesugaredType();
    if (desugared_type == type) {
      break;
    }

    type = desugared_type;
  }

  return true;
}

bool CheckIPCVisitor::CheckTemplateArgument(const TemplateArgument& arg,
                                            CheckDetails* details) const {
  return arg.getKind() != TemplateArgument::Type ||
      CheckType(arg.getAsType(), details);
}

void CheckIPCVisitor::ReportCheckError(const CheckDetails& details,
                                       SourceLocation loc,
                                       unsigned error) {
  DiagnosticsEngine& diagnostics = compiler_.getDiagnostics();

  std::string entry_type = details.entry_type.getAsString();
  std::string exit_type = details.exit_type.getAsString();

  std::string via;
  if (entry_type != exit_type) {
    via = " via '" + entry_type + "'";
  }
  diagnostics.Report(loc, error) << exit_type << via;

  for (const TypedefType* tdef: details.typedefs) {
    diagnostics.Report(tdef->getDecl()->getLocation(), note_see_here_);
  }
}

}  // namespace chrome_checker
