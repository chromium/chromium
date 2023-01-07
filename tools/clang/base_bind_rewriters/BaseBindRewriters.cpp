// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <memory>
#include <string>

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
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"

using Replacements = std::vector<clang::tooling::Replacement>;
using clang::ASTContext;
using clang::CFG;
using clang::CFGBlock;
using clang::CFGLifetimeEnds;
using clang::CFGStmt;
using clang::CallExpr;
using clang::Decl;
using clang::DeclRefExpr;
using clang::FunctionDecl;
using clang::LambdaExpr;
using clang::Stmt;
using clang::UnaryOperator;
using clang::ast_type_traits::DynTypedNode;
using clang::tooling::CommonOptionsParser;
using namespace clang::ast_matchers;

namespace {

class Rewriter {
 public:
  virtual ~Rewriter() {}
};

// Removes unneeded base::Passed() on a parameter of base::BindOnce().
// Example:
//   // Before
//   base::BindOnce(&Foo, base::Passed(&bar));
//   base::BindOnce(&Foo, base::Passed(std::move(baz)));
//   base::BindOnce(&Foo, base::Passed(qux));
//
//   // After
//   base::BindOnce(&Foo, std::move(bar));
//   base::BindOnce(&Foo, std::move(baz));
//   base::BindOnce(&Foo, std::move(*qux));
class PassedToMoveRewriter : public MatchFinder::MatchCallback,
                             public Rewriter {
 public:
  explicit PassedToMoveRewriter(Replacements* replacements)
      : replacements_(replacements) {}

  StatementMatcher GetMatcher() {
    auto is_passed = namedDecl(hasName("::base::Passed"));
    auto is_bind_once_call = callee(namedDecl(hasName("::base::BindOnce")));

    // Matches base::Passed() call on a base::BindOnce() argument.
    return callExpr(is_bind_once_call,
                    hasAnyArgument(ignoringImplicit(
                        callExpr(callee(is_passed)).bind("target"))));
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* target = result.Nodes.getNodeAs<CallExpr>("target");
    auto* callee = target->getCallee()->IgnoreImpCasts();

    auto* callee_decl = clang::dyn_cast<DeclRefExpr>(callee)->getDecl();
    auto* passed_decl = clang::dyn_cast<FunctionDecl>(callee_decl);
    auto* param_type = passed_decl->getParamDecl(0)->getType().getTypePtr();

    if (param_type->isRValueReferenceType()) {
      // base::Passed(xxx) -> xxx.
      // The parameter type is already an rvalue reference.
      // Example:
      //   std::unique_ptr<int> foo();
      //   std::unique_ptr<int> bar;
      //   base::Passed(foo());
      //   base::Passed(std::move(bar));
      // In these cases, we can just remove base::Passed.
      auto left = clang::CharSourceRange::getTokenRange(
          result.SourceManager->getSpellingLoc(target->getBeginLoc()),
          result.SourceManager->getSpellingLoc(target->getArg(0)->getExprLoc())
              .getLocWithOffset(-1));
      auto r_paren = clang::CharSourceRange::getTokenRange(
          result.SourceManager->getSpellingLoc(target->getRParenLoc()),
          result.SourceManager->getSpellingLoc(target->getRParenLoc()));
      replacements_->emplace_back(*result.SourceManager, left, " ");
      replacements_->emplace_back(*result.SourceManager, r_paren, " ");
      return;
    }

    if (!param_type->isPointerType())
      return;

    auto* passed_arg = target->getArg(0)->IgnoreImpCasts();
    if (auto* unary = clang::dyn_cast<clang::UnaryOperator>(passed_arg)) {
      if (unary->getOpcode() == clang::UO_AddrOf) {
        // base::Passed(&xxx) -> std::move(xxx).
        auto left = clang::CharSourceRange::getTokenRange(
            result.SourceManager->getSpellingLoc(target->getBeginLoc()),
            result.SourceManager->getSpellingLoc(
                target->getArg(0)->getExprLoc()));
        replacements_->emplace_back(*result.SourceManager, left, "std::move(");
        return;
      }
    }

    // base::Passed(xxx) -> std::move(*xxx)
    auto left = clang::CharSourceRange::getTokenRange(
        result.SourceManager->getSpellingLoc(target->getBeginLoc()),
        result.SourceManager->getSpellingLoc(target->getArg(0)->getExprLoc())
            .getLocWithOffset(-1));
    replacements_->emplace_back(*result.SourceManager, left, "std::move(*");
  }

 private:
  Replacements* replacements_;
};

// Replace base::Bind() and base::BindRepeating() to base::BindOnce() where
// resulting callbacks are implicitly converted into base::OnceCallback.
// Example:
//   // Before
//   base::PostTask(FROM_HERE, base::BindRepeating(&Foo));
//   base::OnceCallback<void()> cb = base::BindRepeating(&Foo);
//
//   // After
//   base::PostTask(FROM_HERE, base::BindOnce(&Foo));
//   base::OnceCallback<void()> cb = base::BindOnce(&Foo);
class BindOnceRewriter : public MatchFinder::MatchCallback, public Rewriter {
 public:
  explicit BindOnceRewriter(Replacements* replacements)
      : replacements_(replacements) {}

  StatementMatcher GetMatcher() {
    auto is_once_callback = hasType(hasCanonicalType(hasDeclaration(
        classTemplateSpecializationDecl(hasName("::base::OnceCallback")))));
    auto is_repeating_callback =
        hasType(hasCanonicalType(hasDeclaration(classTemplateSpecializationDecl(
            hasName("::base::RepeatingCallback")))));

    auto bind_call =
        callExpr(callee(namedDecl(anyOf(hasName("::base::Bind"),
                                        hasName("::base::BindRepeating")))))
            .bind("target");
    auto parameter_construction =
        cxxConstructExpr(is_repeating_callback, argumentCountIs(1),
                         hasArgument(0, ignoringImplicit(bind_call)));
    auto constructor_conversion = cxxConstructExpr(
        is_once_callback, argumentCountIs(1),
        hasArgument(0, ignoringImplicit(parameter_construction)));
    return implicitCastExpr(is_once_callback,
                            hasSourceExpression(constructor_conversion));
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* target = result.Nodes.getNodeAs<clang::CallExpr>("target");
    auto* callee = target->getCallee();
    auto range = clang::CharSourceRange::getTokenRange(
        result.SourceManager->getSpellingLoc(callee->getEndLoc()),
        result.SourceManager->getSpellingLoc(callee->getEndLoc()));
    replacements_->emplace_back(*result.SourceManager, range, "BindOnce");
  }

 private:
  Replacements* replacements_;
};

// Converts pass-by-const-ref base::RepeatingCallback's to
// pass-by-value. Example:
//   // Before
//   using BarCallback = base::RepeatingCallback<void(void*)>;
//   void Foo(const base::RepeatingCallback<void(int)>& cb);
//   void Bar(const BarCallback& cb);
//
//   // After
//   using BarCallback = base::RepeatingCallback<void(void*)>;
//   void Foo(base::RepeatingCallback<void(int)> cb);
//   void Bar(BarCallback cb);
class PassByValueRewriter : public MatchFinder::MatchCallback, public Rewriter {
 public:
  explicit PassByValueRewriter(Replacements* replacements)
      : replacements_(replacements) {}

  DeclarationMatcher GetMatcher() {
    auto is_repeating_callback =
        namedDecl(hasName("::base::RepeatingCallback"));
    return parmVarDecl(
               hasType(hasCanonicalType(references(is_repeating_callback))))
        .bind("target");
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* target = result.Nodes.getNodeAs<clang::ParmVarDecl>("target");
    auto qual_type = target->getType();
    auto* ref_type =
        clang::dyn_cast<clang::LValueReferenceType>(qual_type.getTypePtr());
    if (!ref_type || !ref_type->getPointeeType().isLocalConstQualified())
      return;

    // Remove the leading `const` and the following `&`.
    auto type_loc = target->getTypeSourceInfo()->getTypeLoc();
    auto const_keyword = clang::CharSourceRange::getTokenRange(
        result.SourceManager->getSpellingLoc(target->getBeginLoc()),
        result.SourceManager->getSpellingLoc(target->getBeginLoc()));
    auto lvalue_ref = clang::CharSourceRange::getTokenRange(
        result.SourceManager->getSpellingLoc(type_loc.getEndLoc()),
        result.SourceManager->getSpellingLoc(type_loc.getEndLoc()));
    replacements_->emplace_back(*result.SourceManager, const_keyword, " ");
    replacements_->emplace_back(*result.SourceManager, lvalue_ref, " ");
  }

 private:
  Replacements* replacements_;
};

// Adds std::move() to base::RepeatingCallback<> where it looks relevant.
// Example:
//   // Before
//   void Foo(base::RepeatingCallback<void(int)> cb1) {
//     base::RepeatingClosure cb2 = base::BindRepeating(cb1, 42);
//     PostTask(FROM_HERE, cb2);
//   }
//
//   // After
//   void Foo(base::RepeatingCallback<void(int)> cb1) {
//     base::RepeatingClosure cb2 = base::BindRepeating(std::move(cb1), 42);
//     PostTask(FROM_HERE, std::move(cb2));
//   }
class AddStdMoveRewriter : public MatchFinder::MatchCallback, public Rewriter {
 public:
  explicit AddStdMoveRewriter(Replacements* replacements)
      : replacements_(replacements) {}

  StatementMatcher GetMatcher() {
    return declRefExpr(
               hasType(hasCanonicalType(hasDeclaration(
                   namedDecl(hasName("::base::RepeatingCallback"))))),
               anyOf(hasAncestor(cxxConstructorDecl().bind("enclosing_ctor")),
                     hasAncestor(functionDecl().bind("enclosing_func")),
                     hasAncestor(lambdaExpr().bind("enclosing_lambda"))))
        .bind("target");
  }

  // Build Control Flow Graph (CFG) for |stmt| and populate class members with
  // the content of the graph. Returns true if the analysis finished
  // successfully.
  bool ExtractCFGContentToMembers(Stmt* stmt, ASTContext* context) {
    // Try to make a cache entry. The failure implies it's already in the cache.
    auto inserted = cfg_cache_.emplace(stmt, nullptr);
    if (!inserted.second)
      return !!inserted.first->second;

    std::unique_ptr<CFG>& cfg = inserted.first->second;
    CFG::BuildOptions opts;
    opts.AddInitializers = true;
    opts.AddLifetime = true;
    opts.AddStaticInitBranches = true;
    cfg = CFG::buildCFG(nullptr, stmt, context, opts);

    // CFG construction may fail. Report it to the caller.
    if (!cfg)
      return false;
    if (!parent_map_)
      parent_map_ = std::make_unique<clang::ParentMap>(stmt);
    else
      parent_map_->addStmt(stmt);

    // Populate |top_stmts_|, that contains Stmts that is evaluated in its own
    // CFGElement.
    for (auto* block : *cfg) {
      for (auto& elem : *block) {
        if (auto stmt = elem.getAs<CFGStmt>())
          top_stmts_.insert(stmt->getStmt());
      }
    }

    // Populate |enclosing_block_|, that maps a Stmt to a CFGBlock that contains
    // the Stmt.
    std::function<void(const CFGBlock*, const Stmt*)> recursive_set_enclosing =
        [&](const CFGBlock* block, const Stmt* stmt) {
          enclosing_block_[stmt] = block;
          for (auto* c : stmt->children()) {
            if (!c)
              continue;
            if (top_stmts_.find(c) != top_stmts_.end())
              continue;
            recursive_set_enclosing(block, c);
          }
        };
    for (auto* block : *cfg) {
      for (auto& elem : *block) {
        if (auto stmt = elem.getAs<CFGStmt>())
          recursive_set_enclosing(block, stmt->getStmt());
      }
    }

    return true;
  }

  const Stmt* EnclosingCxxStatement(const Stmt* stmt) {
    while (true) {
      const Stmt* parent = parent_map_->getParentIgnoreParenCasts(stmt);
      assert(parent);
      switch (parent->getStmtClass()) {
        case Stmt::CompoundStmtClass:
        case Stmt::ForStmtClass:
        case Stmt::CXXForRangeStmtClass:
        case Stmt::WhileStmtClass:
        case Stmt::DoStmtClass:
        case Stmt::IfStmtClass:

          // Other candidates:
          //   Stmt::CXXTryStmtClass
          //   Stmt::CXXCatchStmtClass
          //   Stmt::CapturedStmtClass
          //   Stmt::SwitchStmtClass
          //   Stmt::SwitchCaseClass
          return stmt;
        default:
          stmt = parent;
          break;
      }
    }
  }

  bool WasPointerTaken(const Stmt* stmt, const Decl* decl) {
    std::function<bool(const Stmt*)> visit_stmt = [&](const Stmt* stmt) {
      if (auto* op = clang::dyn_cast<UnaryOperator>(stmt)) {
        if (op->getOpcode() == clang::UO_AddrOf) {
          auto* ref = clang::dyn_cast<DeclRefExpr>(op->getSubExpr());
          // |ref| may be null if the sub-expr has a dependent type.
          if (ref && ref->getDecl() == decl)
            return true;
        }
      }

      for (auto* c : stmt->children()) {
        if (!c)
          continue;
        if (visit_stmt(c))
          return true;
      }
      return false;
    };
    return visit_stmt(stmt);
  }

  bool HasCapturingLambda(const Stmt* stmt, const Decl* decl) {
    std::function<bool(const Stmt*)> visit_stmt = [&](const Stmt* stmt) {
      if (auto* l = clang::dyn_cast<LambdaExpr>(stmt)) {
        for (auto c : l->captures()) {
          if (c.getCapturedVar() == decl)
            return true;
        }
      }

      for (auto* c : stmt->children()) {
        if (!c)
          continue;

        if (visit_stmt(c))
          return true;
      }

      return false;
    };
    return visit_stmt(stmt);
  }

  // Returns true if there are multiple occurrences to |decl| in one of C++
  // statements in |stmt|.
  bool HasUnorderedOccurrences(const Decl* decl, const Stmt* stmt) {
    int count = 0;
    std::function<void(const Stmt*)> visit_stmt = [&](const Stmt* s) {
      if (auto* ref = clang::dyn_cast<DeclRefExpr>(s)) {
        if (ref->getDecl() == decl)
          ++count;
      }
      for (auto* c : s->children()) {
        if (!c)
          continue;
        visit_stmt(c);
      }
    };

    visit_stmt(EnclosingCxxStatement(stmt));
    return count > 1;
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* target = result.Nodes.getNodeAs<clang::DeclRefExpr>("target");
    auto* decl = clang::dyn_cast<clang::VarDecl>(target->getDecl());

    // Other than local variables and parameters are out-of-scope.
    if (!decl || !decl->isLocalVarDeclOrParm())
      return;

    auto qual_type = decl->getType();
    // Qualified variables are out-of-scope. They are likely not movable.
    if (qual_type.getCanonicalType().hasQualifiers())
      return;

    auto* type = qual_type.getTypePtr();
    // References and pointers are out-of-scope.
    if (type->isReferenceType() || type->isPointerType())
      return;

    Stmt* body = nullptr;
    if (auto* ctor = result.Nodes.getNodeAs<LambdaExpr>("enclosing_ctor"))
      return;  // Skip constructor case for now. TBD.
    else if (auto* func =
                 result.Nodes.getNodeAs<FunctionDecl>("enclosing_func"))
      body = func->getBody();
    else if (auto* lambda =
                 result.Nodes.getNodeAs<LambdaExpr>("enclosing_lambda"))
      body = lambda->getBody();
    else
      return;

    // Disable the replacement if there is a lambda that captures |decl|.
    if (HasCapturingLambda(body, decl))
      return;

    // Disable the replacement if the pointer to |decl| is taken in the scope.
    if (WasPointerTaken(body, decl))
      return;

    if (!ExtractCFGContentToMembers(body, result.Context))
      return;

    auto* parent = parent_map_->getParentIgnoreParenCasts(target);
    if (auto* p = clang::dyn_cast<CallExpr>(parent)) {
      auto* callee = p->getCalleeDecl();
      // |callee| may be null if the CallExpr has an unresolved look up.
      if (!callee)
        return;
      auto* callee_decl = clang::dyn_cast<clang::NamedDecl>(callee);
      auto name = callee_decl->getQualifiedNameAsString();

      // Disable the replacement if it's already in std::move() or
      // std::forward().
      if (name == "std::__1::move" || name == "std::__1::forward")
        return;
    } else if (parent->getStmtClass() == Stmt::ReturnStmtClass) {
      // Disable the replacement if it's in a return statement.
      return;
    }

    // If the same C++ statement contains multiple reference to the variable,
    // don't insert std::move() to be conservative.
    if (HasUnorderedOccurrences(decl, target))
      return;

    bool saw_reuse = false;
    ForEachFollowingStmts(target, [&](const Stmt* stmt) {
      if (auto* ref = clang::dyn_cast<DeclRefExpr>(stmt)) {
        if (ref->getDecl() == decl) {
          saw_reuse = true;
          return false;
        }
      }

      // TODO: Detect Reset() and operator=() to stop the traversal.
      return true;
    });
    if (saw_reuse)
      return;

    replacements_->emplace_back(
        *result.SourceManager,
        result.SourceManager->getSpellingLoc(target->getBeginLoc()), 0,
        "std::move(");
    replacements_->emplace_back(
        *result.SourceManager,
        clang::Lexer::getLocForEndOfToken(target->getEndLoc(), 0,
                                          *result.SourceManager,
                                          result.Context->getLangOpts()),
        0, ")");
  }

  // Invokes |handler| for each Stmt that follows |target| until it reaches the
  // end of the lifetime of the variable that |target| references.
  // If |handler| returns false, stops following the current control flow.
  void ForEachFollowingStmts(const DeclRefExpr* target,
                             std::function<bool(const Stmt*)> handler) {
    auto* decl = target->getDecl();
    auto* block = enclosing_block_[target];

    std::set<const clang::CFGBlock*> visited;
    std::vector<const clang::CFGBlock*> stack = {block};

    bool saw_target = false;
    std::function<bool(const Stmt*)> visit_stmt = [&](const Stmt* s) {
      for (auto* t : s->children()) {
        if (!t)
          continue;

        // |t| is evaluated elsewhere if a sub-Stmt is in |top_stmt_|.
        if (top_stmts_.find(t) != top_stmts_.end())
          continue;

        if (!visit_stmt(t))
          return false;
      }

      if (!saw_target) {
        if (s == target)
          saw_target = true;
        return true;
      }

      return handler(s);
    };

    bool visited_initial_block_twice = false;
    while (!stack.empty()) {
      auto* b = stack.back();
      stack.pop_back();
      if (!visited.insert(b).second) {
        if (b != block || visited_initial_block_twice)
          continue;
        visited_initial_block_twice = true;
      }

      bool cont = true;
      for (auto e : *b) {
        if (auto s = e.getAs<CFGStmt>()) {
          if (!visit_stmt(s->getStmt())) {
            cont = false;
            break;
          }
        } else if (auto l = e.getAs<CFGLifetimeEnds>()) {
          if (l->getVarDecl() == decl) {
            cont = false;
            break;
          }
        }
      }

      if (cont) {
        for (auto s : b->succs()) {
          if (!s)
            continue;  // Unreachable block.
          stack.push_back(s);
        }
      }
    }
  }

 private:
  // Function body to CFG.
  std::map<const Stmt*, std::unique_ptr<CFG>> cfg_cache_;

  // Statement to the enclosing CFGBlock.
  std::map<const Stmt*, const CFGBlock*> enclosing_block_;

  // Stmt to its parent Stmt.
  std::unique_ptr<clang::ParentMap> parent_map_;

  // A set of Stmt that a CFGElement has it directly.
  std::set<const Stmt*> top_stmts_;

  Replacements* replacements_;
};

llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);
llvm::cl::OptionCategory rewriter_category("Rewriter Options");

llvm::cl::opt<std::string> rewriter_option(
    "rewriter",
    llvm::cl::desc(R"(One of the name of rewriter to apply.
Available rewriters are:
    remove_unneeded_passed
    bind_to_bind_once
    pass_by_value
    add_std_move
The default is remove_unneeded_passed.
)"),
    llvm::cl::init("remove_unneeded_passed"),
    llvm::cl::cat(rewriter_category));

}  // namespace.

int main(int argc, const char* argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  CommonOptionsParser options(argc, argv, rewriter_category);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  MatchFinder match_finder;
  std::vector<clang::tooling::Replacement> replacements;

  std::unique_ptr<Rewriter> rewriter;
  if (rewriter_option == "remove_unneeded_passed") {
    auto passed_to_move = std::make_unique<PassedToMoveRewriter>(&replacements);
    match_finder.addMatcher(passed_to_move->GetMatcher(), passed_to_move.get());
    rewriter = std::move(passed_to_move);
  } else if (rewriter_option == "bind_to_bind_once") {
    auto bind_once = std::make_unique<BindOnceRewriter>(&replacements);
    match_finder.addMatcher(bind_once->GetMatcher(), bind_once.get());
    rewriter = std::move(bind_once);
  } else if (rewriter_option == "pass_by_value") {
    auto pass_by_value = std::make_unique<PassByValueRewriter>(&replacements);
    match_finder.addMatcher(pass_by_value->GetMatcher(), pass_by_value.get());
    rewriter = std::move(pass_by_value);
  } else if (rewriter_option == "add_std_move") {
    auto add_std_move = std::make_unique<AddStdMoveRewriter>(&replacements);
    match_finder.addMatcher(add_std_move->GetMatcher(), add_std_move.get());
    rewriter = std::move(add_std_move);
  } else {
    abort();
  }

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
