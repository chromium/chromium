// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Clang tool to change accesses to forward calls to a member function of a
// class through another member function of that class.
// This can be useful to update callsites when splitting a large class into
// subclasses.
// In particular, this is used to split the `autofill::PersonalDataManager`.

#include <cassert>
#include <memory>
#include <string>

#include "clang/AST/ASTContext.h"
#include "clang/AST/ExprCXX.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Core/Replacement.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"

namespace {

llvm::cl::extrahelp common_help(
    clang::tooling::CommonOptionsParser::HelpMessage);
llvm::cl::extrahelp more_help(
    "The rewriter forwards calls to functions-of-interest on objects of type "
    "class-of-interest through forward-through.");
llvm::cl::OptionCategory rewriter_category("Rewriter Options");
llvm::cl::opt<std::string> class_of_interest_option(
    "class-of-interest",
    llvm::cl::desc("Fully qualified names of the class whose "
                   "calls are to be forwarded"),
    llvm::cl::init("Foo"),
    llvm::cl::cat(rewriter_category));
llvm::cl::opt<std::string> functions_of_interest_option(
    "functions-of-interest",
    llvm::cl::desc("Comma-separated function names of the class-of-interest "
                   "that are to be forwarded"),
    llvm::cl::init("bar"),
    llvm::cl::cat(rewriter_category));
llvm::cl::opt<std::string> forward_through_option(
    "forward-through",
    llvm::cl::desc("Name of the function to forward calls through"),
    llvm::cl::init("baz"),
    llvm::cl::cat(rewriter_category));
llvm::cl::opt<std::string> include_header_option(
    "header",
    llvm::cl::desc("Name of the header to include in every touched file"),
    llvm::cl::init("some/file.h"),
    llvm::cl::cat(rewriter_category));

// Generates substitution directives according to the format documented in
// tools/clang/scripts/run_tool.py.
//
// We do not use `clang::tooling::Replacements` because we don't need any
// buffering, and we'd need to implement the serialization of
// `clang::tooling::Replacement` anyway.
class OutputHelper : public clang::tooling::SourceFileCallbacks {
 public:
  // Replaces `replacement_range` with `replacement_text`.
  void Replace(const clang::CharSourceRange& replacement_range,
               std::string replacement_text,
               const clang::SourceManager& source_manager,
               const clang::LangOptions& lang_opts) {
    clang::tooling::Replacement replacement(source_manager, replacement_range,
                                            replacement_text, lang_opts);
    llvm::StringRef file_path = replacement.getFilePath();
    if (file_path.empty()) {
      return;
    }
    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    Add(file_path, replacement.getOffset(), replacement.getLength(),
        replacement_text);
  }

 private:
  // clang::tooling::SourceFileCallbacks:
  bool handleBeginSource(clang::CompilerInstance& compiler) override {
    const clang::FrontendOptions& frontend_options = compiler.getFrontendOpts();

    assert((frontend_options.Inputs.size() == 1) &&
           "run_tool.py should invoke the rewriter one file at a time");
    const clang::FrontendInputFile& input_file = frontend_options.Inputs[0];
    assert(input_file.isFile() &&
           "run_tool.py should invoke the rewriter on actual files");

    llvm::outs() << "==== BEGIN EDITS ====\n";
    return true;
  }

  void handleEndSource() override { llvm::outs() << "==== END EDITS ====\n"; }

  void Add(llvm::StringRef file_path,
           unsigned offset,
           unsigned length,
           llvm::StringRef replacement_text) {
    llvm::outs() << "r:::" << file_path << ":::" << offset << ":::" << length
                 << ":::" << replacement_text << "\n";
    llvm::outs() << "include-user-header:::" << file_path
                 << ":::-1:::-1:::" << include_header_option << "\n";
  }
};

// Matches `foo.bar()` and `foo->bar()` calls, independently of the parameters
// to bar, where:
// - The type of `foo` is `class_name` or derived from it.
// - `bar` is one of `function_names`.
auto IsCallOfInterest(llvm::StringRef class_name,
                      llvm::SmallVector<llvm::StringRef> function_names) {
  using namespace clang::ast_matchers;
  return cxxMemberCallExpr(
             on(anyOf(hasType(cxxRecordDecl(
                          isSameOrDerivedFrom(hasName(class_name)))),
                      hasType(pointsTo(cxxRecordDecl(
                          isSameOrDerivedFrom(hasName(class_name))))))),
             callee(cxxMethodDecl(hasAnyName(function_names))))
      .bind("call");
}

// Rewrites calls of interest (as per `IsCallOfInterest()`) to go through
// `forward_through_option`. E.g.:
// - `foo.bar()` -> `foo.baz().bar()`
// - `foo->bar()` -> `foo->baz().bar()`
class ForwardCallRewriter
    : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  explicit ForwardCallRewriter(OutputHelper* output_helper)
      : output_helper_(*output_helper) {}

  void AddMatchers(clang::ast_matchers::MatchFinder& match_finder) {
    llvm::SmallVector<llvm::StringRef> function_names;
    // `functions_of_interest_option` outlives `ForwardCallRewriter`, so the
    // `llvm::StringRef`s returned by `split()` remain valid.
    llvm::StringRef(functions_of_interest_option).split(function_names, ",");
    match_finder.addMatcher(
        IsCallOfInterest(class_of_interest_option, std::move(function_names)),
        this);
  }

 private:
  void run(
      const clang::ast_matchers::MatchFinder::MatchResult& result) override {
    const auto* call = result.Nodes.getNodeAs<clang::CXXMemberCallExpr>("call");
    assert(call);
    clang::CharSourceRange range = clang::CharSourceRange::getTokenRange(
        clang::SourceRange(call->getCallee()->getExprLoc()));
    auto source_text = clang::Lexer::getSourceText(
        range, *result.SourceManager, result.Context->getLangOpts());
    std::string replacement_text =
        (forward_through_option + "()." + source_text).str();
    output_helper_.Replace(range, replacement_text, *result.SourceManager,
                           result.Context->getLangOpts());
  }

  OutputHelper& output_helper_;
};

}  // namespace

int main(int argc, const char* argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();

  llvm::Expected<clang::tooling::CommonOptionsParser> options =
      clang::tooling::CommonOptionsParser::create(argc, argv,
                                                  rewriter_category);
  assert(static_cast<bool>(options));
  clang::tooling::ClangTool tool(options->getCompilations(),
                                 options->getSourcePathList());

  OutputHelper output_helper;
  ForwardCallRewriter rewriter(&output_helper);
  clang::ast_matchers::MatchFinder match_finder;
  rewriter.AddMatchers(match_finder);

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder, &output_helper);
  return tool.run(factory.get());
}
