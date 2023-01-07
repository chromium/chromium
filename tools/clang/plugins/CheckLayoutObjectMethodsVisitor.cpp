// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CheckLayoutObjectMethodsVisitor.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang::ast_matchers;

namespace chrome_checker {

std::string CheckLayoutObjectMethodsVisitor::layout_directory =
    "third_party/blink/renderer/core/layout";
std::string CheckLayoutObjectMethodsVisitor::test_directory =
    "tools/clang/plugins/tests";

namespace {

const char kLayoutObjectMethodWithoutIsNotDestroyedCheck[] =
    "[layout] LayoutObject's method %0 in %1 must call CheckIsNotDestroyed() "
    "at the beginning.";

class DiagnosticsReporter {
 public:
  explicit DiagnosticsReporter(clang::CompilerInstance& instance)
      : instance_(instance), diagnostic_(instance.getDiagnostics()) {
    diag_layout_object_method_without_is_not_destroyed_check_ =
        diagnostic_.getCustomDiagID(
            getErrorLevel(), kLayoutObjectMethodWithoutIsNotDestroyedCheck);
  }

  bool hasErrorOccurred() const { return diagnostic_.hasErrorOccurred(); }

  clang::DiagnosticsEngine::Level getErrorLevel() const {
    return diagnostic_.getWarningsAsErrors()
               ? clang::DiagnosticsEngine::Error
               : clang::DiagnosticsEngine::Warning;
  }

  void LayoutObjectMethodWithoutIsNotDestroyedCheck(
      const clang::CXXMethodDecl* expr,
      const clang::CXXRecordDecl* record) {
    ReportDiagnostic(expr->getBeginLoc(),
                     diag_layout_object_method_without_is_not_destroyed_check_)
        << expr << record << expr->getSourceRange();
  }

 private:
  clang::DiagnosticBuilder ReportDiagnostic(clang::SourceLocation location,
                                            unsigned diag_id) {
    clang::SourceManager& manager = instance_.getSourceManager();
    clang::FullSourceLoc full_loc(location, manager);
    return diagnostic_.Report(full_loc, diag_id);
  }

  clang::CompilerInstance& instance_;
  clang::DiagnosticsEngine& diagnostic_;

  unsigned diag_layout_object_method_without_is_not_destroyed_check_;
};

class LayoutObjectMethodMatcher : public MatchFinder::MatchCallback {
 public:
  explicit LayoutObjectMethodMatcher(class DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    const DeclarationMatcher function_call =
        cxxMethodDecl(
            hasParent(
                cxxRecordDecl(isSameOrDerivedFrom("::blink::LayoutObject"))),
            has(compoundStmt()),
            // Avoid matching the following cases
            unless(anyOf(isConstexpr(), isDefaulted(), isPure(),
                         cxxConstructorDecl(), cxxDestructorDecl(),
                         isStaticStorageClass(),
                         // Do not trace lambdas (no name, possibly tracking
                         // more parameters than intended because of [&]).
                         hasParent(cxxRecordDecl(isLambda())),
                         // Do not include CheckIsDestroyed() itself.
                         hasName("CheckIsNotDestroyed"),
                         // Do not include tracing methods.
                         hasName("Trace"), hasName("TraceAfterDispatch"))))
            .bind("layout_method");
    match_finder.addDynamicMatcher(function_call, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* method =
        result.Nodes.getNodeAs<clang::CXXMethodDecl>("layout_method");

    const auto* stmt = method->getBody();
    assert(stmt);

    if (!llvm::dyn_cast<clang::CompoundStmt>(stmt)->body_empty()) {
      auto* stmts = llvm::dyn_cast<clang::CompoundStmt>(stmt)->body_front();
      if (clang::CXXMemberCallExpr::classof(stmts)) {
        auto* call = llvm::dyn_cast<clang::CXXMemberCallExpr>(stmts);
        const std::string& name = call->getMethodDecl()->getNameAsString();
        if (name == "CheckIsNotDestroyed")
          return;
      }
    }

    auto* type = method->getParent();
    diagnostics_.LayoutObjectMethodWithoutIsNotDestroyedCheck(method, type);
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

}  // namespace

CheckLayoutObjectMethodsVisitor::CheckLayoutObjectMethodsVisitor(
    clang::CompilerInstance& compiler)
    : compiler_(compiler) {}

void CheckLayoutObjectMethodsVisitor::VisitLayoutObjectMethods(
    clang::ASTContext& ast_context) {
  const clang::FileEntry* file_entry =
      ast_context.getSourceManager().getFileEntryForID(
          ast_context.getSourceManager().getMainFileID());
  if (!file_entry)
    return;

  auto file_name_ref = file_entry->tryGetRealPathName();
  if (file_name_ref.empty())
    return;
  std::string file_name = file_name_ref.str();
#if defined(_WIN32)
  std::replace(file_name.begin(), file_name.end(), '\\', '/');
#endif
  if (file_name.find(layout_directory) == std::string::npos &&
      file_name.find(test_directory) == std::string::npos)
    return;

  MatchFinder match_finder;
  DiagnosticsReporter diagnostics(compiler_);

  LayoutObjectMethodMatcher layout_object_method_matcher(diagnostics);
  layout_object_method_matcher.Register(match_finder);

  match_finder.matchAST(ast_context);
}

}  // namespace chrome_checker
