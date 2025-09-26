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
#include <variant>
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

// Specifies how `EmitContainerPointerRewrites()` should behave.
enum class ContainerPointerRewritesMode {
  // When `container` is not (and will not be) a span, but is fed into a
  // spanified context (e.g. a spanified function), `container` must be
  // wrapped in `base::span()`.
  kWrapWithBaseSpan,

  // When `container` will be a span, but is positioned on a frontier
  // (and must have `.data()` appended), `container` should not be
  // re-wrapped in `base::span`.
  kDontWrapWithBaseSpan,
};

// Forward declarations
std::string GetArraySize(const clang::ArrayTypeLoc& array_type_loc,
                         const clang::SourceManager& source_manager,
                         const clang::ASTContext& ast_context);
clang::SourceLocation EmitContainerPointerRewrites(
    const MatchFinder::MatchResult& result,
    std::string_view key,
    ContainerPointerRewritesMode mode);

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

// Precedence values for EmitReplacement.
//
// The `extract_edits.py` script sorts multiple insertions at the same code
// location by these precedence values in ascending numerical order.
//
// Paired insertions (e.g., an opening and its corresponding closing bracket)
// typically use a precedence of `+K` for the "opening" part and `-K` for the
// "closing" part, where K is one of the constants defined below. This is
// because, for a given position, we usually want to close the bracket before
// opening a new one. A higher precedence value is used when the replacement
// has a higher tie with the expression.
enum Precedence {
  kNeutralPrecedence = 0,

  // Lower priority (weaker ties to the target)
  kAppendDataCallPrecedence,
  kDecaySpanToPointerPrecedence,
  kAdaptBinaryOperationPrecedence,
  kEmitSingleVariableSpanPrecedence,
  kAdaptBinaryPlusEqOperationPrecedence,
  kRewriteUnaryOperationPrecedence,
  // Higher priority (stronger ties to the target)
};

// Returns true if the `loc` is inside a macro expansion, except for the case
// that the `loc` is at a macro argument of the exceptional macros (EXPECT_ and
// ASSERT_ family).
// Tests are in: gtest-macro-original.cc
bool IsInExcludedMacro(clang::SourceLocation loc,
                       const clang::ASTContext& ast_context,
                       const clang::SourceManager& source_manager) {
  if (!loc.isMacroID()) [[likely]] {
    return false;
  }

  // Get the outermost macro name which takes a macro argument at `loc`. Macros
  // are often implemented with nested macros, and the outermost macro is the
  // most interesting for us.
  //
  // Example:
  //     #define TOP_MACRO(arg) do { INNER_MACRO(arg); } while (false)
  //     #define INNER_MACRO(arg2) arg2 += 1
  //     TOP_MACRO(var);
  // then, the macro expansion will be
  //     do { var += 1; } while (false);
  // When `loc` is at "var" (of "var += 1"), we're interested in the macro name
  // "TOP_MACRO" rather than "INNER_MACRO".
  std::string outermost_macro_name;
  while (source_manager.isMacroArgExpansion(loc)) {
    outermost_macro_name = std::string(clang::Lexer::getImmediateMacroName(
        loc, source_manager, ast_context.getLangOpts()));
    loc = source_manager.getImmediateSpellingLoc(loc);
  }

  if (loc.isMacroID()) {
    // This branch handles the following case:
    //     #define EXPECT_TRUE(expect_arg) if (expect_arg) ; else Crash()
    //     #define MY_MACRO() EXPECT_TRUE(immediate_value)
    //     MY_MACRO();
    // The macro expansion will be:
    //     if (immediate_value) ; else Crash();
    // When (the original value of) `loc` was at "immediate_value" (of
    // "if (immediate_value)"), it was inside a macro expansion of MY_MACRO,
    // which should be excluded (at least for now).
    return true;
  }

  // When `loc` is at a macro argument of the following macros, we'll attempt
  // the regular rewriting.
  if (outermost_macro_name.starts_with("ASSERT_") ||
      outermost_macro_name == "CHECK" ||
      outermost_macro_name.starts_with("CHECK_") ||
      outermost_macro_name == "DCHECK" ||
      outermost_macro_name.starts_with("DCHECK_") ||
      outermost_macro_name.starts_with("EXPECT_")) {
    return false;
  }

  return true;
}

// Returns true if the Node is inside a macro expansion, except for the case
// that the Node is inside a macro argument of the exceptional macros (EXPECT_
// and ASSERT_ family).
// Tests are in: gtest-macro-original.cc
AST_POLYMORPHIC_MATCHER(isInExcludedMacroLocation,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(clang::Decl,
                                                        clang::Stmt,
                                                        clang::TypeLoc)) {
  auto loc = Node.getBeginLoc();
  const clang::ASTContext& ast_context = Finder->getASTContext();
  const clang::SourceManager& source_manager = ast_context.getSourceManager();

  return IsInExcludedMacro(std::move(loc), ast_context, source_manager);
}

// This iterates over function parameters and matches the ones that match
// parm_var_decl_matcher.
AST_MATCHER_P(clang::FunctionDecl,
              forEachParmVarDecl,
              clang::ast_matchers::internal::Matcher<clang::ParmVarDecl>,
              parm_var_decl_matcher) {
  const clang::FunctionDecl& function_decl = Node;

  const unsigned num_params = function_decl.getNumParams();
  bool is_matching = false;
  clang::ast_matchers::internal::BoundNodesTreeBuilder result;
  for (unsigned i = 0; i < num_params; i++) {
    const clang::ParmVarDecl* param = function_decl.getParamDecl(i);
    clang::ast_matchers::internal::BoundNodesTreeBuilder param_matches(
        *Builder);
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

std::string GetReplacementDirective(const clang::SourceRange& replacement_range,
                                    std::string replacement_text,
                                    const clang::SourceManager& source_manager,
                                    int precedence = kNeutralPrecedence) {
  clang::tooling::Replacement replacement(
      source_manager, clang::CharSourceRange::getCharRange(replacement_range),
      replacement_text);
  llvm::StringRef file_path = replacement.getFilePath();
  assert(!file_path.empty() && "Replacement file path is empty.");
  // For replacements that span multiple lines, make sure to remove the newline
  // character.
  // `./apply-edits.py` expects `\n` to be escaped as '\0'.
  std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');

  return llvm::formatv("r:::{0}:::{1}:::{2}:::{3}:::{4}", file_path,
                       replacement.getOffset(), replacement.getLength(),
                       precedence, replacement_text);
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

// Returns a function that, given the SourceLocation argument, tries to get the
// spelling location (= the original code location before macro expansion) if
// the given SourceLocation is at a macro argument.
//
// Note that `source_manager` and `lang_opts` arguments must outlive the
// returned function because the returned function references them.
//
// Tests are in: gtest-macro-original.cc
std::function<clang::SourceLocation(clang::SourceLocation)> GetSpellingLocFunc(
    const clang::SourceManager& source_manager [[clang::lifetimebound]],
    const clang::LangOptions& lang_opts [[clang::lifetimebound]]) {
  return [&](clang::SourceLocation loc) -> clang::SourceLocation {
    if (!loc.isMacroID()) [[likely]] {
      return loc;
    }
    clang::SourceLocation original_loc = loc;
    while (source_manager.isMacroArgExpansion(loc)) {
      loc = source_manager.getImmediateSpellingLoc(loc);
    }
    return loc.isValid() && loc.isFileID() ? loc : original_loc;
  };
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
clang::SourceRange GetExprRange(const clang::Expr& expr,
                                const clang::SourceManager& source_manager,
                                const clang::LangOptions& lang_opts) {
  auto ToSpellingLoc = GetSpellingLocFunc(source_manager, lang_opts);

  if (const auto* member_expr = clang::dyn_cast<clang::MemberExpr>(&expr)) {
    clang::SourceLocation member_loc =
        ToSpellingLoc(member_expr->getMemberLoc());
    size_t member_name_length = member_expr->getMemberDecl()->getName().size();
    return {member_loc, member_loc.getLocWithOffset(member_name_length)};
  }

  if (const auto* decl_ref = clang::dyn_cast<clang::DeclRefExpr>(&expr)) {
    assert(decl_ref->getBeginLoc() == decl_ref->getEndLoc() &&
           "DeclRefExpr doesn't have the expected end loc.");
    clang::SourceLocation begin_loc = ToSpellingLoc(decl_ref->getBeginLoc());
    auto name = decl_ref->getNameInfo().getName().getAsString();
    return {begin_loc, begin_loc.getLocWithOffset(name.size())};
  }

  if (const auto* call_expr = clang::dyn_cast<clang::CallExpr>(&expr)) {
    // Disclaimer: This doesn't support edge cases like following.
    //     #define MY_MACRO(func) func
    //     MY_MACRO(func)(arg1, arg2);
    //     // The returned range will be `func)(arg1, arg2)`.
    return {ToSpellingLoc(call_expr->getBeginLoc()),
            ToSpellingLoc(call_expr->getRParenLoc()).getLocWithOffset(1)};
  }

  if (auto* binary_op = clang::dyn_cast<clang::BinaryOperator>(&expr)) {
    // Disclaimer: This doesn't support edge cases like following.
    //     #define MY_MACRO(arg) arg
    //     MY_MACRO(1) + 2;  // The returned range will be `1) + 2`.
    //     MY_MACRO(1 +) 2;  // The returned range will be `1 +) 2`.
    return {
        ToSpellingLoc(expr.getBeginLoc()),
        GetExprRange(*binary_op->getRHS(), source_manager, lang_opts).getEnd()};
  }

  if (auto* uett_expr =
          clang::dyn_cast<clang::UnaryExprOrTypeTraitExpr>(&expr)) {
    if (uett_expr->getKind() == clang::UETT_SizeOf) {
      // Somehow in case of sizeof expr, the last token is not included in the
      // source range. So skip the next token after the end loc.
      assert(expr.getBeginLoc() != expr.getEndLoc());
      clang::SourceLocation begin_loc = ToSpellingLoc(expr.getBeginLoc());
      clang::SourceLocation end_loc = ToSpellingLoc(expr.getEndLoc());
      size_t token_length =
          clang::Lexer::MeasureTokenLength(end_loc, source_manager, lang_opts);
      return {begin_loc, end_loc.getLocWithOffset(token_length)};
    }
  }

  // Somehow single token expressions do not have the expected end location.
  const clang::SourceLocation begin_location = expr.getBeginLoc();
  const clang::SourceLocation end_location = expr.getEndLoc();
  if (begin_location != end_location) {
    llvm::errs() << "Error: expected token with unhelpful `SourceLocation`s, "
                    "but got:\n  "
                 << begin_location.printToString(source_manager) << "\nand\n  "
                 << end_location.printToString(source_manager) << "\n";
    assert(false && "Defaults to a single token expr.");
  }

  clang::SourceLocation begin_loc = ToSpellingLoc(expr.getBeginLoc());
  size_t token_length =
      clang::Lexer::MeasureTokenLength(begin_loc, source_manager, lang_opts);
  return {begin_loc, begin_loc.getLocWithOffset(token_length)};
}

std::string GetTypeAsString(const clang::QualType& qual_type,
                            const clang::ASTContext& ast_context) {
  clang::PrintingPolicy printing_policy(ast_context.getLangOpts());
  printing_policy.SuppressScope = 0;
  printing_policy.SuppressTagKeyword = 0;
  printing_policy.SuppressUnwrittenScope = 1;
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
clang::SourceRange getSourceRange(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::LangOptions& lang_opts = result.Context->getLangOpts();

  auto ToSpellingLoc = GetSpellingLocFunc(source_manager, lang_opts);

  if (auto* op =
          result.Nodes.getNodeAs<clang::UnaryOperator>("unaryOperator")) {
    if (op->isPostfix()) {
      return {ToSpellingLoc(op->getBeginLoc()),
              ToSpellingLoc(op->getEndLoc()).getLocWithOffset(2)};
    }
    auto* expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr");
    // Disclaimer: This doesn't support edge cases like following.
    //     #define MACRO(var) var
    //     ++MACRO(rhs);  // The range will be `++MACRO(rhs`.
    return {ToSpellingLoc(op->getBeginLoc()),
            GetExprRange(*expr, source_manager, lang_opts).getEnd()};
  }

  if (auto* op = result.Nodes.getNodeAs<clang::Expr>("binaryOperator")) {
    auto* sub_expr = result.Nodes.getNodeAs<clang::Expr>("binary_op_rhs");
    auto end_loc = GetExprRange(*sub_expr, source_manager, lang_opts).getEnd();
    // Disclaimer: This doesn't support edge cases like following.
    //     #define MACRO(var) var
    //     MACRO(lhs) + MACRO(rhs);  // The range will be `lhs) + MACRO(rhs`.
    return {ToSpellingLoc(op->getBeginLoc()), end_loc};
  }

  if (auto* op = result.Nodes.getNodeAs<clang::CXXOperatorCallExpr>(
          "raw_ptr_operator++")) {
    auto* callee = op->getDirectCallee();
    if (callee->getNumParams() == 0) {  // postfix op++ on raw_ptr;
      auto* expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr");
      return clang::SourceRange(
          GetExprRange(*expr, source_manager, lang_opts).getEnd());
    }
    return clang::SourceRange(
        ToSpellingLoc(op->getEndLoc()).getLocWithOffset(2));
  }

  if (auto* expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr")) {
    return clang::SourceRange(
        GetExprRange(*expr, source_manager, lang_opts).getEnd());
  }

  if (auto* size_expr = result.Nodes.getNodeAs<clang::Expr>("size_node")) {
    return clang::SourceRange(
        GetExprRange(*size_expr, source_manager, lang_opts).getEnd());
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

// Unwraps typedef type locs and returns the body type loc.
//
// Note that using-declared types are also represented with typedef types in
// clang, so this function works for both 'typedef' and 'using' declarations.
//
// Example TypeLoc structures:
//     // Given T2 where typedef int T1; using T2 = T1;
//     TypedefTypeLoc('T2')
//       --(getDecl)--> TypedefTypeLoc('T1')
//         --(getDecl)--> BuiltinTypeLoc('int')
//     => returns BuiltinTypeLoc('int').
clang::TypeLoc UnwrapTypedefTypeLoc(clang::TypeLoc type_loc) {
  while (const clang::TypedefTypeLoc typedef_type_loc =
             type_loc.getAs<clang::TypedefTypeLoc>()) {
    const clang::TypedefNameDecl* typedef_name_decl =
        typedef_type_loc.getDecl();
    type_loc = typedef_name_decl->getTypeSourceInfo()->getTypeLoc();
  }
  return type_loc;
}

std::string getNodeFromPointerTypeLoc(const clang::PointerTypeLoc* type_loc,
                                      const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();

  // We are in the case of a function return type loc.
  // This doesn't always generate the right range since type_loc doesn't
  // account for qualifiers (like const). Didn't find a proper way for now
  // to get the location with type qualifiers taken into account.
  //
  // We may simply be hosed:
  // *  `PointerTypeLoc` inherits from unqualified types.
  // *  `QualifiedTypeLoc` deliberately does not provide source locations
  //    for qualifiers [1].
  //
  // As a best effort, if we bound `qualified_type_loc`, we abuse the
  // Lexer to back up one token behind `type_loc`. Take a deep breath
  // and hope that it's the `const` qualifier.
  //
  // [1]
  // https://github.com/llvm/llvm-project/blob/6cf656eca717890a43975c026d0ae34c16c6c455/clang/include/clang/AST/TypeLoc.h#L288
  clang::SourceRange replacement_range = [type_loc, &result, &source_manager,
                                          &lang_opts]() {
    const auto* qualified_type_loc =
        result.Nodes.getNodeAs<clang::QualifiedTypeLoc>("qualified_type_loc");
    clang::SourceRange result = {type_loc->getBeginLoc(),
                                 type_loc->getEndLoc().getLocWithOffset(1)};
    if (!qualified_type_loc ||
        !qualified_type_loc->getType().isConstQualified()) {
      return result;
    }
    std::optional<clang::Token> previous_token =
        clang::Lexer::findPreviousToken(type_loc->getBeginLoc(), source_manager,
                                        lang_opts, /*IncludeComments=*/false);
    // If we can't find the previous token, bail out to the previous
    // behavior.
    if (!previous_token.has_value()) {
      return result;
    }
    std::string_view hopefully_const_qualifier = clang::Lexer::getSourceText(
        clang::CharSourceRange::getCharRange(
            {previous_token->getLocation(), previous_token->getEndLoc()}),
        source_manager, lang_opts);
    if (hopefully_const_qualifier != "const") {
      // A patch hitting this will likely fail to compile.
      llvm::errs() << "WARNING: `getNodeFromPointerTypeLoc()` expected "
                      "`const`, but got: "
                   << hopefully_const_qualifier << " instead.\n";
      return result;
    }

    // Extend the replacement range leftward to include `const` in the
    // type to be rewritten.
    result.setBegin(previous_token->getLocation());
    return result;
  }();

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

std::string getNodeFromRawPtrTypeLoc(
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
std::string getNodeFromFunctionArrayParameter(
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

std::string getNodeFromDecl(const clang::DeclaratorDecl* decl,
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

void DecaySpanToPointer(const MatchFinder::MatchResult& result) {
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

  EmitReplacement(
      GetRHS(result),
      GetReplacementDirective(begin_range, begin_replacement_text,
                              source_manager, -kDecaySpanToPointerPrecedence));

  EmitReplacement(
      GetRHS(result),
      GetReplacementDirective(end_range, end_replacement_text, source_manager,
                              kDecaySpanToPointerPrecedence));
}

clang::SourceLocation GetBinaryOperationOperatorLoc(
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

struct RangedReplacement {
  clang::SourceRange range;
  std::string text;
};

// Specifies an edit: `base::checked_cast<size_t>(...)`
struct CheckedCastReplacement {
  RangedReplacement opener;
  RangedReplacement closer;
};

// There are three possible subspan expr replacements, respectively:
// 1. No replacement (leave as is)
// 2. Append a `u` to an integer literal.
// 3. Wrap the expression in `base::checked_cast<size_t>(...)`.
using SubspanExprReplacement =
    std::variant<std::monostate, RangedReplacement, CheckedCastReplacement>;

SubspanExprReplacement GetSubspanExprReplacement(
    const clang::Expr* expr,
    const MatchFinder::MatchResult& result,
    std::string_view key) {
  clang::QualType type = expr->getType();
  const clang::ASTContext& ast_context = *result.Context;

  const uint64_t size_t_bits =
      ast_context.getTypeSize(ast_context.getSizeType());
  const bool is_unsigned_type =
      type == ast_context.getCorrespondingUnsignedType(type);
  if (is_unsigned_type && ast_context.getTypeSize(type) <= size_t_bits) {
    return {};
  }

  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::SourceRange range =
      GetExprRange(*expr, source_manager, result.Context->getLangOpts());

  if (const auto* integer_literal =
          clang::dyn_cast<clang::IntegerLiteral>(expr)) {
    assert(integer_literal->getValue().isNonNegative());
    return RangedReplacement{.range = range.getEnd(), .text = "u"};
  }

  EmitReplacement(key, GetIncludeDirective(range, source_manager,
                                           "base/numerics/safe_conversions.h"));
  EmitReplacement(key, GetIncludeDirective(range, source_manager, "cstdint",
                                           /*is_system_include_path=*/true));
  return CheckedCastReplacement{
      .opener = {.range = range.getBegin(),
                 .text = "base::checked_cast<size_t>("},
      .closer = {.range = range.getEnd(), .text = ")"}};
}

// When a binary operation and rhs expr appear inside a macro expansion,
// this function produces an expression like:
//     UNSAFE_TODO(MACRO(will_be_span.data()))
// where MACRO is defined as something like below:
//     #define MACRO(arg) (arg + offset)
//
// Known issue:
// The following code implicitly assumes that the will_be_span object is a
// macro argument, and cannot handle the following case appropriately.
//     #define MACRO() (will_be_span + offset)
//
// See test: 'span-frontier-macro-original.cc'
void AdaptBinaryOpInMacro(const MatchFinder::MatchResult& result,
                          const std::string& key) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();

  const auto* decl_ref =
      result.Nodes.getNodeAs<clang::DeclRefExpr>("declRefExpr");
  if (!decl_ref) {
    llvm::errs()
        << "\n"
           "Error: In case of a binary operation in a macro expansion, "
           "only `declRefExpr` is supported for now.\n";
    DumpMatchResult(result);
    return;
  }

  EmitReplacement(
      key, GetReplacementDirective(
               GetExprRange(*decl_ref, source_manager, lang_opts).getEnd(),
               ".data()", source_manager));

  clang::CharSourceRange macro_range =
      source_manager.getExpansionRange(decl_ref->getBeginLoc());
  EmitReplacement(key, GetReplacementDirective(macro_range.getBegin(),
                                               "UNSAFE_TODO(", source_manager));
  // `macro_range.getEnd()` points to the last character of the macro call,
  // i.e. the closing parenthesis of the macro call, so +1 offset is needed.
  // Note that `macro_range` is a CharSourceRange, not a SourceRange.
  EmitReplacement(
      key, GetReplacementDirective(macro_range.getEnd().getLocWithOffset(1),
                                   ")", source_manager));
}

// Closes an open `base::span(` if present.
// Returns a `.subspan(` opener.
// Opens a `base::checked_cast(` if necessary.
std::string CreateSubspanOpener(
    std::string_view prefix,
    const SubspanExprReplacement* subspan_expr_replacement) {
  std::string_view maybe_checked_cast_opener = "";
  if (const auto* replacement =
          std::get_if<CheckedCastReplacement>(subspan_expr_replacement)) {
    maybe_checked_cast_opener = replacement->opener.text;
  }
  return llvm::formatv("{0}.subspan({1}", prefix, maybe_checked_cast_opener);
}

// Returns a `.subspan(` closer.
// Closes an open `base::checked_cast(` if necessary,
// or appends a `u` to the integer literal expression.
std::string CreateSubspanCloser(
    const SubspanExprReplacement* subspan_expr_replacement) {
  std::string_view maybe_closer = "";
  if (const auto* replacement =
          std::get_if<RangedReplacement>(subspan_expr_replacement)) {
    maybe_closer = replacement->text;
  } else if (const auto* replacement = std::get_if<CheckedCastReplacement>(
                 subspan_expr_replacement)) {
    maybe_closer = replacement->closer.text;
  }
  return llvm::formatv("{0})", maybe_closer);
}

void AdaptBinaryOperation(const MatchFinder::MatchResult& result) {
  const clang::ASTContext& ast_context = *result.Context;
  const clang::SourceManager& source_manager = *result.SourceManager;
  const auto* binary_operation =
      GetNodeOrCrash<clang::Expr>(result, "binary_operation", __FUNCTION__);
  const auto* rhs_expr =
      GetNodeOrCrash<clang::Expr>(result, "rhs_expr", __FUNCTION__);
  const std::string key = GetRHS(result);

  // If `binary_operation` and `rhs_expr` appear inside a macro expansion, then
  // add ".data()" call in the call site instead of adding ".subspan(offset)".
  if (IsInExcludedMacro(binary_operation->getBeginLoc(), ast_context,
                        source_manager) &&
      IsInExcludedMacro(rhs_expr->getBeginLoc(), ast_context, source_manager)) {
    AdaptBinaryOpInMacro(result, key);
    return;
  }

  // C-style arrays are rewritten to `std::array`, not `base::span`, so
  // a binary operation on the rewritten array must explicitly construct
  // a `base::span` of it before calling `.subspan()`.
  //
  // Emit a replacement to that effect:
  // `base::span( <binary operation lhs> `
  // ...but leave the closing right-parenthesis for the `).subspan()` call.
  const auto* rhs_array_type =
      result.Nodes.getNodeAs<clang::ArrayTypeLoc>("rhs_array_type_loc");
  if (rhs_array_type) {
    const auto* concrete_binary_operation =
        GetNodeOrCrash<clang::BinaryOperator>(
            result, "binary_operation",
            "C-style array should not involve `CXXOperatorCallExpr` or "
            "`CXXRewrittenBinaryOperator`");
    EmitReplacement(
        key, GetReplacementDirective(
                 concrete_binary_operation->getLHS()->getBeginLoc(),
                 llvm::formatv("base::span<{0}>(",
                               GetTypeAsString(rhs_array_type->getInnerType(),
                                               *result.Context)),
                 source_manager, kAdaptBinaryOperationPrecedence));
    // Emit the closing `)` of `base::span(...)` below.
  }

  // Rather than emit a pure "insertion" replacement (zero-length
  // range), assume that the binary operation is a single char and
  // manually construct a `SourceRange` that overwrites exactly that.
  // `git cl format` later takes care of the errant whitespace. E.g.:
  //
  // a + b
  //   ^
  //
  // becomes
  //
  // a .subspan( b
  const auto* binary_op_RHS =
      GetNodeOrCrash<clang::Expr>(result, "binary_op_rhs", __FUNCTION__);
  const auto subspan_expr_replacement =
      GetSubspanExprReplacement(binary_op_RHS, result, key);

  // Close the open `base::span(` expression if present.
  std::string_view prefix = rhs_array_type ? ")" : "";
  std::string subspan_opener =
      CreateSubspanOpener(prefix, &subspan_expr_replacement);

  const clang::SourceLocation binary_operator_begin =
      GetBinaryOperationOperatorLoc(binary_operation, result);
  EmitReplacement(
      key,
      GetReplacementDirective(
          {binary_operator_begin, binary_operator_begin.getLocWithOffset(1)},
          subspan_opener, source_manager, -kAdaptBinaryOperationPrecedence));

  const clang::SourceRange operator_rhs_range = GetExprRange(
      *binary_op_RHS, source_manager, result.Context->getLangOpts());

  std::string subspan_closer = CreateSubspanCloser(&subspan_expr_replacement);
  EmitReplacement(key, GetReplacementDirective(
                           operator_rhs_range.getEnd(), subspan_closer,
                           source_manager, -kAdaptBinaryOperationPrecedence));

  // It's possible we emitted a rewrite that creates a temporary but
  // unnamed `base::span` (issue 408018846). This could end up being
  // the only reference in the file, and so it has to carry the
  // `#include` directive itself.
  EmitReplacement(key, GetIncludeDirective(binary_operation->getBeginLoc(),
                                           source_manager));
}

void AdaptBinaryPlusEqOperation(const MatchFinder::MatchResult& result) {
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
  auto lhs_expr_range = GetExprRange(*lhs_expr, source_manager, lang_opts);
  auto binary_op_rhs_range =
      GetExprRange(*binary_op_RHS, source_manager, lang_opts);
  auto source_range = clang::SourceRange(lhs_expr_range.getEnd(),
                                         binary_op_rhs_range.getBegin());

  const std::string& key = GetRHS(result);

  auto subspan_arg_fixup =
      GetSubspanExprReplacement(binary_op_RHS, result, key);
  std::string lhs_expr_text =
      clang::Lexer::getSourceText(
          clang::CharSourceRange::getCharRange(lhs_expr_range), source_manager,
          lang_opts)
          .str();

  EmitReplacement(key,
                  GetReplacementDirective(
                      source_range,
                      CreateSubspanOpener(
                          std::string(llvm::formatv("= {0}", lhs_expr_text)),
                          &subspan_arg_fixup),
                      source_manager, kAdaptBinaryPlusEqOperationPrecedence));

  std::string subspan_closer = CreateSubspanCloser(&subspan_arg_fixup);

  EmitReplacement(
      key, GetReplacementDirective(
               clang::SourceRange(binary_op_rhs_range.getEnd()), subspan_closer,
               source_manager, -kAdaptBinaryPlusEqOperationPrecedence));
}

// Handles boolean operations that need to be adapted after a span rewrite.
//   if(expr) => if(!expr.empty())
//   if(!expr) => if(expr.empty())
// Tests are in: operator-bool-original.cc
void DecaySpanToBooleanOp(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const std::string& key = GetRHS(result);

  if (const auto* logical_not_op =
          result.Nodes.getNodeAs<clang::UnaryOperator>("logical_not_op")) {
    const clang::SourceRange logical_not_range{
        logical_not_op->getBeginLoc(),
        logical_not_op->getBeginLoc().getLocWithOffset(1)};
    EmitReplacement(
        key, GetReplacementDirective(logical_not_range, "", source_manager));
  } else {
    const auto* operand =
        result.Nodes.getNodeAs<clang::Expr>("boolean_op_operand");
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
  const std::string key = GetRHS(result);
  auto rep_range = clang::SourceRange(getSourceRange(result).getEnd());

  std::string replacement_text = ".data()";

  if (result.Nodes.getNodeAs<clang::Expr>("unaryOperator")) {
    if (result.Nodes.getNodeAs<clang::Expr>("container_buff_address")) {
      rep_range = EmitContainerPointerRewrites(
          result, key, ContainerPointerRewritesMode::kDontWrapWithBaseSpan);
    } else {
      // Insert enclosing parenthesis for expressions with UnaryOperators
      auto begin_range = clang::SourceRange(getSourceRange(result).getBegin());
      EmitReplacement(key,
                      GetReplacementDirective(begin_range, "(", source_manager,
                                              kAppendDataCallPrecedence));
      replacement_text = ").data()";
    }
  }

  EmitReplacement(
      key, GetReplacementDirective(rep_range, replacement_text, source_manager,
                                   -kAppendDataCallPrecedence));
}

// Given that we want to emit `.subspan(expr)`,
// *  if `expr` is observably unsigned, does nothing.
// *  if `expr` is a signed int literal, appends `u`.
// *  otherwise, wraps `expr` with `checked_cast`.
void RewriteExprForSubspan(const clang::Expr* expr,
                           const MatchFinder::MatchResult& result,
                           std::string_view key) {
  const auto replacement = GetSubspanExprReplacement(expr, result, key);
  if (const auto* u_suffix = std::get_if<RangedReplacement>(&replacement)) {
    EmitReplacement(key,
                    GetReplacementDirective(u_suffix->range, u_suffix->text,
                                            *result.SourceManager));
    return;
  }

  if (const auto* checked_cast_replacement =
          std::get_if<CheckedCastReplacement>(&replacement)) {
    const auto& [opener, closer] = *checked_cast_replacement;
    EmitReplacement(key, GetReplacementDirective(opener.range, opener.text,
                                                 *result.SourceManager));
    EmitReplacement(key, GetReplacementDirective(closer.range, closer.text,
                                                 *result.SourceManager));
    return;
  }

  if (!std::get_if<std::monostate>(&replacement)) {
    llvm::errs() << "Unexpected variant in `RewriteExprForSubspan()`.";
    DumpMatchResult(result);
    return;
  }
}

// Helper function for `EmitContainerPointerRewrites()`.
//
// A `&container[offset]` could either be
// *  a C-style array subscript or
// *  something with `operator[]` defined.
//
// This function helps find the right bracket in either case.
clang::SourceLocation FindRightBracket(const MatchFinder::MatchResult& result,
                                       const clang::Expr* subscript_expr) {
  if (const auto* array_subscript_expr =
          clang::dyn_cast<clang::ArraySubscriptExpr>(subscript_expr)) {
    return array_subscript_expr->getRBracketLoc();
  } else if (const auto* operator_subscript_expr =
                 clang::dyn_cast<clang::CXXOperatorCallExpr>(subscript_expr)) {
    return operator_subscript_expr->getRParenLoc();
  }
  llvm::errs() << "Error: no matching cast for `subscript_expr` in "
               << __FUNCTION__ << "\n";
  DumpMatchResult(result);
  assert(false);
}

// Helper function for `EmitContainerPointerRewrites()`.
//
// Same motivation as `FindRightBracket()`; returns the expression inside the
// square brackets.
const clang::Expr* GetIndexExprForSubspan(
    const MatchFinder::MatchResult& result,
    const clang::Expr* subscript_expr) {
  if (const auto* array_subscript_expr =
          clang::dyn_cast<clang::ArraySubscriptExpr>(subscript_expr)) {
    return array_subscript_expr->getIdx();
  } else if (const auto* operator_subscript_expr =
                 clang::dyn_cast<clang::CXXOperatorCallExpr>(subscript_expr)) {
    assert(operator_subscript_expr->getNumArgs() == 2u);

    // Call `IgnoreImpCasts()` to see past the implicit promotion to
    // `...::size_type` and see the "original" type of the expression.
    return operator_subscript_expr->getArg(1u)->IgnoreImpCasts();
  }
  llvm::errs() << "Error: no matching cast for `subscript_expr` in "
               << __FUNCTION__ << "\n";
  DumpMatchResult(result);
  assert(false);
}

// Handles `&container[offset]` being used as a buffer.
//
// To handle a value passed into a newly spanified function:
// 1. replaces `&` with `base::span<T>(`
// 2. replaces `[` with `).subspan(`
// 3. fixes up the `offset` expression if necessary
// 4. replaces `]` with `)`
//
// To handle a frontier value inside a newly spanified function:
// [skip step 1]
// 2. replaces `[` with `.subspan(`
// etc.
//
// Returns the source location just beyond the right-hand bracket.
clang::SourceLocation EmitContainerPointerRewrites(
    const MatchFinder::MatchResult& result,
    std::string_view key,
    ContainerPointerRewritesMode mode) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::LangOptions& lang_opts = result.Context->getLangOpts();
  auto replacement_range = GetNodeOrCrash<clang::UnaryOperator>(
                               result, "unaryOperator", __FUNCTION__)
                               ->getSourceRange();

  // Stretch across the `&`.
  replacement_range.setEnd(replacement_range.getBegin().getLocWithOffset(1));

  const auto* subscript_expr =
      GetNodeOrCrash<clang::Expr>(result, "subscript_expr", __FUNCTION__);

  std::string_view declref_bind_name = "container_decl_ref";
  std::string_view subspan_opener = ").subspan(";
  if (mode == ContainerPointerRewritesMode::kDontWrapWithBaseSpan) {
    declref_bind_name = "rhs_expr";
    subspan_opener = ".subspan(";
  }

  const auto& container_decl_ref =
      *GetNodeOrCrash<clang::Expr>(result, declref_bind_name, __FUNCTION__);
  const clang::SourceLocation left_bracket =
      GetExprRange(container_decl_ref, source_manager, lang_opts).getEnd();
  clang::SourceLocation right_bracket =
      FindRightBracket(result, subscript_expr);

  // Special case: we detected and bound a zero offset (`&buf[0]`).
  // Rather than emit a `.subspan(...)`, we delete the subscript
  // expression entirely.
  if (result.Nodes.getNodeAs<clang::IntegerLiteral>("zero_container_offset")) {
    EmitReplacement(key, GetReplacementDirective(replacement_range, "",
                                                 *result.SourceManager));
    replacement_range = {left_bracket, right_bracket.getLocWithOffset(1)};
    EmitReplacement(key, GetReplacementDirective(replacement_range, "",
                                                 *result.SourceManager));
    return right_bracket.getLocWithOffset(1);
  }

  // Step 1
  if (mode == ContainerPointerRewritesMode::kWrapWithBaseSpan) {
    // Emit the opening `base::span(`.
    const auto& contained_type = *GetNodeOrCrash<clang::QualType>(
        result, "contained_type", __FUNCTION__);
    EmitReplacement(
        key,
        GetReplacementDirective(
            replacement_range,
            llvm::formatv("base::span<{0}>(",
                          GetTypeAsString(contained_type, *result.Context)),
            source_manager));
  } else {
    // Just delete the `&`.
    EmitReplacement(key,
                    GetReplacementDirective(
                        replacement_range,
                        // Mysteriously, emitting a pure deletion replacement
                        // also eats the preceding comma in our test cases.
                        " ", source_manager));
  }

  // Step 2
  EmitReplacement(key, GetReplacementDirective(
                           {left_bracket, left_bracket.getLocWithOffset(1)},
                           std::string(subspan_opener), source_manager));

  // Step 3
  const clang::Expr* index = GetIndexExprForSubspan(result, subscript_expr);
  assert(index);
  RewriteExprForSubspan(index, result, key);

  // Step 4
  EmitReplacement(key, GetReplacementDirective(
                           {right_bracket, right_bracket.getLocWithOffset(1)},
                           ")", source_manager));
  return right_bracket.getLocWithOffset(1);
}

// Handles code that passes address to a local variable as a single element
// buffer. Wrap it with a span of size=1. Tests are in
// single-element-buffer-original.cc.
void EmitSingleVariableSpan(const std::string& key,
                            const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const auto& lang_opts = result.Context->getLangOpts();

  const auto* expr =
      result.Nodes.getNodeAs<clang::UnaryOperator>("address_expr");
  const auto* operand_expr =
      result.Nodes.getNodeAs<clang::Expr>("address_expr_operand");
  if (!expr || !operand_expr) {
    llvm::errs()
        << "\n"
           "Error: EmitSingleVariableSpan() encountered an unexpected match.\n";
    DumpMatchResult(result);
    assert(false && "Unexpected match in EmitSingleVariableSpan()");
  }

  // This range is just one character, covering the '&' symbol.
  clang::SourceLocation ampersand_loc = expr->getOperatorLoc();
  clang::SourceRange ampersand_range = {
      ampersand_loc, clang::Lexer::getLocForEndOfToken(
                         ampersand_loc, 0u, source_manager, lang_opts)};

  EmitReplacement(key, GetReplacementDirective(
                           ampersand_range, "base::span_from_ref(",
                           source_manager, kEmitSingleVariableSpanPrecedence));
  EmitReplacement(
      key, GetReplacementDirective(
               GetExprRange(*operand_expr, source_manager, lang_opts).getEnd(),
               ")", source_manager, -kEmitSingleVariableSpanPrecedence));
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
void EmitUnsafeCxxMethodCall(const std::string& key,
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

  EmitReplacement(key, GetIncludeDirective(
                           member_call_expr->getSourceRange(), source_manager,
                           kBaseAutoSpanificationHelperIncludePath));
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
void EmitUnsafeFreeFuncCall(const std::string& key,
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

  // Replace the function name with the macro name.
  const clang::SourceLocation& func_loc = call_expr->getCallee()->getBeginLoc();
  EmitReplacement(
      key, GetReplacementDirective(
               clang::SourceRange(func_loc, func_loc.getLocWithOffset(
                                                entry.function_name.length())),
               std::string(entry.macro_name), source_manager));

  EmitReplacement(
      key, GetIncludeDirective(call_expr->getSourceRange(), source_manager,
                               kBaseAutoSpanificationHelperIncludePath));
}

void EmitUnsafeFunctionCall(const std::string& key,
                            const clang::CallExpr* call_expr,
                            const MatchFinder::MatchResult& result) {
  if (const clang::CXXMemberCallExpr* member_call_expr =
          clang::dyn_cast<clang::CXXMemberCallExpr>(call_expr)) {
    EmitUnsafeCxxMethodCall(key, member_call_expr, result);
    return;
  }
  EmitUnsafeFreeFuncCall(key, call_expr, result);
}

// Rewrites:
//     auto it = std::begin(c_array);
//     it == std::end(c_array)
// To:
//     auto it = base::SpanificationArrayBegin(c_array);
//     it == base::SpanificationArrayEnd(c_array)
//
// Note that `auto it = ...` is rewritten to `base::span<T> it = ...`
// separately.
//
// Tests are in: array-tests-original.cc
void EmitCArrayIterCallExpr(const std::string& key,
                            const clang::CallExpr* call_expr,
                            const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::LangOptions& lang_opts = result.Context->getLangOpts();

  const auto* func_decl =
      clang::dyn_cast<clang::FunctionDecl>(call_expr->getCalleeDecl());
  assert(func_decl);
  const std::string& function_name = func_decl->getQualifiedNameAsString();

  struct FuncMapping {
    const std::string_view function_name;
    const std::string_view replacement_function_name;
  };
  static constexpr FuncMapping func_mapping_table[] = {
      {"std::begin", "base::SpanificationArrayBegin"},
      {"std::end", "base::SpanificationArrayEnd"},
      {"std::cbegin", "base::SpanificationArrayCBegin"},
      {"std::cend", "base::SpanificationArrayCEnd"},
  };
  std::string replacement_function_name;
  for (const auto& entry : func_mapping_table) {
    if (function_name == entry.function_name) {
      replacement_function_name = entry.replacement_function_name;
      break;
    }
  }
  assert(!replacement_function_name.empty());

  const clang::SourceRange replacement_range(
      call_expr->getCallee()->getBeginLoc(),
      clang::Lexer::getLocForEndOfToken(call_expr->getCallee()->getEndLoc(), 0u,
                                        source_manager, lang_opts));
  EmitReplacement(
      key, GetReplacementDirective(replacement_range, replacement_function_name,
                                   source_manager));
  EmitReplacement(key,
                  GetIncludeDirective(replacement_range, source_manager,
                                      kBaseAutoSpanificationHelperIncludePath));
}

std::string GetNodeFromSizeExpr(const clang::Expr* size_expr,
                                const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const std::string key = NodeKey(size_expr, source_manager);

  // "size_node" assumes that third party functions that return a buffer
  // provide some way to know the size, however special handling is required
  // to extract that, thus here we add support for functions returning a
  // buffer that also have size support.
  if (const auto* unsafe_call_expr = result.Nodes.getNodeAs<clang::CallExpr>(
          "unsafe_function_call_expr")) {
    EmitUnsafeFunctionCall(key, unsafe_call_expr, result);
  }

  if (const auto* c_array_iter_call_expr =
          result.Nodes.getNodeAs<clang::CallExpr>("c_array_iter_call_expr")) {
    EmitCArrayIterCallExpr(key, c_array_iter_call_expr, result);
  }

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
    EmitContainerPointerRewrites(
        result, key, ContainerPointerRewritesMode::kWrapWithBaseSpan);
  }

  EmitReplacement(key, GetIncludeDirective(replacement_range, source_manager));
  EmitSink(key);
  return key;
}

// Rewrite:
//   `ptr++` or `++ptr`
// Into:
//   `base::PreIncrementSpan(span)` or `base::PostIncrementSpan(span)`.
void RewriteUnaryOperation(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const auto& lang_opts = result.Context->getLangOpts();

  const clang::Expr* operand = nullptr;
  bool is_prefix = false;
  clang::SourceLocation operator_loc;

  if (const auto* unary_op =
          result.Nodes.getNodeAs<clang::UnaryOperator>("unaryOperator")) {
    operand = unary_op->getSubExpr();
    is_prefix = unary_op->isPrefix();
    operator_loc = unary_op->getOperatorLoc();
  } else if (const auto* cxx_op_call =
                 result.Nodes.getNodeAs<clang::CXXOperatorCallExpr>(
                     "raw_ptr_operator++")) {
    operand = cxx_op_call->getArg(0);
    const auto* method_decl =
        clang::dyn_cast<clang::CXXMethodDecl>(cxx_op_call->getCalleeDecl());
    assert(method_decl);
    // For CXXOperatorCallExpr, prefix increment has 0 parameters (e.g.,
    // operator++()) postfix increment has 1 parameter (e.g., operator++(int)).
    is_prefix = (method_decl->getNumParams() == 0);
    operator_loc = cxx_op_call->getOperatorLoc();
  }

  if (!operand) {
    // This block should ideally not be reached if matchers are well-defined.
    llvm::errs()
        << "\n"
        << "Error: RewriteUnaryOperation() encountered an unexpected match.\n"
        << "Expected a unaryOperator or raw_ptr_operator++ to be bound.\n";
    DumpMatchResult(result);
    assert(false && "Unexpected match in RewriteUnaryOperation()");
    return;
  }

  assert(operator_loc.isValid());

  // Get the source range of the operand (the 'ptr' part).
  clang::SourceRange operand_range =
      GetExprRange(*operand->IgnoreParenImpCasts(), source_manager, lang_opts);
  assert(operand_range.isValid());

  clang::SourceLocation operator_end_loc = clang::Lexer::getLocForEndOfToken(
      operator_loc, 0, source_manager, lang_opts);
  assert(operator_end_loc.isValid());
  clang::SourceRange op_token_range(operator_loc, operator_end_loc);

  std::string begin_insert_text;
  clang::SourceRange begin_replacement_range;
  clang::SourceRange end_replacement_range;

  if (is_prefix) {
    begin_insert_text = "base::PreIncrementSpan(";
    // Replace the '++' with "base::PreIncrementSpan(".
    begin_replacement_range = op_token_range;
    // Insert ")" at the end of the operand.
    end_replacement_range =
        clang::SourceRange(operand_range.getEnd(), operand_range.getEnd());
  } else {
    begin_insert_text = "base::PostIncrementSpan(";
    // Insert "base::PostIncrementSpan(" at the beginning of the operand.
    begin_replacement_range =
        clang::SourceRange(operand_range.getBegin(), operand_range.getBegin());
    // Replace "++"" with ")".
    end_replacement_range = op_token_range;
  }

  assert(begin_replacement_range.isValid());
  assert(end_replacement_range.isValid());

  const std::string key = GetRHS(result);

  EmitReplacement(key, GetReplacementDirective(
                           begin_replacement_range, begin_insert_text,
                           source_manager, kRewriteUnaryOperationPrecedence));

  EmitReplacement(
      key, GetReplacementDirective(end_replacement_range, ")", source_manager,
                                   -kRewriteUnaryOperationPrecedence));

  EmitReplacement(key,
                  GetIncludeDirective(operand_range, source_manager,
                                      kBaseAutoSpanificationHelperIncludePath));
}

// Rewrite:
//   `sizeof(c_array)`
// Into:
//   `base::SpanificationSizeofForStdArray(std_array)`
// Tests are in: array-tests-original.cc
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

  const std::string& key = GetRHS(result);
  const clang::SourceRange replacement_range = {
      sizeof_expr->getBeginLoc(),
      sizeof_expr->getEndLoc().getLocWithOffset(end_offset)};
  EmitReplacement(key,
                  GetReplacementDirective(
                      replacement_range,
                      llvm::formatv("base::SpanificationSizeofForStdArray({0})",
                                    array_decl_as_string),
                      source_manager));
  EmitReplacement(key,
                  GetIncludeDirective(replacement_range, source_manager,
                                      kBaseAutoSpanificationHelperIncludePath));
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
                 GetReplacementDirective(begin_range, "(", source_manager,
                                         kAppendDataCallPrecedence));
    replacement_text = ").data()";
  }

  // Use kAppendDataCallPrecedence because some rewrites will be duplicates of
  // the ones in AppendDataCall().
  EmitFrontier(
      lhs_key, rhs_key,
      GetReplacementDirective(rep_range, replacement_text, source_manager,
                              -kAppendDataCallPrecedence));
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

const clang::Expr* GetInitExpr(const clang::DeclaratorDecl* decl) {
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

bool IsMutable(const clang::DeclaratorDecl* decl) {
  if (const auto* field_decl =
          clang::dyn_cast_or_null<clang::FieldDecl>(decl)) {
    return field_decl->isMutable();
  }
  return false;
}

bool IsConstexpr(const clang::DeclaratorDecl* decl) {
  if (const auto* var_decl = clang::dyn_cast_or_null<clang::VarDecl>(decl)) {
    return var_decl->isConstexpr();
  }
  return false;
}

bool IsInlineVarDecl(const clang::DeclaratorDecl* decl) {
  if (const auto* var_decl = clang::dyn_cast_or_null<clang::VarDecl>(decl)) {
    return var_decl->isInlineSpecified();
  }
  return false;
}

bool IsStaticLocalOrStaticStorageClass(const clang::DeclaratorDecl* decl) {
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
    // `GetTypeAsString` doesn't remove a tag keyword (struct, class, enum, or
    // union), but we'd like to remove the tag keyword here.
    clang::PrintingPolicy printing_policy(ast_context.getLangOpts());
    printing_policy.SuppressTagKeyword = 1;
    printing_policy.SuppressUnwrittenScope = 1;
    printing_policy.SuppressInlineNamespace = 1;
    printing_policy.SuppressDefaultTemplateArgs = 1;
    printing_policy.PrintAsCanonical = 1;
    element_type_as_string = new_element_type.getAsString(printing_policy);
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
    if (original_element_type.isConstant(ast_context) ||
        IsConstexpr(array_decl)) {
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

// Handles comparison expressions between a will-be-span object and a C array
// iterator.
//   it == std::begin(c_array)
//   it != std::end(c_array)
// Tests are in: array-tests-original.cc
void RewriteComparisonWithCArrayIter(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::CallExpr* call_expr = GetNodeOrCrash<clang::CallExpr>(
      result, "c_array_iter_call_expr",
      "std::c?{begin,end} for a C array is expected");
  const std::string& lhs = GetLHS(result);
  const std::string& rhs = NodeKey(call_expr, source_manager);
  EmitCArrayIterCallExpr(rhs, call_expr, result);
  EmitEdge(lhs, rhs);
  EmitEdge(rhs, lhs);
}

// When a function declaration (= function type) gets rewritten, rewrites
// variables of a function pointer type to which the function is assigned.
//
// Example:
//     // function declaration being spanified
//     int* func(int* arg);
//     // function pointer variable to be spanified
//     int* (*var)(int* arg) = func;
// In the following implementation, `var` is called LHS and `func` is called
// RHS.
//
// Tests are in: func-ptr-var-original.cc
void RewriteFunctionPointerType(const MatchFinder::MatchResult& result) {
  const clang::VarDecl* lhs_var_decl = GetNodeOrCrash<clang::VarDecl>(
      result, "lhs_funcptrvardecl",
      "The rewriting target variable of function pointer type must be bound.");

  // Get the FunctionProtoTypeLoc of the LHS variable.
  clang::FunctionProtoTypeLoc lhs_func_proto_type_loc;
  {
    const clang::TypeLoc var_type_loc =
        UnwrapTypedefTypeLoc(lhs_var_decl->getTypeSourceInfo()->getTypeLoc());
    if (var_type_loc.getAs<clang::AutoTypeLoc>() ||
        var_type_loc.getAs<clang::DecltypeTypeLoc>()) {
      return;  // No need to rewrite auto/decltype types.
    }
    const clang::PointerTypeLoc pointer_type_loc =
        var_type_loc.getAs<clang::PointerTypeLoc>();
    assert(pointer_type_loc && "Failed to get a PointerTypeLoc.");
    clang::TypeLoc pointee_type_loc = pointer_type_loc.getPointeeLoc();
    // Unwrap paren type locs.
    while (clang::ParenTypeLoc paren_type_loc =
               pointee_type_loc.getAs<clang::ParenTypeLoc>()) {
      pointee_type_loc = paren_type_loc.getInnerLoc();
    }
    lhs_func_proto_type_loc =
        pointee_type_loc.getAs<clang::FunctionProtoTypeLoc>();
  }
  assert(lhs_func_proto_type_loc && "Failed to get a FunctionProtoTypeLoc.");

  // RHS matches with one of the parameter types or the return type of the
  // function declaration. Rewrite the one matched.
  const std::string& rhs_key = GetRHS(result);

  // LHS matches with the function pointer type variable (not a parameter type
  // nor return type unlike RHS). Find the parameter or return type
  // corresponding to the RHS match, and rewrite it.
  std::string lhs_key;
  if (const clang::ParmVarDecl* rhs_parm_var_decl =
          result.Nodes.getNodeAs<clang::ParmVarDecl>("rhs_begin")) {
    // One of the function parameter types matches on RHS.
    const unsigned parm_index = rhs_parm_var_decl->getFunctionScopeIndex();
    const clang::ParmVarDecl* lhs_parm_var_decl =
        lhs_func_proto_type_loc.getParam(parm_index);
    const clang::TypeLoc lhs_parm_type_loc = UnwrapTypedefTypeLoc(
        lhs_parm_var_decl->getTypeSourceInfo()->getTypeLoc());
    if (lhs_parm_type_loc.getAs<clang::ArrayTypeLoc>()) {
      lhs_key = getNodeFromFunctionArrayParameter(&lhs_parm_type_loc,
                                                  lhs_parm_var_decl, result);
    } else if (lhs_parm_type_loc.getAs<clang::PointerTypeLoc>()) {
      lhs_key = getNodeFromDecl(lhs_parm_var_decl, result);
    } else if (const clang::TemplateSpecializationTypeLoc lhs_raw_ptr_type_loc =
                   lhs_parm_type_loc
                       .getAs<clang::TemplateSpecializationTypeLoc>()) {
      lhs_key = getNodeFromRawPtrTypeLoc(&lhs_raw_ptr_type_loc, result);
    } else {
      assert(false && "Unknown kind of clang::TypeLoc at `lhs_parm_type_loc`");
    }
  } else {
    // The function return type matches on RHS.
    const clang::PointerTypeLoc lhs_return_type_loc =
        lhs_func_proto_type_loc.getReturnLoc().getAs<clang::PointerTypeLoc>();
    assert(lhs_return_type_loc);
    lhs_key = getNodeFromPointerTypeLoc(&lhs_return_type_loc, result);
  }

  // Whenever RHS (function type) is rewritten, LHS (function pointer type)
  // should be rewritten, too.
  EmitEdge(lhs_key, rhs_key);
  EmitEdge(rhs_key, lhs_key);
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
// have `result` for arg1 at [1]. It's easier to create node_arg1_{1st,2nd} than
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

// If we rewrite a node, we generally don't want `reinterpret_cast`
// involved. We might replace it with
// *  `base::as_byte_span()`.
// *  some other spanification helper that computes a different-width
//    "view" of the underlying type.
// *  nothing, causing a compile error, letting a human deal with it.
//
// TODO(crbug.com/414914153): This currently only emits
// `base::as_byte_span()`. Have it do the other stuff, too.
void RemoveReinterpretCastExpr(const MatchFinder::MatchResult& result,
                               std::string_view node_key) {
  auto* cast_expr =
      result.Nodes.getNodeAs<clang::CXXReinterpretCastExpr>("reinterpret_cast");
  if (!cast_expr) {
    return;
  }

  // Repurpose the parentheses of `reinterpret_cast()` for our edit,
  // i.e. rewrite only this range:
  //
  // reinterpret_cast<T*>(...);
  // |------------------|
  const clang::SourceRange replacement_range = {
      cast_expr->getBeginLoc(),
      cast_expr->getAngleBrackets().getEnd().getLocWithOffset(1u)};

  if (result.Nodes.getNodeAs<clang::QualType>("reinterpret_cast_to_bytes")) {
    const bool target_type_is_const =
        GetNodeOrCrash<clang::QualType>(
            result, "target_type", "`reinterpret_cast` implies `target_type`")
            ->isConstQualified();
    std::string replacement = target_type_is_const
                                  ? "base::as_byte_span"
                                  : "base::as_writable_byte_span";

    return EmitReplacement(
        node_key, GetReplacementDirective(replacement_range, replacement,
                                          *result.SourceManager));
  }
}

std::string GetRHSImpl(const MatchFinder::MatchResult& result) {
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
    return GetNodeFromSizeExpr(size_expr, result);
  }

  // Not supposed to get here.
  llvm::errs() << "\n"
                  "Error: "
               << __FUNCTION__
               << " encountered an unexpected match.\n"
                  "Expected one of : \n"
                  "  - rhs_type_loc\n"
                  "  - rhs_raw_ptr_type_loc\n"
                  "  - rhs_array_type_loc\n"
                  "  - rhs_begin\n"
                  "  - member_data_call\n"
                  "  - size_node\n"
                  "\n";
  DumpMatchResult(result);
  assert(false);
}

// Extracts the rhs node from the match result.
std::string GetRHS(const MatchFinder::MatchResult& result) {
  std::string node_key = GetRHSImpl(result);
  RemoveReinterpretCastExpr(result, node_key);
  return node_key;
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
        raw_ptr_plugin::ImplicitFieldDeclaration(), isInExcludedMacroLocation(),
        raw_ptr_plugin::isInLocationListedInFilterFile(&paths_to_exclude_));

    // Standard exclusions include `raw_ptr` and `span`.
    auto exclusions = anyOf(
        frontier_exclusions,
        hasAncestor(cxxRecordDecl(anyOf(hasName("raw_ptr"), hasName("span")))));

    // Matches a pointer type, including `auto*`, but not `auto` which deduces
    // to a pointer type.
    auto non_auto_pointer_type = pointerType(pointee(qualType(unless(
        anyOf(qualType(hasDeclaration(
                  cxxRecordDecl(raw_ptr_plugin::isAnonymousStructOrUnion()))),
              hasUnqualifiedDesugaredType(
                  anyOf(functionType(), memberPointerType(), voidType())),
              // Exclude literal strings as these need to become string_view.
              hasCanonicalType(
                  anyOf(asString("const char"), asString("const wchar_t"),
                        asString("const char8_t"), asString("const char16_t"),
                        asString("const char32_t"))))))));
    // Matches a pointer type, including `auto` which deduces to a pointer type.
    auto pointer_type = type(anyOf(
        non_auto_pointer_type,
        autoType(hasDeducedType(anyOf(
            qualType(non_auto_pointer_type),
            decltypeType(hasUnderlyingType(qualType(non_auto_pointer_type)))))),
        decltypeType(hasUnderlyingType(qualType(non_auto_pointer_type)))));

    // Matches a pointer type loc without a restriction like `pointer_type`,
    // which excludes certain pointer types.
    //
    // If the pointee is qualified (e.g. `const`), make a note of that
    // for use in `getNodeFromPointerTypeLoc()`.
    auto pointer_type_loc = pointerTypeLoc(optionally(
        hasPointeeLoc(qualifiedTypeLoc().bind("qualified_type_loc"))));

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
        functionDecl(hasReturnTypeLoc(pointer_type_loc.bind("rhs_type_loc")),
                     exclude_literal_strings, unless(exclusions))));

    auto lhs_call_expr = callExpr(callee(
        functionDecl(hasReturnTypeLoc(pointer_type_loc.bind("lhs_type_loc")),
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
                    .bind("subscript_expr"),
                arraySubscriptExpr(
                    hasBase(
                        declRefExpr(to(varDecl(hasType(arrayType(hasElementType(
                                        qualType().bind("contained_type")))))))
                            .bind("container_decl_ref")),
                    optionally(hasIndex(integerLiteral(equals(0u))
                                            .bind("zero_container_offset"))))
                    .bind("subscript_expr"))))
            .bind("unaryOperator");

    // T* a = buf.data();
    auto member_data_call =
        cxxMemberCallExpr(
            callee(functionDecl(
                hasName("data"),
                hasParent(cxxRecordDecl(hasMethod(hasName("size")))))),
            has(memberExpr().bind("data_member_expr")))
            .bind("member_data_call");

    auto has_std_array_type = hasType(hasCanonicalType(hasDeclaration(
        classTemplateSpecializationDecl(hasName("::std::array")))));

    // Array excluded because it might be used as a buffer with >1 size.
    auto single_var_span_exclusions =
        unless(anyOf(exclusions, hasType(arrayType()), hasType(functionType()),
                     has_std_array_type));

    // Matches |&var| where |var| is a local variable, a parameter or member
    // field. Doesn't match when |var| is a function or an array.
    auto buff_address_from_single_var =
        unaryOperator(
            hasOperatorName("&"),
            hasUnaryOperand(anyOf(
                declRefExpr(to(anyOf(varDecl(single_var_span_exclusions),
                                     parmVarDecl(single_var_span_exclusions))))
                    .bind("address_expr_operand"),
                memberExpr(member(fieldDecl(single_var_span_exclusions)))
                    .bind("address_expr_operand"))))
            .bind("address_expr");

    // Matches `std::c?{begin,end}(c_array)`, which will be rewritten to
    // `SpanificationArrayC?{Begin,End}`.
    auto c_array_iter_call_expr =
        callExpr(callee(functionDecl(matchesName("std::c?(begin|end)"))),
                 hasArgument(0, hasType(arrayType())))
            .bind("c_array_iter_call_expr");

    // Used to look "outward" one layer from other expressions matched
    // below s.t. we can remove `reinterpret_cast` from spanified
    // things.
    //
    // Attached to matchers that compose into others, not just
    // `rhs_expr_variations`.
    //
    // TODO(414914153): this ought to work when attached directly to
    // `rhs_expr_variations`, but empirically we observe that it does
    // not. Investigate?
    const auto reinterpret_cast_wrapper = optionally(hasParent(
        cxxReinterpretCastExpr(
            hasDestinationType(qualType(pointsTo(
                qualType(anyOf(qualType(asString("uint8_t"))
                                   .bind("reinterpret_cast_to_bytes"),
                               qualType(isAnyCharacter())
                                   .bind("reinterpret_cast_to_bytes"),
                               qualType(isInteger())
                                   .bind("reinterpret_cast_to_integral_type")))
                    .bind("target_type")))),
            unless(isInExcludedMacroLocation()))
            .bind("reinterpret_cast")));

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
    //
    // This is put under the `reinterpret_cast` wrapper to handle the
    // case where we would end up with:
    //
    // base::span foo = reinterpret_cast<...>(bar.data());
    //
    // where `bar` has size information available, putting it under
    // this matcher.
    auto size_node_matcher = expr(
        anyOf(
            member_data_call,
            expr(anyOf(callExpr(callee(functionDecl(
                                           unsafeFunctionToBeRewrittenToMacro())
                                           .bind("unsafe_function_decl")))
                           .bind("unsafe_function_call_expr"),
                       c_array_iter_call_expr,
                       callExpr(callee(functionDecl(
                           hasReturnTypeLoc(pointer_type_loc),
                           anyOf(raw_ptr_plugin::isInThirdPartyLocation(),
                                 isExpansionInSystemHeader(),
                                 raw_ptr_plugin::isInExternCContext())))),
                       cxxNullPtrLiteralExpr().bind("nullptr_expr"),
                       cxxNewExpr(),
                       expr(buff_address_from_container)
                           .bind("container_buff_address"),
                       buff_address_from_single_var))
                .bind("size_node")),
        reinterpret_cast_wrapper);

    auto rhs_expr =
        expr(ignoringParenCasts(anyOf(
                 declRefExpr(to(anyOf(rhs_var, rhs_param))).bind("declRefExpr"),
                 memberExpr(member(rhs_field)).bind("memberExpr"),
                 rhs_call_expr.bind("callExpr"))))
            .bind("rhs_expr");

    auto get_calls_on_raw_ptr = cxxMemberCallExpr(
        callee(cxxMethodDecl(hasName("get"), ofClass(hasName("raw_ptr")))),
        has(memberExpr(has(rhs_expr))));

    // Much the same as `buff_address_from_container` above. This reuses
    // the same business logic in `EmitContainerPointerRewrites()`, but
    // specifically covers frontier parameters that already came from
    // nodes without size information.
    //
    // There are two obstacles to total unification:
    // 1. The call site of `EmitContainerPointerRewrites()` is hard to
    //    position. We need a key, and depending on the usage mode
    //    (this site, or the size nodes matched by
    //    `buff_address_from_container`), the surrounding plumbing
    //    also needs to change.
    // 2. The different bindings are hard to unify. Unification is also
    //    unlikely to greatly improve readability.
    auto index_into_pointer =
        unaryOperator(
            hasOperatorName("&"),
            hasUnaryOperand(anyOf(
                arraySubscriptExpr(
                    hasBase(declRefExpr(to(rhs_var)).bind("rhs_expr")),
                    optionally(hasIndex(integerLiteral(equals(0u))
                                            .bind("zero_container_offset"))))
                    .bind("subscript_expr"),
                cxxOperatorCallExpr(
                    callee(functionDecl(hasName("operator[]"))),
                    hasArgument(0, hasType(raw_ptr_type)),
                    hasDescendant(declRefExpr(to(rhs_var)).bind("rhs_expr")),
                    optionally(
                        hasDescendant(integerLiteral(equals(0u))
                                          .bind("zero_container_offset"))))
                    .bind("subscript_expr"))))
            .bind("unaryOperator");

    auto rhs_exprs_without_size_nodes =
        expr(ignoringParenCasts(anyOf(
                 rhs_expr,
                 binaryOperation(
                     binary_plus_or_minus_operation(
                         binaryOperation(hasLHS(rhs_expr), hasOperatorName("+"),
                                         unless(isInExcludedMacroLocation()))),
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
                 get_calls_on_raw_ptr,
                 expr(index_into_pointer).bind("container_buff_address"))),
             reinterpret_cast_wrapper)
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
                     hasLHS(lhs_expr_variations),
                     hasRHS(expr(hasType(isInteger())))),
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
             unless(isInExcludedMacroLocation()))
            .bind("deref_expr"));
    Match(deref_expression, DecaySpanToPointer);

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
    auto boolean_op_operand =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 expr(rhs_exprs_without_size_nodes).bind("boolean_op_operand"));
    auto raw_ptr_op_bool_call_expr =
        cxxMemberCallExpr(on(boolean_op_operand),
                          callee(cxxMethodDecl(hasName("operator bool"),
                                               ofClass(hasName("raw_ptr")))));
    auto boolean_op = traverse(
        clang::TK_AsIs,
        expr(anyOf(implicitCastExpr(
                       hasCastKind(clang::CastKind::CK_PointerToBoolean),
                       hasSourceExpression(boolean_op_operand)),
                   implicitCastExpr(has(raw_ptr_op_bool_call_expr))),
             optionally(hasParent(
                 unaryOperator(hasOperatorName("!")).bind("logical_not_op")))));
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
            callExpr(
                callee(functionDecl(
                    frontier_exclusions,
                    unless(matchesName(
                        "std::(size|c?r?begin|c?r?end|empty|swap|ranges::)")))),
                forEachArgumentWithParam(expr(rhs_exprs_without_size_nodes),
                                         parmVarDecl())),
            cxxConstructExpr(
                hasDeclaration(cxxConstructorDecl(frontier_exclusions)),
                forEachArgumentWithParam(expr(rhs_exprs_without_size_nodes),
                                         parmVarDecl())))));
    Match(buffer_to_external_func, AppendDataCall);

    // Handles unary arithmetic operations (pre/post increment)
    auto unary_op = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(ignoringParenCasts(anyOf(
                 unaryOperator(hasOperatorName("++"), hasUnaryOperand(rhs_expr))
                     .bind("unaryOperator"),
                 cxxOperatorCallExpr(
                     callee(cxxMethodDecl(ofClass(hasName("raw_ptr")))),
                     hasOperatorName("++"), hasArgument(0, rhs_expr))
                     .bind("raw_ptr_operator++"))))
            .bind("unary_op"));
    Match(unary_op, RewriteUnaryOperation);

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
    auto binary_op =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 expr(ignoringParenCasts(binaryOperation(
                     binary_plus_or_minus_operation(
                         binaryOperation(hasLHS(rhs_expr), hasOperatorName("+"),
                                         hasRHS(expr(hasType(isInteger()))))
                             .bind("binary_operation")),
                     hasRHS(expr().bind("binary_op_rhs")),
                     unless(hasParent(binaryOperation(anyOf(
                         hasOperatorName("+"), hasOperatorName("-")))))))));
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
    auto assignment_relationship = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        binaryOperation(hasOperatorName("="),
                        hasOperands(lhs_expr_variations,
                                    anyOf(rhs_expr_variations,
                                          conditionalOperator(hasTrueExpression(
                                              rhs_expr_variations)))),
                        unless(isExpansionInSystemHeader())));
    Match(assignment_relationship, MatchAdjacency);

    // Creates the edge from lhs to false_expr in a ternary conditional
    // operator.
    auto assignment_relationship2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        binaryOperation(hasOperatorName("="),
                        hasOperands(lhs_expr_variations,
                                    conditionalOperator(hasFalseExpression(
                                        rhs_expr_variations))),
                        unless(isExpansionInSystemHeader())));
    Match(assignment_relationship2, MatchAdjacency);

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
    // it == std::begin(c_array)
    // it != std::end(c_array)
    auto equality_op =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 binaryOperation(
                     anyOf(hasOperatorName("=="), hasOperatorName("!=")),
                     hasOperands(ignoringParenCasts(lhs_expr_variations),
                                 ignoringParenCasts(c_array_iter_call_expr))));
    Match(equality_op, RewriteComparisonWithCArrayIter);

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
                hasReturnTypeLoc(pointer_type_loc.bind("lhs_type_loc")),
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
                       hasReturnTypeLoc(pointer_type_loc.bind("lhs_type_loc")),
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

    // Handles member field initializers.
    auto field_init = fieldDecl(lhs_field, has(rhs_expr_variations),
                                unless(isExpansionInSystemHeader()));
    Match(field_init, MatchAdjacency);

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

    // Function pointer types to arbitrary function types, including typedef
    // types and using-aliased types to function pointer types. No restriction
    // to parameter types and return type, but the following queries require
    // the function type to be compatible with the RHS function type.
    auto fct_ptr_type = type(hasUnqualifiedDesugaredType(
        pointerType(pointee(ignoringParens(functionProtoType())))));

    // Function declaration with pointer/array/raw_ptr parameter types and/or
    // pointer return type.
    //
    // Note that this query matches each of parameter types and return type
    // respectively.
    auto fct_decl =
        functionDecl(
            eachOf(forEachParmVarDecl(rhs_param),
                   hasReturnTypeLoc(pointer_type_loc.bind("rhs_type_loc"))),
            unless(exclusions))
            .bind("fct_decl");
    auto fct_decl_expr = expr(ignoringParenCasts(declRefExpr(to(fct_decl))));

    // Supports:
    //     void (*var)(int*) = func;
    //     int* (*var)() = func;
    // and equivalent typedef/using variants like:
    //     using FuncType = void (*)(int*);
    //     FuncType var = func;
    auto fct_ptr_var_construction = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        varDecl(hasType(fct_ptr_type), has(fct_decl_expr), unless(exclusions))
            .bind("lhs_funcptrvardecl"));
    Match(fct_ptr_var_construction, RewriteFunctionPointerType);

    // Supports:
    //     void (*var)(int*); var = func;
    //     int* (*var)(); var = func;
    // and equivalent typedef/using variants like:
    //     typedef int* (*FuncType)();
    //     FuncType var;
    //     var = func;
    auto fct_ptr_var_assignment = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        binaryOperator(hasOperatorName("="),
                       hasLHS(declRefExpr(
                           to(varDecl(hasType(fct_ptr_type), unless(exclusions))
                                  .bind("lhs_funcptrvardecl")))),
                       hasRHS(fct_decl_expr)));
    Match(fct_ptr_var_assignment, RewriteFunctionPointerType);

    // Map function declaration signature to function definition signature;
    // This is problematic in the case of callbacks defined in function.
    auto fct_decls = traverse(clang::TK_IgnoreUnlessSpelledInSource, fct_decl);
    Match(fct_decls, RewriteFunctionParamAndReturnType);
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
