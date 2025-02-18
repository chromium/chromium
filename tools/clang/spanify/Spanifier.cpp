// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "RawPtrHelpers.h"
#include "SeparateRepositoryPaths.h"
#include "SpanifyManualPathsToIgnore.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::ast_matchers;

namespace {

// Forward declarations
std::string GetArraySize(const clang::ArrayTypeLoc& array_type_loc,
                         const clang::SourceManager& source_manager,
                         const clang::ASTContext& ast_context);

// For debugging/assertions. Dump the match result to stderr.
void DumpMatchResult(const MatchFinder::MatchResult& result) {
  llvm::errs() << "Matched nodes:\n";
  for (const auto& node : result.Nodes.getMap()) {
    llvm::errs() << " - " << node.first << ":\n";
  }

  for (const auto& node : result.Nodes.getMap()) {
    llvm::errs() << "\nDump for node " << node.first << ":\n";
    node.second.dump(llvm::errs(), *result.Context);
  }
}

const char kBaseSpanIncludePath[] = "base/containers/span.h";

// Include path that needs to be added to all the files where
// base::raw_span<...> replaces a raw_ptr<...>.
const char kBaseRawSpanIncludePath[] = "base/memory/raw_span.h";

const char kArrayIncludePath[] = "array";

const char kStringViewIncludePath[] = "string_view";

// This iterates over function parameters and matches the ones that match
// parm_var_decl_matcher.
AST_MATCHER_P(clang::FunctionDecl,
              forEachParmVarDecl,
              clang::ast_matchers::internal::Matcher<clang::ParmVarDecl>,
              parm_var_decl_matcher) {
  const clang::FunctionDecl& function_decl = Node;

  unsigned num_params = function_decl.getNumParams();
  bool is_matching = false;
  clang::ast_matchers::internal::BoundNodesTreeBuilder result;
  for (unsigned i = 0; i < num_params; i++) {
    const clang::ParmVarDecl* param = function_decl.getParamDecl(i);
    clang::ast_matchers::internal::BoundNodesTreeBuilder param_matches;
    if (parm_var_decl_matcher.matches(*param, Finder, &param_matches)) {
      is_matching = true;
      result.addMatch(param_matches);
    }
  }
  *Builder = std::move(result);
  return is_matching;
}

AST_MATCHER(clang::ParmVarDecl, isArrayParm) {
  return Node.getOriginalType()->isArrayType();
}

// Returns the size of the array/string literal. It is stored in the
// `output_size` parameter.
// Returns true if the size is known, false otherwise.
bool ArraySize(const clang::Expr* expr, uint64_t* output_size) {
  // C-style arrays with a known size.
  //
  // Note that some arrays in templates have a known size at instantiating
  // time, but it is not determined at this point.
  if (const auto* constant_array = clang::dyn_cast<clang::ConstantArrayType>(
          expr->getType()->getUnqualifiedDesugaredType())) {
    *output_size = constant_array->getSize().getLimitedValue();
    return true;
  }

  // String literals.
  if (const auto* string_literal =
          clang::dyn_cast<clang::StringLiteral>(expr)) {
    *output_size = string_literal->getLength() + 1;
    return true;
  }

  return false;
}

// Return whether the subscript access is guaranteed to be safe, false
// otherwise.
//
// This is based on `llvm-project/clang/lib/Analysis/UnsafeBufferUsage.cpp`.
AST_MATCHER(clang::ArraySubscriptExpr, isSafeArraySubscript) {
  // No guarantees if the array's size is not known.
  uint64_t size = 0;
  if (!ArraySize(Node.getBase()->IgnoreParenImpCasts(), &size)) {
    return false;
  }

  // If the index depends on a template parameter, it could be out of bounds, we
  // don't know yet at this point.
  clang::Expr::EvalResult eval_index;
  const clang::Expr* index_expr = Node.getIdx();
  if (index_expr->isValueDependent()) {
    return false;
  }

  // Try to evaluate the index expression. If we can't evaluate it, we can't
  // provide any guarantees.
  if (!index_expr->EvaluateAsInt(eval_index, Finder->getASTContext())) {
    return false;
  }
  // `APInt` stands for Arbitrary Precision Integer.
  llvm::APInt index_value = eval_index.Val.getInt();

  // Negative indices are out of bounds. Hopefully, this never happens in
  // Chromium. Print a warning for Chromium developers to know about them.
  if (index_value.isNegative()) {
    clang::SourceManager& source_manager =
        Finder->getASTContext().getSourceManager();
    llvm::errs() << llvm::formatv(
        "{0}:{1}: Warning: array subscript out of bounds: {0} < 0\n",
        source_manager.getFilename(Node.getExprLoc()),
        source_manager.getSpellingLineNumber(Node.getExprLoc()),
        index_value.getSExtValue());
    return false;
  }

  // If the index is greater than or equal to the size of the array, it's out of
  // bounds. Hopefully, this never happens in Chromium. Print a warning for
  // Chromium developers to know about them.
  if (index_value.uge(size)) {
    clang::SourceManager& source_manager =
        Finder->getASTContext().getSourceManager();
    llvm::errs() << llvm::formatv(
        "{0}:{1}: Warning: array subscript out of bounds: {2} >= {3}\n",
        source_manager.getFilename(Node.getExprLoc()),
        source_manager.getSpellingLineNumber(Node.getExprLoc()),
        index_value.getSExtValue(), size);
    return false;
  }

  // The subscript is guaranteed to be safe!
  return true;
}

// Convert a number to a string with leading zeros. This is useful to ensure
// that the alphabetical order of the strings is the same as the numerical
// order.
std::string ToStringWithPadding(size_t value, size_t padding) {
  std::string str = std::to_string(value);
  assert(str.size() <= padding);
  return std::string(padding - str.size(), '0') + str;
}

// A simple base64 hash function.
std::string HashBase64(const std::string& input, size_t output_size = 4) {
  std::hash<std::string> hasher;
  size_t hash = hasher(input);
  constexpr std::array<char, 64> charset = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C',
      'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
      'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c',
      'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
      'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '-', '_',
  };
  std::string output(output_size, '0');
  for (size_t i = 0; i < output.size(); i++) {
    output[i] = charset[hash % charset.size()];
    hash /= charset.size();
  }
  return output;
}

// An identifier for a node.
//
// Important properties:
//   - The same range in a file, but processed from two different compile units
//     must have the same key. This is useful to combine rewrites from different
//     compile units.
//   - Sorting nodes alphabetically must preserve the order of the ranges in
//     the file. This is useful to associate the function's argument using their
//     alphabetical order.
//
// `human_readable`
// ----------------
// It is a debugging flag that makes the node key easier to read for debugging
// purposes. It can potentially be useful for external debugging tools to
// visualize the graph. However, it increase the output size by 25%.
//
//                              offset  hash
// Example:                     ------- --------
//  - human_readable = false => 0000426:NrcSLRGt
//  - human_readable = true  => 0000426:M4TJ:main.cc:11:8:3
//                              ------- ---- ------- -- - -
//                              offset  hash filename | | `length
//                                                    | `- column
//                                                    `--- line
template <bool human_readable = false /* Tweak this to debug*/>
std::string NodeKeyFromRange(const clang::SourceRange& range,
                             const clang::SourceManager& sources,
                             const std::string& optional_seed = "") {
  clang::tooling::Replacement replacement(
      sources, clang::CharSourceRange::getCharRange(range), "");
  llvm::StringRef path = replacement.getFilePath();
  llvm::StringRef file_name = llvm::sys::path::filename(path);

  // Note that the largest file is 7.2Mbytes long. So every offsets can be
  // represented using 7 digits. `ToStringWithPadding` ensures that the
  // alphabetical order of the nodes is the same as the order of the ranges in
  // the file.
  if constexpr (!human_readable) {
    return llvm::formatv(
        "{0}:{1}", ToStringWithPadding(replacement.getOffset(), 7),
        HashBase64(NodeKeyFromRange<true>(range, sources, optional_seed), 8));
  }

  return llvm::formatv("{0}:{1}:{2}:{3}:{4}:{5}",
                       ToStringWithPadding(replacement.getOffset(), 7),
                       HashBase64(path.str() + optional_seed), file_name,
                       sources.getSpellingLineNumber(range.getBegin()),
                       sources.getSpellingColumnNumber(range.getBegin()),
                       replacement.getLength());
}

template <typename T>
std::string NodeKey(const T* t, const clang::SourceManager& sources) {
  return NodeKeyFromRange(t->getSourceRange(), sources);
}

std::string GetRHS(const MatchFinder::MatchResult& result);
std::string GetLHS(const MatchFinder::MatchResult& result);

// Emit a generic instruction to the output stream. This removes duplicates.
void Emit(const std::string& line) {
  static std::set<std::string> emitted;
  if (emitted.count(line) == 0) {
    emitted.insert(line);
    llvm::outs() << line;
  }
}

// Associate a change with a node.
// A replacement has one of following format:
// - r:::<file path>:::<offset>:::<length>:::<replacement text>
// - include-user-header:::<file path>:::-1:::-1:::<include text>
// - include-system-header:::<file path>:::-1:::-1:::<include text>
//
// It is associated with a "Node", which is a unique identifier.
void EmitReplacement(const std::string& node, const std::string& replacement) {
  Emit(llvm::formatv("r {0} {1}\n", node, replacement));
}

void EmitEdge(const std::string& lhs, const std::string& rhs) {
  Emit(llvm::formatv("e {0} {1}\n", lhs, rhs));
}

// Emits a source node.
//
// A source node is a node that triggers the rewrite. All rewrites will start
// from sources.
void EmitSource(const std::string& node) {
  Emit(llvm::formatv("s {0}\n", node));
}

// Emits a sink node.
//
// Those are nodes where we the size of the memory region is known
// This is true for nodes representing the following assignments:
//  - nullptr => size is zero
//  - new/new[n] => size is 1/n
//  - constant arrays buf[1024] => size is 1024
//  - calls to third_party functions that we can't rewrite (they should provide
//    a size for the pointer returned)
//
// A rewrite is applied from a source if all the reachable end nodes are
// sinks.
void EmitSink(const std::string& node) {
  Emit(llvm::formatv("i {0}\n", node));
}

// Emit `replacement` if `rhs_key` is rewritten, but `lhs_key` is not.
//
// `lhs_key` and `rhs_key` are the unique identifiers of the nodes.
void EmitFrontier(const std::string& lhs_key,
                  const std::string& rhs_key,
                  const std::string& replacement) {
  Emit(llvm::formatv("f {0} {1} {2}\n", lhs_key, rhs_key, replacement));
}

static std::string GetReplacementDirective(
    const clang::SourceRange& replacement_range,
    std::string replacement_text,
    const clang::SourceManager& source_manager) {
  clang::tooling::Replacement replacement(
      source_manager, clang::CharSourceRange::getCharRange(replacement_range),
      replacement_text);
  llvm::StringRef file_path = replacement.getFilePath();
  assert(!file_path.empty() && "Replacement file path is empty.");
  // For replacements that span multiple lines, make sure to remove the newline
  // character.
  // `./apply-edits.py` expects `\n` to be escaped as '\0'.
  std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');

  return llvm::formatv("r:::{0}:::{1}:::{2}:::{3}", file_path,
                       replacement.getOffset(), replacement.getLength(),
                       replacement_text);
}

std::string GetIncludeDirective(const clang::SourceRange replacement_range,
                                const clang::SourceManager& source_manager,
                                const char* include_path = kBaseSpanIncludePath,
                                bool is_system_include_path = false) {
  return llvm::formatv(
      "{0}:::{1}:::-1:::-1:::{2}",
      is_system_include_path ? "include-system-header" : "include-user-header",
      GetFilename(source_manager, replacement_range.getBegin(),
                  raw_ptr_plugin::FilenameLocationType::kSpellingLoc),
      include_path);
}

// The semantics of `getBeginLoc()` and `getEndLoc()` are somewhat
// surprising (e.g. https://stackoverflow.com/a/59718238). This function
// tries to do the least surprising thing, specializing for
//
// *  `clang::MemberExpr`
// *  `clang::DeclRefExpr`
// *  `clang::CallExpr`
//
// and defaults to returning the range of token `expr`.
clang::SourceRange getExprRange(const clang::Expr* expr,
                                const clang::SourceManager& source_manager,
                                const clang::LangOptions& lang_options) {
  if (const auto* member_expr = clang::dyn_cast<clang::MemberExpr>(expr)) {
    clang::SourceLocation begin_loc = member_expr->getMemberLoc();
    size_t member_name_length = member_expr->getMemberDecl()->getName().size();
    clang::SourceLocation end_loc =
        begin_loc.getLocWithOffset(member_name_length);
    return {begin_loc, end_loc};
  }

  if (const auto* decl_ref = clang::dyn_cast<clang::DeclRefExpr>(expr)) {
    auto name = decl_ref->getNameInfo().getName().getAsString();
    return {decl_ref->getBeginLoc(),
            decl_ref->getEndLoc().getLocWithOffset(name.size())};
  }

  if (const auto* call_expr = clang::dyn_cast<clang::CallExpr>(expr)) {
    return {call_expr->getBeginLoc(),
            call_expr->getRParenLoc().getLocWithOffset(1)};
  }

  return {
      expr->getBeginLoc(),
      clang::Lexer::getLocForEndOfToken(expr->getExprLoc(), 0u, source_manager,
                                        lang_options),
  };
}

std::string GetTypeAsString(const clang::QualType& qual_type,
                            const clang::ASTContext& ast_context) {
  clang::PrintingPolicy printing_policy(ast_context.getLangOpts());
  printing_policy.SuppressScope = 0;
  printing_policy.SuppressUnwrittenScope = 1;
  printing_policy.SuppressElaboration = 0;
  printing_policy.SuppressInlineNamespace = 1;
  printing_policy.SuppressDefaultTemplateArgs = 1;
  printing_policy.PrintCanonicalTypes = 0;
  return qual_type.getAsString(printing_policy);
}

// It is intentional that this function ignores cast expressions and applies
// the `.data()` addition to the internal expression. if we have:
// type* ptr = reinterpret_cast<type*>(buf);  where buf needs to be rewritten
// to span and ptr doesn't. The `.data()` call is added right after buffer as
// follows: type* ptr = reinterpret_cast<type*>(buf.data());
static clang::SourceRange getSourceRange(
    const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::LangOptions& lang_opts = result.Context->getLangOpts();
  if (auto* op =
          result.Nodes.getNodeAs<clang::UnaryOperator>("unaryOperator")) {
    if (op->isPostfix()) {
      return {op->getBeginLoc(), op->getEndLoc().getLocWithOffset(2)};
    }
    auto* expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr");
    return {op->getBeginLoc(),
            getExprRange(expr, source_manager, lang_opts).getEnd()};
  }
  if (auto* op = result.Nodes.getNodeAs<clang::Expr>("binaryOperator")) {
    auto* sub_expr = result.Nodes.getNodeAs<clang::Expr>("bin_op_rhs");
    auto end_loc = getExprRange(sub_expr, source_manager, lang_opts).getEnd();
    return {op->getBeginLoc(), end_loc};
  }
  if (auto* op = result.Nodes.getNodeAs<clang::CXXOperatorCallExpr>(
          "raw_ptr_operator++")) {
    auto* callee = op->getDirectCallee();
    if (callee->getNumParams() == 0) {  // postfix op++ on raw_ptr;
      auto* expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr");
      return clang::SourceRange(
          getExprRange(expr, source_manager, lang_opts).getEnd());
    }
    return clang::SourceRange(op->getEndLoc().getLocWithOffset(2));
  }

  if (auto* expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr")) {
    return clang::SourceRange(
        getExprRange(expr, source_manager, lang_opts).getEnd());
  }

  if (auto* size_expr = result.Nodes.getNodeAs<clang::Expr>("size_node")) {
    return clang::SourceRange(
        getExprRange(size_expr, source_manager, lang_opts).getEnd());
  }

  // Not supposed to get here.
  llvm::errs() << "\n"
                  "Error: getSourceRange() encountered an unexpected match.\n"
                  "Expected one of : \n"
                  " - unaryOperator\n"
                  " - binaryOperator\n"
                  " - raw_ptr_operator++\n"
                  " - rhs_expr\n"
                  "\n";
  DumpMatchResult(result);
  assert(false && "Unexpected match in getSourceRange()");
}

static void maybeUpdateSourceRangeIfInMacro(
    const clang::SourceManager& source_manager,
    const MatchFinder::MatchResult& result,
    clang::SourceRange& range) {
  if (!range.isValid() || !range.getBegin().isMacroID()) {
    return;
  }
  // We need to find the reference to the object that might be getting
  // accessed and rewritten to find the location to rewrite. SpellingLocation
  // returns a different position if the source was pointing into the macro
  // definition. See clang::SourceManager for details but relevant section:
  //
  // "Spelling locations represent where the bytes corresponding to a token came
  // from and expansion locations represent where the location is in the user's
  // view. In the case of a macro expansion, for example, the spelling location
  // indicates where the expanded token came from and the expansion location
  // specifies where it was expanded."
  auto* rhs_decl_ref =
      result.Nodes.getNodeAs<clang::DeclRefExpr>("declRefExpr");
  if (!rhs_decl_ref) {
    return;
  }
  // We're extracting the spellingLocation's position and then we'll move the
  // location forward by the length of the variable. This will allow us to
  // insert .data() at the end of the decl_ref.
  clang::SourceLocation correct_start =
      source_manager.getSpellingLoc(rhs_decl_ref->getLocation());

  bool invalid_line, invalid_col = false;
  auto line =
      source_manager.getSpellingLineNumber(correct_start, &invalid_line);
  auto col =
      source_manager.getSpellingColumnNumber(correct_start, &invalid_col);
  assert(correct_start.isValid() && !invalid_line && !invalid_col &&
         "Unable to get SpellingLocation info");
  // Get the name and find the end of the decl_ref.
  std::string name = rhs_decl_ref->getFoundDecl()->getNameAsString();
  clang::SourceLocation correct_end = source_manager.translateLineCol(
      source_manager.getFileID(correct_start), line, col + name.size());
  assert(correct_end.isValid() &&
         "Incorrectly got an End SourceLocation for macro");
  // This returns at the end of the variable being referenced so we can
  // insert .data(), if we wanted it wrapped in params (variable).data()
  // we'd need {correct_start, correct_end} but this doesn't seem needed in
  // macros tested on so far.
  range = clang::SourceRange{correct_end};
}

static std::string getNodeFromPointerTypeLoc(
    const clang::PointerTypeLoc* type_loc,
    const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();
  // We are in the case of a function return type loc.
  // This doesn't always generate the right range since type_loc doesn't
  // account for qualifiers (like const). Didn't find a proper way for now
  // to get the location with type qualifiers taken into account.
  clang::SourceRange replacement_range = {
      type_loc->getBeginLoc(), type_loc->getEndLoc().getLocWithOffset(1)};
  std::string initial_text =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(replacement_range),
          source_manager, lang_opts)
          .str();
  initial_text.pop_back();
  std::string replacement_text = "base::span<" + initial_text + ">";

  const std::string key = NodeKey(type_loc, source_manager);
  EmitReplacement(key,
                  GetReplacementDirective(replacement_range, replacement_text,
                                          source_manager));
  EmitReplacement(key, GetIncludeDirective(replacement_range, source_manager));
  return key;
}

// Return node key representing the array variable. Multiple replacements are
// applied to this node:
// - `RewriteUnsafeArray` - Rewrite type of array.
// - `RewriteArraySizeof` - Rewrite sizeof(array).
// - `AppendDataCall`     - Append .data() when passed to an external function.
static std::string ArrayVariableNode(const MatchFinder::MatchResult& result) {
  const clang::VarDecl* array_variable =
      result.Nodes.getNodeAs<clang::VarDecl>("array_variable");
  return NodeKey(array_variable, *result.SourceManager);
}

static std::string getNodeFromRawPtrTypeLoc(
    const clang::TemplateSpecializationTypeLoc* raw_ptr_type_loc,
    const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  auto replacement_range = clang::SourceRange(raw_ptr_type_loc->getBeginLoc(),
                                              raw_ptr_type_loc->getLAngleLoc());

  const std::string key = NodeKey(raw_ptr_type_loc, source_manager);
  EmitReplacement(key,
                  GetReplacementDirective(replacement_range, "base::raw_span",
                                          source_manager));
  EmitReplacement(key, GetIncludeDirective(replacement_range, source_manager,
                                           kBaseRawSpanIncludePath));
  return key;
}

static std::string getNodeFromDecl(const clang::DeclaratorDecl* decl,
                                   const MatchFinder::MatchResult& result) {
  clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;

  // By default, the replacement_range includes type only and doesn't include
  // the identifier. E.g. "const int*" of "const int* ptr".
  // However, in case of array types, it'll be expanded to include the
  // identifier and brackets. E.g. "const int arr[3]" as the entire thing.
  clang::SourceRange replacement_range{decl->getBeginLoc(),
                                       decl->getLocation()};
  std::string replacement_text;

  // Preserve qualifiers.
  const clang::QualType& qual_type = decl->getType();
  std::ostringstream qualifiers;
  qualifiers << (qual_type.isConstQualified() ? "const " : "")
             << (qual_type.isVolatileQualified() ? "volatile " : "");

  // If the original type cannot be recovered from the source, we need to
  // consult the clang deduced type.
  //
  // Please note that the deduced type may not be the same as the original type.
  // For example, if we have the following code:
  //   const auto* p = get_buffer<uint16_t>();
  // we will get:`unsigned short` instead of `uint16_t`.
  std::string type = GetTypeAsString(qual_type->getPointeeType(), ast_context);

  // Assume the original type is a pointer type. When this is not the case, it
  // is mutated below.
  replacement_text =
      qualifiers.str() + llvm::formatv("base::span<{0}>", type).str();

  // If the declaration is a function parameter of an array type, try to extract
  // the exact span size.
  if (auto* parm_var_decl = clang::dyn_cast_or_null<clang::ParmVarDecl>(decl)) {
    const auto* type_loc =
        result.Nodes.getNodeAs<clang::TypeLoc>("array_type_loc");
    if (const clang::QualType& array_type = parm_var_decl->getOriginalType();
        array_type->isArrayType() && type_loc) {
      if (const clang::ArrayTypeLoc& array_type_loc =
              type_loc->getUnqualifiedLoc().getAs<clang::ArrayTypeLoc>();
          !array_type_loc.isNull()) {
        const std::string& array_size_as_string =
            GetArraySize(array_type_loc, source_manager, ast_context);
        std::string span_type;
        if (array_size_as_string.empty()) {
          span_type = llvm::formatv("base::span<{0}> ", type).str();
        } else {
          span_type =
              llvm::formatv("base::span<{0}, {1}> ", type, array_size_as_string)
                  .str();
        }
        // In case of array types, replacement_range is expanded to include the
        // brackets, and replacement_text includes the identifier accordingly.
        // E.g. "const int arr[3]" and "const base::span<int, 3> arr".
        replacement_range.setEnd(
            array_type_loc.getRBracketLoc().getLocWithOffset(1));
        replacement_text =
            qualifiers.str() + span_type + decl->getNameAsString();
      }
    }
  }

  // Since the `type` might be clang deduced type, this node is keyed by the
  // type because it could be different depending on the context. This
  // effectively prevents deduced types from being rewritten.
  // See test: 'span-template-original.cc' for an example.
  const std::string key =
      NodeKeyFromRange(replacement_range, source_manager, type);
  EmitReplacement(key,
                  GetReplacementDirective(replacement_range, replacement_text,
                                          source_manager));
  EmitReplacement(key, GetIncludeDirective(replacement_range, source_manager));

  return key;
}

static void DecaySpanToPointer(const MatchFinder::MatchResult& result) {
  const clang::Expr* deref_expr =
      result.Nodes.getNodeAs<clang::Expr>("deref_expr");
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();
  auto source_range = clang::SourceRange(deref_expr->getBeginLoc(),
                                         getSourceRange(result).getEnd());
  std::string initial_text =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(source_range), source_manager,
          lang_opts)
          .str();

  std::string replacement_text = initial_text.substr(1) + "[0]";
  if (result.Nodes.getNodeAs<clang::Expr>("unaryOperator") ||
      result.Nodes.getNodeAs<clang::Expr>("binaryOperator")) {
    replacement_text = "(" + initial_text.substr(1) + ")[0]";
  }

  EmitReplacement(
      GetRHS(result),
      GetReplacementDirective(source_range, replacement_text, source_manager));
}

// Erases the member call expression. For example:
//  ... = member_.get();
//        ^^^^^^^^^^^^^------ member_expr
// becomes:
//  ... = member_;
//
// This supports both `->` and `.` operators to return the called expression in
// both cases.
//
// This is used to avoid decaying a container / raw_ptr to a pointer when the
// lhs expression is rewritten to a base::span.
void EraseMemberCall(const std::string& node,
                     const clang::MemberExpr* member_expr,
                     const clang::SourceManager& source_manager) {
  // Add '*' before the member call, if needed.
  if (member_expr->isArrow()) {
    clang::SourceRange replacement_range(member_expr->getBase()->getBeginLoc(),
                                         member_expr->getBeginLoc());
    EmitReplacement(
        node, GetReplacementDirective(replacement_range, "*", source_manager));
  }

  // Remove the member call: `->call()` or `.call()`.
  {
    clang::SourceRange replacement_range(
        member_expr->getMemberLoc().getLocWithOffset(
            member_expr->isArrow() ? -2 : -1),
        member_expr->getMemberLoc().getLocWithOffset(
            member_expr->getMemberDecl()->getName().size() + 2));
    EmitReplacement(
        node, GetReplacementDirective(replacement_range, "", source_manager));
  }
}

// Return a replacement that appends `.data()` to the matched expression.
std::string AppendDataCall(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();
  auto rep_range = getSourceRange(result);
  std::string initial_text =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(rep_range), source_manager,
          lang_opts)
          .str();
  std::string replacement_text =
      initial_text.empty() ? ".data()" : "(" + initial_text + ").data()";
  return GetReplacementDirective(rep_range, replacement_text, source_manager);
}

static std::string getNodeFromSizeExpr(const clang::Expr* size_expr,
                                       const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const std::string key = NodeKey(size_expr, source_manager);

  auto replacement_range =
      clang::SourceRange(size_expr->getSourceRange().getBegin(),
                         size_expr->getSourceRange().getBegin());
  if (const auto* nullptr_expr =
          result.Nodes.getNodeAs<clang::CXXNullPtrLiteralExpr>(
              "nullptr_expr")) {
    // The hardcoded offset corresponds to the length of "nullptr" keyword.
    clang::SourceRange nullptr_range = {
        nullptr_expr->getBeginLoc(),
        nullptr_expr->getBeginLoc().getLocWithOffset(7)};
    EmitReplacement(
        key, GetReplacementDirective(nullptr_range, "{}", source_manager));
  }

  EmitReplacement(key, GetIncludeDirective(replacement_range, source_manager));
  EmitSink(key);
  return key;
}

// Rewrite:
//   `sizeof(c_array)`
// Into:
//  `std_array.size() * sizeof(element_size)`.
void RewriteArraySizeof(const MatchFinder::MatchResult& result) {
  clang::SourceManager& source_manager = *result.SourceManager;

  const auto* sizeof_expr =
      result.Nodes.getNodeAs<clang::UnaryExprOrTypeTraitExpr>("sizeof_expr");

  const auto* array = result.Nodes.getNodeAs<clang::VarDecl>("array_variable");
  const std::string& array_variable_as_string = array->getNameAsString();

  // sizeof_expr matches with "sizeof(c_array)" in case of
  // `sizeof(c_array)`, and "sizeof " in case of `sizeof c_array`. In the
  // latter case, we need to include "c_array" in the replacement range.
  int end_offset = 1;
  if (const auto* decl_ref = clang::dyn_cast_or_null<clang::DeclRefExpr>(
          sizeof_expr->getArgumentExpr())) {
    // Unfortunately decl_ref matches with "" (the empty string) at the
    // beginning of "c_array", so we cannot use decl_ref->getSourceRange().
    // Count the length of "c_array" (variable name) instead.
    const clang::DeclarationNameInfo& name_info = decl_ref->getNameInfo();
    const clang::DeclarationName& name = name_info.getName();
    end_offset = name.getAsString().length();
  }

  const clang::SourceRange replacement_range = {
      sizeof_expr->getBeginLoc(),
      sizeof_expr->getEndLoc().getLocWithOffset(end_offset)};

  // The outer-most parentheses are redundant for most cases. But it's
  // necessary in cases like "x / sizeof(c_array)", which is unlikely though.
  std::string replacement_text =
      llvm::formatv("({0}.size() * sizeof(decltype({0})::value_type))",
                    array_variable_as_string);
  std::string replacement_directive = GetReplacementDirective(
      replacement_range, std::move(replacement_text), source_manager);

  EmitReplacement(ArrayVariableNode(result), replacement_directive);
}

// Add `.data()` at the frontier of a span change. This is applied if the node
// identified by `lhs_key` is not rewritten, but `rhs_key` is.
//
// This decays the span to a pointer.
void AddSpanFrontierChange(const std::string& lhs_key,
                           const std::string& rhs_key,
                           const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();
  auto rep_range = getSourceRange(result);

  // If we're inside a macro the rep_range computed above is going to be
  // incorrect because it will point into the file where the macro is defined.
  // We need to get the "SpellingLocation", and then we figure out the end of
  // the parameter so we can insert .data() at the end if needed.
  maybeUpdateSourceRangeIfInMacro(source_manager, result, rep_range);

  std::string initial_text =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(rep_range), source_manager,
          lang_opts)
          .str();
  std::string replacement_text =
      initial_text.empty() ? ".data()" : "(" + initial_text + ").data()";
  EmitFrontier(
      lhs_key, rhs_key,
      GetReplacementDirective(rep_range, replacement_text, source_manager));
}

// Generate a class name for rewriting unnamed struct/class types. This is
// based on the `var_name` which is the name of the array variable.
std::string GenerateClassName(std::string var_name) {
  // Chrome coding style is either:
  // - snake_case for variables.
  // - kCamelCase for constants.
  //
  // Variables are rewritten in CamelCase. Same for constants, but we need to
  // drop the 'k'.
  const bool is_constant =
      var_name.size() > 2 && var_name[0] == 'k' &&
      std::isupper(static_cast<unsigned char>(var_name[1])) &&
      var_name.find('_') == std::string::npos;
  if (is_constant) {
    var_name = var_name.substr(1);
  }

  // Convert to CamelCase:
  char prev = '_';  // Force the first character to be uppercase.
  for (char& c : var_name) {
    if (prev == '_') {
      c = llvm::toUpper(c);
    }
    prev = c;
  }
  // Now we need to remove the '_'s from the string, recall std::remove moves
  // everything to the end and then returns the first '_' (or end()). We then
  // call erase from there to the end to actually remove.
  llvm::erase(var_name, '_');
  return var_name;
}

// Checks if the given array definition involves an unnamed struct type
// or is declared inline within a struct/class definition.
//
// These cases currently pose challenges for the C array to std::array
// conversion and are therefore skipped by the tool.
//
// Examples of problematic definitions:
//   - Unnamed struct:
//     `struct { int x, y; } point_array[10];`
//   - Inline definition:
//     `struct Point { int x, y; } inline_points[5];`
//
// Returns the pair of a suggested type name (if unnamed struct, empty string
// otherwise) and the inline definition with a semi-colon ';' added to split it
// away from the declaration (empty string otherwise).
// I.E.:
//   - {"", ""} -> If this is not one of the problematic definitions above.
//   - {"", "struct Point { int x, y; };"} -> for the inline definition case.
//   - {"PointArray", "struct PointArray { ... };"} -> for the unnamed struct
//     case.
std::pair<std::string, std::string> maybeGetUnnamedAndDefinition(
    const clang::QualType element_type,
    const clang::VarDecl* array_variable,
    const std::string& array_variable_as_string,
    const clang::ASTContext& ast_context) {
  std::string new_class_name_string;
  std::string class_definition;
  // Structs/classes can be defined alongside an option list of variable
  // declarations.
  //
  // struct <OptionalName> { ... } var1[3];
  //
  // In this case we need the class_definition and in the case of unnamed
  // types, we have to construct a name to use instead of the compiler
  // generated one.
  if (auto record_decl = element_type->getAsRecordDecl()) {
    // If the `VarDecl` contains the `RecordDecl`'s {}, the `VarDecl` contains
    // the struct/class definition.
    bool has_definition = array_variable->getSourceRange().fullyContains(
        record_decl->getBraceRange());
    bool is_unnamed = record_decl->getDeclName().isEmpty();

    // If the struct/class has an empty name (=unnamed) and has its
    // definition, we will temporariliy assign a new name to the `RecordDecl`
    // and invoke `getAsString()` to obtain the definition with the new name.
    clang::DeclarationName original_name = record_decl->getDeclName();
    clang::DeclarationName temporal_class_name;
    if (is_unnamed) {
      new_class_name_string = GenerateClassName(array_variable_as_string);
      clang::StringRef new_class_name(new_class_name_string);
      clang::IdentifierInfo& new_class_name_identifier =
          ast_context.Idents.get(new_class_name);
      temporal_class_name = ast_context.DeclarationNames.getIdentifier(
          &new_class_name_identifier);
      record_decl->setDeclName(temporal_class_name);
    }

    if (has_definition) {
      // Use `SourceManager` to capture the `{ ... }` part of the struct
      // definition.
      const clang::SourceManager& source_manager =
          ast_context.getSourceManager();
      llvm::StringRef struct_body_with_braces = clang::Lexer::getSourceText(
          clang::CharSourceRange::getTokenRange(record_decl->getBraceRange()),
          source_manager, ast_context.getLangOpts());

      // Create new class definition.
      if (is_unnamed) {
        std::string type_keyword;
        if (record_decl->isClass()) {
          type_keyword = "class";
        } else if (record_decl->isUnion()) {
          type_keyword = "union";
        } else if (record_decl->isEnum()) {
          type_keyword = "enum";
        } else {
          assert(record_decl->isStruct());
          type_keyword = "struct";
        }

        class_definition = type_keyword + " " + new_class_name_string + " " +
                           struct_body_with_braces.str() + ";\n";
      } else {
        // Because of class/struct definition, drop any qualifiers from
        // `element_type`. E.g. `const struct { int val; }` must be
        // `struct { int val; }`.
        clang::QualType unqualified_type = element_type.getUnqualifiedType();
        std::string unqualified_type_str = unqualified_type.getAsString();
        class_definition =
            unqualified_type_str + " " + struct_body_with_braces.str() + ";\n";
      }
    }
    if (is_unnamed) {
      record_decl->setDeclName(original_name);
    }
  }
  return std::make_pair(new_class_name_string, class_definition);
}

// Gets the array size as written in the source code if it's explicitly
// specified. Otherwise, returns the empty string.
std::string GetArraySize(const clang::ArrayTypeLoc& array_type_loc,
                         const clang::SourceManager& source_manager,
                         const clang::ASTContext& ast_context) {
  assert(!array_type_loc.isNull());

  clang::SourceRange source_range(
      array_type_loc.getLBracketLoc().getLocWithOffset(1),
      array_type_loc.getRBracketLoc());
  return clang::Lexer::getSourceText(
             clang::CharSourceRange::getCharRange(source_range), source_manager,
             ast_context.getLangOpts())
      .str();
}

// Produces a std::array type from the given (potentially nested) C array type.
// Returns a string representation of the std::array type.
std::string RewriteCArrayToStdArray(const clang::QualType& type,
                                    const clang::TypeLoc& type_loc,
                                    const clang::SourceManager& source_manager,
                                    const clang::ASTContext& ast_context) {
  const clang::ArrayType* array_type = ast_context.getAsArrayType(type);
  if (!array_type) {
    return GetTypeAsString(type, ast_context);
  }
  const clang::ArrayTypeLoc& array_type_loc =
      type_loc.getUnqualifiedLoc().getAs<clang::ArrayTypeLoc>();
  assert(!array_type_loc.isNull());

  const clang::QualType& element_type = array_type->getElementType();
  const clang::TypeLoc& element_type_loc = array_type_loc.getElementLoc();
  const std::string& element_type_as_string = RewriteCArrayToStdArray(
      element_type, element_type_loc, source_manager, ast_context);

  const std::string& size_as_string =
      GetArraySize(array_type_loc, source_manager, ast_context);

  std::ostringstream result;
  result << "std::array<" << element_type_as_string << ", " << size_as_string
         << ">";
  return result.str();
}

// Returns an initializer list(`initListExpr`) of the given
// `var_decl`(`clang::VarDecl`) if exists. Otherwise, returns `nullptr`.
const clang::InitListExpr* GetArrayInitList(const clang::VarDecl* var_decl) {
  const clang::Expr* init_expr = var_decl->getInit();
  if (!init_expr) {
    return nullptr;
  }

  const clang::InitListExpr* init_list_expr =
      clang::dyn_cast_or_null<clang::InitListExpr>(init_expr);
  if (init_list_expr) {
    return init_list_expr;
  }

  // If we have the following array of std::vector<>:
  //   `std::vector<Quad> quad[2] = {{...},{...}};`
  // we may not be able to use `dyn_cast` with `init_expr` to obtain
  // `InitListExpr`:
  //   ExprWithCleanups 0x557ea7bdc860 'std::vector<Quad>[2]'
  //   `-InitListExpr 0x557ea7ba3950 'std::vector<Quad>[2]'
  //     |-CXXConstructExpr 0x557ea7bdc750  ...
  //     ...
  //     `-CXXConstructExpr
  //       ...
  // `init_expr` is an instance of `ExprWithCleanups`.
  const clang::ExprWithCleanups* expr_with_cleanups =
      clang::dyn_cast_or_null<clang::ExprWithCleanups>(init_expr);
  if (!expr_with_cleanups) {
    return nullptr;
  }

  auto first_child = expr_with_cleanups->child_begin();
  if (first_child == expr_with_cleanups->child_end()) {
    return nullptr;
  }
  return clang::dyn_cast_or_null<clang::InitListExpr>(*first_child);
}

std::string GetStringViewType(const clang::QualType element_type,
                              const clang::ASTContext& ast_context) {
  if (element_type->isCharType()) {
    return "std::string_view";  // c++17
  }
  if (element_type->isWideCharType()) {
    return "std::wstring_view";  // c++17
  }
  if (element_type->isChar8Type()) {
    return "std::u8string_view";  // c++20
  }
  if (element_type->isChar16Type()) {
    return "std::u16string_view";  // c++17
  }
  if (element_type->isChar32Type()) {
    return "std::u32string_view";  // c++17
  }
  clang::QualType element_type_without_qualifiers(element_type.getTypePtr(), 0);
  return llvm::formatv(
             "std::basic_string_view<{0}>",
             GetTypeAsString(element_type_without_qualifiers, ast_context))
      .str();
}

// Determines whether a trailing comma should be inserted after the
// `init_list_expr` to make the code more readable. Adding a trailing comma
// makes clang-format put each element on a new line. Everything is aligned
// nicely, and the output is more readable. This is particularly helpful when
// the original code is not formatted with clang-format, and isn't using a
// trailing comma, but was originally formatted on multiple lines.
bool ShouldInsertTrailingComma(const clang::InitListExpr* init_list_expr,
                               const clang::SourceManager& source_manager) {
  // To allow for one-liner, we don't add the trailing comma when the size is
  // below 3 or the content length is below 40.
  const int length =
      source_manager.getFileOffset(init_list_expr->getRBraceLoc()) -
      source_manager.getFileOffset(init_list_expr->getLBraceLoc());
  if (init_list_expr->getNumInits() < 3 || length < 40) {
    return false;
  }

  const clang::Expr* last_element =
      init_list_expr->getInit(init_list_expr->getNumInits() - 1);

  // Conservatively search for the trailing comma. If it's already there, we do
  // not need to insert another one.
  for (auto loc = last_element->getEndLoc().getLocWithOffset(1);
       loc != init_list_expr->getRBraceLoc(); loc = loc.getLocWithOffset(1)) {
    if (source_manager.getCharacterData(loc)[0] == ',') {
      return false;
    }
  }

  return true;
}

// Return if braces can be elided when initializing an std::array of type
// `element_type` from an `init_list_expr`.
//
// This is also known as avoiding the "double braces" std::array initialization.
//
// Explanation:
// ============
// `std::array` is a struct that encapsulates a fixed-size array as its only
// member variable. It's an aggregate type, meaning it can be initialized using
// aggregate initialization (like plain arrays and structs). Unlike
// `std::vector` or other containers, `std::array` doesn't have constructors
// that explicitly take initializer lists.
//
// For instance, the following code initializes an `std::array` of one
// `Aggregate`.
// > std::array<Aggregate, 1> buffer =
// > {      // Initialization of std::array<Aggregate,1>
// >   {    // Initialization of std::array<Aggregate,1>::inner_ C-style array.
// >     {  // Initialization of Aggregate
// >        1,2,3
// >     }
// >   }
// > }
//
// Thanks to: https://cplusplus.github.io/CWG/issues/1270.html
// the extra braces can be elided under certain conditions. The rules are
// complexes, but they can be conservatively summarized by the need for extra
// braces when the elements themselves are initialized with braces.
bool CanElideBracesForStdArrayInitialization(
    const clang::InitListExpr* init_list_expr,
    const clang::SourceManager& source_manager) {
  // If the init list contains brace-enclosed elements, we can't always elide
  // the braces.
  for (const clang::Expr* expr : init_list_expr->inits()) {
    const clang::SourceLocation& begin_loc = expr->getBeginLoc();
    if (source_manager.getCharacterData(begin_loc)[0] == '{') {
      return false;
    }
  }
  return true;
}

// Returns a pair of replacements necessary to rewrite a C-style array
// with an initializer list to a std::array.
// The replacement is split into two, the first being a textual rewrite to an
// std::array up and until the start of the initializer list, and the second
// being a full replacement directive format (created with
// GetReplacementDirective) pointing to the end of the initializer list to
// handle closing brackets. This way, we don't need to include the initializer
// list test and don't need to escape special characters.
std::pair<std::string, std::string> RewriteStdArrayWithInitList(
    const clang::ArrayType* array_type,
    const std::string& type,
    const std::string& var,
    const std::string& size,
    const clang::InitListExpr* init_list_expr,
    const clang::SourceManager& source_manager,
    const clang::ASTContext& ast_context) {
  bool needs_trailing_comma =
      ShouldInsertTrailingComma(init_list_expr, source_manager);

  clang::SourceRange init_list_closing_brackets_range = {
      init_list_expr->getSourceRange().getEnd(),
      init_list_expr->getSourceRange().getEnd().getLocWithOffset(1)};

  // Implicitly sized arrays are rewritten to std::to_array. This is because the
  // std::array constructor does not allow the size to be omitted.
  if (size.empty()) {
    auto closing_brackets_replacement_directive = GetReplacementDirective(
        init_list_closing_brackets_range, needs_trailing_comma ? ",})" : "})",
        source_manager);
    return std::make_pair(
        llvm::formatv("auto {0} = std::to_array<{1}>(", var, type),
        closing_brackets_replacement_directive);
  }

  // Warn for array and initializer list size mismatch, except for empty lists.
  if (const auto* constant_array_type =
          llvm::dyn_cast<clang::ConstantArrayType>(array_type)) {
    if (init_list_expr->getNumInits() != 0 &&
        constant_array_type->getSize().getZExtValue() !=
            init_list_expr->getNumInits()) {
      const clang::SourceLocation& location = init_list_expr->getBeginLoc();
      llvm::errs() << "Array and initializer list size mismatch in file "
                   << source_manager.getFilename(location) << ":"
                   << source_manager.getSpellingLineNumber(location) << "\n";
    }
  }

  const bool elide_braces =
      CanElideBracesForStdArrayInitialization(init_list_expr, source_manager);

  if (elide_braces) {
    return std::make_pair(
        llvm::formatv("std::array<{0}, {1}> {2} = ", type, size, var), "");
  }

  auto closing_brackets_replacement_directive = GetReplacementDirective(
      init_list_closing_brackets_range, needs_trailing_comma ? ",}}" : "}}",
      source_manager);

  return std::make_pair(
      llvm::formatv("std::array<{0}, {1}> {2} = {{", type, size, var),
      closing_brackets_replacement_directive);
}

// Creates a replacement node for c-style arrays on which we invoke operator[].
// These arrays are rewritten to std::array<Type, Size>.
void RewriteUnsafeArray(const MatchFinder::MatchResult& result) {
  const std::string key = ArrayVariableNode(result);

  // The array is accessed unsafely. So, it's a source.
  EmitSource(key);
  // From its type declaration, the array size is know, it can be rewritten. So
  // it's a sink.
  EmitSink(key);

  clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;

  const auto* type_loc =
      result.Nodes.getNodeAs<clang::TypeLoc>("array_type_loc");
  const clang::ArrayTypeLoc& array_type_loc =
      type_loc->getUnqualifiedLoc().getAs<clang::ArrayTypeLoc>();
  assert(!array_type_loc.isNull());
  const auto* array_type =
      result.Nodes.getNodeAs<clang::ArrayType>("array_type");
  const auto* array_variable =
      result.Nodes.getNodeAs<clang::VarDecl>("array_variable");
  const std::string& array_variable_as_string =
      array_variable->getNameAsString();
  const std::string& array_size_as_string =
      GetArraySize(array_type_loc, source_manager, ast_context);
  const clang::QualType& original_element_type = array_type->getElementType();

  std::stringstream qualifier_string;
  if (array_variable->isConstexpr()) {
    qualifier_string << "constexpr ";
  }
  if (array_variable->isStaticLocal()) {
    qualifier_string << "static ";
  }

  // Move const qualifier from the element type to the array type.
  // This is equivalent, because std::array provides a 'const' overload for the
  // operator[].
  // -       reference operator[](size_type pos);
  // - const_reference operator[](size_type pos) const;
  //
  // Note 1: The `volatile` qualifier is not moved to the array type. It is kept
  //         in the element type. This is correct. Anyway, Chrome doesn't have
  //         any volatile arrays at the moment.
  //
  // Note 2: Since 'constexpr' implies 'const', we don't need to add 'const' to
  //         the element type if the array is 'constexpr'.
  clang::QualType new_element_type = original_element_type;
  new_element_type.removeLocalConst();
  if (original_element_type.isConstant(ast_context) &&
      !array_variable->isConstexpr()) {
    qualifier_string << "const ";
  }

  // TODO(yukishiino): Currently we support only simple cases like:
  //   - Unnamed struct/class
  //   - Redundant struct/class keyword
  // and
  //   - Multi-dimensional array
  // But we need to support combinations of above:
  //   - Multi-dimensional array of unnamed struct/class
  //   - Multi-dimensional array with redundant struct/class keyword
  std::string element_type_as_string;
  const auto& [unnamed_class, class_definition] = maybeGetUnnamedAndDefinition(
      new_element_type, array_variable, array_variable_as_string, ast_context);
  if (!unnamed_class.empty()) {
    element_type_as_string = unnamed_class;
  } else if (original_element_type->isElaboratedTypeSpecifier()) {
    // If the `original_element_type` is an elaborated type with a keyword, i.e.
    // `struct`, `class`, `union`, we will create another ElaboratedType
    // without the keyword. So `struct funcHasName` will be `funcHasHame`.
    auto* original_type = new_element_type->getAs<clang::ElaboratedType>();

    // Create a new ElaboratedType without 'struct', 'class', 'union'
    // keywords.
    auto new_element_type = ast_context.getElaboratedType(
        // Use `None` to suppress tag names.
        clang::ElaboratedTypeKeyword::None,
        // Keep the same as the original.
        original_type->getQualifier(),
        // Keep the same as the original.
        original_type->getNamedType(),
        // Remove `OwnedTagDecl`. We don't need IncludeTagDefinition.
        nullptr);
    element_type_as_string = GetTypeAsString(new_element_type, ast_context);
  } else {
    element_type_as_string = RewriteCArrayToStdArray(
        new_element_type, array_type_loc.getElementLoc(), source_manager,
        ast_context);
  }

  const clang::InitListExpr* init_list_expr = GetArrayInitList(array_variable);
  const clang::StringLiteral* init_string_literal =
      clang::dyn_cast_or_null<clang::StringLiteral>(array_variable->getInit());

  //   static const char* array[] = {...};
  //   |            |
  //   |            +-- type_loc->getSourceRange().getBegin()
  //   |
  //   +---- array_variable->getSourceRange().getBegin()
  //
  // The `static` is a part of `VarDecl`, but the `const` is a part of
  // the element type, i.e. `const char*`.
  //
  // The array must be rewritten into:
  //
  //   static auto array = std::to_array<const char*>({...});
  //
  // So the `replacement_range` need to include the `const` and
  // `init_list_expr` if any.
  clang::SourceRange replacement_range = {
      array_variable->getSourceRange().getBegin(),
      init_list_expr ? init_list_expr->getBeginLoc()
                     : type_loc->getSourceRange().getEnd().getLocWithOffset(1)};

  const char* include_path = kArrayIncludePath;
  std::string replacement_text;
  std::string additional_replacement;
  if (init_string_literal) {
    assert(original_element_type->isAnyCharacterType());
    if (original_element_type.isConstant(ast_context)) {
      replacement_text = llvm::formatv(
          "{0} {1}", GetStringViewType(new_element_type, ast_context),
          array_variable_as_string);
      include_path = kStringViewIncludePath;
    } else {
      // In case of a non-const array initialized with a string literal, we
      // need to explicitly specify the element type and size of the std::array
      // (i.e. they're not deducible) because the deduced element type will be
      // a const type. Hence,
      //
      //     char arr[] = "abc";
      //
      // is rewritten to
      //
      //     std::array<char, 4> arr{"abc"};
      //
      // Note that `std::array<char, 4> arr = "abc";` doesn't compile.

      replacement_range.setEnd(init_string_literal->getBeginLoc());
      replacement_text = llvm::formatv(
          "std::array<{0}, {1}> {2}{{", element_type_as_string,
          !array_size_as_string.empty()
              ? array_size_as_string
              : llvm::formatv("{0}", init_string_literal->getLength() +
                                         1 /* nul-terminator */),
          array_variable_as_string);

      const clang::SourceLocation& end_of_string_literal =
          init_string_literal
              ->getLocationOfByte(init_string_literal->getByteLength(),
                                  source_manager, ast_context.getLangOpts(),
                                  ast_context.getTargetInfo())
              .getLocWithOffset(1);  // The last closing quote
      EmitReplacement(key, GetReplacementDirective(
                               clang::SourceRange(end_of_string_literal), "}",
                               source_manager));
    }
  } else if (init_list_expr) {
    auto replacements = RewriteStdArrayWithInitList(
        array_type, element_type_as_string, array_variable_as_string,
        array_size_as_string, init_list_expr, source_manager, ast_context);
    replacement_text = replacements.first;
    if (!replacements.second.empty()) {
      EmitReplacement(key, replacements.second);
    }
  } else {
    replacement_text =
        llvm::formatv("std::array<{0}, {1}> {2}", element_type_as_string,
                      array_size_as_string, array_variable_as_string);
  }
  replacement_text =
      class_definition + qualifier_string.str() + replacement_text;

  EmitReplacement(key,
                  GetReplacementDirective(replacement_range, replacement_text,
                                          source_manager));
  EmitReplacement(
      key, GetIncludeDirective(replacement_range, source_manager, include_path,
                               /*is_system_include_header=*/true));
}

// Extracts the lhs node from the match result.
//
// This is only used for spanification, not for rewriting std::array.
std::string GetLHS(const MatchFinder::MatchResult& result) {
  if (auto* type_loc =
          result.Nodes.getNodeAs<clang::PointerTypeLoc>("lhs_type_loc")) {
    return getNodeFromPointerTypeLoc(type_loc, result);
  }

  if (auto* raw_ptr_type_loc =
          result.Nodes.getNodeAs<clang::TemplateSpecializationTypeLoc>(
              "lhs_raw_ptr_type_loc")) {
    return getNodeFromRawPtrTypeLoc(raw_ptr_type_loc, result);
  }

  if (auto* lhs_begin =
          result.Nodes.getNodeAs<clang::DeclaratorDecl>("lhs_begin")) {
    return getNodeFromDecl(lhs_begin, result);
  }

  // Not supposed to get here.
  llvm::errs() << "\n"
                  "Error: getLHS() encountered an unexpected match.\n"
                  "Expected one of : \n"
                  "  - lhs_type_loc\n"
                  "  - lhs_raw_ptr_type_loc\n"
                  "  - lhs_begin\n"
                  "\n";
  DumpMatchResult(result);
  assert(false && "Unexpected match in getLHS()");
}

// Extracts the rhs node from the match result.
//
// This is only used for spanification, not for rewriting std::array.
std::string GetRHS(const MatchFinder::MatchResult& result) {
  if (auto* type_loc =
          result.Nodes.getNodeAs<clang::PointerTypeLoc>("rhs_type_loc")) {
    return getNodeFromPointerTypeLoc(type_loc, result);
  }

  if (auto* raw_ptr_type_loc =
          result.Nodes.getNodeAs<clang::TemplateSpecializationTypeLoc>(
              "rhs_raw_ptr_type_loc")) {
    return getNodeFromRawPtrTypeLoc(raw_ptr_type_loc, result);
  }

  if (auto* rhs_begin =
          result.Nodes.getNodeAs<clang::DeclaratorDecl>("rhs_begin")) {
    return getNodeFromDecl(rhs_begin, result);
  }

  if (result.Nodes.getNodeAs<clang::CXXMemberCallExpr>("member_data_call")) {
    clang::SourceManager& source_manager = *result.SourceManager;
    const clang::MemberExpr* data_member_expr =
        result.Nodes.getNodeAs<clang::MemberExpr>("data_member_expr");
    // Create a node representing the member .data() call.
    // This node can be rewritten (e.g. it's a sink), from the span, by removing
    // the `.data()` call.
    const std::string key = NodeKey(data_member_expr, source_manager);
    EmitSink(key);  // This node can be rewritten, because the span can.
    EraseMemberCall(key, data_member_expr, source_manager);
    return key;
  }

  if (const clang::Expr* size_expr =
          result.Nodes.getNodeAs<clang::Expr>("size_node")) {
    return getNodeFromSizeExpr(size_expr, result);
  }

  // Not supposed to get here.
  llvm::errs() << "\n"
                  "Error: getRHS() encountered an unexpected match.\n"
                  "Expected one of : \n"
                  "  - rhs_type_loc\n"
                  "  - rhs_raw_ptr_type_loc\n"
                  "  - rhs_begin\n"
                  "  - member_data_call\n"
                  "  - size_node\n"
                  "\n";
  DumpMatchResult(result);
  assert(false && "Unexpected match in getRHS()");
}

// Called when it exist a dependency in between `lhs` and `rhs` nodes. To apply
// the rewrite of `lhs`, the rewrite of `rhs` is required.
void MatchAdjacency(const MatchFinder::MatchResult& result) {
  std::string lhs = GetLHS(result);
  std::string rhs = GetRHS(result);

  if (result.Nodes.getNodeAs<clang::Expr>("span_frontier")) {
    AddSpanFrontierChange(lhs, rhs, result);
  }

  EmitEdge(lhs, rhs);
}

// Called when the registered Match is found in the AST.
//
// The match includes:
// - A parmVarDecl or RTNode
// - Corresponding function declaration
//
// Using the function declaration, this:
// 1. Create a unique key for the current function: `current_key`
// 2. If the function has previous declarations or is overridden:
//    - Retrieve previous declarations
//    - Create keys for each previous declaration: `prev_key`
//    - For each `prev_key`, add the pair (`current_key`, `prev_key`) to
//      `fct_sig_pairs_`
//
// Using the parmVarDecl or RTNode, this:
// 1. Create a node
// 2. Insert the node into `fct_sig_nodes_[current_key]`
//
// At the end of the tool run for a given translation unit, edges between
// corresponding nodes of two adjacent function signatures are created.
class FunctionSignatureNodes : public MatchFinder::MatchCallback {
 public:
  explicit FunctionSignatureNodes(
      std::map<std::string, std::set<std::string>>& sig_nodes,
      std::vector<std::pair<std::string, std::string>>& sig_pairs)
      : fct_sig_nodes_(sig_nodes), fct_sig_pairs_(sig_pairs) {}

  FunctionSignatureNodes(const FunctionSignatureNodes&) = delete;
  FunctionSignatureNodes& operator=(const FunctionSignatureNodes&) = delete;

 private:
  std::string getNodeFromMatchResult(const MatchFinder::MatchResult& result) {
    if (auto* type_loc =
            result.Nodes.getNodeAs<clang::PointerTypeLoc>("rhs_type_loc")) {
      return getNodeFromPointerTypeLoc(type_loc, result);
    }

    if (auto* raw_ptr_type_loc =
            result.Nodes.getNodeAs<clang::TemplateSpecializationTypeLoc>(
                "rhs_raw_ptr_type_loc")) {
      return getNodeFromRawPtrTypeLoc(raw_ptr_type_loc, result);
    }

    // "rhs_begin" match id could refer to a declaration that has a raw_ptr
    // type. Those are handled in getNodeFromRawPtrTypeLoc. We
    // should always check for a "rhs_raw_ptr_type_loc" match id and call
    // getNodeFromRawPtrTypeLoc first.
    if (auto* rhs_begin =
            result.Nodes.getNodeAs<clang::DeclaratorDecl>("rhs_begin")) {
      return getNodeFromDecl(rhs_begin, result);
    }

    // Shouldn't get here.
    llvm::errs() << "\n"
                    "Error: getNodeFromMatchResult() encountered an unexpected "
                    "match.\n"
                    "Expected one of : \n"
                    "  - rhs_type_loc\n"
                    "  - rhs_raw_ptr_type_loc\n"
                    "  - rhs_begin\n"
                    "\n";
    assert(false && "Unexpected match in getNodeFromMatchResult()");
  }

  void run(const MatchFinder::MatchResult& result) override {
    const clang::SourceManager& source_manager = *result.SourceManager;
    const clang::FunctionDecl* fct_decl =
        result.Nodes.getNodeAs<clang::FunctionDecl>("fct_decl");
    const clang::CXXMethodDecl* method_decl =
        result.Nodes.getNodeAs<clang::CXXMethodDecl>("fct_decl");

    const std::string current_key = NodeKey(fct_decl, source_manager);

    // Function related by separate declaration and definition:
    {
      for (auto* previous_decl = fct_decl->getPreviousDecl(); previous_decl;
           previous_decl = previous_decl->getPreviousDecl()) {
        // TODO(356666773): The `previous_decl` might be part of third_party/.
        // Then it won't be matched by the matcher. So only one of the pair
        // would have a node.
        const std::string previous_key = NodeKey(previous_decl, source_manager);
        fct_sig_pairs_.push_back({
            current_key,
            previous_key,
        });
      }
    }

    // Function related by overriding:
    if (method_decl) {
      for (auto* m : method_decl->overridden_methods()) {
        const std::string previous_key = NodeKey(m, source_manager);
        fct_sig_pairs_.push_back({
            current_key,
            previous_key,
        });
      }
    }

    std::string n = getNodeFromMatchResult(result);
    fct_sig_nodes_[current_key].insert(n);
  }

  // Map a function signature, which is modeled as a string representing file
  // location, to its matched graph nodes (RTNode and ParmVarDecl nodes).
  // Note: `RTNode` represents a function return type node.
  // In order to avoid relying on the order with which nodes are matched in
  // the AST, and to guarantee that nodes are stored in the file declaration
  // order, we use a `std::set<std::string>` which sorts Nodes based on their
  // keys. Node that keys are properly ordered to reflect the order in the
  // file. This property is important, because at the end of a tool run on a
  // translationUnit, for each pair of function signatures, we iterate
  // concurrently through the two sets of Nodes creating edges between nodes
  // that appear at the same index.
  std::map<std::string, std::set<std::string>>& fct_sig_nodes_;

  // Map related function signatures to each other, this is needed for
  // functions with separate definition and declaration, and for overridden
  // functions.
  std::vector<std::pair<std::string, std::string>>& fct_sig_pairs_;
};

raw_ptr_plugin::FilterFile PathsToExclude() {
  std::vector<std::string> paths_to_exclude_lines;
  paths_to_exclude_lines.insert(paths_to_exclude_lines.end(),
                                kSpanifyManualPathsToIgnore.begin(),
                                kSpanifyManualPathsToIgnore.end());
  paths_to_exclude_lines.insert(paths_to_exclude_lines.end(),
                                kSeparateRepositoryPaths.begin(),
                                kSeparateRepositoryPaths.end());
  return raw_ptr_plugin::FilterFile(paths_to_exclude_lines);
}

class Spanifier {
 public:
  explicit Spanifier(
      MatchFinder& finder,
      std::map<std::string, std::set<std::string>>& sig_nodes,
      std::vector<std::pair<std::string, std::string>>& sig_pairs)
      : match_finder_(finder), fct_sig_nodes_(sig_nodes, sig_pairs) {
    auto exclusions = anyOf(
        isExpansionInSystemHeader(), raw_ptr_plugin::isInExternCContext(),
        raw_ptr_plugin::isInThirdPartyLocation(),
        raw_ptr_plugin::isInGeneratedLocation(),
        raw_ptr_plugin::ImplicitFieldDeclaration(),
        raw_ptr_plugin::isInMacroLocation(),
        raw_ptr_plugin::isInLocationListedInFilterFile(&paths_to_exclude_),
        hasAncestor(cxxRecordDecl(anyOf(hasName("raw_ptr"), hasName("span")))));

    // Exclude literal strings as these need to become string_view
    auto pointer_type = pointerType(pointee(qualType(unless(anyOf(
        qualType(hasDeclaration(
            cxxRecordDecl(raw_ptr_plugin::isAnonymousStructOrUnion()))),
        hasUnqualifiedDesugaredType(anyOf(functionType(), memberPointerType())),
        hasCanonicalType(
            anyOf(asString("const char"), asString("const wchar_t"),
                  asString("const char8_t"), asString("const char16_t"),
                  asString("const char32_t"))))))));

    auto raw_ptr_type = qualType(
        hasDeclaration(classTemplateSpecializationDecl(hasName("raw_ptr"))));
    auto raw_ptr_type_loc = templateSpecializationTypeLoc(loc(raw_ptr_type));

    auto lhs_type_loc = anyOf(
        hasType(pointer_type),
        allOf(hasType(raw_ptr_type),
              hasDescendant(raw_ptr_type_loc.bind("lhs_raw_ptr_type_loc"))));
    auto rhs_type_loc = anyOf(
        hasType(pointer_type),
        allOf(hasType(raw_ptr_type),
              hasDescendant(raw_ptr_type_loc.bind("rhs_raw_ptr_type_loc"))));

    auto lhs_field =
        fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(lhs_type_loc),
                  unless(exclusions),
                  unless(hasParent(cxxRecordDecl(hasName("raw_ptr")))))
            .bind("lhs_begin");
    auto rhs_field =
        fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(rhs_type_loc),
                  unless(exclusions),
                  unless(hasParent(cxxRecordDecl(hasName("raw_ptr")))))
            .bind("rhs_begin");

    auto lhs_var = varDecl(lhs_type_loc, unless(exclusions)).bind("lhs_begin");
    auto rhs_var = varDecl(rhs_type_loc, unless(exclusions)).bind("rhs_begin");

    auto lhs_param =
        parmVarDecl(
            anyOf(lhs_type_loc,
                  // In addition to pointer type params, we'd like to rewrite
                  // array type params with base::span<T, size>.
                  allOf(isArrayParm(),
                        hasTypeLoc(
                            loc(qualType(anything())).bind("array_type_loc")))),
            unless(exclusions))
            .bind("lhs_begin");

    auto rhs_param =
        parmVarDecl(
            anyOf(rhs_type_loc,
                  // In addition to pointer type params, we'd like to rewrite
                  // array type params with base::span<T, size>.
                  allOf(isArrayParm(),
                        hasTypeLoc(
                            loc(qualType(anything())).bind("array_type_loc")))),
            unless(exclusions))
            .bind("rhs_begin");

    // Exclude functions returning literal strings as these need to become
    // string_view.
    auto exclude_literal_strings =
        unless(returns(qualType(pointsTo(qualType(hasCanonicalType(
            anyOf(asString("const char"), asString("const wchar_t"),
                  asString("const char8_t"), asString("const char16_t"),
                  asString("const char32_t"))))))));

    auto rhs_call_expr = callExpr(callee(
        functionDecl(hasReturnTypeLoc(pointerTypeLoc().bind("rhs_type_loc")),
                     exclude_literal_strings, unless(exclusions))));

    auto lhs_call_expr = callExpr(callee(
        functionDecl(hasReturnTypeLoc(pointerTypeLoc().bind("lhs_type_loc")),
                     exclude_literal_strings, unless(exclusions))));

    auto lhs_expr = expr(anyOf(declRefExpr(to(anyOf(lhs_var, lhs_param))),
                               memberExpr(member(lhs_field)), lhs_call_expr));

    auto constant_array_exprs =
        declRefExpr(to(anyOf(varDecl(hasType(constantArrayType())),
                             parmVarDecl(hasType(constantArrayType())),
                             fieldDecl(hasType(constantArrayType())))));

    // Matches statements of the form: &buf[n] where buf is a container type
    // (span, vector, array,...).
    auto buff_address_from_container = unaryOperator(
        hasOperatorName("&"),
        hasUnaryOperand(cxxOperatorCallExpr(callee(functionDecl(
            hasName("operator[]"),
            hasParent(cxxRecordDecl(hasMethod(hasName("size")))))))));

    // T* a = buf.data();
    auto member_data_call =
        cxxMemberCallExpr(
            callee(functionDecl(
                hasName("data"),
                hasParent(cxxRecordDecl(hasMethod(hasName("size")))))),
            has(memberExpr().bind("data_member_expr")))
            .bind("member_data_call");

    // Defines nodes that contain size information, these include:
    //  - nullptr => size is zero
    //  - calls to new/new[n] => size is 1/n
    //  - constant arrays buf[1024] => size is 1024
    //  - calls to third_party functions that we can't rewrite (they should
    //    provide a size for the pointer returned)
    // TODO(353710304): Consider handling functions taking in/out args ex:
    //                  void alloc(**ptr);
    // TODO(353710304): Consider making member_data_call and size_node mutually
    //                  exclusive. We rely here on the ordering of expressions
    //                  in the anyOf matcher to first match member_data_call
    //                  which is a subset of size_node.
    auto size_node_matcher = expr(anyOf(
        member_data_call,
        expr(anyOf(callExpr(callee(functionDecl(
                       hasReturnTypeLoc(pointerTypeLoc()),
                       anyOf(raw_ptr_plugin::isInThirdPartyLocation(),
                             isExpansionInSystemHeader(),
                             raw_ptr_plugin::isInExternCContext())))),
                   cxxNullPtrLiteralExpr().bind("nullptr_expr"), cxxNewExpr(),
                   constant_array_exprs, buff_address_from_container))
            .bind("size_node")));

    auto rhs_expr =
        expr(ignoringParenCasts(anyOf(
                 declRefExpr(to(anyOf(rhs_var, rhs_param))).bind("declRefExpr"),
                 memberExpr(member(rhs_field)).bind("memberExpr"),
                 rhs_call_expr.bind("callExpr"))))
            .bind("rhs_expr");

    auto get_calls_on_raw_ptr = cxxMemberCallExpr(
        callee(cxxMethodDecl(hasName("get"), ofClass(hasName("raw_ptr")))),
        has(memberExpr(has(rhs_expr))));

    auto rhs_exprs_without_size_nodes =
        expr(ignoringParenCasts(anyOf(
                 rhs_expr,
                 binaryOperation(hasOperatorName("+"), hasLHS(rhs_expr),
                                 hasRHS(expr().bind("bin_op_rhs")))
                     .bind("binaryOperator"),
                 unaryOperator(hasOperatorName("++"), hasUnaryOperand(rhs_expr))
                     .bind("unaryOperator"),
                 cxxOperatorCallExpr(
                     callee(cxxMethodDecl(ofClass(hasName("raw_ptr")))),
                     hasOperatorName("++"), hasArgument(0, rhs_expr))
                     .bind("raw_ptr_operator++"),
                 get_calls_on_raw_ptr)))
            .bind("span_frontier");

    // This represents the forms under which an expr could appear on the right
    // hand side of an assignment operation, var construction, or an expr passed
    // as callExpr argument. Examples:
    // rhs_expr, rhs_expr++, ++rhs_expr, rhs_expr + n, cast(rhs_expr);
    auto rhs_expr_variations = expr(ignoringParenCasts(
        anyOf(size_node_matcher, rhs_exprs_without_size_nodes)));

    auto lhs_expr_variations = expr(ignoringParenCasts(lhs_expr));

    // Expressions used to decide the pointer is used as a buffer include:
    // expr[n], expr++, ++expr, expr + n, expr += n
    auto unsafe_buffer_access_from_ptr = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(ignoringParenCasts(anyOf(
            // Unsafe pointer subscript:
            arraySubscriptExpr(hasLHS(lhs_expr_variations),
                               unless(isSafeArraySubscript())),
            // Unsafe pointer arithmetic:
            binaryOperation(anyOf(hasOperatorName("+="), hasOperatorName("+")),
                            hasLHS(lhs_expr_variations)),
            unaryOperator(hasOperatorName("++"),
                          hasUnaryOperand(lhs_expr_variations)),
            // Unsafe base::raw_ptr arithmetic:
            cxxOperatorCallExpr(
                anyOf(hasOverloadedOperatorName("[]"), hasOperatorName("++")),
                hasArgument(0, lhs_expr_variations))))));
    Match(unsafe_buffer_access_from_ptr, [](const auto& result) {
      EmitSource(GetLHS(result));  // Declare unsafe buffer access.
    });

    auto array_variable =
        varDecl(hasType(arrayType().bind("array_type")),
                hasTypeLoc(loc(qualType(anything())).bind("array_type_loc")),
                unless(exclusions), unless(hasExternalFormalLinkage()))
            .bind("array_variable");

    auto unsafe_array_access =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 expr(ignoringParenCasts(arraySubscriptExpr(
                     unless(isSafeArraySubscript()),
                     hasLHS(declRefExpr(to(array_variable)))))));

    Match(unsafe_array_access, RewriteUnsafeArray);

    // `sizeof(c_array)` is rewritten to
    // `std_array.size() * sizeof(element_size)`.
    auto sizeof_array_expr = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        sizeOfExpr(has(declRefExpr(to(array_variable)))).bind("sizeof_expr"));
    Match(sizeof_array_expr, RewriteArraySizeof);

    auto deref_expression = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(anyOf(unaryOperator(hasOperatorName("*"),
                                 hasUnaryOperand(rhs_exprs_without_size_nodes)),
                   cxxOperatorCallExpr(
                       hasOverloadedOperatorName("*"),
                       hasArgument(0, rhs_exprs_without_size_nodes))),
             unless(raw_ptr_plugin::isInMacroLocation()))
            .bind("deref_expr"));
    Match(deref_expression, DecaySpanToPointer);

    auto rhs_expr_variations_ignoring_non_spelled_nodes = traverse(
        clang::TK_IgnoreUnlessSpelledInSource, expr(rhs_expr_variations));
    auto raw_ptr_op_bool = cxxMemberCallExpr(
        callee(cxxMethodDecl(hasName("operator bool"),
                             ofClass(hasName("raw_ptr")))),
        has(memberExpr(has(expr(ignoringParenCasts(
            rhs_expr_variations_ignoring_non_spelled_nodes))))));
    // Handles boolean operations that need to be adapted after a span rewrite.
    // Currently:
    //   if(expr) => if(expr.size())
    // TODO(394367201): Rewrite boolean operations as follows:
    //   if(expr) => if(!expr.empty())
    //   if(!expr) => if(expr.empty())
    // Notice here that the implicit cast part of the expression is traversed
    // using the default traversal mode `clang::TK_AsIs`, while the expression
    // variation matcher is traversed using
    // `clang::TK_IgnoreUnlessSpelledInSource`. The traversal mode
    // `clang::TK_IgnoreUnlessSpelledInSource`, while very useful in simplifying
    // the matchers, wouldn't detect boolean operations on pointers hence the
    // need for a hybrid traversal mode in this matcher.
    auto boolean_op = expr(anyOf(
        implicitCastExpr(hasCastKind(clang::CastKind::CK_PointerToBoolean),
                         hasSourceExpression(expr(
                             rhs_expr_variations_ignoring_non_spelled_nodes))),
        raw_ptr_op_bool));
    Match(boolean_op, [](const MatchFinder::MatchResult& result) {
      EmitReplacement(GetRHS(result),
                      GetReplacementDirective(getSourceRange(result), ".size()",
                                              *result.SourceManager));
    });

    // This is needed to remove the `.get()` call on raw_ptr from rewritten
    // expressions. Example: raw_ptr<T> member; auto* temp = member.get(); if
    // member's type is rewritten to a raw_span<T>, this matcher is used to
    // remove the `.get()` call.
    auto raw_ptr_get_call = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxMemberCallExpr(
            callee(cxxMethodDecl(hasName("get"), ofClass(hasName("raw_ptr")))),
            has(memberExpr(has(rhs_expr)).bind("get_member_expr"))));
    Match(raw_ptr_get_call, [](const MatchFinder::MatchResult& result) {
      clang::SourceManager& source_manager = *result.SourceManager;
      EraseMemberCall(
          GetRHS(result),
          result.Nodes.getNodeAs<clang::MemberExpr>("get_member_expr"),
          source_manager);
    });

    // When passing now-span buffers to third_party functions as parameters, we
    // need to add `.data()` to extract the pointer and keep things compiling.
    auto buffer_to_external_func = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        callExpr(callee(functionDecl(
                     anyOf(isExpansionInSystemHeader(),
                           raw_ptr_plugin::isInExternCContext(),
                           raw_ptr_plugin::isInThirdPartyLocation()))),
                 forEachArgumentWithParam(
                     expr(rhs_expr_variations,
                          unless(anyOf(
                              castExpr(hasSourceExpression(size_node_matcher)),
                              size_node_matcher))),
                     parmVarDecl())));
    Match(buffer_to_external_func, [](const MatchFinder::MatchResult& result) {
      EmitReplacement(GetRHS(result), AppendDataCall(result));
    });

    // When passing c-style arrays to third_party functions as parameters, we
    // need to add `.data()` to extract the pointer and keep things compiling.
    //
    // Functions that are annotated with UNSAFE_BUFFER_USAGE also get this
    // treatment because the annotation means it was left there intentionally.
    // And since they emit warnings we can easily find and spanify them later.
    // Functions that are known to accept both c-style arrays and std::array,
    // like std::size() are excluded.
    auto passing_a_c_array_to_external_functions_etc = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        callExpr(callee(functionDecl(
                     anyOf(isExpansionInSystemHeader(),
                           raw_ptr_plugin::isInExternCContext(),
                           raw_ptr_plugin::isInThirdPartyLocation(),
                           hasAttr(clang::attr::UnsafeBufferUsage)),
                     unless(matchesName(
                         "^::std::(size|begin|end|empty|swap|ranges::)")))),
                 forEachArgumentWithParam(
                     expr(declRefExpr(to(array_variable)).bind("rhs_expr")),
                     parmVarDecl())));
    Match(passing_a_c_array_to_external_functions_etc, [](const auto& result) {
      EmitReplacement(ArrayVariableNode(result), AppendDataCall(result));
    });

    // Handles assignment:
    // a = b;
    // a = fct();
    // a = reinterpret_cast<>(b);
    // a = (cond) ? expr1 : expr2;
    auto assignement_relationship = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        binaryOperation(hasOperatorName("="),
                        hasOperands(lhs_expr_variations,
                                    anyOf(rhs_expr_variations,
                                          conditionalOperator(hasTrueExpression(
                                              rhs_expr_variations)))),
                        unless(isExpansionInSystemHeader())));
    Match(assignement_relationship, MatchAdjacency);

    // Creates the edge from lhs to false_expr in a ternary conditional
    // operator.
    auto assignement_relationship2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        binaryOperation(hasOperatorName("="),
                        hasOperands(lhs_expr_variations,
                                    conditionalOperator(hasFalseExpression(
                                        rhs_expr_variations))),
                        unless(isExpansionInSystemHeader())));
    Match(assignement_relationship2, MatchAdjacency);

    // Supports:
    // T* temp = member;
    // T* temp = init();
    // T* temp = (cond) ? expr1 : expr2;
    // T* temp = reinterpret_cast<>(b);
    auto var_construction = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        varDecl(
            lhs_var,
            has(expr(anyOf(
                rhs_expr_variations,
                conditionalOperator(hasTrueExpression(rhs_expr_variations)),
                cxxConstructExpr(has(expr(anyOf(
                    rhs_expr_variations, conditionalOperator(hasTrueExpression(
                                             rhs_expr_variations))))))))),
            unless(isExpansionInSystemHeader())));
    Match(var_construction, MatchAdjacency);

    // Creates the edge from lhs to false_expr in a ternary conditional
    // operator.
    auto var_construction2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        varDecl(
            lhs_var,
            has(expr(anyOf(
                conditionalOperator(hasFalseExpression(rhs_expr_variations)),
                cxxConstructExpr(has(expr(conditionalOperator(
                    hasFalseExpression(rhs_expr_variations)))))))),
            unless(isExpansionInSystemHeader())));
    Match(var_construction2, MatchAdjacency);

    // Supports:
    // return member;
    // return fct();
    // return reinterpret_cast(expr);
    // return (cond) ? expr1 : expr2;
    auto returned_var_or_member = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        returnStmt(
            hasReturnValue(expr(anyOf(
                rhs_expr_variations,
                conditionalOperator(hasTrueExpression(rhs_expr_variations))))),
            unless(isExpansionInSystemHeader()),
            forFunction(functionDecl(
                hasReturnTypeLoc(pointerTypeLoc().bind("lhs_type_loc")),
                unless(exclusions))))
            .bind("lhs_stmt"));
    Match(returned_var_or_member, MatchAdjacency);

    // Creates the edge from lhs to false_expr in a ternary conditional
    // operator.
    auto returned_var_or_member2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        returnStmt(hasReturnValue(conditionalOperator(
                       hasFalseExpression(rhs_expr_variations))),
                   unless(isExpansionInSystemHeader()),
                   forFunction(functionDecl(
                       hasReturnTypeLoc(pointerTypeLoc().bind("lhs_type_loc")),
                       unless(exclusions))))
            .bind("lhs_stmt"));
    Match(returned_var_or_member2, MatchAdjacency);

    // Handles expressions of the form member(arg).
    // A(const T* arg): member(arg){}
    // member(init());
    // member(fct());
    auto ctor_initilizer = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxCtorInitializer(withInitializer(anyOf(
                               cxxConstructExpr(has(expr(rhs_expr_variations))),
                               rhs_expr_variations)),
                           forField(lhs_field)));
    Match(ctor_initilizer, MatchAdjacency);

    // Supports:
    // S* temp;
    // Obj o(temp); Obj o{temp};
    // This links temp to the parameter in Obj's constructor.
    auto var_passed_in_constructor = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxConstructExpr(forEachArgumentWithParam(
            expr(anyOf(
                rhs_expr_variations,
                conditionalOperator(hasTrueExpression(rhs_expr_variations)))),
            lhs_param)));
    Match(var_passed_in_constructor, MatchAdjacency);

    // Creates the edge from lhs to false_expr in a ternary conditional
    // operator.
    auto var_passed_in_constructor2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxConstructExpr(forEachArgumentWithParam(
            expr(conditionalOperator(hasFalseExpression(rhs_expr_variations))),
            lhs_param)));
    Match(var_passed_in_constructor2, MatchAdjacency);

    // handles Obj o{temp} when Obj has no constructor.
    // This creates a link between the expr and the underlying field.
    auto var_passed_in_initlistExpr = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        initListExpr(raw_ptr_plugin::forEachInitExprWithFieldDecl(
            expr(anyOf(
                rhs_expr_variations,
                conditionalOperator(hasTrueExpression(rhs_expr_variations)))),
            lhs_field)));
    Match(var_passed_in_initlistExpr, MatchAdjacency);

    auto var_passed_in_initlistExpr2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        initListExpr(raw_ptr_plugin::forEachInitExprWithFieldDecl(
            expr(conditionalOperator(hasFalseExpression(rhs_expr_variations))),
            lhs_field)));
    Match(var_passed_in_initlistExpr2, MatchAdjacency);

    // Link var/field passed as function arguments to function parameter
    // This handles func(var/member/param), func(func2())
    // cxxOpCallExprs excluded here since operator= can be invoked as a call
    // expr for classes/structs.
    auto call_expr = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        callExpr(forEachArgumentWithParam(
                     expr(anyOf(rhs_expr_variations,
                                conditionalOperator(
                                    hasTrueExpression(rhs_expr_variations)))),
                     lhs_param),
                 unless(isExpansionInSystemHeader()),
                 unless(cxxOperatorCallExpr(hasOperatorName("=")))));
    Match(call_expr, MatchAdjacency);

    // Map function declaration signature to function definition signature;
    // This is problematic in the case of callbacks defined in function.
    auto fct_decls_params =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 functionDecl(forEachParmVarDecl(rhs_param), unless(exclusions))
                     .bind("fct_decl"));
    match_finder_.addMatcher(fct_decls_params, &fct_sig_nodes_);

    auto fct_decls_returns = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        functionDecl(hasReturnTypeLoc(pointerTypeLoc().bind("rhs_type_loc")),
                     unless(exclusions))
            .bind("fct_decl"));
    match_finder_.addMatcher(fct_decls_returns, &fct_sig_nodes_);
  }

 private:
  // An adapter class to execute a callback on a match.
  //
  // This allows developers to pass a regular function as callbacks. It avoids
  // the need of creating a new class for each callback. This promotes more
  // localized code, as it avoids the temptation of reusing a previously
  // created class.
  class MatchCallback : public MatchFinder::MatchCallback {
   public:
    explicit MatchCallback(
        std::function<void(const MatchFinder::MatchResult&)> callback)
        : callback_(callback) {}

    void run(const MatchFinder::MatchResult& result) override {
      callback_(result);
    }

   private:
    std::function<void(const MatchFinder::MatchResult&)> callback_;
  };

  // Registers a matcher and a callback to be executed on a match.
  template <typename Matcher>
  void Match(const Matcher& matcher,
             std::function<void(const MatchFinder::MatchResult&)> fn) {
    auto match_callback = std::make_unique<MatchCallback>(std::move(fn));
    match_finder_.addMatcher(matcher, match_callback.get());
    match_callbacks_.push_back(std::move(match_callback));
  }

  raw_ptr_plugin::FilterFile paths_to_exclude_ = PathsToExclude();
  MatchFinder& match_finder_;
  FunctionSignatureNodes fct_sig_nodes_;
  std::vector<std::unique_ptr<MatchCallback>> match_callbacks_;
};

}  // namespace

int main(int argc, const char* argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::cl::OptionCategory category(
      "spanifier: changes"
      " 1- |T* var| to |base::span<T> var|."
      " 2- |raw_ptr<T> var| to |base::raw_span<T> var|");

  llvm::Expected<clang::tooling::CommonOptionsParser> options =
      clang::tooling::CommonOptionsParser::create(argc, argv, category);
  assert(static_cast<bool>(options));  // Should not return an error.
  clang::tooling::ClangTool tool(options->getCompilations(),
                                 options->getSourcePathList());

  // Map a function signature, which is modeled as a string representing file
  // location, to it's graph nodes (RTNode and ParmVarDecl nodes).
  // RTNode represents a function return type.
  std::map<std::string, std::set<std::string>> fct_sig_nodes;
  // Map related function signatures to each other, this is needed for functions
  // with separate definition and declaration, and for overridden functions.
  std::vector<std::pair<std::string, std::string>> fct_sig_pairs;
  MatchFinder match_finder;
  Spanifier rewriter(match_finder, fct_sig_nodes, fct_sig_pairs);

  // Prepare and run the tool.
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(factory.get());

  // Establish connections between corresponding parameters of adjacent function
  // signatures. Two functions are considered adjacent if one overrides the
  // other or if one is a function declaration while the other is its
  // corresponding definition.
  for (auto& [l, r] : fct_sig_pairs) {
    // By construction, only the left side of the pair is guaranteed to have a
    // matching set of nodes.
    assert(fct_sig_nodes.find(l) != fct_sig_nodes.end());

    // TODO(356666773): Handle the case where both side of the pair haven't
    // been matched. This happens when a function is declared in third_party/,
    // but implemented in first party.
    if (fct_sig_nodes.find(r) == fct_sig_nodes.end()) {
      continue;
    }

    auto& s1 = fct_sig_nodes[l];
    auto& s2 = fct_sig_nodes[r];
    assert(s1.size() == s2.size());
    auto i1 = s1.begin();
    auto i2 = s2.begin();
    while (i1 != s1.end()) {
      EmitEdge(*i1, *i2);
      EmitEdge(*i2, *i1);
      i1++;
      i2++;
    }
  }

  return result;
}
