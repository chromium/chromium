// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the implementation of a clang tool that rewrites containers of
// pointer fields into raw_ptr<T>:
//     std::vector<Pointee*> field_
// becomes:
//     std::vector<raw_ptr<Pointee>> field_
//
// Note that the tool emits two kinds of outputs:
//   1- A pairs of nodes formatted as {lhs};{rhs}\n representing an edge between
//      two nodes.
//   2- A single node formatted as {lhs}\n
// The concatenated outputs from multiple tool runs are then used to construct
// the graph and emit relevant edits using extract_edits.py
//
// A node (lhs, rhs) has the following format:
// '{is_field,is_excluded,has_auto_type,r:::<file
// path>:::<offset>:::<length>:::<replacement text>,include-user-header:::<file
// path>:::-1:::-1:::<include text>}'
//
// where `is_field`,`is_excluded`, and `has_auto_type` are booleans represendted
// as  0 or 1.
//
// For more details, see the doc here:
// https://docs.google.com/document/d/1P8wLVS3xueI4p3EAPO4JJP6d1_zVp5SapQB0EW9iHQI/

#include <assert.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "RawPtrHelpers.h"
#include "RawPtrManualPathsToIgnore.h"
#include "SeparateRepositoryPaths.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/CharInfo.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/MacroArgs.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/LineIterator.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::ast_matchers;

namespace {

// Include path that needs to be added to all the files where raw_ptr<...>
// replaces a raw pointer.
const char kRawPtrIncludePath[] = "base/memory/raw_ptr.h";

const char kOverrideExcludePathsParamName[] = "override-exclude-paths";

// This iterates over function parameters and matches the ones that match
// parm_var_decl_matcher.
AST_MATCHER_P(clang::FunctionDecl,
              forEachParmVarDecl,
              clang::ast_matchers::internal::Matcher<clang::ParmVarDecl>,
              parm_var_decl_matcher) {
  const clang::FunctionDecl& function_decl = Node;

  auto num_params = function_decl.getNumParams();
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

// Returns a StringRef of the elements apprearing after the pattern
// '(anonymous namespace)::' if any, otherwise returns input.
static llvm::StringRef RemoveAnonymous(llvm::StringRef input) {
  constexpr llvm::StringRef kAnonymousNamespace{"(anonymous namespace)::"};
  auto loc = input.find(kAnonymousNamespace);
  if (loc != input.npos) {
    return input.substr(loc + kAnonymousNamespace.size());
  }
  return input;
}

// Statements of the form: for (auto* i : affected_expr)
// need to be changed to: for (type_name* i : affected_expr)
// in order to extract the pointer type from now raw_ptr.
// The text returned by type's `getAsString()` can contain some unuseful
// data. Example: 'const struct n1::(anonymous namespace)::n2::type_name'. As
// is, this wouldn't compile. This needs to be reinterpreted as
// 'const n2::type_name'.
// `RemovePrefix` removes the class/struct keyword if any,
// conserves the constness, and trims '(anonymous namespace)::'
// as well as anything on it's lhs using `RemoveAnonymous`.
static std::string RemovePrefix(llvm::StringRef input) {
  constexpr llvm::StringRef kClassPrefix{"class "};
  constexpr llvm::StringRef kStructPrefix{"struct "};
  constexpr llvm::StringRef kConstPrefix{"const "};

  std::string result;
  result.reserve(input.size());

  if (input.consume_front(kConstPrefix)) {
    result += kConstPrefix;
  }

  input.consume_front(kClassPrefix);
  input.consume_front(kStructPrefix);

  result += RemoveAnonymous(input);
  return result;
}

struct Node {
  bool is_field = false;
  // This is set to true for Fields annotated with RAW_PTR_EXCLUSION
  bool is_excluded = false;
  // auto type variables don't need to be rewritten. They still need to be
  // present in the graph to propagate the rewrite to non auto expressions.
  // Example:
  // auto temp = member_; vector<T*>::iterator it = temp.begin();
  // `it`'s type needs to be rewritten when member's type is.
  bool has_auto_type = false;
  // A replacement follows the following format:
  // `r:::<file path>:::<offset>:::<length>:::<replacement text>`
  std::string replacement;
  // An include directive follows the following format:
  // `include-user-header:::<file path>:::-1:::-1:::<include text>`
  std::string include_directive;
  bool operator==(const Node& other) const {
    return replacement == other.replacement;
  }
  bool operator<(const Node& other) const {
    return replacement < other.replacement;
  }
  // The resulting string follows the following format:
  // {is_field\,is_excluded\,has_auto_type\,r:::<file
  // path>:::<offset>:::<length>:::<replacement
  // text>\,include-user-header:::<file path>:::-1:::-1:::<include text>}
  // where is_field,is_excluded, and has_auto_type are booleans represendted as
  // 0 or 1.
  std::string ToString() const {
    return llvm::formatv("{{{0:d}\\,{1:d}\\,{2:d}\\,{3}\\,{4}}", is_field,
                         is_excluded, has_auto_type, replacement,
                         include_directive);
  }
};

// Helper class to add edges to the set of node_pairs_;
class OutputHelper {
 public:
  OutputHelper() = default;

  void AddEdge(const Node& lhs, const Node& rhs) {
    node_pairs_.insert(
        llvm::formatv("{0};{1}\n", lhs.ToString(), rhs.ToString()));
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
  // The format is: {lhs};{rhs}\n where lhs & rhs generated using
  // Node::ToString().
  // There are two cases where the line contains only a lhs node {lhs}\n
  // 1- To make sure that fields that are not connected to any other node are
  // represented in the graph.
  // 2- Fields annotated with RAW_PTR_EXCLUSION are also inserted as a single
  // node to the list.
  std::set<std::string> node_pairs_;
};

// This visitor is used to extract a FunctionDecl* bound with a node id
// "fct_decl" from a given match.
// This is used in the `forEachArg` and `forEachBindArg` matchers below.
class LocalVisitor
    : public clang::ast_matchers::internal::BoundNodesTreeBuilder::Visitor {
 public:
  void visitMatch(
      const clang::ast_matchers::BoundNodes& BoundNodesView) override {
    if (const auto* ptr =
            BoundNodesView.getNodeAs<clang::FunctionDecl>("fct_decl")) {
      fct_decl_ = BoundNodesView.getNodeAs<clang::FunctionDecl>("fct_decl");
      is_lambda_ = false;
    } else {
      const clang::LambdaExpr* lambda_expr =
          BoundNodesView.getNodeAs<clang::LambdaExpr>("lambda_expr");
      fct_decl_ = lambda_expr->getCallOperator();
      is_lambda_ = true;
    }
  }
  const clang::FunctionDecl* fct_decl_;
  bool is_lambda_;
};

// This is used to map arguments passed to std::make_unique to the underlying
// constructor parameters. For each expr that matches, using `LocalVisitor`, we
// extract the ptr to clang::FunctionDecl which represents here the
// constructorDecl and use it to get the parmVarDecl corresponding to the
// argument.
// This iterates over a callExpressions's arguments and matches the ones that
// match expr_matcher. For each argument matched, retrieve the corresponding
// constructor parameter. The constructor parameter is then checked against
// parm_var_decl_matcher.
AST_MATCHER_P2(clang::CallExpr,
               forEachArg,
               clang::ast_matchers::internal::Matcher<clang::Expr>,
               expr_matcher,
               clang::ast_matchers::internal::Matcher<clang::ParmVarDecl>,
               parm_var_decl_matcher) {
  const clang::CallExpr& call_expr = Node;

  auto num_args = call_expr.getNumArgs();
  bool is_matching = false;
  clang::ast_matchers::internal::BoundNodesTreeBuilder result;
  for (unsigned i = 0; i < num_args; i++) {
    const clang::Expr* arg = call_expr.getArg(i);
    clang::ast_matchers::internal::BoundNodesTreeBuilder arg_matches;
    if (expr_matcher.matches(*arg, Finder, &arg_matches)) {
      LocalVisitor l;
      arg_matches.visitMatches(&l);
      const auto* fct_decl = l.fct_decl_;
      if (fct_decl) {
        const auto* param = fct_decl->getParamDecl(i);
        clang::ast_matchers::internal::BoundNodesTreeBuilder parm_var_matches(
            arg_matches);
        if (parm_var_decl_matcher.matches(*param, Finder, &parm_var_matches)) {
          is_matching = true;
          result.addMatch(parm_var_matches);
        }
      }
    }
  }
  *Builder = std::move(result);
  return is_matching;
}

// This is used to handle expressions of the form:
// base::BindOnce(
//                [](std::vector<raw_ptr<Label>>& message_labels,
//                    Label* message_label) {
//                   message_labels.push_back(message_label);
//                 },
//                 std::ref(message_labels_))))
// This creates a link between the parmVarDecl's in the lambda/functionPointer
// passed as 1st argument and the rest of the arguments passed to the bind call.
AST_MATCHER_P2(clang::CallExpr,
               forEachBindArg,
               clang::ast_matchers::internal::Matcher<clang::Expr>,
               expr_matcher,
               clang::ast_matchers::internal::Matcher<clang::ParmVarDecl>,
               parm_var_decl_matcher) {
  const clang::CallExpr& call_expr = Node;

  auto num_args = call_expr.getNumArgs();
  if (num_args == 1) {
    // No arguments to map to the lambda/fct parmVarDecls.
    return false;
  }

  bool is_matching = false;
  clang::ast_matchers::internal::BoundNodesTreeBuilder result;
  for (unsigned i = 1; i < num_args; i++) {
    const clang::Expr* arg = call_expr.getArg(i);
    clang::ast_matchers::internal::BoundNodesTreeBuilder arg_matches;
    if (expr_matcher.matches(*arg, Finder, &arg_matches)) {
      LocalVisitor l;
      arg_matches.visitMatches(&l);
      const auto* fct_decl = l.fct_decl_;

      // start_index=1 when we start with second arg for Bind and first arg for
      // lambda/fct
      unsigned start_index = 1;
      // start_index=2 when the second arg is a pointer to the object on which
      // the function is to be invoked. This is done when the function pointer is
      // not a static class function.
      // isGlobal is true for free functions as well as static member functions,
      // both of which don't need a pointer to the object on which they are
      // invoked.
      if (!l.is_lambda_ && !l.fct_decl_->isGlobal()) {
        start_index = 2;
        // Skip the second argument passed to BindOnce/BindRepeating as it is an
        // object pointer unrelated to target function args.
        if (i == 1) {
          continue;
        }
      }
      const auto* param = fct_decl->getParamDecl(i - start_index);
      clang::ast_matchers::internal::BoundNodesTreeBuilder
          parm_var_decl_matches(arg_matches);
      if (parm_var_decl_matcher.matches(*param, Finder,
                                        &parm_var_decl_matches)) {
        is_matching = true;
        result.addMatch(parm_var_decl_matches);
      }
    }
  }
  *Builder = std::move(result);
  return is_matching;
}

static std::string GenerateNewType(const clang::ASTContext& ast_context,
                                   const clang::QualType& pointer_type) {
  std::string result;

  clang::QualType pointee_type = pointer_type->getPointeeType();

  // Preserve qualifiers.
  assert(!pointer_type.isRestrictQualified() &&
         "|restrict| is a C-only qualifier and raw_ptr<T>/raw_ref<T> need C++");
  if (pointer_type.isConstQualified()) {
    result += "const ";
  }
  if (pointer_type.isVolatileQualified()) {
    result += "volatile ";
  }

  // Convert pointee type to string.
  clang::PrintingPolicy printing_policy(ast_context.getLangOpts());
  printing_policy.SuppressScope = 1;  // s/blink::Pointee/Pointee/
  std::string pointee_type_as_string =
      pointee_type.getAsString(printing_policy);
  result += llvm::formatv("raw_ptr<{0}>", pointee_type_as_string);

  return result;
}

static std::pair<std::string, std::string> GetReplacementAndIncludeDirectives(
    const clang::PointerTypeLoc* type_loc,
    const clang::TemplateSpecializationTypeLoc* tst_loc,
    std::string replacement_text,
    const clang::SourceManager& source_manager) {
  clang::SourceLocation begin_loc = tst_loc->getLAngleLoc().getLocWithOffset(1);
  // This is done to skip the star '*' because type_loc's end loc is just
  // before the star position.
  clang::SourceLocation end_loc = type_loc->getEndLoc().getLocWithOffset(1);

  clang::SourceRange replacement_range(begin_loc, end_loc);

  clang::tooling::Replacement replacement(
      source_manager, clang::CharSourceRange::getCharRange(replacement_range),
      replacement_text);
  llvm::StringRef file_path = replacement.getFilePath();
  if (file_path.empty()) {
    return {"", ""};
  }

  std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
  std::string replacement_directive = llvm::formatv(
      "r:::{0}:::{1}:::{2}:::{3}", file_path, replacement.getOffset(),
      replacement.getLength(), replacement_text);

  std::string include_directive =
      llvm::formatv("include-user-header:::{0}:::-1:::-1:::{1}", file_path,
                    kRawPtrIncludePath);

  return {replacement_directive, include_directive};
}

std::string GenerateReplacementForAutoLoc(
    const clang::TypeLoc* auto_loc,
    const std::string& replacement_text,
    const clang::SourceManager& source_manager,
    const clang::ASTContext& ast_context) {
  clang::SourceLocation begin_loc = auto_loc->getBeginLoc();

  clang::SourceRange replacement_range(begin_loc, begin_loc);

  clang::tooling::Replacement replacement(
      source_manager, clang::CharSourceRange::getCharRange(replacement_range),
      replacement_text);
  llvm::StringRef file_path = replacement.getFilePath();

  return llvm::formatv("r:::{0}:::{1}:::{2}:::{3}", file_path,
                       replacement.getOffset(), replacement.getLength(),
                       replacement_text);
}

// Called when the Match registered for it was successfully found in the AST.
// The matches registered represent two categories:
//   1- An adjacency relationship
//      In that case, a node pair is created, using matched node ids, and added
//      to the node_pair list using `OutputHelper::AddEdge`
//   2- A single fieldDecl node match
//      In that case, a single node is created and added to the node_pair list
//      using `OutputHelper::AddSingleNode`
class PotentialNodes : public MatchFinder::MatchCallback {
 public:
  explicit PotentialNodes(OutputHelper& helper) : output_helper_(helper) {}

  PotentialNodes(const PotentialNodes&) = delete;
  PotentialNodes& operator=(const PotentialNodes&) = delete;

  void run(const MatchFinder::MatchResult& result) override {
    const clang::SourceManager& source_manager = *result.SourceManager;
    const clang::ASTContext& ast_context = *result.Context;

    Node lhs;

    if (auto* type_loc = result.Nodes.getNodeAs<clang::PointerTypeLoc>(
            "lhs_argPointerLoc")) {
      std::string replacement_text =
          GenerateNewType(ast_context, type_loc->getType());

      const clang::TemplateSpecializationTypeLoc* tst_loc =
          result.Nodes.getNodeAs<clang::TemplateSpecializationTypeLoc>(
              "lhs_tst_loc");

      auto p = GetReplacementAndIncludeDirectives(
          type_loc, tst_loc, replacement_text, source_manager);
      lhs.replacement = p.first;
      lhs.include_directive = p.second;

      if (const clang::FieldDecl* field_decl =
              result.Nodes.getNodeAs<clang::FieldDecl>("lhs_field")) {
        lhs.is_field = true;
      }

      // To make sure we add all field decls to the graph.(Specifically those
      // not connected to other nodes)
      if (const clang::FieldDecl* field_decl =
              result.Nodes.getNodeAs<clang::FieldDecl>("field_decl")) {
        lhs.is_field = true;
        output_helper_.AddSingleNode(lhs);
        return;
      }

      // RAW_PTR_EXCLUSION is not captured when adding edges between nodes. For
      // that reason, fields annotated with RAW_PTR_EXCLUSION are added as
      // single nodes to the list, this is then used as a starting point to
      // propagate the exclusion to all neighboring nodes.
      if (const clang::FieldDecl* field_decl =
              result.Nodes.getNodeAs<clang::FieldDecl>("excluded_field_decl")) {
        lhs.is_field = true;
        lhs.is_excluded = true;
        output_helper_.AddSingleNode(lhs);
        return;
      }
    } else if (const clang::TypeLoc* auto_loc =
                   result.Nodes.getNodeAs<clang::TypeLoc>("lhs_auto_loc")) {
      lhs.replacement = GenerateReplacementForAutoLoc(
          auto_loc, "replacement_text", source_manager, ast_context);
      lhs.include_directive = lhs.replacement;
      // No need to emit a rewrite for auto type variables. They still need to
      // appear in the graph to propagate the rewrite to non-auto type nodes
      // codes connected to them.
      lhs.has_auto_type = true;
    } else {  // Not supposed to get here
      assert(false);
    }

    Node rhs;
    if (const clang::FieldDecl* field_decl =
            result.Nodes.getNodeAs<clang::FieldDecl>("rhs_field")) {
      rhs.is_field = true;
    }

    if (auto* type_loc = result.Nodes.getNodeAs<clang::PointerTypeLoc>(
            "rhs_argPointerLoc")) {
      std::string replacement_text =
          GenerateNewType(ast_context, type_loc->getType());

      const clang::TemplateSpecializationTypeLoc* tst_loc =
          result.Nodes.getNodeAs<clang::TemplateSpecializationTypeLoc>(
              "rhs_tst_loc");

      auto p = GetReplacementAndIncludeDirectives(
          type_loc, tst_loc, replacement_text, source_manager);

      rhs.replacement = p.first;
      rhs.include_directive = p.second;
    } else if (const clang::TypeLoc* auto_loc =
                   result.Nodes.getNodeAs<clang::TypeLoc>("rhs_auto_loc")) {
      rhs.replacement = GenerateReplacementForAutoLoc(
          auto_loc, "replacement_text", source_manager, ast_context);
      rhs.include_directive = rhs.replacement;
      // No need to emit a rewrite for auto type variables. They still need to
      // appear in the graph to propagate the rewrite to non-auto type nodes
      // codes connected to them.
      rhs.has_auto_type = true;
    } else {  // Not supposed to get here
      assert(false);
    }

    output_helper_.AddEdge(lhs, rhs);
  }

 private:
  OutputHelper& output_helper_;
};

// Called when the Match registered for it was successfully found in the AST.
// The match represents a parmVarDecl Node or an RTNode and the corresponding
// function declaration. Using the function declaration:
//         1- Create a unique key `current_key`
//         2- if the function has a previous declaration or is overridden,
//            retrieve previous decls and create their keys `prev_key`
//         3- for each `prev_key`, add pair `current_key`, `prev_key` to
//         `fct_sig_pairs_`
//
// Using the parmVarDecl or RTNode:
//        1- Create a node
//        2- insert node into `fct_sig_nodes_[current_key]`
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
    // needed to handle cases where the function is in a Macro Expansion. In
    // that case, multiple functionDecls will have the same location and this
    // will create problems for argument mapping. Example:
    // MOCK_METHOD0(GetAllStreams, std::vector<DemuxerStream*>());
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

  void run(const MatchFinder::MatchResult& result) override {
    const clang::SourceManager& source_manager = *result.SourceManager;
    const clang::ASTContext& ast_context = *result.Context;

    const clang::FunctionDecl* fct_decl =
        result.Nodes.getNodeAs<clang::FunctionDecl>("fct_decl");

    std::string key = GetKey(fct_decl, source_manager);
    if (auto* prev_decl = fct_decl->getPreviousDecl()) {
      std::string prev_key = GetKey(prev_decl, source_manager);
      fct_sig_pairs_.push_back({prev_key, key});
    }

    if (const clang::CXXMethodDecl* method_decl =
            result.Nodes.getNodeAs<clang::CXXMethodDecl>("fct_decl")) {
      for (auto* m : method_decl->overridden_methods()) {
        std::string prev_key = GetKey(m, source_manager);
        fct_sig_pairs_.push_back({prev_key, key});
      }
    }

    auto* type_loc =
        result.Nodes.getNodeAs<clang::PointerTypeLoc>("rhs_argPointerLoc");
    Node rhs;
    std::string replacement_text =
        GenerateNewType(ast_context, type_loc->getType());

    const clang::TemplateSpecializationTypeLoc* tst_loc =
        result.Nodes.getNodeAs<clang::TemplateSpecializationTypeLoc>(
            "rhs_tst_loc");

    auto p = GetReplacementAndIncludeDirectives(
        type_loc, tst_loc, replacement_text, source_manager);
    rhs.replacement = p.first;
    rhs.include_directive = p.second;

    fct_sig_nodes_[key].insert(rhs);
  }

 private:
  // Map a function signature, which is modeled as a string representing file
  // location, to it's matched graph nodes (RTNode and ParmVarDecl nodes).
  // Note: `RTNode` represents a function return type node.
  // In order to avoid relying on the order with which nodes are matched in the
  // AST, and to guarantee that nodes are stored in the file declaration order,
  // we use a `std::set<Node>` which sorts Nodes based on the replacement
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
  // Map related function signatures to each other, this is needed for functions
  // with separate definition and declaration, and for overridden functions.
  std::vector<std::pair<std::string, std::string>>& fct_sig_pairs_;
};

// Called when the Match registered for it was successfully found in the AST.
// The matches registered represent three categories:
//   1- Range-based for loops of the form:
//        for (auto* i : ctn_expr) => for (type_name* i : ctn_expr)
//
//   2- Expressions of the form:
//      auto* var = ctn_expr.front(); => auto* var = ctn_expr.front().get();
//
//   3- Expressions of the form:
//      auto* var = ctn_expr[index]; => auto* var = ctn_expr[index].get();
//
// In each of the above cases a node pair is created and added to node_pairs
// using `OutputHelper::AddEdge`
class AffectedPtrExprRewriter : public MatchFinder::MatchCallback {
 public:
  explicit AffectedPtrExprRewriter(OutputHelper& helper)
      : output_helper_(helper) {}

  AffectedPtrExprRewriter(const AffectedPtrExprRewriter&) = delete;
  AffectedPtrExprRewriter& operator=(const AffectedPtrExprRewriter&) = delete;

  void run(const MatchFinder::MatchResult& result) override {
    const clang::SourceManager& source_manager = *result.SourceManager;
    const clang::ASTContext& ast_context = *result.Context;

    Node lhs;
    if (const clang::VarDecl* var_decl =
            result.Nodes.getNodeAs<clang::VarDecl>("autoVarDecl")) {
      auto* type_loc = result.Nodes.getNodeAs<clang::TypeLoc>("autoLoc");

      clang::SourceRange replacement_range(var_decl->getBeginLoc(),
                                           type_loc->getEndLoc());

      std::string replacement_text =
          var_decl->getType()->getPointeeType().getAsString();

      replacement_text = RemovePrefix(replacement_text);
      lhs.replacement = getReplacementDirective(
          replacement_text, replacement_range, source_manager, ast_context);
      lhs.include_directive = lhs.replacement;

    } else if (const clang::CXXMemberCallExpr* member_expr =
                   result.Nodes.getNodeAs<clang::CXXMemberCallExpr>(
                       "affectedMemberExpr")) {
      clang::SourceLocation insertion_loc =
          member_expr->getEndLoc().getLocWithOffset(1);

      clang::SourceRange replacement_range(insertion_loc, insertion_loc);
      std::string replacement_text = ".get()";
      lhs.replacement = getReplacementDirective(
          replacement_text, replacement_range, source_manager, ast_context);
      lhs.include_directive = lhs.replacement;
    } else if (const clang::CXXOperatorCallExpr* op_call_expr =
                   result.Nodes.getNodeAs<clang::CXXOperatorCallExpr>(
                       "affectedOpCall")) {
      clang::SourceLocation insertion_loc =
          op_call_expr->getEndLoc().getLocWithOffset(1);
      clang::SourceRange replacement_range(insertion_loc, insertion_loc);
      std::string replacement_text = ".get()";
      lhs.replacement = getReplacementDirective(
          replacement_text, replacement_range, source_manager, ast_context);
      lhs.include_directive = lhs.replacement;
    } else if (const clang::ParmVarDecl* var_decl =
                   result.Nodes.getNodeAs<clang::ParmVarDecl>(
                       "lambda_parmVarDecl")) {
      auto* type_loc =
          result.Nodes.getNodeAs<clang::TypeLoc>("template_type_param_loc");

      auto* md = result.Nodes.getNodeAs<clang::CXXMethodDecl>("md");

      clang::SourceRange replacement_range(var_decl->getBeginLoc(),
                                           type_loc->getEndLoc());

      std::string replacement_text =
          (md->getParamDecl(var_decl->getFunctionScopeIndex()))
              ->getType()
              ->getPointeeType()
              .getAsString();

      replacement_text = RemovePrefix(replacement_text);
      lhs.replacement = getReplacementDirective(
          replacement_text, replacement_range, source_manager, ast_context);
      lhs.include_directive = lhs.replacement;
    }

    Node rhs;
    if (const clang::FieldDecl* field_decl =
            result.Nodes.getNodeAs<clang::FieldDecl>("rhs_field")) {
      rhs.is_field = true;
    }

    if (auto* type_loc = result.Nodes.getNodeAs<clang::PointerTypeLoc>(
            "rhs_argPointerLoc")) {
      std::string replacement_text =
          GenerateNewType(ast_context, type_loc->getType());

      const clang::TemplateSpecializationTypeLoc* tst_loc =
          result.Nodes.getNodeAs<clang::TemplateSpecializationTypeLoc>(
              "rhs_tst_loc");

      auto p = GetReplacementAndIncludeDirectives(
          type_loc, tst_loc, replacement_text, source_manager);

      rhs.replacement = p.first;
      rhs.include_directive = p.second;
    } else if (const clang::TypeLoc* auto_loc =
                   result.Nodes.getNodeAs<clang::TypeLoc>("rhs_auto_loc")) {
      rhs.replacement = GenerateReplacementForAutoLoc(
          auto_loc, "replacement_text", source_manager, ast_context);
      rhs.include_directive = rhs.replacement;
      rhs.has_auto_type = true;
    } else {
      // Should not get here.
      assert(false);
    }
    output_helper_.AddEdge(lhs, rhs);
  }

 private:
  std::string getReplacementDirective(
      std::string& replacement_text,
      clang::SourceRange replacement_range,
      const clang::SourceManager& source_manager,
      const clang::ASTContext& ast_context) {
    clang::tooling::Replacement replacement(
        source_manager, clang::CharSourceRange::getCharRange(replacement_range),
        replacement_text);
    llvm::StringRef file_path = replacement.getFilePath();

    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    return llvm::formatv("r:::{0}:::{1}:::{2}:::{3}", file_path,
                         replacement.getOffset(), replacement.getLength(),
                         replacement_text);
  }

  OutputHelper& output_helper_;
};

class ExprVisitor
    : public clang::ast_matchers::internal::BoundNodesTreeBuilder::Visitor {
 public:
  void visitMatch(
      const clang::ast_matchers::BoundNodes& BoundNodesView) override {
    expr_ = BoundNodesView.getNodeAs<clang::Expr>("expr");
  }
  const clang::Expr* expr_;
};

const clang::Expr* getExpr(
    clang::ast_matchers::internal::BoundNodesTreeBuilder& matches) {
  ExprVisitor v;
  matches.visitMatches(&v);
  return v.expr_;
}

// The goal of this matcher is to handle all possible combinations of matching
// expressions. This works by unpacking the expression nodes recursively (in any
// order they appear in) until we reach the matching lhs_expr/rhs_expr. This
// allows to handle cases like the following:
// std::map<int, std::vector<S*>> member;
// std::vector<S*>::iterator it = member.begin()->second;
AST_MATCHER_P(clang::Expr,
              expr_variations,
              clang::ast_matchers::internal::Matcher<clang::Expr>,
              InnerMatcher) {
  auto iterator = cxxMemberCallExpr(
      callee(functionDecl(
          anyOf(hasName("begin"), hasName("cbegin"), hasName("rbegin"),
                hasName("crbegin"), hasName("end"), hasName("cend"),
                hasName("rend"), hasName("crend"), hasName("find"),
                hasName("upper_bound"), hasName("lower_bound"),
                hasName("equal_range"), hasName("emplace"), hasName("Get")))),
      has(memberExpr(has(expr().bind("expr")))));

  auto search_calls = callExpr(callee(functionDecl(matchesName("find"))),
                               hasArgument(0, expr().bind("expr")));

  auto unary_op = unaryOperator(has(expr().bind("expr")));

  auto reversed_expr = callExpr(callee(functionDecl(hasName("base::Reversed"))),
                                hasArgument(0, expr().bind("expr")));

  auto bracket_op_call = cxxOperatorCallExpr(
      has(declRefExpr(to(cxxMethodDecl(hasName("operator[]"))))),
      has(expr(unless(declRefExpr(to(cxxMethodDecl(hasName("operator[]"))))))
              .bind("expr")));

  auto arrow_op_call = cxxOperatorCallExpr(
      has(declRefExpr(to(cxxMethodDecl(hasName("operator->"))))),
      has(expr(unless(declRefExpr(to(cxxMethodDecl(hasName("operator->"))))))
              .bind("expr")));

  auto star_op_call = cxxOperatorCallExpr(
      has(declRefExpr(to(cxxMethodDecl(hasName("operator*"))))),
      has(expr(unless(declRefExpr(to(cxxMethodDecl(hasName("operator*"))))))
              .bind("expr")));

  auto second_member =
      memberExpr(member(hasName("second")), has(expr().bind("expr")));

  auto items = {iterator,        search_calls,  unary_op,     reversed_expr,
                bracket_op_call, arrow_op_call, star_op_call, second_member};
  clang::ast_matchers::internal::BoundNodesTreeBuilder matches;
  const clang::Expr* n = nullptr;
  std::any_of(items.begin(), items.end(), [&](auto& item) {
    if (item.matches(Node, Finder, &matches)) {
      n = getExpr(matches);
      return true;
    }
    return false;
  });

  if (n) {
    auto matcher = expr_variations(InnerMatcher);
    return matcher.matches(*n, Finder, Builder);
  }
  return InnerMatcher.matches(Node, Finder, Builder);
}

class DeclVisitor
    : public clang::ast_matchers::internal::BoundNodesTreeBuilder::Visitor {
 public:
  void visitMatch(
      const clang::ast_matchers::BoundNodes& BoundNodesView) override {
    decl_ = BoundNodesView.getNodeAs<clang::TypedefNameDecl>("decl");
  }
  const clang::TypedefNameDecl* decl_;
};
const clang::TypedefNameDecl* getDecl(
    clang::ast_matchers::internal::BoundNodesTreeBuilder& matches) {
  DeclVisitor v;
  matches.visitMatches(&v);
  return v.decl_;
}
// This allows us to unpack typedefs recursively until we reach the node
// matching InnerMatcher.
// Example:
// using VECTOR = std::vector<S*>;
// using MAP = std::map<int, VECTOR>;
// MAP member; => this will lead to VECTOR being rewritten.
AST_MATCHER_P(clang::TypedefNameDecl,
              type_def_name_decl,
              clang::ast_matchers::internal::Matcher<clang::TypedefNameDecl>,
              InnerMatcher) {
  auto type_def_matcher = typedefNameDecl(
      hasDescendant(loc(qualType(hasDeclaration(
          typedefNameDecl(unless(isExpansionInSystemHeader())).bind("decl"))))),
      unless(isExpansionInSystemHeader()));

  clang::ast_matchers::internal::BoundNodesTreeBuilder matches;
  if (type_def_matcher.matches(Node, Finder, &matches)) {
    const clang::TypedefNameDecl* n = getDecl(matches);
    auto matcher = type_def_name_decl(InnerMatcher);
    return matcher.matches(*n, Finder, Builder);
  }
  return InnerMatcher.matches(Node, Finder, Builder);
}

class ContainerRewriter {
 public:
  explicit ContainerRewriter(
      MatchFinder& finder,
      OutputHelper& output_helper,
      std::map<std::string, std::set<Node>>& sig_nodes,
      std::vector<std::pair<std::string, std::string>>& sig_pairs,
      const raw_ptr_plugin::FilterFile* excluded_paths)
      : match_finder_(finder),
        affected_ptr_expr_rewriter_(output_helper),
        potentail_nodes_(output_helper),
        fct_sig_nodes_(sig_nodes, sig_pairs),
        paths_to_exclude(excluded_paths) {}

  void addMatchers() {
    // Assume every container has the three following methods: begin, end, size
    auto container_methods =
        anyOf(allOf(hasMethod(hasName("push_back")),
                    hasMethod(hasName("pop_back")), hasMethod(hasName("size"))),
              allOf(hasMethod(hasName("insert")), hasMethod(hasName("erase")),
                    hasMethod(hasName("size"))),
              allOf(hasMethod(hasName("push")), hasMethod(hasName("pop")),
                    hasMethod(hasName("size"))));

    // Exclude maps as they need special handling to be rewritten.
    // TODO: handle rewriting maps.
    auto excluded_containers = matchesName("map");

    auto supported_containers = anyOf(
        hasDeclaration(classTemplateSpecializationDecl(
            container_methods, unless(excluded_containers))),
        hasDeclaration(typeAliasTemplateDecl(has(typeAliasDecl(
            hasType(qualType(hasDeclaration(classTemplateDecl(has(cxxRecordDecl(
                container_methods, unless(excluded_containers))))))))))));

    auto tst_type_loc = templateSpecializationTypeLoc(
        loc(qualType(supported_containers)),
        hasTemplateArgumentLoc(
            0, hasTypeLoc(loc(qualType(allOf(
                   raw_ptr_plugin::supported_pointer_type(),
                   unless(raw_ptr_plugin::const_char_pointer_type(false))))))));

    auto lhs_location =
        templateSpecializationTypeLoc(
            tst_type_loc,
            hasTemplateArgumentLoc(
                0, hasTypeLoc(pointerTypeLoc().bind("lhs_argPointerLoc"))))
            .bind("lhs_tst_loc");

    auto rhs_location =
        templateSpecializationTypeLoc(
            tst_type_loc,
            hasTemplateArgumentLoc(
                0, hasTypeLoc(pointerTypeLoc().bind("rhs_argPointerLoc"))))
            .bind("rhs_tst_loc");

    auto exclude_callbacks = anyOf(
        hasType(typedefNameDecl(hasType(qualType(hasDeclaration(
            recordDecl(anyOf(hasName("base::RepeatingCallback"),
                             hasName("base::OnceCallback")))))))),
        hasType(qualType(
            hasDeclaration(recordDecl(anyOf(hasName("base::RepeatingCallback"),
                                            hasName("base::OnceCallback")))))));

    auto field_exclusions =
        anyOf(isExpansionInSystemHeader(), raw_ptr_plugin::isInExternCContext(),
              raw_ptr_plugin::isInThirdPartyLocation(),
              raw_ptr_plugin::isInGeneratedLocation(),
              raw_ptr_plugin::ImplicitFieldDeclaration(), exclude_callbacks,
              // Exclude fieldDecls in macros.
              // `raw_ptr_plugin::isInMacroLocation()` is also true for fields
              // annotated with RAW_PTR_EXCLUSION. The annotated fields are not
              // included in `field_exclusions` as they are handled differently
              // by the `excluded_field_decl` matcher.
              allOf(raw_ptr_plugin::isInMacroLocation(),
                    unless(raw_ptr_plugin::isRawPtrExclusionAnnotated())));

    // Supports typedefs as well.
    auto lhs_type_loc =
        anyOf(hasDescendant(loc(qualType(hasDeclaration(typedefNameDecl(
                  type_def_name_decl(hasDescendant(lhs_location))))))),
              hasDescendant(lhs_location));

    // Supports typedefs as well.
    auto rhs_type_loc =
        anyOf(hasDescendant(loc(qualType(hasDeclaration(typedefNameDecl(
                  type_def_name_decl(hasDescendant(rhs_location))))))),
              hasDescendant(rhs_location));

    auto lhs_field =
        fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(lhs_type_loc),
                  unless(field_exclusions))
            .bind("lhs_field");
    auto rhs_field =
        fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(rhs_type_loc),
                  unless(field_exclusions))
            .bind("rhs_field");

    auto lhs_var = anyOf(
        varDecl(hasDescendant(loc(qualType(autoType())).bind("lhs_auto_loc"))),
        varDecl(lhs_type_loc).bind("lhs_var"));

    auto rhs_var = anyOf(
        varDecl(hasDescendant(loc(qualType(autoType())).bind("rhs_auto_loc"))),
        varDecl(rhs_type_loc).bind("rhs_var"));

    auto lhs_param =
        parmVarDecl(raw_ptr_plugin::hasExplicitParmVarDecl(lhs_type_loc))
            .bind("lhs_param");

    auto rhs_param =
        parmVarDecl(raw_ptr_plugin::hasExplicitParmVarDecl(rhs_type_loc))
            .bind("rhs_param");

    auto rhs_call_expr =
        callExpr(callee(functionDecl(hasReturnTypeLoc(rhs_type_loc))));

    auto lhs_call_expr =
        callExpr(callee(functionDecl(hasReturnTypeLoc(lhs_type_loc))));

    auto lhs_expr = expr(
        ignoringImpCasts(anyOf(declRefExpr(to(anyOf(lhs_var, lhs_param))),
                               memberExpr(member(lhs_field)), lhs_call_expr)));

    auto rhs_expr = expr(
        ignoringImpCasts(anyOf(declRefExpr(to(anyOf(rhs_var, rhs_param))),
                               memberExpr(member(rhs_field)), rhs_call_expr)));

    // To make sure we add all field decls to the graph.(Specifically those not
    // connected to other nodes)
    auto field_decl =
        fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(lhs_type_loc),
                  unless(anyOf(field_exclusions,
                               raw_ptr_plugin::isRawPtrExclusionAnnotated())))
            .bind("field_decl");
    match_finder_.addMatcher(field_decl, &potentail_nodes_);

    // Fields annotated with RAW_PTR_EXCLUSION (as well as fields in excluded
    // paths) cannot be filtered using field exclusions. They need to appear in
    // the graph so that we can properly propagate the exclusion to reachable
    // nodes. For this reason, and in order to capture this information,
    // RAW_PTR_EXCLUSION fields are added as single nodes to the list and then
    // used as a starting point to propagate the exclusion before running dfs on
    // the graph.
    auto excluded_field_decl =
        fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(lhs_type_loc),
                  anyOf(raw_ptr_plugin::isRawPtrExclusionAnnotated(),
                        isInLocationListedInFilterFile(paths_to_exclude)))
            .bind("excluded_field_decl");
    match_finder_.addMatcher(excluded_field_decl, &potentail_nodes_);

    auto ref_cref_move =
        anyOf(hasName("std::move"), hasName("std::ref"), hasName("std::cref"));
    auto rhs_move_call =
        callExpr(callee(functionDecl(ref_cref_move)), hasArgument(0, rhs_expr));

    // This is needed for ternary cond operator true_expr. (cond) ? true_expr :
    // false_expr;
    auto lhs_move_call =
        callExpr(callee(functionDecl(ref_cref_move)), hasArgument(0, lhs_expr));

    auto rhs_cxx_temp_expr = cxxTemporaryObjectExpr(rhs_type_loc);

    auto lhs_cxx_temp_expr = cxxTemporaryObjectExpr(lhs_type_loc);

    // This represents the forms under which an expr could appear on the right
    // hand side of an assignment operation, var construction, or an expr passed
    // as callExpr argument. Examples: rhs_expr, &rhs_expr, *rhs_expr,
    // fct_call(),*fct_call(), &fct_call(), std::move(), .begin();
    auto rhs_expr_variations =
        expr_variations(anyOf(rhs_expr, rhs_move_call, rhs_cxx_temp_expr));

    auto lhs_expr_variations =
        expr_variations(anyOf(lhs_expr, lhs_move_call, lhs_cxx_temp_expr));

    // rewrite affected expressions
    {
      // This is needed to handle container-like types that implement a begin()
      // method. Range-based for loops over such types also need to be
      // rewritten.
      auto ctn_like_type =
          expr(hasType(cxxRecordDecl(has(functionDecl(
                   hasName("begin"), hasReturnTypeLoc(rhs_type_loc))))),
               unless(isExpansionInSystemHeader()));

      auto reversed_expr =
          callExpr(callee(functionDecl(hasName("base::Reversed"))),
                   hasArgument(0, rhs_expr_variations));

      // handles statements of the form: for (auto* i : member/var/param/fct())
      // that should be modified after rewriting the container.
      auto auto_star_in_range_stmt = traverse(
          clang::TK_IgnoreUnlessSpelledInSource,
          cxxForRangeStmt(
              has(varDecl(hasDescendant(loc(qualType(pointsTo(autoType())))
                                            .bind("autoLoc")))
                      .bind("autoVarDecl")),
              has(expr(
                  anyOf(rhs_expr_variations, reversed_expr, ctn_like_type)))));
      match_finder_.addMatcher(auto_star_in_range_stmt,
                               &affected_ptr_expr_rewriter_);

      // handles expressions of the form: auto* var = member.front();
      // This becomes: auto* var = member.front().get();
      auto affected_expr = traverse(
          clang::TK_IgnoreUnlessSpelledInSource,
          declStmt(has(varDecl(
              hasType(pointsTo(autoType())),
              has(cxxMemberCallExpr(
                      callee(
                          functionDecl(anyOf(hasName("front"), hasName("back"),
                                             hasName("at"), hasName("top")))),
                      has(memberExpr(has(expr(rhs_expr_variations)))))
                      .bind("affectedMemberExpr"))))));
      match_finder_.addMatcher(affected_expr, &affected_ptr_expr_rewriter_);

      // handles expressions of the form: auto* var = member[0];
      // This becomes: auto* var = member[0].get();
      auto affected_op_call =
          traverse(clang::TK_IgnoreUnlessSpelledInSource,
                   declStmt(has(varDecl(
                       hasType(pointsTo(autoType())),
                       has(cxxOperatorCallExpr(has(expr(rhs_expr_variations)))
                               .bind("affectedOpCall"))))));
      match_finder_.addMatcher(affected_op_call, &affected_ptr_expr_rewriter_);

      // handles expressions of the form:
      // base::ranges::any_of(view->children(), [](const auto* v) {
      //     ...
      //   });
      // where auto* needs to be rewritten into type_name*.
      auto range_exprs = callExpr(
          callee(functionDecl(anyOf(
              matchesName("find"), matchesName("any_of"), matchesName("all_of"),
              matchesName("transform"), matchesName("copy"),
              matchesName("accumulate"), matchesName("count")))),
          hasArgument(0, traverse(clang::TK_IgnoreUnlessSpelledInSource,
                                  expr(anyOf(rhs_expr_variations, reversed_expr,
                                             ctn_like_type)))),
          hasAnyArgument(expr(allOf(
              traverse(
                  clang::TK_IgnoreUnlessSpelledInSource,
                  lambdaExpr(
                      has(parmVarDecl(
                              hasTypeLoc(loc(qualType(anything()))
                                             .bind("template_type_param_loc")),
                              hasType(pointsTo(templateTypeParmType())))
                              .bind("lambda_parmVarDecl")))
                      .bind("lambda_expr")),
              lambdaExpr(has(cxxRecordDecl(has(functionTemplateDecl(has(
                  cxxMethodDecl(isTemplateInstantiation()).bind("md")))))))))));
      match_finder_.addMatcher(range_exprs, &affected_ptr_expr_rewriter_);
    }

    // needed for ternary operator expr: (cond) ? true_expr : false_expr;
    // true_expr => lhs; false_expr => rhs;
    // creates a link between false_expr and true_expr of a ternary conditional
    // operator;
    // handles:
    // (cond) ? (a/&a/*a/std::move(a)/fct()/*fct()/&fct()/a.begin()) :
    // (b/&b/*b/std::move(b)/fct()/*fct()/&fct()/b.begin())
    auto ternary_cond_expr =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 conditionalOperator(hasTrueExpression(lhs_expr_variations),
                                     hasFalseExpression(rhs_expr_variations),
                                     unless(isExpansionInSystemHeader())));
    match_finder_.addMatcher(ternary_cond_expr, &potentail_nodes_);

    // Handles assignment:
    // a = b;
    // a = &b;
    // *a = b;
    // *a = *b;
    // a = fct();
    // a = *fct();
    // a = std::move(b);
    // a = &fct();
    // it = member.begin();
    // a = vector<S*>();
    // a = (cond) ? expr1 : expr2;
    auto assignement_relationship = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        binaryOperation(hasOperatorName("="),
                        hasOperands(lhs_expr_variations,
                                    anyOf(rhs_expr_variations,
                                          conditionalOperator(hasTrueExpression(
                                              rhs_expr_variations)))),
                        unless(isExpansionInSystemHeader())));
    match_finder_.addMatcher(assignement_relationship, &potentail_nodes_);

    // Supports:
    // std::vector<T*>* temp = &member;
    // std::vector<T*>& temp = member;
    // std::vector<T*> temp = *member;
    // std::vector<T*> temp = member;  and other similar stmts.
    // std::vector<T*> temp = init();
    // std::vector<T*> temp = *fct();
    // std::vector<T*>::iterator it = member.begin();
    // std::vector<T*> temp = (cond) ? expr1 : expr2;
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
    match_finder_.addMatcher(var_construction, &potentail_nodes_);

    // Supports:
    // return member;
    // return *member;
    // return &member;
    // return fct();
    // return *fct();
    // return std::move(member);
    // return member.begin();
    // return (cond) ? expr1 : expr2;
    auto returned_var_or_member = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        returnStmt(
            hasReturnValue(expr(anyOf(
                rhs_expr_variations,
                conditionalOperator(hasTrueExpression(rhs_expr_variations))))),
            unless(isExpansionInSystemHeader()),
            forFunction(functionDecl(hasReturnTypeLoc(lhs_type_loc))
                            .bind("lhs_fct_return")))
            .bind("lhs_stmt"));
    match_finder_.addMatcher(returned_var_or_member, &potentail_nodes_);

    // Handles expressions of the form member(arg).
    // A(const std::vector<T*>& arg): member(arg){}
    // member(init());
    // member(*fct());
    // member2(&member1);
    auto ctor_initilizer = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxCtorInitializer(withInitializer(anyOf(
                               cxxConstructExpr(has(expr(rhs_expr_variations))),
                               rhs_expr_variations)),
                           forField(lhs_field)));

    match_finder_.addMatcher(ctor_initilizer, &potentail_nodes_);

    // link var/field passed as function arguments to function parameter
    // This handles func(var/member/param), func(&var/member/param),
    // func(*var/member/param), func(func2()), func(&func2())
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
    match_finder_.addMatcher(call_expr, &potentail_nodes_);

    // Handles: member.swap(temp); temp.swap(member);
    auto member_swap_call = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxMemberCallExpr(callee(functionDecl(hasName("swap"))),
                          hasArgument(0, rhs_expr_variations),
                          has(memberExpr(has(expr(lhs_expr_variations)))),
                          unless(isExpansionInSystemHeader())));
    match_finder_.addMatcher(member_swap_call, &potentail_nodes_);

    // Handles: std::swap(member, temp); std::swap(temp, member);
    auto std_swap_call =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 callExpr(callee(functionDecl(hasName("std::swap"))),
                          hasArgument(0, lhs_expr_variations),
                          hasArgument(1, rhs_expr_variations),
                          unless(isExpansionInSystemHeader())));
    match_finder_.addMatcher(std_swap_call, &potentail_nodes_);

    auto assert_expect_eq =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 callExpr(anyOf(isExpandedFromMacro("EXPECT_EQ"),
                                isExpandedFromMacro("ASSERT_EQ")),
                          hasArgument(2, lhs_expr_variations),
                          hasArgument(3, rhs_expr_variations),
                          unless(isExpansionInSystemHeader())));
    match_finder_.addMatcher(assert_expect_eq, &potentail_nodes_);

    // Supports:
    // std::vector<S*> temp;
    // Obj o(temp); Obj o{temp};
    // This links temp to the parameter in Obj's constructor.
    auto var_passed_in_constructor = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxConstructExpr(forEachArgumentWithParam(
            expr(anyOf(
                rhs_expr_variations,
                conditionalOperator(hasTrueExpression(rhs_expr_variations)))),
            lhs_param)));
    match_finder_.addMatcher(var_passed_in_constructor, &potentail_nodes_);

    // handles Obj o{temp} when Obj has no constructor.
    // This creates a link between the expr and the underlying field.
    auto var_passed_in_initlistExpr = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        initListExpr(raw_ptr_plugin::forEachInitExprWithFieldDecl(
            expr(anyOf(
                rhs_expr_variations,
                conditionalOperator(hasTrueExpression(rhs_expr_variations)))),
            lhs_field)));
    match_finder_.addMatcher(var_passed_in_initlistExpr, &potentail_nodes_);

    // This creates a link between each argument passed to the make_unique call
    // and the corresponding constructor parameter.
    auto make_unique_call = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        callExpr(
            callee(functionDecl(hasName("std::make_unique"))),
            forEachArg(
                expr(rhs_expr_variations,
                     hasParent(callExpr(
                         callee(functionDecl(hasDescendant(cxxConstructExpr(
                             has(cxxNewExpr(has(cxxConstructExpr(hasDeclaration(
                                 functionDecl().bind("fct_decl"))))))))))))),
                lhs_param)));
    match_finder_.addMatcher(make_unique_call, &potentail_nodes_);

    // This creates a link between lhs and an argument passed to emplace.
    // Example:
    // std::map<int, std::vector<S*>> m;
    // m.emplace(index, o.member);
    // where member has type std::vector<S*>;
    auto emplace_call_with_arg =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 cxxMemberCallExpr(callee(functionDecl(hasName("emplace"))),
                                   has(memberExpr(has(rhs_expr_variations))),
                                   hasAnyArgument(lhs_expr_variations)));
    match_finder_.addMatcher(emplace_call_with_arg, &potentail_nodes_);

    // Handle BindOnce/BindRepeating;
    auto first_arg = hasParent(callExpr(hasArgument(
        0, anyOf(lambdaExpr().bind("lambda_expr"),
                 unaryOperator(
                     has(declRefExpr(to(functionDecl().bind("fct_decl")))))))));

    auto bind_args = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        callExpr(
            callee(functionDecl(
                anyOf(hasName("BindOnce"), hasName("BindRepeating")))),
            forEachBindArg(expr(rhs_expr_variations, first_arg), lhs_param)));
    match_finder_.addMatcher(bind_args, &potentail_nodes_);

    // This is useful to handle iteration over maps with vector as value.
    // Example:
    // std::vector<int, std::vector<S*>> m;
    // for (auto& p : m){
    // ...
    // }
    // This creates a link between p and m.
    auto for_range_stmts = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        cxxForRangeStmt(
            hasLoopVariable(decl(lhs_var, unless(has(pointerTypeLoc())))),
            hasRangeInit(rhs_expr_variations)));
    match_finder_.addMatcher(for_range_stmts, &potentail_nodes_);

    // Map function declaration signature to function definition signature;
    // This is problematic in the case of callbacks defined in function.
    auto fct_decls_params = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        functionDecl(forEachParmVarDecl(rhs_param),
                     unless(anyOf(isExpansionInSystemHeader(),
                                  raw_ptr_plugin::isInMacroLocation())))
            .bind("fct_decl"));
    match_finder_.addMatcher(fct_decls_params, &fct_sig_nodes_);

    auto fct_decls_returns = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        functionDecl(hasReturnTypeLoc(rhs_type_loc),
                     unless(anyOf(isExpansionInSystemHeader(),
                                  raw_ptr_plugin::isInMacroLocation())))
            .bind("fct_decl"));
    match_finder_.addMatcher(fct_decls_returns, &fct_sig_nodes_);

    auto macro_fct_signatures = traverse(
        clang::TK_IgnoreUnlessSpelledInSource,
        templateSpecializationTypeLoc(
            rhs_location,
            hasAncestor(
                cxxMethodDecl(raw_ptr_plugin::isInMacroLocation(),
                              anyOf(isExpandedFromMacro("MOCK_METHOD"),
                                    isExpandedFromMacro("MOCK_METHOD0"),
                                    isExpandedFromMacro("MOCK_METHOD1"),
                                    isExpandedFromMacro("MOCK_METHOD2"),
                                    isExpandedFromMacro("MOCK_METHOD3"),
                                    isExpandedFromMacro("MOCK_METHOD4"),
                                    isExpandedFromMacro("MOCK_METHOD5"),
                                    isExpandedFromMacro("MOCK_METHOD6"),
                                    isExpandedFromMacro("MOCK_METHOD7"),
                                    isExpandedFromMacro("MOCK_METHOD8"),
                                    isExpandedFromMacro("MOCK_METHOD9"),
                                    isExpandedFromMacro("MOCK_METHOD10"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD0"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD1"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD2"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD3"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD4"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD5"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD6"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD7"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD8"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD9"),
                                    isExpandedFromMacro("MOCK_CONST_METHOD10")),
                              unless(isExpansionInSystemHeader()))
                    .bind("fct_decl"))));
    match_finder_.addMatcher(macro_fct_signatures, &fct_sig_nodes_);

    // TODO: handle calls to templated functions
  }

 private:
  MatchFinder& match_finder_;
  AffectedPtrExprRewriter affected_ptr_expr_rewriter_;
  PotentialNodes potentail_nodes_;
  FunctionSignatureNodes fct_sig_nodes_;
  const raw_ptr_plugin::FilterFile* paths_to_exclude;
};

}  // namespace

int main(int argc, const char* argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::cl::OptionCategory category(
      "rewrite_templated_container_fields: changes |vector<T*> field_| to "
      "|vector<raw_ptr<T>> field_|.");

  llvm::cl::opt<std::string> override_exclude_paths_param(
      kOverrideExcludePathsParamName, llvm::cl::value_desc("filepath"),
      llvm::cl::desc(
          "override file listing paths to be blocked (not rewritten)"));
  llvm::Expected<clang::tooling::CommonOptionsParser> options =
      clang::tooling::CommonOptionsParser::create(argc, argv, category);
  assert(static_cast<bool>(options));  // Should not return an error.
  clang::tooling::ClangTool tool(options->getCompilations(),
                                 options->getSourcePathList());

  std::unique_ptr<raw_ptr_plugin::FilterFile> paths_to_exclude;
  if (override_exclude_paths_param.getValue().empty()) {
    std::vector<std::string> paths_to_exclude_lines;
    for (auto* const line : kRawPtrManualPathsToIgnore) {
      paths_to_exclude_lines.push_back(line);
    }
    for (auto* const line : kSeparateRepositoryPaths) {
      paths_to_exclude_lines.push_back(line);
    }
    paths_to_exclude =
        std::make_unique<raw_ptr_plugin::FilterFile>(paths_to_exclude_lines);
  } else {
    paths_to_exclude = std::make_unique<raw_ptr_plugin::FilterFile>(
        override_exclude_paths_param,
        override_exclude_paths_param.ArgStr.str());
  }

  // Map a function signature, which is modeled as a string representing file
  // location, to it's graph nodes (RTNode and ParmVarDecl nodes).
  // RTNode represents a function return type.
  std::map<std::string, std::set<Node>> fct_sig_nodes;
  // Map related function signatures to each other, this is needed for functions
  // with separate definition and declaration, and for overridden functions.
  std::vector<std::pair<std::string, std::string>> fct_sig_pairs;
  OutputHelper output_helper;
  MatchFinder match_finder;
  ContainerRewriter rewriter(match_finder, output_helper, fct_sig_nodes,
                             fct_sig_pairs, paths_to_exclude.get());
  rewriter.addMatchers();

  // Prepare and run the tool.
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(factory.get());

  // For each pair of adjacent function signatures, create a link between
  // corresponding parameters.
  // 2 functions are said to be adjacent if one overrides the other, or if one
  // is a function definition and the other is that function's declaration.
  for (auto& [l, r] : fct_sig_pairs) {
    if (fct_sig_nodes.find(l) == fct_sig_nodes.end()) {
      continue;
    }
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
      i1++;
      i2++;
    }
  }

  // Emits the list of edges.
  output_helper.Emit();

  return result;
}
