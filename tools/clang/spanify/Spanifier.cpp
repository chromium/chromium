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
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::ast_matchers;

namespace {

// Special keywords:
constexpr char kEmptyKeyword[] = "<empty>";

const char kBaseSpanIncludePath[] = "base/containers/span.h";

// Include path that needs to be added to all the files where
// base::raw_span<...> replaces a raw_ptr<...>.
const char kBaseRawSpanIncludePath[] = "base/memory/raw_span.h";

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

std::string EscapeReplacementText(std::string text) {
  static const std::string_view escaped = "\n\r%@,:<>";
  static const std::string_view hex = "0123456789ABCDEF";

  // <empty> is a special keyword. It is never escaped.
  if (text == kEmptyKeyword) {
    return text;
  }

  std::string out;
  for (auto ch : text) {
    if (escaped.find(ch) != std::string_view::npos) {
      uint8_t value = static_cast<uint8_t>(ch);
      out += '%';
      out += hex[(value >> 4) & 0x0F];
      out += hex[(value >> 0) & 0x0F];
    } else {
      out += ch;
    }
  }
  return out;
}

struct Node {
  bool is_buffer = false;

  // A replacement follows the following format:
  // `r:::<file path>:::<offset>:::<length>:::<replacement text>`
  std::string replacement;

  // An include directive follows the following format:
  // `include-user-header:::<file path>:::-1:::-1:::<include text>`
  std::string include_directive;

  // This is true for nodes representing the following:
  //  - nullptr => size is zero
  //  - calls to new/new[n] => size is 1/n
  //  - constant arrays buf[1024] => size is 1024
  //  - calls to third_party functions that we can't rewrite (they should
  //    provide a size for the pointer returned)
  bool size_info_available = false;

  // This is true for dereference expressions.
  // Example: *buf, *fct(), *(buf++), ...
  bool is_deref_expr = false;

  // This is true for the cases where the lhs node doesn't get rewritten while
  // the rhs does. in that case, we create a special node that adds a `.data()`
  // call to the rhs. Example: ptr[index] = something; => ptr is used as a
  // buffer => gets spanified T* temp = ptr; => temp never used as a buffer =>
  // need to add `.data()` The statement becomes: T* temp = ptr.data();
  bool is_data_change = false;

  bool operator==(const Node& other) const {
    return replacement == other.replacement;
  }

  bool operator<(const Node& other) const {
    return replacement < other.replacement;
  }

  // The resulting string follows the following format:
  // {is_buffer\,r:::<filepath>:::<offset>:::<length>:::<replacement_text>
  //\,include-user-header:::<file path>:::-1:::-1:::<include
  // text>\,size_info_available\,is_deref_expr\,is_data_change}
  // where the booleans are represented as 0 or 1.
  std::string ToString() const {
    return llvm::formatv("{{{0:d}\\,{1}\\,{2}\\,{3:d}\\,{4:d}\\,{5:d}}",
                         is_buffer, replacement, include_directive,
                         size_info_available, is_deref_expr, is_data_change);
  }
};

// Helper class to add edges to the set of node_pairs_;
class OutputHelper {
 public:
  OutputHelper() = default;

  void AddEdge(const Node& lhs, const Node& rhs) {
    node_pairs_.insert(
        llvm::formatv("{0}@{1}\n", lhs.ToString(), rhs.ToString()));
  }

  void AddSingleNode(const Node& lhs) {
    node_pairs_.insert(llvm::formatv("{0}\n", lhs.ToString()));
  }

  void Emit() {
    for (const auto& p : node_pairs_) {
      llvm::outs() << p;
    }
  }

 private:
  // This represents a line for every 2 adjacent nodes.
  // The format is: {lhs};{rhs}\n where lhs & rhs are generated using
  // Node::ToString().
  // Buffer expressions are added to the graph as a single node
  // in which case the line is {lhs};\n
  std::set<std::string> node_pairs_;
};

static std::pair<std::string, std::string> GetReplacementAndIncludeDirectives(
    const clang::SourceRange replacement_range,
    std::string replacement_text,
    const clang::SourceManager& source_manager,
    const char* include_path = nullptr,
    bool is_system_include_path = false) {
  clang::tooling::Replacement replacement(
      source_manager, clang::CharSourceRange::getCharRange(replacement_range),
      replacement_text);
  llvm::StringRef file_path = replacement.getFilePath();
  if (file_path.empty()) {
    return {"", ""};
  }
  // If `replacement_text` is a special keyword, e.g. "<empty>", should not
  // escape `replacement_text`.
  replacement_text = EscapeReplacementText(replacement_text);
  std::string replacement_directive = llvm::formatv(
      "r:::{0}:::{1}:::{2}:::{3}", file_path, replacement.getOffset(),
      replacement.getLength(), replacement_text);

  if (!include_path) {
    include_path = kBaseSpanIncludePath;
    is_system_include_path = false;
  }
  std::string include_directive;
  if (is_system_include_path) {
    include_directive = llvm::formatv(
        "include-system-header:::{0}:::-1:::-1:::{1}", file_path, include_path);
  } else {
    include_directive = llvm::formatv(
        "include-user-header:::{0}:::-1:::-1:::{1}", file_path, include_path);
  }

  return {replacement_directive, include_directive};
}

// Clang doesn't seem to be providing correct begin/end locations for
// clang::MemberExpr and clang::DeclRefExpr. This function handles these cases,
// otherwise returns expression's begin_loc and end_loc offset by 1.
clang::SourceRange getExprRange(const clang::Expr* expr) {
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

  return {expr->getBeginLoc(), expr->getEndLoc().getLocWithOffset(1)};
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

// This functions generates a string representing the converted type from a
// raw pointer type to a base::span type. It handles preservation of
// const/volatile qualifiers and uses a specific printing policy to format the
// underlying pointee type.
// This functions generates a string representing the converted type from a
// raw pointer type to a base::span type. It handles preservation of
// const/volatile qualifiers and uses a specific printing policy to format the
// underlying pointee type.
std::string GenerateSpanType(clang::SourceManager& source_manager,
                             const clang::ASTContext& ast_context,
                             const clang::DeclaratorDecl& decl) {
  // Preserve qualifiers.
  const clang::QualType& pointer_type = decl.getType();
  std::ostringstream qualifiers;
  qualifiers << (pointer_type.isConstQualified() ? "const " : "")
             << (pointer_type.isVolatileQualified() ? "volatile " : "");

  // If the original type cannot be recovered from the source, we need to
  // consult the clang deduced type.
  //
  // Please note that the deduced type may not be the same as the original type.
  // For example, if we have the following code:
  //   const auto* p = get_buffer<uint16_t>();
  // we will get:`unsigned short` instead of `uint16_t`.
  std::string type =
      GetTypeAsString(pointer_type->getPointeeType(), ast_context);
  return qualifiers.str() + llvm::formatv("base::span<{0}>", type).str();
}

// It is intentional that this function ignores cast expressions and applies
// the `.data()` addition to the internal expression. if we have:
// type* ptr = reinterpret_cast<type*>(buf);  where buf needs to be rewritten
// to span and ptr doesn't. The `.data()` call is added right after buffer as
// follows: type* ptr = reinterpret_cast<type*>(buf.data());
static clang::SourceRange getSourceRange(
    const MatchFinder::MatchResult& result) {
  if (auto* op =
          result.Nodes.getNodeAs<clang::UnaryOperator>("unaryOperator")) {
    if (op->isPostfix()) {
      return {op->getBeginLoc(), op->getEndLoc().getLocWithOffset(2)};
    }
    auto* expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr");
    return {op->getBeginLoc(), getExprRange(expr).getEnd()};
  }
  if (auto* op = result.Nodes.getNodeAs<clang::Expr>("binaryOperator")) {
    auto* sub_expr = result.Nodes.getNodeAs<clang::Expr>("bin_op_rhs");
    auto end_loc = getExprRange(sub_expr).getEnd();
    return {op->getBeginLoc(), end_loc};
  }
  if (auto* op = result.Nodes.getNodeAs<clang::CXXOperatorCallExpr>(
          "raw_ptr_operator++")) {
    auto* callee = op->getDirectCallee();
    if (callee->getNumParams() == 0) {  // postfix op++ on raw_ptr;
      auto* expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr");
      return clang::SourceRange(getExprRange(expr).getEnd());
    }
    return clang::SourceRange(op->getEndLoc().getLocWithOffset(2));
  }

  auto* expr = result.Nodes.getNodeAs<clang::Expr>("rhs_expr");
  return clang::SourceRange(getExprRange(expr).getEnd());
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

static Node getNodeFromPointerTypeLoc(const clang::PointerTypeLoc* type_loc,
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
  auto replacement_and_include_pair = GetReplacementAndIncludeDirectives(
      replacement_range, replacement_text, source_manager);
  Node n;
  n.replacement = replacement_and_include_pair.first;
  n.include_directive = replacement_and_include_pair.second;
  return n;
}

static Node getNodeFromRawPtrTypeLoc(
    const clang::TemplateSpecializationTypeLoc* raw_ptr_type_loc,
    const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  auto replacement_range = clang::SourceRange(raw_ptr_type_loc->getBeginLoc(),
                                              raw_ptr_type_loc->getLAngleLoc());

  auto replacement_and_include_pair = GetReplacementAndIncludeDirectives(
      replacement_range, "base::raw_span", source_manager,
      kBaseRawSpanIncludePath);
  Node n;
  n.replacement = replacement_and_include_pair.first;
  n.include_directive = replacement_and_include_pair.second;
  return n;
}

static Node getNodeFromDecl(const clang::DeclaratorDecl* decl,
                            const MatchFinder::MatchResult& result) {
  clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  clang::SourceRange replacement_range{decl->getBeginLoc(),
                                       decl->getLocation()};
  auto replacement_text = GenerateSpanType(source_manager, ast_context, *decl);
  auto replacement_and_include_pair = GetReplacementAndIncludeDirectives(
      replacement_range, replacement_text, source_manager);
  Node n;
  n.replacement = replacement_and_include_pair.first;
  n.include_directive = replacement_and_include_pair.second;
  return n;
}

static Node getNodeFromDerefExpr(const clang::Expr* deref_expr,
                                 const MatchFinder::MatchResult& result) {
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

  auto replacement_and_include_pair = GetReplacementAndIncludeDirectives(
      source_range, replacement_text, source_manager);
  Node n;
  n.replacement = replacement_and_include_pair.first;
  n.include_directive = "<empty>";
  n.is_deref_expr = true;
  return n;
}

static Node getNodeFromMemberCallExpr(const clang::CXXMemberCallExpr* get_call,
                                      const char* member_expr_id,
                                      const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::MemberExpr* member_expr =
      result.Nodes.getNodeAs<clang::MemberExpr>(member_expr_id);
  clang::SourceLocation begin_loc = member_expr->getMemberLoc();
  size_t member_name_length =
      member_expr->getMemberDecl()->getName().size() + 2;
  clang::SourceLocation end_loc =
      begin_loc.getLocWithOffset(member_name_length);
  begin_loc = begin_loc.getLocWithOffset(-1);
  clang::SourceRange replacement_range(begin_loc, end_loc);

  // This deletes the member call expression part. Example:
  // char* ptr = member_.get(); which is then rewritten to
  // span<char> ptr = member_;
  // member_ here is a raw_ptr
  auto replacement_and_include_pair = GetReplacementAndIncludeDirectives(
      replacement_range, " ", source_manager);
  Node n;
  n.replacement = replacement_and_include_pair.first;
  n.include_directive = replacement_and_include_pair.second;
  return n;
}

static Node getNodeFromCallToExternalFunction(
    const MatchFinder::MatchResult& result) {
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
  auto replacement_and_include_pair = GetReplacementAndIncludeDirectives(
      rep_range, replacement_text, source_manager);
  Node n;
  n.replacement = replacement_and_include_pair.first;
  n.include_directive = "<empty>";
  n.is_deref_expr = true;
  return n;
}

static Node getNodeFromSizeExpr(const clang::Expr* size_expr,
                                const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  std::string replacement = kEmptyKeyword;
  clang::SourceRange replacement_range;
  if (const auto* nullptr_expr =
          result.Nodes.getNodeAs<clang::CXXNullPtrLiteralExpr>(
              "nullptr_expr")) {
    replacement = "{}";
    // The hardcoded offset corresponds to the length of "nullptr" keyword.
    replacement_range = {nullptr_expr->getBeginLoc(),
                         nullptr_expr->getBeginLoc().getLocWithOffset(7)};
  } else {
    // Generate empty insertion just to keep track of the node's loc;
    replacement_range =
        clang::SourceRange(size_expr->getSourceRange().getBegin(),
                           size_expr->getSourceRange().getBegin());
  }

  auto replacement_and_include_pair = GetReplacementAndIncludeDirectives(
      replacement_range, replacement, source_manager);
  Node n;
  n.size_info_available = true;
  n.replacement = replacement_and_include_pair.first;
  n.include_directive = replacement_and_include_pair.second;
  return n;
}

static Node getDataChangeNode(const std::string& lhs_replacement,
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
  auto replacement_and_include_pair = GetReplacementAndIncludeDirectives(
      rep_range, replacement_text, source_manager);
  Node data_node;
  data_node.replacement = replacement_and_include_pair.first;
  // We need a way to check whether the lhs node was rewritten, in which
  // case we don't need to add this change. We achieve this by storing the
  // lhs key (the replacement which is unique) in the data_node's include
  // directive.
  data_node.include_directive = lhs_replacement;
  data_node.is_data_change = true;
  return data_node;
}

// Gets the array size as written in the source code (if possible), otherwise
// relies on the compile time value as seen in the ConstantArrayType.
// Returns an empty string in case of error.
std::string getArraySize(const MatchFinder::MatchResult& result) {
  const clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;
  const auto& lang_opts = ast_context.getLangOpts();

  const auto* type_loc =
      result.Nodes.getNodeAs<clang::TypeLoc>("array_type_loc");

  auto array_type_loc = type_loc->getAs<clang::ArrayTypeLoc>();

  // This is the case for arrays where the size expression is omitted. Example:
  // int a[] = {1,2,3,4};
  // For such cases, we rely on getting the compile-time size from the
  // ConstantArrayType below.
  if (array_type_loc.getLBracketLoc() != array_type_loc.getRBracketLoc()) {
    auto source_range =
        clang::SourceRange(array_type_loc.getLBracketLoc().getLocWithOffset(1),
                           array_type_loc.getRBracketLoc());
    auto size_text = clang::Lexer::getSourceText(
                         clang::CharSourceRange::getCharRange(source_range),
                         source_manager, lang_opts)
                         .str();
    if (!size_text.empty()) {
      return size_text;
    }
  }
  auto* array_type = result.Nodes.getNodeAs<clang::ArrayType>("array_type");
  if (const clang::ConstantArrayType* type =
          clang::dyn_cast<clang::ConstantArrayType>(array_type)) {
    return std::to_string(*type->getSize().getRawData());
  }
  assert(false && "Unable to determine array size.");
}

// Takes in a copy of a variable assumed to be in snake_case and switches it
// into CamelCase.
std::string snakeCaseToCamelCase(std::string snake_case) {
  // We want the first char to be capitalized so start with '_'.
  char prev = '_';
  for (char& c : snake_case) {
    if (prev == '_') {
      c = std::toupper(c);
    }
    prev = c;
  }
  // Now we need to remove the '_'s from the string, recall std::remove moves
  // everything to the end and then returns the first '_' (or end()). We then
  // call erase from there to the end to actually remove.
  snake_case.erase(std::remove(snake_case.begin(), snake_case.end(), '_'),
                   snake_case.end());
  return snake_case;
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
  if (!element_type->hasUnnamedOrLocalType()) {
    return std::make_pair("", "");
  }

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
      new_class_name_string = snakeCaseToCamelCase(array_variable_as_string);
      clang::StringRef new_class_name(new_class_name_string);
      clang::IdentifierInfo& new_class_name_identifier =
          ast_context.Idents.get(new_class_name);
      temporal_class_name = ast_context.DeclarationNames.getIdentifier(
          &new_class_name_identifier);
      record_decl->setDeclName(temporal_class_name);
    }
    if (has_definition) {
      clang::PrintingPolicy printing_policy(ast_context.getLangOpts());
      // Because of class/struct definition, we will drop any qualifiers from
      // `element_type`. E.g. `const struct { int val; }` must be
      // `struct { int val; }`.
      clang::QualType new_qual_type(element_type.getTypePtr(), 0);
      printing_policy.SuppressScope = 0;
      printing_policy.SuppressUnwrittenScope = 1;
      printing_policy.SuppressElaboration = 0;
      printing_policy.SuppressInlineNamespace = 1;
      printing_policy.SuppressDefaultTemplateArgs = 1;
      printing_policy.PrintCanonicalTypes = 0;
      printing_policy.IncludeTagDefinition = 1;
      printing_policy.AnonymousTagLocations = 1;
      class_definition = new_qual_type.getAsString(printing_policy) + ";\n";
    }
    if (is_unnamed) {
      record_decl->setDeclName(original_name);
    }
  }
  return std::make_pair(new_class_name_string, class_definition);
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

// Returns true if the array's initializer list needs one more brace({}) after
// rewriting into `std::array<>`.
bool ArrayInitListNeedsExtraBrace(clang::QualType element_type,
                                  const clang::InitListExpr* init_list_expr) {
  if (!init_list_expr) {
    return false;
  }
  if (!(element_type->isArrayType() ||
        element_type->isStructureOrClassType())) {
    return false;
  }
  // `std::array` is a struct that encapsulates a fixed-size array as a member
  // variable. It's an aggregate type, meaning it can be initialized using
  // aggregate initialization (like plain arrays and structs).  Unlike
  // `std::vector` or other containers, `std::array` doesn't have constructors
  // that explicitly take initializer lists. However, aggregate initialization
  // allows sub elements to be initialized with initializer lists.
  //
  // The extra brace is required to indicate that the inner initializer list
  // corresponds to the initialization of a single element of the inner
  // array, not a field of the wrapper (e.g. a field of std::array).
  //
  // For instance:
  // std::array<Aggregate, 1> buffer =
  // {      // Initialization of std::array<Aggregate,1>
  //   {    // Initialization of std::array<Aggregate,1>::inner_ C-style array.
  //     {  // Initialization of Aggregate
  //        1,2,3
  //     }
  //   }
  // }
  //
  for (unsigned int index = 0; index < init_list_expr->getNumInits(); ++index) {
    const clang::Expr* expr = init_list_expr->getInit(index);
    if (clang::dyn_cast_or_null<clang::InitListExpr>(expr)) {
      return true;
    }
  }
  return false;
}

// Creates a replacement node for c-style arrays on which we invoke operator[].
// These arrays are rewritten to std::array<Type, Size>.
Node getNodeFromArrayType(const MatchFinder::MatchResult& result) {
  clang::SourceManager& source_manager = *result.SourceManager;
  const clang::ASTContext& ast_context = *result.Context;

  auto* array_type_loc =
      result.Nodes.getNodeAs<clang::TypeLoc>("array_type_loc");
  auto* array_type = result.Nodes.getNodeAs<clang::ArrayType>("array_type");
  auto* array_variable =
      result.Nodes.getNodeAs<clang::VarDecl>("array_variable");

  auto element_type = array_type->getElementType();

  std::string element_type_as_string;

  // If the `element_type` is an elaborated type with a keyword, i.e.
  // `struct`, `class`, `union`, we will create another ElaboratedType
  // without the keyword. So `struct funcHasName` will be `funcHasHame`.
  if (element_type->isElaboratedTypeSpecifier()) {
    auto original_type = element_type->getAs<clang::ElaboratedType>();
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
    element_type_as_string = GetTypeAsString(element_type, ast_context);
  }

  std::string array_size_as_string = getArraySize(result);
  std::string array_variable_as_string = array_variable->getNameAsString();
  std::stringstream qualifier_string;

  //   static const char* array[32] = ...;
  //   |            |
  //   |            +--array_type_loc->getSourceRange().getBegin()
  //   |
  //   +---- array_variable->getSourceRange().getBegin()
  //
  // The `static` is a part of `VarDecl`, but the `const` is a part of
  // the element type, i.e. `const char*`.
  //
  // The array must be rewritten into:
  //
  //   static std::array<const char*, 32> array = ...;
  //
  // So the `replace_range` need to include the `const`.
  clang::SourceRange replacement_range = {
      array_variable->getSourceRange().getBegin(),
      array_type_loc->getSourceRange().getEnd().getLocWithOffset(1)};

  if (array_variable->isConstexpr()) {
    qualifier_string << "constexpr ";
  }
  if (array_variable->isStaticLocal()) {
    qualifier_string << "static ";
  }

  const auto& [unnamed_class, class_definition] = maybeGetUnnamedAndDefinition(
      element_type, array_variable, array_variable_as_string, ast_context);
  if (!unnamed_class.empty()) {
    element_type_as_string = unnamed_class;
  }

  // `const int buf[] = ...` must be `const std::array<int,...> buf = ...`.
  if (!element_type->isPointerOrReferenceType() &&
      element_type.isConstant(ast_context)) {
    qualifier_string << "const ";
  }

  const clang::InitListExpr* init_list_expr = GetArrayInitList(array_variable);

  std::string replacement_text;
  if (ArrayInitListNeedsExtraBrace(element_type, init_list_expr)) {
    clang::Rewriter rw(source_manager, ast_context.getLangOpts());
    std::string init_expr_as_string = rw.getRewrittenText(clang::SourceRange(
        init_list_expr->getBeginLoc(), init_list_expr->getEndLoc()));

    replacement_range =
        clang::SourceRange(array_variable->getSourceRange().getBegin(),
                           init_list_expr->getEndLoc().getLocWithOffset(1));
    replacement_text = llvm::formatv(
        "std::array<{0},{1}> {2} = {{{3}}", element_type_as_string,
        array_size_as_string, array_variable_as_string, init_expr_as_string);
  } else {
    replacement_text =
        llvm::formatv("std::array<{0},{1}> {2}", element_type_as_string,
                      array_size_as_string, array_variable_as_string);
  }

  auto replacement_and_include_pair = GetReplacementAndIncludeDirectives(
      replacement_range,
      class_definition + qualifier_string.str() + replacement_text,
      source_manager, "array",
      /* is_system_include_header =*/true);
  Node n;
  n.replacement = replacement_and_include_pair.first;
  n.include_directive = replacement_and_include_pair.second;
  n.size_info_available = true;
  return n;
}

// Called when the Match registered for it was successfully found in the AST.
// The matches registered represent two categories:
//   1- An adjacency relationship
//      In that case, a node pair is created, using matched node ids, and added
//      to the node_pair list using `OutputHelper::AddEdge`
//   2- A single is_buffer node match
//      In that case, a single node is created and added to the node_pair list
//      using `OutputHelper::AddSingleNode`
class PotentialNodes : public MatchFinder::MatchCallback {
 public:
  explicit PotentialNodes(OutputHelper& helper) : output_helper_(helper) {}

  PotentialNodes(const PotentialNodes&) = delete;
  PotentialNodes& operator=(const PotentialNodes&) = delete;

  // Extracts the lhs node from the match result.
  Node getLHSNodeFromMatchResult(const MatchFinder::MatchResult& result) {
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

    if (auto* deref_op = result.Nodes.getNodeAs<clang::Expr>("deref_expr")) {
      return getNodeFromDerefExpr(deref_op, result);
    }

    if (auto* get_call = result.Nodes.getNodeAs<clang::CXXMemberCallExpr>(
            "raw_ptr_get_call")) {
      Node n = getNodeFromMemberCallExpr(get_call, "get_member_expr", result);
      n.include_directive = "<empty>";
      n.is_deref_expr = true;
      return n;
    }

    if (result.Nodes.getNodeAs<clang::Expr>(
            "passing_a_buffer_to_third_party_function")) {
      return getNodeFromCallToExternalFunction(result);
    }

    if (result.Nodes.getNodeAs<clang::VarDecl>("array_variable")) {
      return getNodeFromArrayType(result);
    }
    assert(false);
  }

  // Extracts the rhs node from the match result.
  Node getRHSNodeFromMatchResult(const MatchFinder::MatchResult& result) {
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

    if (const clang::CXXMemberCallExpr* data_call =
            result.Nodes.getNodeAs<clang::CXXMemberCallExpr>(
                "member_data_call")) {
      auto node =
          getNodeFromMemberCallExpr(data_call, "data_member_expr", result);
      node.size_info_available = true;
      return node;
    }

    if (const clang::Expr* size_expr =
            result.Nodes.getNodeAs<clang::Expr>("size_node")) {
      return getNodeFromSizeExpr(size_expr, result);
    }
    // Not supposed to get here.
    assert(false);
  }

  // MatchFinder::MatchCallback:
  void run(const MatchFinder::MatchResult& result) override {
    Node lhs = getLHSNodeFromMatchResult(result);

    // Buffer usage expressions are added as a single node, return
    // early in this case.
    if (result.Nodes.getNodeAs<clang::Expr>("buffer_expr")) {
      lhs.is_buffer = true;
      output_helper_.AddSingleNode(lhs);
      return;
    }

    Node rhs = getRHSNodeFromMatchResult(result);

    auto* expr = result.Nodes.getNodeAs<clang::Expr>("span_frontier");
    if (expr && !lhs.is_deref_expr && !rhs.size_info_available) {
      // Node to add `.data()`;
      // This is needed in the case where rhs is rewritten and lhs is not.
      // Adding `.data()` is thus needed to extract the pointer since lhs and
      // rhs no longer have the same type.
      Node data_node = getDataChangeNode(lhs.replacement, result);
      output_helper_.AddEdge(data_node, rhs);
    }

    output_helper_.AddEdge(lhs, rhs);
  }

 private:
  OutputHelper& output_helper_;
};

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
      std::map<std::string, std::set<Node>>& sig_nodes,
      std::vector<std::pair<std::string, std::string>>& sig_pairs)
      : fct_sig_nodes_(sig_nodes), fct_sig_pairs_(sig_pairs) {}

  FunctionSignatureNodes(const FunctionSignatureNodes&) = delete;
  FunctionSignatureNodes& operator=(const FunctionSignatureNodes&) = delete;

  // Key here means a unique string generated from a function signature
  std::string GetKey(const clang::FunctionDecl* fct_decl,
                     const clang::SourceManager& source_manager) {
    auto name = fct_decl->getNameInfo().getName().getAsString();
    clang::SourceLocation start_loc = fct_decl->getBeginLoc();
    // This is done here to get the spelling loc of a functionDecl. This is
    // needed to handle cases where the function is in a Macro Expansion.
    clang::SourceRange replacement_range(source_manager.getFileLoc(start_loc),
                                         source_manager.getFileLoc(start_loc));
    clang::tooling::Replacement replacement(
        source_manager, clang::CharSourceRange::getCharRange(replacement_range),
        name.c_str());
    llvm::StringRef file_path = replacement.getFilePath();

    return llvm::formatv("r:::{0}:::{1}:::{2}:::{3}", file_path,
                         replacement.getOffset(), replacement.getLength(),
                         name.c_str());
  }

  Node getNodeFromMatchResult(const MatchFinder::MatchResult& result) {
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
    assert(false);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const clang::SourceManager& source_manager = *result.SourceManager;
    const clang::FunctionDecl* fct_decl =
        result.Nodes.getNodeAs<clang::FunctionDecl>("fct_decl");
    const clang::CXXMethodDecl* method_decl =
        result.Nodes.getNodeAs<clang::CXXMethodDecl>("fct_decl");

    const std::string current_key = GetKey(fct_decl, source_manager);

    // Function related by separate declaration and definition:
    {
      for (auto* previous_decl = fct_decl->getPreviousDecl(); previous_decl;
           previous_decl = previous_decl->getPreviousDecl()) {
        // TODO(356666773): The `previous_decl` might be part of third_party/.
        // Then it won't be matched by the matcher. So only one of the pair
        // would have a node.
        const std::string previous_key = GetKey(previous_decl, source_manager);
        fct_sig_pairs_.push_back({
            current_key,
            previous_key,
        });
      }
    }

    // Function related by overriding:
    if (method_decl) {
      for (auto* m : method_decl->overridden_methods()) {
        const std::string previous_key = GetKey(m, source_manager);
        fct_sig_pairs_.push_back({
            current_key,
            previous_key,
        });
      }
    }

    Node n = getNodeFromMatchResult(result);
    fct_sig_nodes_[current_key].insert(n);
  }

 private:
  // Map a function signature, which is modeled as a string representing file
  // location, to its matched graph nodes (RTNode and ParmVarDecl nodes).
  // Note: `RTNode` represents a function return type node.
  // In order to avoid relying on the order with which nodes are matched in
  // the AST, and to guarantee that nodes are stored in the file declaration
  // order, we use a `std::set<Node>` which sorts Nodes based on the replacement
  // directive which contains the file offset of a given node.
  // Note that a replacement directive has the following format:
  // `r:::<file path>:::<offset>:::<length>:::<replacement text>`
  // The order is important because at the end of a tool run on a
  // translationUnit, for each pair of function signatures, we iterate
  // concurrently through the two sets of Nodes creating edges between nodes
  // that appear at the same index.
  // AddEdge(first function's node1, second function's node1)
  // AddEdge(first function's node2, second function's node2)
  // and so on...
  std::map<std::string, std::set<Node>>& fct_sig_nodes_;

  // Map related function signatures to each other, this is needed for
  // functions
  // with separate definition and declaration, and for overridden functions.
  std::vector<std::pair<std::string, std::string>>& fct_sig_pairs_;
};

class Spanifier {
 public:
  explicit Spanifier(
      MatchFinder& finder,
      OutputHelper& output_helper,
      std::map<std::string, std::set<Node>>& sig_nodes,
      std::vector<std::pair<std::string, std::string>>& sig_pairs)
      : match_finder_(finder),
        potential_nodes_(output_helper),
        fct_sig_nodes_(sig_nodes, sig_pairs) {}

  void addMatchers() {
    auto exclusions = anyOf(
        isExpansionInSystemHeader(), raw_ptr_plugin::isInExternCContext(),
        raw_ptr_plugin::isInThirdPartyLocation(),
        raw_ptr_plugin::isInGeneratedLocation(),
        raw_ptr_plugin::ImplicitFieldDeclaration(),
        raw_ptr_plugin::isInMacroLocation(),
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

    // t* a = buf.data();
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
    auto buffer_expr1 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(ignoringParenCasts(anyOf(
                 arraySubscriptExpr(hasLHS(lhs_expr_variations)),
                 binaryOperation(
                     anyOf(hasOperatorName("+="), hasOperatorName("+")),
                     hasLHS(lhs_expr_variations)),
                 unaryOperator(hasOperatorName("++"),
                               hasUnaryOperand(lhs_expr_variations)),
                 // for raw_ptr ops
                 cxxOperatorCallExpr(anyOf(hasOverloadedOperatorName("[]"),
                                           hasOperatorName("++")),
                                     hasArgument(0, lhs_expr_variations)))))
            .bind("buffer_expr"));
    match_finder_.addMatcher(buffer_expr1, &potential_nodes_);

    auto buffer_expr2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(ignoringParenCasts(arraySubscriptExpr(hasLHS(declRefExpr(to(
                 varDecl(hasType(arrayType().bind("array_type")),
                         hasTypeLoc(
                             loc(qualType(anything())).bind("array_type_loc")),
                         unless(exclusions), unless(hasExternalFormalLinkage()))
                     .bind("array_variable")))))))
            .bind("buffer_expr"));
    match_finder_.addMatcher(buffer_expr2, &potential_nodes_);

    auto deref_expression = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        expr(anyOf(unaryOperator(hasOperatorName("*"),
                                 hasUnaryOperand(rhs_exprs_without_size_nodes)),
                   cxxOperatorCallExpr(
                       hasOverloadedOperatorName("*"),
                       hasArgument(0, rhs_exprs_without_size_nodes))),
             unless(raw_ptr_plugin::isInMacroLocation()))
            .bind("deref_expr"));
    match_finder_.addMatcher(deref_expression, &potential_nodes_);

    // This is needed to remove the `.get()` call on raw_ptr from rewritten
    // expressions. Example: raw_ptr<T> member; auto* temp = member.get(); if
    // member's type is rewritten to a raw_span<T>, this matcher is used to
    // remove the `.get()` call.
    auto raw_ptr_get_call = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxMemberCallExpr(
            callee(cxxMethodDecl(hasName("get"), ofClass(hasName("raw_ptr")))),
            has(memberExpr(has(rhs_expr)).bind("get_member_expr")))
            .bind("raw_ptr_get_call"));
    match_finder_.addMatcher(raw_ptr_get_call, &potential_nodes_);

    // When passing now-span buffers to third_party functions as parameters, we
    // need to add `.data()` to extract the pointer and keep things compiling.
    auto passing_a_buffer_to_external_functions = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        callExpr(callee(functionDecl(
                     anyOf(isExpansionInSystemHeader(),
                           raw_ptr_plugin::isInExternCContext(),
                           raw_ptr_plugin::isInThirdPartyLocation()))),
                 forEachArgumentWithParam(
                     expr(rhs_expr_variations,
                          unless(anyOf(
                              castExpr(hasSourceExpression(size_node_matcher)),
                              size_node_matcher)))
                         .bind("passing_a_buffer_to_third_party_function"),
                     parmVarDecl())));
    match_finder_.addMatcher(passing_a_buffer_to_external_functions,
                             &potential_nodes_);

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
    match_finder_.addMatcher(assignement_relationship, &potential_nodes_);

    // Creates the edge from lhs to false_expr in a ternary conditional
    // operator.
    auto assignement_relationship2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        binaryOperation(hasOperatorName("="),
                        hasOperands(lhs_expr_variations,
                                    conditionalOperator(hasFalseExpression(
                                        rhs_expr_variations))),
                        unless(isExpansionInSystemHeader())));
    match_finder_.addMatcher(assignement_relationship2, &potential_nodes_);

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
    match_finder_.addMatcher(var_construction, &potential_nodes_);

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
    match_finder_.addMatcher(var_construction2, &potential_nodes_);

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
    match_finder_.addMatcher(returned_var_or_member, &potential_nodes_);

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
    match_finder_.addMatcher(returned_var_or_member2, &potential_nodes_);

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

    match_finder_.addMatcher(ctor_initilizer, &potential_nodes_);

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
    match_finder_.addMatcher(var_passed_in_constructor, &potential_nodes_);

    // Creates the edge from lhs to false_expr in a ternary conditional
    // operator.
    auto var_passed_in_constructor2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxConstructExpr(forEachArgumentWithParam(
            expr(conditionalOperator(hasFalseExpression(rhs_expr_variations))),
            lhs_param)));
    match_finder_.addMatcher(var_passed_in_constructor2, &potential_nodes_);

    // handles Obj o{temp} when Obj has no constructor.
    // This creates a link between the expr and the underlying field.
    auto var_passed_in_initlistExpr = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        initListExpr(raw_ptr_plugin::forEachInitExprWithFieldDecl(
            expr(anyOf(
                rhs_expr_variations,
                conditionalOperator(hasTrueExpression(rhs_expr_variations)))),
            lhs_field)));
    match_finder_.addMatcher(var_passed_in_initlistExpr, &potential_nodes_);

    auto var_passed_in_initlistExpr2 = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        initListExpr(raw_ptr_plugin::forEachInitExprWithFieldDecl(
            expr(conditionalOperator(hasFalseExpression(rhs_expr_variations))),
            lhs_field)));
    match_finder_.addMatcher(var_passed_in_initlistExpr2, &potential_nodes_);

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
    match_finder_.addMatcher(call_expr, &potential_nodes_);

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
  MatchFinder& match_finder_;
  PotentialNodes potential_nodes_;
  FunctionSignatureNodes fct_sig_nodes_;
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
  std::map<std::string, std::set<Node>> fct_sig_nodes;
  // Map related function signatures to each other, this is needed for functions
  // with separate definition and declaration, and for overridden functions.
  std::vector<std::pair<std::string, std::string>> fct_sig_pairs;
  OutputHelper output_helper;
  MatchFinder match_finder;
  Spanifier rewriter(match_finder, output_helper, fct_sig_nodes, fct_sig_pairs);
  rewriter.addMatchers();

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
      output_helper.AddEdge(*i1, *i2);
      output_helper.AddEdge(*i2, *i1);
      i1++;
      i2++;
    }
  }

  // Emits the list of edges.
  output_helper.Emit();
  return result;
}
