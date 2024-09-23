// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Clang tool to change accesses to data members to calling getters or setters.
//
// For example, if `foo` and `bar` are of a type whose `member` is supposed to
// be rewritten, the rewriter replaces `foo.member = bar.member` with
// `foo.set_member(bar.member())`.
//
// In particular, this rewriter is used to convert the structs
// `autofill::FormData` and `autofill::FormFieldData` to classes.

#include <cassert>
#include <iostream>
#include <memory>
#include <regex>
#include <string>

#include "clang/AST/ASTContext.h"
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
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/TargetSelect.h"

// Prints a clang::SourceLocation or clang::SourceRange.
#define LOG(e)                                                     \
  llvm::errs() << __FILE__ << ":" << __LINE__ << ": " << #e << " " \
               << (e).printToString(*result.SourceManager) << '\n';

namespace {

llvm::cl::extrahelp common_help(
    clang::tooling::CommonOptionsParser::HelpMessage);
llvm::cl::extrahelp more_help(
    "The rewriter turns references to ClassOfInterest::field_of_interest into\n"
    "getter and setter calls.");
llvm::cl::OptionCategory rewriter_category("Rewriter Options");
llvm::cl::opt<std::string> classes_of_interest_option(
    "classes-of-interest",
    llvm::cl::desc("Comma-separated fully qualified names of the classes whose "
                   "field are to be rewritten"),
    llvm::cl::init("::autofill::FormFieldData"),
    llvm::cl::cat(rewriter_category));
llvm::cl::opt<std::string> fields_of_interest_option(
    "fields-of-interest",
    llvm::cl::desc("Comma-separated names of data members of any class of "
                   "interest that are to be rewritten"),
    llvm::cl::init("value"),
    llvm::cl::cat(rewriter_category));

// Generates substitution directives according to the format documented in
// tools/clang/scripts/run_tool.py.
//
// We do not use `clang::tooling::Replacements` because we don't need any
// buffering, and we'd need to implement the serialization of
// `clang::tooling::Replacement` anyway.
class OutputHelper : public clang::tooling::SourceFileCallbacks {
 public:
  OutputHelper() = default;
  ~OutputHelper() = default;

  OutputHelper(const OutputHelper&) = delete;
  OutputHelper& operator=(const OutputHelper&) = delete;

  // Deletes `replacement_range`.
  void Delete(const clang::CharSourceRange& replacement_range,
              const clang::SourceManager& source_manager,
              const clang::LangOptions& lang_opts) {
    Replace(replacement_range, "", source_manager, lang_opts);
  }

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

  // Inserts `lhs` and `rhs` to the left and right of `replacement_range`.
  void Wrap(const clang::CharSourceRange& replacement_range,
            std::string_view lhs,
            std::string_view rhs,
            const clang::SourceManager& source_manager,
            const clang::LangOptions& lang_opts) {
    clang::tooling::Replacement replacement(source_manager, replacement_range,
                                            "", lang_opts);
    llvm::StringRef file_path = replacement.getFilePath();
    if (file_path.empty()) {
      return;
    }
    Add(file_path, replacement.getOffset(), 0, lhs);
    Add(file_path, replacement.getOffset() + replacement.getLength(), 0, rhs);
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

    current_language_ = input_file.getKind().getLanguage();

    if (ShouldOutput()) {
      llvm::outs() << "==== BEGIN EDITS ====\n";
    }
    return true;  // Report that |handleBeginSource| succeeded.
  }

  void handleEndSource() override {
    if (ShouldOutput()) {
      llvm::outs() << "==== END EDITS ====\n";
    }
  }

  void Add(llvm::StringRef file_path,
           unsigned offset,
           unsigned length,
           std::string_view replacement_text) {
    if (ShouldOutput()) {
      llvm::outs() << "r:::" << file_path << ":::" << offset << ":::" << length
                   << ":::" << replacement_text << "\n";
    }
  }

  bool ShouldOutput() {
    switch (current_language_) {
      case clang::Language::CXX:
      case clang::Language::OpenCLCXX:
      case clang::Language::ObjCXX:
        return true;
      case clang::Language::Unknown:
      case clang::Language::Asm:
      case clang::Language::LLVM_IR:
      case clang::Language::CIR:
      case clang::Language::OpenCL:
      case clang::Language::CUDA:
      case clang::Language::RenderScript:
      case clang::Language::HIP:
      case clang::Language::HLSL:
      case clang::Language::C:
      case clang::Language::ObjC:
        return false;
    }
    assert(false && "Unrecognized clang::Language");
    return false;
  }

  clang::Language current_language_ = clang::Language::Unknown;
};

namespace matchers {

auto IsClassOfInterest() {
  using namespace clang::ast_matchers;
  static auto names = [] {
    llvm::SmallVector<llvm::StringRef> names;
    llvm::StringRef(classes_of_interest_option).split(names, ",");
    return names;
  }();
  return cxxRecordDecl(hasAnyName(names));
}

auto IsFieldOfInterest() {
  using namespace clang::ast_matchers;
  static auto names = [] {
    llvm::SmallVector<llvm::StringRef> names;
    llvm::StringRef(fields_of_interest_option).split(names, ",");
    return names;
  }();
  return fieldDecl(hasAnyName(names));
}

// Matches `object.member` and `object->member` where `member` is a
// member-of-interest of a class-of-interest
auto IsMemberExprOfInterest() {
  using namespace clang::ast_matchers;
  return memberExpr(
             allOf(member(allOf(IsFieldOfInterest(),
                                fieldDecl(hasParent(IsClassOfInterest())))),
                   // Avoids matching memberExprs in defaulted
                   // constructors and operators.
                   unless(hasAncestor(functionDecl(isDefaulted())))))
      .bind("member_expr");
}

// Matches `f.member = foo`.
auto IsWriteExprOfInterest() {
  using namespace clang::ast_matchers;
  return expr(binaryOperation(
                  hasOperatorName("="),
                  hasLHS(ignoringParenImpCasts(IsMemberExprOfInterest())),
                  hasRHS(expr().bind("rhs"))))
      .bind("assignment");
}

// Matches `f.member` and `f->member`, except on the left-hand side of
// assignments.
auto IsReadExprOfInterest() {
  using namespace clang::ast_matchers;
  return expr(
      anyOf(allOf(IsMemberExprOfInterest(),
                  unless(hasAncestor(IsWriteExprOfInterest()))),
            expr(binaryOperation(
                hasOperatorName("="),
                hasRHS(anyOf(IsMemberExprOfInterest(),
                             hasDescendant(IsMemberExprOfInterest())))))));
}

}  // namespace matchers

class BaseRewriter {
 public:
  explicit BaseRewriter(OutputHelper* output_helper)
      : output_helper_(*output_helper) {}

 protected:
  OutputHelper& output_helper_;
};

// Replaces `object.member` with `object.member()`.
class FieldReadAccessRewriter
    : public BaseRewriter,
      public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  using BaseRewriter::BaseRewriter;

  void AddMatchers(clang::ast_matchers::MatchFinder& match_finder) {
    match_finder.addMatcher(matchers::IsReadExprOfInterest(), this);
  }

 private:
  void run(
      const clang::ast_matchers::MatchFinder::MatchResult& result) override {
    // object.member
    // ^-----------^  "member_expr"
    const auto* member_expr =
        result.Nodes.getNodeAs<clang::MemberExpr>("member_expr");
    assert(member_expr);
    clang::CharSourceRange range = clang::CharSourceRange::getTokenRange(
        clang::SourceRange(member_expr->getMemberLoc()));
    auto source_text = clang::Lexer::getSourceText(
        range, *result.SourceManager, result.Context->getLangOpts());
    std::string replacement_text =
        std::string(source_text.begin(), source_text.end()) + "()";
    output_helper_.Replace(range, replacement_text, *result.SourceManager,
                           result.Context->getLangOpts());
  }
};

// Replaces `object.member = something` with `object.set_value(something)`
// (modulo whitespace).
//
// More precisely, it inserts `set_` before `member`, removes `=`, and puts
// `something` into parentheses. We do it this way to avoid overlapping
// substitutions because `something` might need substitutions on its own.
class FieldWriteAccessRewriter
    : public BaseRewriter,
      public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  using BaseRewriter::BaseRewriter;

  void AddMatchers(clang::ast_matchers::MatchFinder& match_finder) {
    match_finder.addMatcher(matchers::IsWriteExprOfInterest(), this);
  }

 private:
  void run(
      const clang::ast_matchers::MatchFinder::MatchResult& result) override {
    // object.member = something;
    // ^-----------^              "member_expr"
    //                 ^-------^  "rhs"
    // ^-----------------------^  "assignment"
    const auto* assignment = result.Nodes.getNodeAs<clang::Expr>("assignment");
    const auto* member_expr =
        result.Nodes.getNodeAs<clang::MemberExpr>("member_expr");
    const auto* rhs = result.Nodes.getNodeAs<clang::Expr>("rhs");
    assert(assignment);
    assert(member_expr);
    assert(rhs);
    {
      // Replace `member` with `set_member`.
      clang::CharSourceRange range = clang::CharSourceRange::getTokenRange(
          clang::SourceRange(member_expr->getMemberLoc()));
      output_helper_.Wrap(range, "set_", "", *result.SourceManager,
                          result.Context->getLangOpts());
    }
    {
      // Replace `=` with ``.
      clang::CharSourceRange range =
          clang::CharSourceRange::getTokenRange(assignment->getExprLoc());
      output_helper_.Delete(range, *result.SourceManager,
                            result.Context->getLangOpts());
    }
    {
      // Replace `something` with `(something)`.
      clang::CharSourceRange range =
          clang::CharSourceRange::getTokenRange(rhs->getSourceRange());
      output_helper_.Wrap(range, "(", ")", *result.SourceManager,
                          result.Context->getLangOpts());
    }
  }
};

// Replaces `::testing::Field(..., &ClassOfInterest::field_of_interest, ...)`
// with `::testing::Property(..., &ClassOfInterest::field_of_interest, ...)`.
class TestingFieldRewriter
    : public BaseRewriter,
      public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  using BaseRewriter::BaseRewriter;

  void AddMatchers(clang::ast_matchers::MatchFinder& match_finder) {
    using namespace clang::ast_matchers;
    using namespace matchers;
    // Matches `Field(&ClassOfInterest::field_of_interest, ...)`.
    auto testing_field =
        callExpr(
            callee(functionDecl(hasName("::testing::Field"))),
            hasAnyArgument(expr(unaryOperator(
                hasOperatorName("&"), hasUnaryOperand(declRefExpr(to(allOf(
                                          hasParent(IsClassOfInterest()),
                                          fieldDecl(IsFieldOfInterest())))))))))
            .bind("call_expr");
    match_finder.addMatcher(testing_field, this);
  }

 private:
  void run(
      const clang::ast_matchers::MatchFinder::MatchResult& result) override {
    const auto* call_expr =
        result.Nodes.getNodeAs<clang::CallExpr>("call_expr");
    assert(call_expr);
    // Replace `::testing::Field` with `::testing::Property`.
    clang::CharSourceRange range = clang::CharSourceRange::getTokenRange(
        call_expr->getCallee()->getSourceRange());
    auto source_text = clang::Lexer::getSourceText(
        range, *result.SourceManager, result.Context->getLangOpts());
    std::string replacement_text =
        std::string(source_text.begin(), source_text.end());
    replacement_text =
        std::regex_replace(replacement_text, std::regex("Field"), "Property");
    output_helper_.Replace(range, replacement_text, *result.SourceManager,
                           result.Context->getLangOpts());
  }
};

class FieldToFunctionRewriter : public BaseRewriter {
 public:
  using BaseRewriter::BaseRewriter;

  void AddMatchers(clang::ast_matchers::MatchFinder& match_finder) {
    frar_.AddMatchers(match_finder);
    fwar_.AddMatchers(match_finder);
    tfr_.AddMatchers(match_finder);
  }

 private:
  FieldReadAccessRewriter frar_{&output_helper_};
  FieldWriteAccessRewriter fwar_{&output_helper_};
  TestingFieldRewriter tfr_{&output_helper_};
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
  clang::ast_matchers::MatchFinder match_finder;

  FieldToFunctionRewriter ftf_rewriter(&output_helper);
  ftf_rewriter.AddMatchers(match_finder);

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder, &output_helper);
  return tool.run(factory.get());
}
