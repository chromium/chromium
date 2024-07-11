// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/ParentMap.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Analysis/CFG.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Refactoring/AtomicChange.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Transformer/RewriteRule.h"
#include "clang/Tooling/Transformer/SourceCodeBuilders.h"
#include "clang/Tooling/Transformer/Stencil.h"
#include "clang/Tooling/Transformer/Transformer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"

using clang::tooling::AtomicChange;
using clang::tooling::AtomicChanges;
using clang::tooling::Transformer;
using namespace ::clang::ast_matchers;
using namespace ::clang::transformer;

namespace {

// Custom matcher to differentiate variable initializations based on the syntax.
AST_MATCHER_P(clang::VarDecl,
              hasInitStyle,
              clang::VarDecl::InitializationStyle,
              InitStyle) {
  return Node.getInitStyle() == InitStyle;
}

// Like `maybeDeref`, but with support for smart pointers. Assumes that any type
// that overloads `->` also overloads `*`.
Stencil maybeDerefSmart(std::string ID) {
  return run([ID = std::move(ID)](const MatchFinder::MatchResult& result)
                 -> llvm::Expected<std::string> {
    if (const auto* op_call =
            result.Nodes.getNodeAs<clang::CXXOperatorCallExpr>(ID)) {
      if (op_call->getOperator() == clang::OO_Arrow &&
          op_call->getNumArgs() == 1) {
        llvm::Optional<std::string> text = clang::tooling::buildDereference(
            *op_call->getArg(0), *result.Context);
        if (!text) {
          return llvm::make_error<llvm::StringError>(
              llvm::errc::invalid_argument,
              "ID has no corresponding source: " + ID);
        }
        return *text;
      }
    }
    return maybeDeref(std::move(ID))->eval(result);
  });
}

// A matcher that matches the `as_string()` member function call on a
// StringPiece. Binds both the call to `as_string()`, as well as the
// StringPiece.
auto GetAsStringMatcher() {
  return materializeTemporaryExpr(
             has(ignoringParenImpCasts(
                 cxxBindTemporaryExpr(has(cxxMemberCallExpr(
                     on(expr().bind("piece")),
                     callee(cxxMethodDecl(
                         ofClass(hasName("::base::BasicStringPiece")),
                         hasName("as_string")))))))))
      .bind("as_string");
}

// Replaces calls of `piece.as_string()` and `piece_ptr->as_string()` with
// `std::string(piece)` and `std::string(*piece_ptr)` respectively.
RewriteRule ReplaceAsStringWithExplicitStringConversionRule() {
  return makeRule(GetAsStringMatcher(),
                  changeTo(cat("std::string(", maybeDerefSmart("piece"), ")")));
}

// A rule that rewrites expressions like `std::string str = piece.as_string();`
// to `std::string str(foo);`, making use of the explicit conversion from
// base::StringPiece to std::string.
RewriteRule RewriteImplicitToExplicitStringConstructionRule() {
  auto matcher = materializeTemporaryExpr(
      GetAsStringMatcher(), hasParent(cxxConstructExpr(
                                hasDeclaration(cxxConstructorDecl(
                                    ofClass(hasName("::std::basic_string")))),
                                hasParent(exprWithCleanups(hasParent(
                                    varDecl(hasInitStyle(clang::VarDecl::CInit))
                                        .bind("varDecl")))))));
  return makeRule(
      matcher,
      // Remove the existing initialization via assignment and insert a new
      // making use of explicit construction.
      editList({
          remove(between(name("varDecl"), after(node("as_string")))),
          insertAfter(name("varDecl"), cat("(", maybeDerefSmart("piece"), ")")),
      }));
}

// A rule that removes redundant calls to `as_string`. This can happen if:
//
// (1) the resulting string is converted to another string piece,
// (2) the resulting string is involved in a call to a member function (2a) or
//     operator (2b) StringPiece also supports, or
// (3) the as_string call is part of the explicit construction of a std::string.
//     This can either be a local variable that is explicitly constructed (3a),
//     or a class member initialized by the constructor list (3b).
//
// The resulting rewrite rule will replace expressions like `piece.as_string()`
// simply with `piece`, and expressions like `piece_ptr->as_string()` with
// either `*piece_ptr` or `piece_ptr->`, depending on whether or not it is
// followed by a member expression.
RewriteRule RemoveAsStringRule() {
  // List of std::string members that are also supported by base::StringPiece.
  // Note: `data()` is absent from this list, because std::string::data is
  // guaranteed to return a null-terminated string, while
  // base::StringPiece::data is not. Furthermore, `substr()` is missing as well,
  // due to the possibly breaking change in return type (std::string vs
  // base::StringPiece).
  static constexpr llvm::StringRef kMatchingStringMembers[] = {
      "begin",
      "cbegin",
      "end",
      "cend",
      "rbegin",
      "crbegin",
      "rend",
      "crend",
      "at",
      "front",
      "back",
      "size",
      "length",
      "max_size",
      "empty",
      "copy",
      "compare",
      "find",
      "rfind",
      "find_first_of",
      "find_last_of",
      "find_first_not_of",
      "find_last_not_of",
      "npos",
  };

  // List of std::string operators that are also supported by base::StringPiece.
  // Note: `operator[]` is absent from this list, because string::operator[idx]
  // is valid for idx == size(), while base::StringPiece::operator[] is not.
  static constexpr llvm::StringRef kMatchingStringOperators[] = {
      "==", "!=", "<", ">", "<=", ">=", "<<",
  };

  auto string_piece_construct_expr = cxxConstructExpr(hasDeclaration(
      cxxConstructorDecl(ofClass(hasName("::base::BasicStringPiece")))));

  auto matching_string_member_expr =
      memberExpr(member(hasAnyName(kMatchingStringMembers))).bind("member");

  auto matching_string_operator_call_expr = cxxOperatorCallExpr(
      hasAnyOverloadedOperatorName(kMatchingStringOperators));

  auto string_construct_expr = cxxConstructExpr(hasDeclaration(
      cxxConstructorDecl(ofClass(hasName("::std::basic_string")))));

  // Matches the explicit construction of a string variable, i.e. not making use
  // of C-style assignment syntax.
  auto explicit_string_var_construct_expr = cxxConstructExpr(
      string_construct_expr,
      hasParent(exprWithCleanups(
          hasParent(varDecl(unless(hasInitStyle(clang::VarDecl::CInit)))))));

  auto string_class_member_construct_expr = cxxConstructExpr(
      string_construct_expr,
      hasParent(exprWithCleanups(hasParent(cxxConstructorDecl()))));

  auto matcher = materializeTemporaryExpr(
      GetAsStringMatcher(),
      anyOf(
          // Case (1)
          hasParent(string_piece_construct_expr),
          // Case (2a)
          hasParent(matching_string_member_expr),
          // Const APIs like `size()` or `find()` add an extra implicit cast
          // to const std::string here, that we need to ignore.
          hasParent(implicitCastExpr(hasParent(matching_string_member_expr))),
          // Case (2b)
          hasParent(matching_string_operator_call_expr),
          // Case (3a)
          hasParent(explicit_string_var_construct_expr),
          // Case (3b)
          hasParent(string_class_member_construct_expr)));
  return makeRule(
      matcher,
      // In case there is a bound member expression, construct an access
      // expression into the string piece. This is required to handle
      // expressions like `piece_ptr->as_string().some_member()` correctly.
      ifBound("member",
              changeTo(node("member"), access("piece", cat(member("member")))),
              changeTo(maybeDerefSmart("piece"))));
}

// Returns a consumer that adds `change` to `changes` if present.
Transformer::ChangeConsumer GetConsumer(AtomicChanges& changes) {
  return [&changes](llvm::Expected<AtomicChange> change) {
    if (change)
      changes.push_back(*change);
  };
}

}  // namespace

int main(int argc, const char* argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();

  static llvm::cl::OptionCategory tool_options("Tool options");
  clang::tooling::CommonOptionsParser options(argc, argv, tool_options);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  // Combine the above rules into a single one and add an include for the right
  // header.
  RewriteRule as_string_rule = applyFirst({
      RemoveAsStringRule(),
      RewriteImplicitToExplicitStringConstructionRule(),
      ReplaceAsStringWithExplicitStringConversionRule(),
  });

  AtomicChanges changes;
  Transformer transformer(as_string_rule, GetConsumer(changes));

  MatchFinder match_finder;
  transformer.registerMatchers(&match_finder);
  auto factory = clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(factory.get());
  if (result != 0)
    return result;

  if (changes.empty())
    return 0;

  // Serialization format is documented in tools/clang/scripts/run_tool.py
  llvm::outs() << "==== BEGIN EDITS ====\n";
  for (const auto& change : changes) {
    for (const auto& r : change.getReplacements()) {
      std::string replacement(r.getReplacementText());
      std::replace(replacement.begin(), replacement.end(), '\n', '\0');
      llvm::outs() << "r:::" << r.getFilePath() << ":::" << r.getOffset()
                   << ":::" << r.getLength() << ":::" << replacement << "\n";
    }

    for (const auto& header : change.getInsertedHeaders()) {
      llvm::outs() << "include-user-header:::" << change.getFilePath()
                   << ":::-1:::-1:::" << header << "\n";
    }
  }
  llvm::outs() << "==== END EDITS ====\n";

  return 0;
}
