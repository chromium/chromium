// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Clang tool to change calls to scoper::Pass() to just use std::move().

#include <memory>
#include <string>

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::ast_matchers;
using clang::tooling::CommonOptionsParser;
using clang::tooling::Replacement;
using clang::tooling::Replacements;
using llvm::StringRef;

namespace {

class RewriterCallback : public MatchFinder::MatchCallback {
 public:
  explicit RewriterCallback(Replacements* replacements)
      : replacements_(replacements) {}
  virtual void run(const MatchFinder::MatchResult& result) override;

 private:
  Replacements* const replacements_;
};

void RewriterCallback::run(const MatchFinder::MatchResult& result) {
  const clang::CXXMemberCallExpr* call_expr =
      result.Nodes.getNodeAs<clang::CXXMemberCallExpr>("expr");
  const clang::MemberExpr* callee =
      clang::dyn_cast<clang::MemberExpr>(call_expr->getCallee());
  const bool is_arrow = callee->isArrow();
  const clang::Expr* arg = result.Nodes.getNodeAs<clang::Expr>("arg");

  const char kMoveRefText[] = "std::move(";
  const char kMovePtrText[] = "std::move(*";

  auto err = replacements_->add(
      Replacement(*result.SourceManager,
                  result.SourceManager->getSpellingLoc(arg->getLocStart()), 0,
                  is_arrow ? kMovePtrText : kMoveRefText));
  assert(!err);

  // Delete everything but the closing parentheses from the original call to
  // Pass(): the closing parantheses is left to match up with the parantheses
  // just inserted with std::move.
  err = replacements_->add(Replacement(
      *result.SourceManager,
      clang::CharSourceRange::getCharRange(
          result.SourceManager->getSpellingLoc(callee->getOperatorLoc()),
          result.SourceManager->getSpellingLoc(call_expr->getRParenLoc())),
      ""));
  assert(!err);
}

}  // namespace

static llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  // TODO(dcheng): Clang tooling should do this itself.
  // http://llvm.org/bugs/show_bug.cgi?id=21627
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::cl::OptionCategory category(
      "C++11 modernization: change scoped::Pass() to std::move()");
  CommonOptionsParser options(argc, argv, category);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  MatchFinder match_finder;
  Replacements replacements;

  auto pass_matcher = id(
      "expr",
      cxxMemberCallExpr(
          argumentCountIs(0),
          callee(functionDecl(hasName("Pass"), returns(rValueReferenceType()))),
          on(id("arg", expr()))));
  RewriterCallback callback(&replacements);
  match_finder.addMatcher(pass_matcher, &callback);

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(factory.get());
  if (result != 0)
    return result;

  if (replacements.empty())
    return 0;

  // Serialization format is documented in tools/clang/scripts/run_tool.py
  llvm::outs() << "==== BEGIN EDITS ====\n";
  for (const auto& r : replacements) {
    std::string replacement_text = r.getReplacementText().str();
    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    llvm::outs() << "r:::" << r.getFilePath() << ":::" << r.getOffset()
                 << ":::" << r.getLength() << ":::" << replacement_text << "\n";
  }
  llvm::outs() << "==== END EDITS ====\n";

  return 0;
}
