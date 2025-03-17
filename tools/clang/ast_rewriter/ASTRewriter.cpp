// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Clang tool to perform simple rewrites of C++ code using clang's AST matchers.
// For more general documentation, as well as building & running instructions,
// see
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/clang_tool_refactoring.md
//
// As implemented, this tool looks for instances of `b ? "true" : "false"` and
// replaces them with calls to `base::ToString`.
//
// If you want to create your own tool based on this one:
// 1. Copy the ast_rewriter directory, and update CMakeLists.txt appropriately
// 2. Follow the building and running procedure described in
//    the linked documentation:
//    a. Bootstrap the plugin
//    b. Build chrome once normally, without precompiled headers
//    c. Run using run_tool.py
// 3. Perform any post-processing of the generated directives using dedup.py
// 4. Apply the directives as described in the linked documentation
//
// Note: When running the tool, you may get spurious warnings due to chromium-
// specific changes (e.g. #pragma allow_unsafe_buffers) that aren't. If so,
// it's easiest to disable -Werror in build/config/compiler.gni (set
// treat_warnings_as_errors = false). You may also want to disable the warning
// entirely while running the tool, by adding "-Wno-unknown-pragmas" to
// cflags_cc in an appropriate part of build/config/BUILD.gn. Make sure to
// rebuild the project (repeat step 2b) after changing the build config.

#include <string>

#include "OutputHelper.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/TargetSelect.h"

// Prints a clang::SourceLocation or clang::SourceRange.
// Most AST types also have a dump() function to print to stderr.
#define LOG(e)                                                     \
  llvm::errs() << __FILE__ << ":" << __LINE__ << ": " << #e << " " \
               << (e).printToString(*result.SourceManager) << '\n';

namespace {

// Setting up the command-line; you can add additional options here if needed
static llvm::cl::OptionCategory rewriter_category("ast_rewriter options");
llvm::cl::extrahelp common_help(
    clang::tooling::CommonOptionsParser::HelpMessage);
llvm::cl::extrahelp more_help(
    "This tool replaces instances of `b ? \"true\" : \"false\"` into"
    "`base::ToString(b)`");

using namespace clang;
using namespace clang::ast_matchers;

// Specify what code patterns you're looking for here. AST matchers have more
// complete documentation on the clang website: see
// https://clang.llvm.org/docs/LibASTMatchers.html
// and
// https://clang.llvm.org/docs/LibASTMatchersReference.html
//
// This particular matcher looks for ternary operators whose second and third
// operators are "true" and "false", e.g. `b ? "true" : "false"`.
// Unfortunately, the matchers clang supports are incomplete; it can't directly
// check string contents, but it can check string length. Fortunately, we can
// perform additional checks on the AST itself once we have a potential match.
// Therefore, it's usually best to write a general matcher, and narrow down the
// final results later.
//
// The general process for creating a new matcher is to follow the AST matcher
// link above, then manually sift through the gigantic listing to determine
// which matchers (if any) fit your use case. It is strongly recommended to use
// clang-query to test matchers dynamically until you've got them working the
// way you want; see the clang_tool_refactoring.md file for more information.
//
// Arguments to a matcher are sub-matchers that serve to narrow down matches.
// Some arguments (stmt(), expr(), etc) don't narrow at all, but provide a way
// to reference different parts of the match. These arguments can be bound
// by calling .bind() with a string; this allows the part of the match to be
// referenced later by passing that string.
//
// The various kinds of matchers and the way they're expected to be combined is
// complicated; the best way to learn about it is to read the docs and play
// around with clang-query.
StatementMatcher matchTernaryTrueFalse() {
  return conditionalOperator(  // Matches ternary boolean operators ( _ ? _ : _)
      stmt().bind(
          "root"),  // Bind the cond operator itself so we can refer to it
      hasCondition(
          expr().bind("cond")),  // Bind just the condition, same reason

      // Check that the true and false branches are if they're string literals
      // of length 4 and 5, and bind them. Match either order to account for `b
      // ? "false" : "true"`
      hasTrueExpression(
          expr(anyOf(stringLiteral(hasSize(4)), stringLiteral(hasSize(5))))
              .bind("tru")),
      hasFalseExpression(
          expr(anyOf(stringLiteral(hasSize(4)), stringLiteral(hasSize(5))))
              .bind("fls")));
}

const char* headers_to_add[] = {"base/strings/to_string.h"};

// Once you know what you're looking for, the next step is to specify what to do
// when you find it. This can be done by creating a class which inherits from
// MatchFinder::MatchCallback and implements the `run` function.
//
// The Printer class is a minimal example: it takes the result of the matcher,
// pulls out whatever was bound to "root", and dumps it to the screen. Good for
// debugging, although clang-query is better for debugging the matcher itself.
class Printer : public MatchFinder::MatchCallback {
 public:
  virtual void run(const MatchFinder::MatchResult& Result) override {
    // Only works if the matcher bound a Stmt to the name "root".
    if (const Stmt* FS = Result.Nodes.getNodeAs<clang::Stmt>("root")) {
      FS->dump();
    }
  }
};

// The ASTRewriter class is a more interesting example; in addition to the `run`
// function, it stores an OutputHelper, which will also be passed to clang's
// FrontendFactory. The factory will ensure that the OutputHelper's setup and
// teardown methods are invoked at the beginning/end of each run, so our
// rewriter can safely call it to emit output.
class ASTRewriter : public MatchFinder::MatchCallback {
 protected:
  OutputHelper& output_helper_;

 public:
  explicit ASTRewriter(OutputHelper* output_helper)
      : output_helper_(*output_helper) {}

  // Replaces `b ? "true" : "false"` with base::ToString(b).
  // This function has access to the full power of clang's AST, so
  // you can do as much work as you want. Unfortunately, much like matchers, the
  // best way to figure out what AST methods are available to you is to sift
  // through the documentation (https://clang.llvm.org/doxygen/) for whatever
  // classes you have at hand, and hope you find something applicable to your
  // situation.
  virtual void run(const MatchFinder::MatchResult& result) override {
    ASTContext* Context = result.Context;
    // Extract the entire statement we matched.
    const Stmt* root = result.Nodes.getNodeAs<ConditionalOperator>("root");
    if (!root) {
      return;
    }

    // Don't replace in macros
    // Things WILL go wrong if you try
    // Just do them by hand
    if (root->getBeginLoc().isMacroID()) {
      return;
    }

    // Don't replace in third-party code, or in the function we're replacing
    // things with.
    StringRef filename =
        Context->getSourceManager().getFilename(root->getBeginLoc());
    if (filename.contains("third_party/") ||
        filename.contains("base/strings/to_string.h")) {
      return;
    }

    // Extract the various components that we care about.
    const Expr* cond = result.Nodes.getNodeAs<Expr>("cond");
    const StringLiteral* tru = result.Nodes.getNodeAs<StringLiteral>("tru");
    const StringLiteral* fls = result.Nodes.getNodeAs<StringLiteral>("fls");
    if (!cond || !tru || !fls) {
      return;
    }

    bool true_is_first = false;
    if (!tru->getString().compare("true") &&
        !fls->getString().compare("false")) {
      // "true" : "false"
      true_is_first = true;
    } else if (!tru->getString().compare("false") &&
               !fls->getString().compare("true")) {
      // "false" : "true"
      true_is_first = false;
    } else {
      return;
    }

    // An example of something more complicated that we can't easily do with
    // matchers: See if the original expression was parenthesized,
    // and remove the parens as well if so.
    const auto& parents = Context->getParents(*root);
    if (!parents.empty()) {
      const Stmt* paren_root = parents[0].get<ParenExpr>();
      if (paren_root) {
        root = paren_root;
      }
    }

    // We use getTokenRange here because that seems to be the format returned by
    // getSourceRange.
    CharSourceRange root_range =
        CharSourceRange::getTokenRange(root->getSourceRange());
    CharSourceRange cond_range =
        CharSourceRange::getTokenRange(cond->getSourceRange());

    // Compute the replacement text
    auto cond_text = Lexer::getSourceText(cond_range, *result.SourceManager,
                                          result.Context->getLangOpts());
    std::string cond_text_str = std::string(cond_text);
    if (!true_is_first) {
      cond_text_str = "!(" + cond_text_str + ")";
    }
    std::string replacement_text = "base::ToString(" + cond_text_str + ")";

    // Will emit a directive to replace root_tange with replacement_text
    output_helper_.Replace(root_range, replacement_text, *result.SourceManager,
                           result.Context->getLangOpts());
  }
};

}  // namespace

// Putting it all together: this function is mostly boilerplate that combines
// the stuff we've already defined. The most interesting part is specifying the
// traversal method to use; TK_IgnoreUnlessSpelledInSource will ignore most
// implicit AST nodes that the user didn't write themselves. This is required
// to use matchers unless you have a deep understanding of clang's AST.
int main(int argc, const char* argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();

  llvm::Expected<clang::tooling::CommonOptionsParser> options =
      clang::tooling::CommonOptionsParser::create(argc, argv,
                                                  rewriter_category);
  assert(static_cast<bool>(options));
  clang::tooling::ClangTool tool(options->getCompilations(),
                                 options->getSourcePathList());

  OutputHelper output_helper((llvm::StringSet<>(headers_to_add)));

  MatchFinder match_finder;
  MatchFinder::MatchCallback* callback =
      // new Printer();
      new ASTRewriter(&output_helper);

  StatementMatcher final_matcher =
      traverse(TK_IgnoreUnlessSpelledInSource, matchTernaryTrueFalse());
  // More complicated use cases may want to add multiple matchers and callbacks
  match_finder.addMatcher(final_matcher, callback);

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder, &output_helper);
  return tool.run(factory.get());
}
