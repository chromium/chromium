// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This implements a Clang tool to annotate methods with tracing. It should be
// run using the tools/clang/scripts/run_tool.py helper as described in
// README.md

#include <string>
#include <vector>
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"

using namespace clang::ast_matchers;
using clang::tooling::CommonOptionsParser;
using clang::tooling::Replacement;
using clang::tooling::Replacements;

namespace {

class FunctionDefCallback : public MatchFinder::MatchCallback {
 public:
  explicit FunctionDefCallback(std::vector<Replacement>* replacements)
      : replacements_(replacements) {}

  void run(const MatchFinder::MatchResult& result) override;

 private:
  std::vector<Replacement>* const replacements_;
};

class TraceAnnotator {
 public:
  explicit TraceAnnotator(std::vector<Replacement>* replacements)
      : function_def_callback_(replacements) {}

  void SetupMatchers(MatchFinder* match_finder);

 private:
  FunctionDefCallback function_def_callback_;
};

// Given:
//   template <typename T, typename T2> void foo(T t, T2 t2) {};  // N1 and N4
//   template <typename T2> void foo<int, T2>(int t, T2 t) {};    // N2
//   template <> void foo<int, char>(int t, char t2) {};          // N3
//   void foo() {
//     // This creates implicit template specialization (N4) out of the
//     // explicit template definition (N1).
//     foo<bool, double>(true, 1.23);
//   }
// with the following AST nodes:
//   FunctionTemplateDecl foo
//   |-FunctionDecl 0x191da68 foo 'void (T, T2)'         // N1
//   `-FunctionDecl 0x194bf08 foo 'void (bool, double)'  // N4
//   FunctionTemplateDecl foo
//   `-FunctionDecl foo 'void (int, T2)'                 // N2
//   FunctionDecl foo 'void (int, char)'                 // N3
//
// Matches AST node N4, but not AST nodes N1, N2 nor N3.
AST_MATCHER(clang::FunctionDecl, isImplicitFunctionTemplateSpecialization) {
  switch (Node.getTemplateSpecializationKind()) {
    case clang::TSK_ImplicitInstantiation:
      return true;
    case clang::TSK_Undeclared:
    case clang::TSK_ExplicitSpecialization:
    case clang::TSK_ExplicitInstantiationDeclaration:
    case clang::TSK_ExplicitInstantiationDefinition:
      return false;
  }
}

AST_POLYMORPHIC_MATCHER(isInMacroLocation,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(clang::Decl,
                                                        clang::Stmt,
                                                        clang::TypeLoc)) {
  return Node.getBeginLoc().isMacroID();
}

void TraceAnnotator::SetupMatchers(MatchFinder* match_finder) {
  const clang::ast_matchers::DeclarationMatcher function_call =
      functionDecl(
          has(compoundStmt().bind("function body")),
          /* Avoid matching the following cases: */
          unless(anyOf(
              /* Do not match implicit function template specializations to
                 avoid conflicting edits. */
              isImplicitFunctionTemplateSpecialization(),
              /* Do not match constexpr functions. */
              isConstexpr(), isDefaulted(),
              /* Do not match ctor/dtor. */
              cxxConstructorDecl(), cxxDestructorDecl(),
              /* Tracing macros can be tricky (e.g., QuicUint128Impl comparison
                 operators). */
              isInMacroLocation(), has(compoundStmt(isInMacroLocation())),
              /* Do not trace lambdas (no name, possbly tracking more parameters
                 than intended because of [&]). */
              hasParent(cxxRecordDecl(isLambda())))))
          .bind("function");
  match_finder->addMatcher(function_call, &function_def_callback_);
}

// Returns a string containing the qualified name of the function. Does not
// output template parameters of the function or in case of methods of the
// associated class (as opposed to |function->getQualifiedNameAsString|).
std::string getFunctionName(const clang::FunctionDecl* function) {
  std::string qualified_name;
  // Add namespace(s) to the name.
  if (auto* name_space = llvm::dyn_cast<clang::NamespaceDecl>(
          function->getEnclosingNamespaceContext())) {
    qualified_name += name_space->getQualifiedNameAsString();
    qualified_name += "::";
  }
  // If the function is a method, add class name (without templates).
  if (auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(function)) {
    qualified_name += method->getParent()->getNameAsString();
    qualified_name += "::";
  }
  // Add function name (without templates).
  qualified_name += function->getNameAsString();
  return qualified_name;
}

void FunctionDefCallback::run(const MatchFinder::MatchResult& result) {
  const clang::FunctionDecl* function =
      result.Nodes.getNodeAs<clang::FunctionDecl>("function");
  // Using this instead of |function->getBody| prevents conflicts with parameter
  // names in headers and implementations.
  const clang::CompoundStmt* function_body =
      result.Nodes.getNodeAs<clang::CompoundStmt>("function body");
  clang::CharSourceRange range =
      clang::CharSourceRange::getTokenRange(function_body->getBeginLoc());

  const char kReplacementTextTemplate[] = R"( TRACE_EVENT0("test", "{0}"); )";
  std::string function_name = getFunctionName(function);
  std::string replacement_text =
      llvm::formatv(kReplacementTextTemplate, function_name).str();

  const char kAnnotationTemplate[] = " { {0}";
  std::string annotation =
      llvm::formatv(kAnnotationTemplate, replacement_text).str();

  replacements_->push_back(
      Replacement(*result.SourceManager, range, annotation));
}

}  // namespace

static llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  llvm::cl::OptionCategory category("TraceAnnotator Tool");
  llvm::Expected<CommonOptionsParser> options =
      CommonOptionsParser::create(argc, argv, category);
  if (!options) {
    llvm::outs() << llvm::toString(options.takeError());
    return 1;
  }
  clang::tooling::ClangTool tool(options->getCompilations(),
                                 options->getSourcePathList());

  std::vector<Replacement> replacements;
  TraceAnnotator converter(&replacements);
  MatchFinder match_finder;
  converter.SetupMatchers(&match_finder);

  std::unique_ptr<clang::tooling::FrontendActionFactory> frontend_factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(frontend_factory.get());
  if (result != 0)
    return result;

  if (replacements.empty())
    return 0;

  // Each replacement line should have the following format:
  // r:<file path>:<offset>:<length>:<replacement text>
  // Only the <replacement text> field can contain embedded ":" characters.
  // TODO(dcheng): Use a more clever serialization. Ideally we'd use the YAML
  // serialization and then use clang-apply-replacements, but that would require
  // copying and pasting a larger amount of boilerplate for all Chrome clang
  // tools.

  // Keep a set of files where we have already added base_tracing include.
  std::set<std::string> include_added_to;

  llvm::outs() << "==== BEGIN EDITS ====\n";
  for (const auto& r : replacements) {
    // Add base_tracing import if necessary.
    if (include_added_to.find(r.getFilePath().str()) ==
        include_added_to.end()) {
      include_added_to.insert(r.getFilePath().str());
      // Add also copyright so that |test-expected.cc| passes presubmit.
      llvm::outs() << "include-user-header:::" << r.getFilePath()
                   << ":::-1:::-1:::base/trace_event/base_tracing.h"
                   << "\n";
    }
    // Add the actual replacement.
    llvm::outs() << "r:::" << r.getFilePath() << ":::" << r.getOffset()
                 << ":::" << r.getLength() << ":::" << r.getReplacementText()
                 << "\n";
  }
  llvm::outs() << "==== END EDITS ====\n";

  return 0;
}
