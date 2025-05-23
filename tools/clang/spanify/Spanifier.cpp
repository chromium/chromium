// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
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

const char kBaseAutoSpanificationHelperIncludePath[] =
    "base/containers/auto_spanification_helper.h";

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

AST_MATCHER(clang::VarDecl, hasExternalStorage) {
  return Node.hasExternalStorage();
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

struct UnsafeFreeFuncToMacro {
  // The name of an unsafe free function to be rewritten.
  const std::string_view function_name;
  // The helper macro name to be rewritten to.
  const std::string_view macro_name;
};

std::optional<UnsafeFreeFuncToMacro> FindUnsafeFreeFuncToBeRewrittenToMacro(
    const clang::FunctionDecl* function_decl) {
  // The table of unsafe free functions to be rewritten to helper macro calls.
  // Note that C++20 is not supported in tools/clang/spanify/ and we cannot use
  // std::to_array.
  static constexpr UnsafeFreeFuncToMacro unsafe_free_func_table[] = {
      // https://source.chromium.org/chromium/chromium/src/+/main:third_party/boringssl/src/include/openssl/pool.h;drc=c76e4f83a8c5786b463c3e55c070a21ac751b96b;l=81
      {"CRYPTO_BUFFER_data", "UNSAFE_CRYPTO_BUFFER_DATA"},
      // https://source.chromium.org/chromium/chromium/src/+/main:third_party/harfbuzz-ng/src/src/hb-buffer.h;drc=ea6a172f84f2cbcfed803b5ae71064c7afb6b5c2;l=647
      {"hb_buffer_get_glyph_infos", "UNSAFE_HB_BUFFER_GET_GLYPH_INFOS"},
      // https://source.chromium.org/chromium/chromium/src/+/main:third_party/harfbuzz-ng/src/src/hb-buffer.h;drc=c76e4f83a8c5786b463c3e55c070a21ac751b96b;l=651
      {"hb_buffer_get_glyph_positions", "UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS"},
      // https://source.chromium.org/chromium/chromium/src/+/main:remoting/host/xsession_chooser_linux.cc;drc=fca90714b3949f0f4c27f26ef002fe8d33f3cb73;l=274
      {"g_get_system_data_dirs", "UNSAFE_G_GET_SYSTEM_DATA_DIRS"},
  };

  const std::string& function_name = function_decl->getQualifiedNameAsString();

  for (const auto& entry : unsafe_free_func_table) {
    if (function_name == entry.function_name) {
      return entry;
    }
  }

  return std::nullopt;
}

struct UnsafeCxxMethodToMacro {
  // The qualified class name of an unsafe method to be rewritten.
  const std::string_view class_name;
  // The name of an unsafe method to be rewritten.
  const std::string_view method_name;
  // The helper macro name to be rewritten to.
  const std::string_view macro_name;
};

// Given a clang::CXXMethodDecl, find a corresponding UnsafeCxxMethodToMacro
// instance if the method matches. Returns nullptr if not found.
std::optional<UnsafeCxxMethodToMacro> FindUnsafeCxxMethodToBeRewrittenToMacro(
    const clang::CXXMethodDecl* method_decl) {
  // The table of unsafe methods to be rewritten to helper macro calls.
  // Note that C++20 is not supported in tools/clang/spanify/ and we cannot use
  // std::to_array.
  static constexpr UnsafeCxxMethodToMacro unsafe_cxx_method_table[] = {
      {"SkBitmap", "NoArgForTesting", "UNSAFE_SKBITMAP_NOARGFORTESTING"},
      // https://source.chromium.org/chromium/chromium/src/+/main:third_party/skia/include/core/SkBitmap.h;drc=f72bd467feb15edd9323e46eab1b74ab6025bc5b;l=936
      {"SkBitmap", "getAddr32", "UNSAFE_SKBITMAP_GETADDR32"},
  };

  const clang::CXXRecordDecl* class_decl = method_decl->getParent();
  const std::string& method_name = method_decl->getNameAsString();
  const std::string& class_name = class_decl->getQualifiedNameAsString();

  for (const auto& entry : unsafe_cxx_method_table) {
    if (method_name == entry.method_name && class_name == entry.class_name) {
      return entry;
    }
  }

  return std::nullopt;
}

AST_MATCHER(clang::FunctionDecl, unsafeFunctionToBeRewrittenToMacro) {
  const clang::FunctionDecl* function_decl = &Node;
  if (const clang::CXXMethodDecl* method_decl =
          clang::dyn_cast<clang::CXXMethodDecl>(function_decl)) {
    return bool(FindUnsafeCxxMethodToBeRewrittenToMacro(method_decl));
  }
  return bool(FindUnsafeFreeFuncToBeRewrittenToMacro(function_decl));
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
                             const clang::SourceManager& source_manager,
                             const std::string& optional_seed = "") {
  clang::tooling::Replacement replacement(
      source_manager, clang::CharSourceRange::getCharRange(range), "");
  llvm::StringRef path = replacement.getFilePath();
  llvm::StringRef file_name = llvm::sys::path::filename(path);

  // Note that the largest file is 7.2Mbytes long. So every offsets can be
  // represented using 7 digits. `ToStringWithPadding` ensures that the
  // alphabetical order of the nodes is the same as the order of the ranges in
  // the file.
  if constexpr (!human_readable) {
    return llvm::formatv(
        "{0}:{1}", ToStringWithPadding(replacement.getOffset(), 7),
        HashBase64(NodeKeyFromRange<true>(range, source_manager, optional_seed),
                   8));
  }

  return llvm::formatv("{0}:{1}:{2}:{3}:{4}:{5}",
                       ToStringWithPadding(replacement.getOffset(), 7),
                       HashBase64(path.str() + optional_seed), file_name,
                       source_manager.getSpellingLineNumber(range.getBegin()),
                       source_manager.getSpellingColumnNumber(range.getBegin()),
                       replacement.getLength());
}

// Returns the identifier for the given clang node. The returned identifier is
// unique to a pair of (node, optional_seed). See also `NodeKeyFromRange` for
// details.
//
// Arguments:
//   node = A clang node whose identifier is returned.
//   source_manager = The clang::SourceManager of the clang node `node`.
//   optional_seed = The given string is used to make a variation of the
//       identifier of `node`. This argument is useful when `node` alone does
//       not provide enough fine precision.
template <typename T>
std::string NodeKey(const T* node,
                    const clang::SourceManager& source_manager,
                    const std::string& optional_seed = "") {
  return NodeKeyFromRange(node->getSourceRange(), source_manager,
                          optional_seed);
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
void EmitReplacement(std::string_view node, std::string_view replacement) {
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

template <typename T>
const T* GetNodeOrCrash(const MatchFinder::MatchResult& result,
                        std::string_view id,
                        std::string_view assert_message) {
  const T* node = result.Nodes.getNodeAs<T>(id);
  if (!node) {
    llvm::errs() << "\nError: no node for `" << id << "` (" << assert_message
                 << ")\n";
    DumpMatchResult(result);
    assert(false && "`GetNodeOrCrash()`");
  }
  return node;
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

  if (auto* binary_op = clang::dyn_cast_or_null<clang::BinaryOperator>(expr)) {
    return {expr->getBeginLoc(),
            getExprRange(binary_op->getRHS(), source_manager, lang_options)
                .getEnd()};
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
  printing_policy.PrintAsCanonical = 0;
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
    auto* sub_expr = result.Nodes.getNodeAs<clang::Expr>("binary_op_rhs");
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

// This represents a c-style array function parameter.
// Since we don't always have the size of the array parameter, we rewrite the
// parameter to a base::span to preserve the size information for bound
// checking.
// Example:
//    void fct(int arr[])  => void fct(base::span<int> arr)
//    void fct(int arr[3]) => void fct(base::span<int, 3> arr)
static std::string getNodeFromFunctionArrayParameter(
    const clang::TypeLoc* type_loc,
    const clang::ParmVarDecl* param_decl,
    const MatchFinder::MatchResult& result) {
  clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;

  // Preserve qualifiers.
  const clang::QualType& qual_type = param_decl->getType();
  std::ostringstream qualifiers;
  qualifiers << (qual_type.isConstQualified() ? "const " : "")
             << (qual_type.isVolatileQualified() ? "volatile " : "");

  std::string type = GetTypeAsString(qual_type->getPointeeType(), ast_context);

  const clang::ArrayTypeLoc& array_type_loc =
      type_loc->getUnqualifiedLoc().getAs<clang::ArrayTypeLoc>();
  assert(!array_type_loc.isNull());
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
  clang::SourceRange replacement_range{
      param_decl->getBeginLoc(),
      array_type_loc.getRBracketLoc().getLocWithOffset(1)};
  std::string replacement_text =
      qualifiers.str() + span_type + param_decl->getNameAsString();

  const std::string key =
      NodeKeyFromRange(replacement_range, source_manager, type);
  EmitReplacement(key,
                  GetReplacementDirective(replacement_range, replacement_text,
                                          source_manager));
  EmitReplacement(key, GetIncludeDirective(replacement_range, source_manager));

  return key;
}

static std::string getNodeFromDecl(const clang::DeclaratorDecl* decl,
                                   const MatchFinder::MatchResult& result) {
  clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;

  clang::SourceRange replacement_range{decl->getBeginLoc(),
                                       decl->getLocation()};

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

  std::string replacement_text =
      qualifiers.str() + llvm::formatv("base::span<{0}>", type).str();

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
  auto begin_range = clang::SourceRange(
      deref_expr->getBeginLoc(), deref_expr->getBeginLoc().getLocWithOffset(1));
  auto end_range = clang::SourceRange(getSourceRange(result).getEnd());

  // Replacement to delete the leading '*'
  std::string begin_replacement_text = " ";
  std::string end_replacement_text = "[0]";
  if (result.Nodes.getNodeAs<clang::Expr>("unaryOperator")) {
    // For unaryOperators we still encapsulate the expression with parenthesis.
    begin_replacement_text = "(";
    end_replacement_text = ")[0]";
  }

  EmitReplacement(GetRHS(result),
                  GetReplacementDirective(begin_range, begin_replacement_text,
                                          source_manager));

  EmitReplacement(
      GetRHS(result),
      GetReplacementDirective(end_range, end_replacement_text, source_manager));
}

static clang::SourceLocation GetBinaryOperationOperatorLoc(
    const clang::Expr* expr,
    const MatchFinder::MatchResult& result) {
  if (auto* binary_op = clang::dyn_cast_or_null<clang::BinaryOperator>(expr)) {
    return binary_op->getOperatorLoc();
  }

  if (auto* binary_op =
          clang::dyn_cast_or_null<clang::CXXOperatorCallExpr>(expr)) {
    return binary_op->getOperatorLoc();
  }

  if (auto* binary_op =
          clang::dyn_cast_or_null<clang::CXXRewrittenBinaryOperator>(expr)) {
    return binary_op->getOperatorLoc();
  }

  // Not supposed to get here.
  llvm::errs()
      << "\n"
         "Error: GetBinaryOperationOperatorLoc() encountered an unexpected "
         "expression.\n"
         "Expected on of clang::BinaryOperator, clang::CXXOperatorCallExpr, "
         "clang::CXXRewrittenBinaryOperator \n";
  DumpMatchResult(result);
  assert(false && "Unexpected binaryOperation Node");
}

static void AdaptBinaryOperation(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();
  const auto* binary_operation =
      GetNodeOrCrash<clang::Expr>(result, "binary_operation", __FUNCTION__);
  const auto* binary_op_RHS =
      GetNodeOrCrash<clang::Expr>(result, "binary_op_rhs", __FUNCTION__);
  const std::string key = GetRHS(result);

  // C-style arrays are rewritten to `std::array`, not `base::span`, so
  // a binary operation on the rewritten array must explicitly construct
  // a `base::span` of it before calling `.subspan()`.
  //
  // Emit a replacement to that effect:
  // `base::span( <binary operation lhs> )`
  const auto* rhs_array_type =
      result.Nodes.getNodeAs<clang::ArrayTypeLoc>("rhs_array_type_loc");
  if (rhs_array_type) {
    const auto* concrete_binary_operation =
        GetNodeOrCrash<clang::BinaryOperator>(
            result, "binary_operation",
            "C-style array should not involve `CXXOperatorCallExpr` or "
            "`CXXRewrittenBinaryOperator`");
    const clang::SourceRange opener_range =
        concrete_binary_operation->getLHS()->getExprLoc();
    EmitReplacement(
        key, GetReplacementDirective(
                 opener_range,
                 llvm::formatv("base::span<{0}>(",
                               GetTypeAsString(rhs_array_type->getInnerType(),
                                               ast_context)),
                 source_manager));
    // Emit the closing `)` of `base::span(...)` below.
  }

  const auto source_range = clang::SourceRange(
      GetBinaryOperationOperatorLoc(binary_operation, result),
      getExprRange(binary_op_RHS, source_manager, lang_opts).getEnd());

  std::string initial_text =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(source_range), source_manager,
          lang_opts)
          .str();

  // initial_text includes the binary operator as the first character.
  // We make sure to trim it from the replacement string.
  //
  // If we wrapped the span-to-be in `base::span(`, emit the closing `)`
  // now.
  EmitReplacement(
      key, GetReplacementDirective(
               source_range,
               llvm::formatv("{0}.subspan({1})", rhs_array_type ? ")" : "",
                             initial_text.substr(1)),
               source_manager));

  // It's possible we emitted a rewrite that creates a temporary but
  // unnamed `base::span` (issue 408018846). This could end up being
  // the only reference in the file, and so it has to carry the
  // `#include` directive itself.
  EmitReplacement(key, GetIncludeDirective(source_range, source_manager));
}

static void AdaptBinaryPlusEqOperation(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();
  // This function handles binary plusEq operations such as:
  // (lhs|rhs)_expr += offset_expr;
  // This is equivalent to:
  // lhs_expr = rhs_expr + offset_expr (lhs_expr == rhs_expr).
  // While we used the `rhs_expr` matcher, for the propose of this
  // rewrite, this is the left-hand side of
  //    buff += offset_expr.
  // This is why we call the expr and its range as lhs_expr and lhs_expr_range
  // respectively.
  auto* lhs_expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr");
  auto* binary_op_RHS = result.Nodes.getNodeAs<clang::Expr>("binary_op_RHS");
  auto lhs_expr_range = getExprRange(lhs_expr, source_manager, lang_opts);
  auto binary_op_rhs_range =
      getExprRange(binary_op_RHS, source_manager, lang_opts);
  auto source_range =
      clang::SourceRange(lhs_expr_range.getEnd(), binary_op_rhs_range.getEnd());
  std::string lhs_expr_text =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(lhs_expr_range), source_manager,
          lang_opts)
          .str();
  std::string binary_op_rhs_text =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(binary_op_rhs_range),
          source_manager, lang_opts)
          .str();

  std::string replacement_text =
      "=" + lhs_expr_text + ".subspan(" + binary_op_rhs_text + ")";

  EmitReplacement(
      GetRHS(result),
      GetReplacementDirective(source_range, replacement_text, source_manager));
}

// Handles boolean operations that need to be adapted after a span rewrite.
//   if(expr) => if(!expr.empty())
//   if(!expr) => if(expr.empty())
// Tests are in: operator-bool-original.cc
static void DecaySpanToBooleanOp(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const std::string& key = GetRHS(result);
  const auto* operand =
      result.Nodes.getNodeAs<clang::Expr>("boolean_op_operand");

  if (const auto* logical_not_op =
          result.Nodes.getNodeAs<clang::UnaryOperator>("logical_not_op")) {
    EmitReplacement(
        key, GetReplacementDirective(logical_not_op->getSourceRange(), "",
                                     source_manager));
  } else {
    EmitReplacement(key, GetReplacementDirective(operand->getBeginLoc(), "!",
                                                 source_manager));
  }

  EmitReplacement(key, GetReplacementDirective(getSourceRange(result).getEnd(),
                                               ".empty()", source_manager));
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
void AppendDataCall(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  auto rep_range = clang::SourceRange(getSourceRange(result).getEnd());

  std::string replacement_text = ".data()";

  if (result.Nodes.getNodeAs<clang::Expr>("unaryOperator")) {
    // Insert enclosing parenthesis for expressions with UnaryOperators
    auto begin_range = clang::SourceRange(getSourceRange(result).getBegin());
    EmitReplacement(GetRHS(result),
                    GetReplacementDirective(begin_range, "(", source_manager));
    replacement_text = ").data()";
  }

  EmitReplacement(
      GetRHS(result),
      GetReplacementDirective(rep_range, replacement_text, source_manager));
}

// Given that we want to emit `.subspan(expr)`,
// *  if `expr` is observably unsigned, does nothing.
// *  if `expr` is a signed int literal, appends `u`.
// *  otherwise, wraps `expr` with `checked_cast`.
void RewriteExprForSubspan(const clang::Expr* expr,
                           const MatchFinder::MatchResult& result,
                           std::string_view key) {
  clang::QualType type = expr->getType();
  const clang::ASTContext& ast_context = *result.Context;

  // This logic isn't perfect: an unsigned type wider than `size_t`
  // will pop us out of this function, but will fail the `strict_cast`
  // imposed by `subspan()`.
  if (type == ast_context.getCorrespondingUnsignedType(type)) {
    return;
  }

  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::SourceRange range =
      getExprRange(expr, source_manager, result.Context->getLangOpts());

  if (clang::dyn_cast<clang::IntegerLiteral>(expr)) {
    EmitReplacement(
        key, GetReplacementDirective(range.getEnd(), "u", source_manager));
    return;
  }

  EmitReplacement(key, GetReplacementDirective(range.getBegin(),
                                               "base::checked_cast<size_t>(",
                                               source_manager));
  EmitReplacement(key,
                  GetReplacementDirective(range.getEnd(), ")", source_manager));
  EmitReplacement(key, GetIncludeDirective(range, source_manager,
                                           "base/numerics/safe_conversions.h"));
  EmitReplacement(key, GetIncludeDirective(range, source_manager, "cstdint",
                                           /*is_system_include_path=*/true));
}

// Handle the case where we match `&container[<offset>]` being used as a buffer.
void EmitContainerPointerRewrites(const MatchFinder::MatchResult& result,
                                  const std::string& key) {
  auto replacement_range =
      GetNodeOrCrash<clang::UnaryOperator>(
          result, "container_buff_address",
          "`container_buff_address` previously expected here")
          ->getSourceRange();
  replacement_range.setEnd(replacement_range.getEnd().getLocWithOffset(1));
  const auto& container_decl_ref = *GetNodeOrCrash<clang::DeclRefExpr>(
      result, "container_decl_ref",
      "`container_buff_address` implies `container_decl_ref`");

  std::string container_name = container_decl_ref.getNameInfo().getAsString();
  std::string replacement_text;

  // Special case: we detected and bound a zero offset (`&buf[0]`).
  // We need not emit a `.subspan(...)`.
  if (result.Nodes.getNodeAs<clang::IntegerLiteral>("zero_container_offset")) {
    replacement_text = container_name;
  } else {
    // Dance around the offset expression and emit one replacement on
    // either side of it:
    // `base::span<T>(container_decl_ref).subspan(` <offset> `)`

    // Ready and emit the first replacement; pull the replacement
    // range back to the opening bracket of the container.
    replacement_range.setEnd(
        container_decl_ref.getSourceRange().getBegin().getLocWithOffset(
            container_name.length() + 1u));
    const auto& contained_type = *GetNodeOrCrash<clang::QualType>(
        result, "contained_type",
        "`container_buff_address` implies `contained_type`");
    replacement_text = llvm::formatv(
        "base::span<{0}>({1}).subspan(",
        GetTypeAsString(contained_type, *result.Context), container_name);
    std::string replacement_directive = GetReplacementDirective(
        replacement_range, std::move(replacement_text), *result.SourceManager);
    EmitReplacement(key, replacement_directive);

    // Ready the second replacement; advance the replacement range to
    // the closing bracket (beyond the offset expression).
    if (const auto* container_subscript =
            result.Nodes.getNodeAs<clang::CXXOperatorCallExpr>(
                "container_subscript")) {
      // 1. implicit `this` arg and
      // 2. the subscript expression.
      if (container_subscript->getNumArgs() != 2u) {
        llvm::errs() << "\nError: matched `operator[]`, expected exactly two "
                        "args, but got "
                     << container_subscript->getNumArgs() << "!\n";
        DumpMatchResult(result);
        assert(false && "apparently bogus `operator[]`");
      }

      // Call `IgnoreImpCasts()` to see past the implicit promotion to
      // `...::size_type` and look at the "original" type of the
      // expression.
      RewriteExprForSubspan(container_subscript->getArg(1u)->IgnoreImpCasts(),
                            result, key);

      replacement_range = {
          container_subscript->getRParenLoc(),
          container_subscript->getRParenLoc().getLocWithOffset(1)};
    } else {
      // This is a C-style array.
      const auto& c_style_array_with_subscript =
          *GetNodeOrCrash<clang::ArraySubscriptExpr>(
              result, "c_style_array_with_subscript",
              "expected when `container_subscript` is not bound");
      replacement_range = {
          c_style_array_with_subscript.getEndLoc(),
          c_style_array_with_subscript.getEndLoc().getLocWithOffset(1)};
      const auto* subscript = GetNodeOrCrash<clang::Expr>(
          result, "c_style_array_subscript",
          "expected when `container_subscript` is not bound");
      RewriteExprForSubspan(subscript, result, key);
    }
    // Close the call to `.subspan()`.
    replacement_text = ")";
  }
  std::string replacement_directive = GetReplacementDirective(
      replacement_range, std::move(replacement_text), *result.SourceManager);
  EmitReplacement(key, replacement_directive);
}

// Handles code that passes address to a local variable as a single element
// buffer. Wrap it with a span of size=1. Tests are in
// single-element-buffer-original.cc.
static void EmitSingleVariableSpan(const std::string& key,
                                   const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();

  const auto* expr = result.Nodes.getNodeAs<clang::Expr>("address_expr");
  const auto* operand_decl = result.Nodes.getNodeAs<clang::DeclaratorDecl>(
      "address_expr_operand_decl");
  const auto* operand_expr =
      result.Nodes.getNodeAs<clang::Expr>("address_expr_operand");
  if (!expr || !operand_decl || !operand_expr) {
    llvm::errs()
        << "\n"
           "Error: EmitSingleVariableSpan() encountered an unexpected match.\n";
    DumpMatchResult(result);
    assert(false && "Unexpected match in EmitSingleVariableSpan()");
  }

  clang::SourceRange expr_range = {expr->getBeginLoc()};
  std::string type = GetTypeAsString(operand_decl->getType(), ast_context);
  std::string replacement_text = llvm::formatv("base::span<{0}, 1>(", type);
  EmitReplacement(key, GetReplacementDirective(expr_range, replacement_text,
                                               source_manager));
  EmitReplacement(
      key, GetReplacementDirective(
               getExprRange(operand_expr, source_manager, lang_opts).getEnd(),
               ", 1u)", source_manager));
}

// Rewrites unsafe third-party member function calls to helper macro calls.
//
// Example)
//     SkBitmap sk_bitmap;
//     uint32_t* image_row = sk_bitmap.getAddr32(x, y);
// will be rewritten to
//     base::span<uint32_t> image_row =
//         UNSAFE_SKBITMAP_GETADDR32(sk_bitmap, x, y);
// where the receiver expr "sk_bitmap" is moved into the macro call, and the
// macro performs essentially the following.
//     uint32_t* tmp_row = sk_bitmap.getAddr32(x, y);
//     int tmp_width = sk_bitmap.width();
//     base::span<uint32_t> image_row(tmp_row, tmp_width - x);
//
// Tests are in: unsafe-function-to-macro-original.cc and
// //base/containers/auto_spanification_helper_unittest.cc
static std::string GetNodeFromUnsafeCxxMethodCall(
    const clang::Expr* size_expr,
    const clang::CXXMemberCallExpr* member_call_expr,
    const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;

  const auto* method_decl = GetNodeOrCrash<clang::CXXMethodDecl>(
      result, "unsafe_function_decl",
      "`unsafe_function_call_expr` in clang::CXXMemberCallExpr implies "
      "`unsafe_function_decl` in clang::CXXMethodDecl");
  // The match with using `unsafeFunctionToBeRewrittenToMacro` guarantees that
  // there exists an `UnsafeCxxMethodToMacro` instance, so the following
  // "Find..." always succeeds.
  const UnsafeCxxMethodToMacro entry =
      FindUnsafeCxxMethodToBeRewrittenToMacro(method_decl).value();

  // A CXXMemberCallExpr must have a MemberExpr as the callee.
  const clang::MemberExpr* member_expr =
      clang::dyn_cast<clang::MemberExpr>(member_call_expr->getCallee());
  assert(member_expr);

  // `key` is compatible with getNodeFromSizeExpr.
  const std::string& key = NodeKey(size_expr, source_manager);

  // Rewrite a method call into a macro call in two steps. The total rewrite we
  // want is the following. Note that the receiver expression moves into the
  // argument list.
  //
  //     "receier.method(args...)" ==> "MACRO(receiver, args...)"
  //
  // Step 1) Prepend "MACRO(" to make it a macro call.
  //         "receiver.method(args...)"
  //     ==> "MACRO(" + "receiver.method(args...)"
  //
  // Step 2) Replace ".method(" with ", " to make a new argument list including
  //     the receiver expression.
  //         "receiver" + ".method(" + "args...)"
  //     ==> "receiver" + ", " + "args...)"
  //
  // The open parenthesis of the argument list is moved from the right after
  // "method" to the right after "MACRO" while the close parenthesis doesn't
  // change.
  //
  // The arrow operator "->" is supported in the same way as the dot operator
  // ".".
  EmitReplacement(  // Step 1
      key, GetReplacementDirective(
               member_call_expr->getImplicitObjectArgument()->getBeginLoc(),
               llvm::formatv("{0}(", entry.macro_name), source_manager));
  const bool has_arg = member_call_expr->getNumArgs() > 0;
  EmitReplacement(  // Step 2
      key,
      GetReplacementDirective(
          clang::SourceRange(member_expr->getOperatorLoc(),  // "." or "->"
                             has_arg
                                 ? member_call_expr->getArg(0)->getBeginLoc()
                                 : member_call_expr->getRParenLoc()),
          has_arg ? ", " : "", source_manager));

  EmitReplacement(
      key, GetIncludeDirective(size_expr->getSourceRange(), source_manager,
                               kBaseAutoSpanificationHelperIncludePath));
  EmitSink(key);
  return key;
}

// Rewrites unsafe third-party free function calls to helper macro calls.
//
// Example)
//     struct hb_glyph_position_t* positions =
//         hb_buffer_get_glyph_positions(&buffer, &length);
// will be rewritten to
//     base::span<hb_glyph_position_t> positions =
//         UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS(&buffer, &length);
// where the macro performs essentially the following.
//     hb_glyph_position_t* tmp_pos =
//         hb_buffer_get_glyph_positions(&buffer, &length);
//     base::span<hb_glyph_position_t> positions(tmp_pos, length);
//
// Tests are in: unsafe-function-to-macro-original.cc and
// //base/containers/auto_spanification_helper_unittest.cc
static std::string GetNodeFromUnsafeFreeFuncCall(
    const clang::Expr* size_expr,
    const clang::CallExpr* call_expr,
    const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;

  const auto* function_decl = GetNodeOrCrash<clang::FunctionDecl>(
      result, "unsafe_function_decl",
      "`unsafe_function_call_expr` implies `unsafe_function_decl`");
  // The match with using `unsafeFunctionToBeRewrittenToMacro` guarantees that
  // there exists an `UnsafeFreeFuncToMacro` instance, so the following
  // "Find..." always succeeds.
  const UnsafeFreeFuncToMacro entry =
      FindUnsafeFreeFuncToBeRewrittenToMacro(function_decl).value();

  // `key` is compatible with getNodeFromSizeExpr.
  const std::string& key = NodeKey(size_expr, source_manager);

  // Replace the function name with the macro name.
  const clang::SourceLocation& func_loc = call_expr->getCallee()->getBeginLoc();
  EmitReplacement(
      key, GetReplacementDirective(
               clang::SourceRange(func_loc, func_loc.getLocWithOffset(
                                                entry.function_name.length())),
               std::string(entry.macro_name), source_manager));

  EmitReplacement(
      key, GetIncludeDirective(size_expr->getSourceRange(), source_manager,
                               kBaseAutoSpanificationHelperIncludePath));
  EmitSink(key);
  return key;
}

static std::string GetNodeFromUnsafeFunctionCall(
    const clang::Expr* size_expr,
    const clang::CallExpr* call_expr,
    const MatchFinder::MatchResult& result) {
  if (const clang::CXXMemberCallExpr* member_call_expr =
          clang::dyn_cast<clang::CXXMemberCallExpr>(call_expr)) {
    return GetNodeFromUnsafeCxxMethodCall(size_expr, member_call_expr, result);
  }
  return GetNodeFromUnsafeFreeFuncCall(size_expr, call_expr, result);
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
  } else if (result.Nodes.getNodeAs<clang::Expr>("address_expr")) {
    // This case occurs when an address to a variable is used as a buffer:
    //
    //   void UsesBarAsFloatBuffer(size_t size, float* bar);
    //   float bar = 3.0;
    //   UsesBarAsFloatBuffer(1, &bar);
    //
    // In this case, we will rewrite `&bar` to `base::span<float, 1>(&bar)`.
    EmitSingleVariableSpan(key, result);
  }
  if (result.Nodes.getNodeAs<clang::UnaryOperator>("container_buff_address")) {
    EmitContainerPointerRewrites(result, key);
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

  const std::string& array_decl_as_string =
      result.Nodes.getNodeAs<clang::DeclaratorDecl>("rhs_begin")
          ->getNameAsString();

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
  std::string replacement_text = llvm::formatv(
      "({0}.size() * sizeof(decltype({0})::value_type))", array_decl_as_string);
  std::string replacement_directive = GetReplacementDirective(
      replacement_range, std::move(replacement_text), source_manager);

  EmitReplacement(GetRHS(result), replacement_directive);
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
  auto rep_range = clang::SourceRange(getSourceRange(result).getEnd());

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
  std::string replacement_text = ".data()";

  if (result.Nodes.getNodeAs<clang::Expr>("unaryOperator")) {
    // Insert enclosing parenthesis for expressions with UnaryOperators
    auto begin_range = clang::SourceRange(getSourceRange(result).getBegin());
    EmitFrontier(lhs_key, rhs_key,
                 GetReplacementDirective(begin_range, "(", source_manager));
    replacement_text = ").data()";
  }

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
  // Now we need to remove the '_'s from the string.
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
    const clang::DeclaratorDecl* array_decl,
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
    bool has_definition = array_decl->getSourceRange().fullyContains(
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

static const clang::Expr* GetInitExpr(const clang::DeclaratorDecl* decl) {
  const clang::Expr* init_expr = nullptr;
  if (auto* var_decl = clang::dyn_cast_or_null<clang::VarDecl>(decl)) {
    init_expr = var_decl->getInit();
  } else if (auto* field_decl =
                 clang::dyn_cast_or_null<clang::FieldDecl>(decl)) {
    init_expr = field_decl->getInClassInitializer();
  }
  return init_expr;
}

// Returns an initializer list(`initListExpr`) of the given
// `decl`(`clang::VarDecl` OR `clang::FieldDecl`) if exists. Otherwise, returns
// `nullptr`.
const clang::InitListExpr* GetArrayInitList(const clang::DeclaratorDecl* decl) {
  const clang::Expr* init_expr = GetInitExpr(decl);
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

static bool IsMutable(const clang::DeclaratorDecl* decl) {
  if (const auto* field_decl =
          clang::dyn_cast_or_null<clang::FieldDecl>(decl)) {
    return field_decl->isMutable();
  }
  return false;
}

static bool IsConstexpr(const clang::DeclaratorDecl* decl) {
  if (const auto* var_decl = clang::dyn_cast_or_null<clang::VarDecl>(decl)) {
    return var_decl->isConstexpr();
  }
  return false;
}

static bool IsInlineVarDecl(const clang::DeclaratorDecl* decl) {
  if (const auto* var_decl = clang::dyn_cast_or_null<clang::VarDecl>(decl)) {
    return var_decl->isInlineSpecified();
  }
  return false;
}

static bool IsStaticLocalOrStaticStorageClass(
    const clang::DeclaratorDecl* decl) {
  if (const auto* var_decl = clang::dyn_cast_or_null<clang::VarDecl>(decl)) {
    return var_decl->isStaticLocal() ||
           var_decl->getStorageClass() == clang::SC_Static;
  }
  return false;
}

// This function handles local c-style array variables and field decls.
// It creates a proxy_node (marked as a sink) that all other nodes are linked
// to.
// In the case of an unsafe buffer access on the array decl, it emits the
// necessary replacements to rewrite the array type to a std::array.
std::string getNodeFromArrayDecl(const clang::TypeLoc* type_loc,
                                 const clang::DeclaratorDecl* array_decl,
                                 const clang::ArrayType* array_type,
                                 const MatchFinder::MatchResult& result) {
  clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;

  // C-style arrays only need to be rewritten when there's an unsafe buffer
  // access on the array decl itself.
  // Example1:
  //  int a[10]; // => a is never directly used with an unsafe access
  //             // => no need to rewrite a to std::array.
  //  int* b = a;
  //  b[UnsafeIndex()]; // => b is used as an usafe buffer
  //                    // => rewrite b's type to base::span.
  // Example 2:
  //  int a[10];        // => a is directly used with an unsafe access
  //  a[UnsafeIndex()]; // => rewrite a's type to std::array.
  //  int* b = a;
  //  b[UnsafeIndex()]; // => b is used as an usafe buffer
  //                    // => rewrite b's type to base::span.
  // Example 3:
  //  struct Foo {
  //    int a[10];
  //  };
  //  Foo foo;
  //  foo.a[UnsafeIndex()]; // => Foo::a is directly used as an unsafe buffer
  //                        // => rewrite Foo::a's type to std::array.
  //
  // To handle this:
  //   - We create a proxy_node for array decl.
  //   - All other nodes are linked to the proxy node.
  //   - The proxy_node is marked as a sink.
  // In the case of unsafe_buffer_access on the array decl:
  //   - We create an edge from the node n to the proxy_node(n -> proxy_node).
  //   - Emit n as a source node.
  //   - This leads n to be rewritten since the proxy_node is a sink.
  auto proxy_node = NodeKeyFromRange(
      clang::SourceRange(array_decl->getBeginLoc(), array_decl->getBeginLoc()),
      *result.SourceManager);
  EmitSink(proxy_node);
  if (!result.Nodes.getNodeAs<clang::Expr>("unsafe_buffer_access")) {
    // Early return to avoid uselessly determining the array replacement.
    return proxy_node;
  }

  // Unsafe Buffer Access => Need to rewrite the array's type.
  std::string key = NodeKey(array_decl, *result.SourceManager);
  EmitEdge(key, proxy_node);
  EmitSource(key);

  const clang::ArrayTypeLoc& array_type_loc =
      type_loc->getUnqualifiedLoc().getAs<clang::ArrayTypeLoc>();
  assert(!array_type_loc.isNull());
  const std::string& array_variable_as_string = array_decl->getNameAsString();
  const std::string& array_size_as_string =
      GetArraySize(array_type_loc, source_manager, ast_context);
  const clang::QualType& original_element_type = array_type->getElementType();

  std::stringstream qualifier_string;
  if (IsInlineVarDecl(array_decl)) {
    qualifier_string << "inline ";
  }
  if (IsMutable(array_decl)) {
    // While 'mutable' is a storage class specifier, include it with other
    // declaration specifiers that precede the type in source code.
    qualifier_string << "mutable ";
  }
  if (IsStaticLocalOrStaticStorageClass(array_decl)) {
    qualifier_string << "static ";
  }
  if (IsConstexpr(array_decl)) {
    qualifier_string << "constexpr ";
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
      !IsConstexpr(array_decl)) {
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
      new_element_type, array_decl, array_variable_as_string, ast_context);
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

  const clang::InitListExpr* init_list_expr = GetArrayInitList(array_decl);
  const clang::StringLiteral* init_string_literal =
      clang::dyn_cast_or_null<clang::StringLiteral>(GetInitExpr(array_decl));

  //   static const char* array[] = {...};
  //   |            |
  //   |            +-- type_loc->getSourceRange().getBegin()
  //   |
  //   +---- array_decl->getSourceRange().getBegin()
  //
  // The `static` is a part of `VarDecl`, but the `const` is a part of
  // the element type, i.e. `const char*`.
  //
  // The array must be rewritten into:
  //
  //   static auto array = std::to_array<const char*>({...});
  //
  // So the `replacement_range` needs to include the `const` and
  // `init_list_expr` if any.
  clang::SourceRange replacement_range = {
      array_decl->getSourceRange().getBegin(),
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

  // All the other replacements are tied to the proxy_node.
  return proxy_node;
}

std::string getArrayNode(bool is_lhs, const MatchFinder::MatchResult& result) {
  std::string array_type_loc_id =
      (is_lhs) ? "lhs_array_type_loc" : "rhs_array_type_loc";
  std::string begin_id = (is_lhs) ? "lhs_begin" : "rhs_begin";
  std::string array_type_id = (is_lhs) ? "lhs_array_type" : "rhs_array_type";

  auto* type_loc = result.Nodes.getNodeAs<clang::TypeLoc>(array_type_loc_id);
  if (auto* array_param =
          result.Nodes.getNodeAs<clang::ParmVarDecl>(begin_id)) {
    return getNodeFromFunctionArrayParameter(type_loc, array_param, result);
  }

  auto* array_decl = result.Nodes.getNodeAs<clang::DeclaratorDecl>(begin_id);
  auto* array_type = result.Nodes.getNodeAs<clang::ArrayType>(array_type_id);
  if (array_decl) {
    return getNodeFromArrayDecl(type_loc, array_decl, array_type, result);
  }
  // Not supposed to get here.
  llvm::errs() << "\n"
                  "Error: getArrayNode() encountered an unexpected match.\n"
                  "Expected a clang::DeclaratorDecl \n";
  DumpMatchResult(result);
  assert(false && "Unexpected match in getArrayNode()");
}

// Spanifies the matched function parameter/return type, and connects relevant
// function declarations (forward declarations and overridden methods) to each
// other bidirectionally per the matched function parameter/return type. Note
// that a function definition is a function declaration by definition.
// Tests are in: fct-decl-tests-original.cc
//
// Example) Given the following C++ code,
//
//   void F(short* arg1, long* arg2);         // [1] First declaration
//   void F(short* arg1, long* arg2) { ... }  // [2] Second declaration
//   // Only arg1 is connected to a source and sinks.
//
// we build the following node graph:
//
//   node_arg1_1st <==> replace_arg1_1st
//         ^|
//         ||
//         |v
//   node_arg1_2nd <==> replace_arg1_2nd <==> a source-to-sink graph
//
//   node_arg2_1st <==> replace_arg2_1st
//         ^|
//         ||
//         |v
//   node_arg2_2nd <==> replace_arg2_2nd
//
// where
//
//   replace_arg1_1st = `replacement_key` for arg1 at [1]
//                    = GetRHS(arg1 at [1])
//   replace_arg1_2nd = `replacement_key` for arg1 at [2]
//                    = GetRHS(arg1 at [2])
//   node_arg1_1st = `previous_key`
//                 = NodeKey(F at [1], source_manager, "1-th parm type")
//   node_arg1_2nd = `current_key`
//                 = NodeKey(F at [2], source_manager, "1-th parm type")
//   and the same for arg2.
//   (`var` is a local variable name in the implementation.)
//
// Then, arg1 will be rewritten while arg2 will not be rewritten because only
// the arg1 graph is connected to a source-to-sink graph.
//
// Q: Why do we create node_arg1_{1st,2nd} in addition to
// replace_arg1_{1st,2nd}? Does the following graph suffice?
//
//   replace_arg1_1st <==> a source-to-sink graph
//         ^|
//         ||
//         |v
//   replace_arg1_2nd
//
// A: Yes, it does suffice. But it's hard to build because GetRHS takes
// `result` as the argument. When we find a match for arg1 at [2], we no longer
// have `result` for arg1 at [1]. It's easier to create node_arg1_{1st,2nd] than
// saving the results of GetRHS somewhere and retrieving it.
void RewriteFunctionParamAndReturnType(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::FunctionDecl* fct_decl =
      result.Nodes.getNodeAs<clang::FunctionDecl>("fct_decl");

  // This node spanifies the matched function parameter/return type.
  const std::string& replacement_key = GetRHS(result);

  // `parm_or_return_id` (passed in to NodeKey() as `optional_seed` argument) is
  // used to identify the matched parameter/return type so that the spanifier
  // tool can partially spanify some of (not necessarily all of) function
  // parameter types and return type.
  //
  // With the example in the function header comment, we'd like to build two
  // independent graphs for arg1 and arg2.
  //
  // Note: It's easier to make a unique node key from `fct_decl` +
  // `parm_or_return_id` than making a unique node key from the clang::Decl
  // that matches the function parameter/return type of each forward
  // declaration or overridden method.
  std::string parm_or_return_id;
  if (const clang::ParmVarDecl* parm_var_decl =
          result.Nodes.getNodeAs<clang::ParmVarDecl>("rhs_begin")) {
    parm_or_return_id = llvm::formatv("{0}-th parm type",
                                      parm_var_decl->getFunctionScopeIndex());
  } else {
    parm_or_return_id = "return type";
  }

  // `current_key` (node_arg1_2nd in the example in the function header comment)
  // is just a helper node to be identical to `replacement_key`, so connect them
  // bi-directionally to each other.
  const std::string& current_key =
      NodeKey(fct_decl, source_manager, parm_or_return_id);
  EmitEdge(current_key, replacement_key);
  EmitEdge(replacement_key, current_key);

  // Connect to the previous function decl, which is already connected to the
  // previous previous function decl.
  if (const clang::Decl* previous_decl = fct_decl->getPreviousDecl()) {
    const std::string& previous_key =
        NodeKey(previous_decl, source_manager, parm_or_return_id);
    if (raw_ptr_plugin::isNodeInThirdPartyLocation(*previous_decl,
                                                   source_manager)) {
      // A declaration in third party codebase is found, so we do not want to
      // rewrite the parameter/return type in a third party function. This one-
      // way edge prevents making a flow from a source to a sink, hence the
      // rewriting will be cancelled.
      //
      // Example)
      //
      //   node_arg1_1st (No replace_arg1_1st because it's in third_party/)
      //         ^
      //         | (one-way edge)
      //         |
      //   node_arg1_2nd <==> replace_arg1_2nd <==> a source-to-sink graph
      //
      // where node_arg1_1st is not a sink node, so the source node reaches a
      // non-sink end node. Hence, the rewriting will be cancelled.
      EmitEdge(current_key, previous_key);
    } else {
      EmitEdge(current_key, previous_key);
      EmitEdge(previous_key, current_key);
    }
  }

  // Connect to the overridden methods.
  if (const clang::CXXMethodDecl* method_decl =
          clang::dyn_cast<clang::CXXMethodDecl>(fct_decl)) {
    for (auto* overridden_method_decl : method_decl->overridden_methods()) {
      const std::string& overridden_method_key =
          NodeKey(overridden_method_decl, source_manager, parm_or_return_id);
      if (raw_ptr_plugin::isNodeInThirdPartyLocation(*overridden_method_decl,
                                                     source_manager)) {
        // A declaration in third party codebase is found, so we do not want to
        // rewrite the parameter/return type in a third party function. This
        // one-way edge prevents making a flow from a source to a sink, hence
        // the rewriting will be cancelled.
        EmitEdge(current_key, overridden_method_key);
      } else {
        EmitEdge(current_key, overridden_method_key);
        EmitEdge(overridden_method_key, current_key);
      }
    }
  }
}

// Extracts the lhs node from the match result.
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

  if (result.Nodes.getNodeAs<clang::TypeLoc>("lhs_array_type_loc")) {
    return getArrayNode(/*is_lhs=*/true, result);
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
                  "  - lhs_array_type_loc\n"
                  "  - lhs_begin\n"
                  "\n";
  DumpMatchResult(result);
  assert(false && "Unexpected match in getLHS()");
}

// Extracts the rhs node from the match result.
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

  if (result.Nodes.getNodeAs<clang::TypeLoc>("rhs_array_type_loc")) {
    return getArrayNode(/*is_lhs=*/false, result);
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
    // "size_node" assumes that third party functions that return a buffer
    // provide some way to know the size, however special handling is required
    // to extract that, thus here we add support for functions returning a
    // buffer that also have size support.
    if (const auto* unsafe_call_expr = result.Nodes.getNodeAs<clang::CallExpr>(
            "unsafe_function_call_expr")) {
      return GetNodeFromUnsafeFunctionCall(size_expr, unsafe_call_expr, result);
    }
    return getNodeFromSizeExpr(size_expr, result);
  }

  // Not supposed to get here.
  llvm::errs() << "\n"
                  "Error: getRHS() encountered an unexpected match.\n"
                  "Expected one of : \n"
                  "  - rhs_type_loc\n"
                  "  - rhs_raw_ptr_type_loc\n"
                  "  - rhs_array_type_loc\n"
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

class ExprVisitor
    : public clang::ast_matchers::internal::BoundNodesTreeBuilder::Visitor {
 public:
  void visitMatch(
      const clang::ast_matchers::BoundNodes& BoundNodesView) override {
    assert(expr_ == nullptr &&
           "Encountered more than one expression with match id 'LHS'.");
    expr_ = BoundNodesView.getNodeAs<clang::Expr>("LHS");
  }
  const clang::Expr* expr_ = nullptr;
};
const clang::Expr* FindLHSExpr(
    clang::ast_matchers::internal::BoundNodesTreeBuilder& matches) {
  ExprVisitor v;
  matches.visitMatches(&v);
  return v.expr_;
}

// This allows us to unpack binaryOperations recursively until we reach the node
// matching InnerMatcher. This is necessary to handle expressions of the form:
//    buf + expr1 - expr2 + expr3;
// which need to be rewritten to:
//    buf.subspan(expr1 - expr2 + expr3);
AST_MATCHER_P(clang::Expr,
              binary_plus_or_minus_operation,
              clang::ast_matchers::internal::Matcher<clang::Expr>,
              InnerMatcher) {
  auto bin_op_matcher = expr(ignoringParenCasts(
      binaryOperation(anyOf(hasOperatorName("+"), hasOperatorName("-")),
                      hasLHS(expr(binaryOperation(anyOf(hasOperatorName("+"),
                                                        hasOperatorName("-"))))
                                 .bind("LHS")))));

  clang::ast_matchers::internal::BoundNodesTreeBuilder matches;
  if (bin_op_matcher.matches(Node, Finder, &matches)) {
    const clang::Expr* n = FindLHSExpr(matches);
    auto matcher = binary_plus_or_minus_operation(InnerMatcher);
    return matcher.matches(*n, Finder, Builder);
  }
  return InnerMatcher.matches(Node, Finder, Builder);
}

class Spanifier {
 public:
  explicit Spanifier(MatchFinder& finder) : match_finder_(finder) {
    // `raw_ptr` or `span` should not have `.data()` applied.
    auto frontier_exclusions = anyOf(
        isExpansionInSystemHeader(), raw_ptr_plugin::isInExternCContext(),
        raw_ptr_plugin::isInThirdPartyLocation(),
        raw_ptr_plugin::isInGeneratedLocation(),
        raw_ptr_plugin::ImplicitFieldDeclaration(),
        raw_ptr_plugin::isInMacroLocation(),
        raw_ptr_plugin::isInLocationListedInFilterFile(&paths_to_exclude_));

    // Standard exclusions include `raw_ptr` and `span`.
    auto exclusions = anyOf(
        frontier_exclusions,
        hasAncestor(cxxRecordDecl(anyOf(hasName("raw_ptr"), hasName("span")))));

    // Exclude literal strings as these need to become string_view
    auto pointer_type = pointerType(pointee(qualType(unless(
        anyOf(qualType(hasDeclaration(
                  cxxRecordDecl(raw_ptr_plugin::isAnonymousStructOrUnion()))),
              hasUnqualifiedDesugaredType(
                  anyOf(functionType(), memberPointerType(), voidType())),
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
              hasDescendant(raw_ptr_type_loc.bind("lhs_raw_ptr_type_loc"))),
        hasTypeLoc(loc(qualType(arrayType().bind("lhs_array_type")))
                       .bind("lhs_array_type_loc")));

    auto rhs_type_loc = anyOf(
        hasType(pointer_type),
        allOf(hasType(raw_ptr_type),
              hasDescendant(raw_ptr_type_loc.bind("rhs_raw_ptr_type_loc"))),
        hasTypeLoc(loc(qualType(arrayType())).bind("rhs_array_type_loc")));

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

    auto lhs_var =
        varDecl(lhs_type_loc, unless(anyOf(exclusions, hasExternalStorage())))
            .bind("lhs_begin");

    auto rhs_var =
        varDecl(rhs_type_loc, unless(anyOf(exclusions, hasExternalStorage())))
            .bind("rhs_begin");

    auto lhs_param =
        parmVarDecl(lhs_type_loc, unless(exclusions)).bind("lhs_begin");

    auto rhs_param =
        parmVarDecl(rhs_type_loc, unless(exclusions)).bind("rhs_begin");

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

    // Matches statements of the form: &buf[n] where buf is a container type
    // (span, std::vector, std::array, C-style array...).
    auto buff_address_from_container =
        unaryOperator(
            hasOperatorName("&"),
            hasUnaryOperand(anyOf(
                cxxOperatorCallExpr(
                    callee(functionDecl(
                        hasName("operator[]"),
                        hasParent(cxxRecordDecl(hasMethod(hasName("size")))))),
                    hasDescendant(
                        declRefExpr(
                            to(varDecl(hasType(classTemplateSpecializationDecl(
                                hasTemplateArgument(
                                    0, refersToType(qualType().bind(
                                           "contained_type"))))))))
                            .bind("container_decl_ref")),
                    optionally(
                        hasDescendant(integerLiteral(equals(0u))
                                          .bind("zero_container_offset"))))
                    .bind("container_subscript"),
                arraySubscriptExpr(
                    hasBase(
                        declRefExpr(to(varDecl(hasType(arrayType(hasElementType(
                                        qualType().bind("contained_type")))))))
                            .bind("container_decl_ref")),
                    hasIndex(expr().bind("c_style_array_subscript")),
                    optionally(hasIndex(integerLiteral(equals(0u))
                                            .bind("zero_container_offset"))))
                    .bind("c_style_array_with_subscript"))))
            .bind("container_buff_address");

    // T* a = buf.data();
    auto member_data_call =
        cxxMemberCallExpr(
            callee(functionDecl(
                hasName("data"),
                hasParent(cxxRecordDecl(hasMethod(hasName("size")))))),
            has(memberExpr().bind("data_member_expr")))
            .bind("member_data_call");

    // Matchers |&var| where |var| is a local variable, a parameter or member
    // field. Doesn't match when |var| is a function.
    auto buff_address_from_single_var =
        unaryOperator(
            hasOperatorName("&"),
            hasUnaryOperand(anyOf(
                declRefExpr(to(anyOf(varDecl(unless(exclusions))
                                         .bind("address_expr_operand_decl"),
                                     parmVarDecl(unless(exclusions))
                                         .bind("address_expr_operand_decl"))))
                    .bind("address_expr_operand"),
                memberExpr(member(fieldDecl(unless(exclusions))
                                      .bind("address_expr_operand_decl")))
                    .bind("address_expr_operand"))))
            .bind("address_expr");

    // Defines nodes that contain size information, these include:
    //  - nullptr => size is zero
    //  - calls to new/new[n] => size is 1/n
    //  - calls to third_party functions that we can't rewrite (they should
    //    provide a size for the pointer returned)
    //  - address to local variable (e.g. `&foo`) => size is 1
    // TODO(353710304): Consider handling functions taking in/out args ex:
    //                  void alloc(**ptr);
    // TODO(353710304): Consider making member_data_call and size_node mutually
    //                  exclusive. We rely here on the ordering of expressions
    //                  in the anyOf matcher to first match member_data_call
    //                  which is a subset of size_node.
    auto size_node_matcher = expr(anyOf(
        member_data_call,
        expr(anyOf(callExpr(
                       callee(functionDecl(unsafeFunctionToBeRewrittenToMacro())
                                  .bind("unsafe_function_decl")))
                       .bind("unsafe_function_call_expr"),
                   callExpr(callee(functionDecl(
                       hasReturnTypeLoc(pointerTypeLoc()),
                       anyOf(raw_ptr_plugin::isInThirdPartyLocation(),
                             isExpansionInSystemHeader(),
                             raw_ptr_plugin::isInExternCContext())))),
                   cxxNullPtrLiteralExpr().bind("nullptr_expr"), cxxNewExpr(),
                   buff_address_from_container, buff_address_from_single_var))
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
                 binaryOperation(
                     binary_plus_or_minus_operation(binaryOperation(
                         hasLHS(rhs_expr), hasOperatorName("+"),
                         unless(raw_ptr_plugin::isInMacroLocation()))),
                     hasRHS(expr(hasType(isInteger())).bind("binary_op_rhs")),
                     unless(hasParent(binaryOperation(
                         anyOf(hasOperatorName("+"), hasOperatorName("-"))))))
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

    // Expressions used to decide the pointer/array is unsafely used as a
    // buffer including:
    //  expr[n], expr++, ++expr, expr + n, expr += n
    auto unsafe_buffer_access = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(ignoringParenCasts(anyOf(
                 // Unsafe pointer subscript:
                 arraySubscriptExpr(hasLHS(lhs_expr_variations),
                                    unless(isSafeArraySubscript())),
                 // Unsafe pointer arithmetic:
                 binaryOperation(
                     anyOf(hasOperatorName("+="), hasOperatorName("+")),
                     hasLHS(lhs_expr_variations)),
                 unaryOperator(hasOperatorName("++"),
                               hasUnaryOperand(lhs_expr_variations)),
                 // Unsafe base::raw_ptr arithmetic:
                 cxxOperatorCallExpr(anyOf(hasOverloadedOperatorName("[]"),
                                           hasOperatorName("++")),
                                     hasArgument(0, lhs_expr_variations)))))
            .bind("unsafe_buffer_access"));
    Match(unsafe_buffer_access, [](const auto& result) {
      EmitSource(GetLHS(result));  // Declare unsafe buffer access.
    });

    // `sizeof(c_array)` is rewritten to
    // `std_array.size() * sizeof(element_size)`.
    auto sizeof_array_expr = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        sizeOfExpr(has(rhs_exprs_without_size_nodes)).bind("sizeof_expr"));
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

    auto rhs_exprs_without_size_nodes_ignoring_non_spelled_nodes =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 expr(rhs_exprs_without_size_nodes));
    auto raw_ptr_op_bool = cxxMemberCallExpr(
        on(expr().bind("boolean_op_operand")),
        callee(cxxMethodDecl(hasName("operator bool"),
                             ofClass(hasName("raw_ptr")))),
        has(memberExpr(has(expr(ignoringParenCasts(
            rhs_exprs_without_size_nodes_ignoring_non_spelled_nodes))))));
    // Handles boolean operations that need to be adapted after a span rewrite.
    //   if(expr) => if(!expr.empty())
    //   if(!expr) => if(expr.empty())
    // Notice here that the implicit cast part of the expression is traversed
    // using the default traversal mode `clang::TK_AsIs`, while the expression
    // variation matcher is traversed using
    // `clang::TK_IgnoreUnlessSpelledInSource`. The traversal mode
    // `clang::TK_IgnoreUnlessSpelledInSource`, while very useful in simplifying
    // the matchers, wouldn't detect boolean operations on pointers hence the
    // need for a hybrid traversal mode in this matcher.
    auto boolean_op = expr(
        anyOf(
            implicitCastExpr(
                hasCastKind(clang::CastKind::CK_PointerToBoolean),
                hasSourceExpression(
                    expr(
                        rhs_exprs_without_size_nodes_ignoring_non_spelled_nodes)
                        .bind("boolean_op_operand"))),
            raw_ptr_op_bool),
        optionally(hasParent(
            unaryOperator(hasOperatorName("!")).bind("logical_not_op"))));
    Match(boolean_op, DecaySpanToBooleanOp);

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
    // See test: 'array-external-call-original.cc'
    //
    // TODO(crbug.com/419598098): we had trouble exercising the "add
    // `.data()` to frontier calls" logic in our test harness. This
    // might imply that the exclude logic is broken or works differently
    // from prod. If we could figure this out, we could test it.
    auto buffer_to_external_func = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(anyOf(
            callExpr(callee(functionDecl(
                         frontier_exclusions,
                         unless(matchesName(
                             "std::(size|begin|end|empty|swap|ranges::)")))),
                     forEachArgumentWithParam(
                         expr(rhs_exprs_without_size_nodes), parmVarDecl())),
            cxxConstructExpr(
                hasDeclaration(cxxConstructorDecl(frontier_exclusions)),
                forEachArgumentWithParam(expr(rhs_exprs_without_size_nodes),
                                         parmVarDecl())))));
    Match(buffer_to_external_func, AppendDataCall);

    // Handles expressions of the form:
    // a + m, a + n + m, ...
    // which need to be rewritten to:
    // a.subspan(m), a.subspan(n + m), ...
    // These expressions always appear on the right-hand side.
    // Consider the following example:
    // lhs_expr = rhs_expr + offset_expr
    //            ^--------------------^ = BinaryOperation
    //            ^------^               = BinaryOperations' LHS expr
    //                       ^---------^ = BinaryOperation's RHS expr
    //                     ^             = BinaryOperation's Operator
    // Note that BinaryOperations's LHS and RHS expressions refer to what's
    // before and after the binary operator (+) (Not to be confused with
    // lhs_expr and rhs_expr).
    auto binary_op = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(ignoringParenCasts(binaryOperation(
            binary_plus_or_minus_operation(
                binaryOperation(hasLHS(rhs_expr), hasOperatorName("+"),
                                hasRHS(expr(hasType(isInteger()))),
                                unless(raw_ptr_plugin::isInMacroLocation()))
                    .bind("binary_operation")),
            hasRHS(expr().bind("binary_op_rhs")),
            unless(hasParent(binaryOperation(
                anyOf(hasOperatorName("+"), hasOperatorName("-")))))))));
    Match(binary_op, AdaptBinaryOperation);

    // Handles expressions of the form:
    // expr += offset_expr;
    // which is equivalent to:
    // lhs_expr = rhs_expr + offset_expr (Note: lhs_expr == rhs_expr)
    auto binary_plus_eq_op = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(ignoringParenCasts(binaryOperation(
                 hasLHS(rhs_expr), hasOperatorName("+="),
                 hasRHS(expr(hasType(isInteger())).bind("binary_op_RHS")))))
            .bind("binary_plus_eq_op"));
    Match(binary_plus_eq_op, AdaptBinaryPlusEqOperation);

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
    Match(fct_decls_params, RewriteFunctionParamAndReturnType);

    auto fct_decls_returns = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        functionDecl(hasReturnTypeLoc(pointerTypeLoc().bind("rhs_type_loc")),
                     unless(exclusions))
            .bind("fct_decl"));
    Match(fct_decls_returns, RewriteFunctionParamAndReturnType);
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

  MatchFinder match_finder;
  Spanifier rewriter(match_finder);

  // Prepare and run the tool.
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(factory.get());

  return result;
}
