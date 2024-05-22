// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <cstdint>
#include <memory>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Analysis/FlowSensitive/AdornedCFG.h"
#include "clang/Analysis/FlowSensitive/DataflowAnalysis.h"
#include "clang/Analysis/FlowSensitive/DataflowLattice.h"
#include "clang/Analysis/FlowSensitive/NoopLattice.h"
#include "clang/Analysis/FlowSensitive/Value.h"
#include "clang/Analysis/FlowSensitive/WatchedLiteralsSolver.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Tooling/Transformer/Stencil.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/TimeProfiler.h"

// This clang plugin check for iterators used after they have been
// invalidated.
//
// Pre-existing bugs found: https://crbug.com/1421293

namespace {

const char kInvalidIteratorUsage[] =
    "[iterator-checker] Potentially invalid iterator used.";

const char kInvalidIteratorComparison[] =
    "[iterator-checker] Potentially invalid iterator comparison.";

// To understand C++ code, we need a way to encode what is an iterator and what
// are the functions that might invalidate them.
//
// The Clang frontend supports several source-level annotations in the form of
// GCC-style attributes and pragmas that can help make using the Clang Static
// Analyzer useful. We aim to provide support for those annotations. For now, we
// hard code those for "known" interesting classes.
// TODO(crbug.com/40272746) Support source-level annotations.
enum Annotation : uint8_t {
  kNone = 0,

  // Annotate function returning an iterator.
  kReturnIterator = 1 << 0,

  // Annotate function returning an "end" iterator.
  // The distinction with `kReturnIterator` is important because we need to
  // special case its iterator creation.
  kReturnEndIterator = 1 << 1,

  // Annotate function returning a pair of iterators.
  // TODO(crbug.com/40272746) Not yet implemented.
  kReturnIteratorPair = 1 << 2,

  // Annotate function invalidating the iterator in its arguments.
  kInvalidateArgs = 1 << 3,

  // Annotate function invalidating every iterators.
  kInvalidateAll = 1 << 4,
};

static llvm::DenseMap<llvm::StringRef, uint8_t> g_functions_annotations = {
    {"std::begin", Annotation::kReturnIterator},
    {"std::cbegin", Annotation::kReturnIterator},
    {"std::end", Annotation::kReturnEndIterator},
    {"std::cend", Annotation::kReturnEndIterator},
    {"std::next", Annotation::kReturnIterator},
    {"std::prev", Annotation::kReturnIterator},
    {"std::find", Annotation::kReturnIterator},
    // TODO(crbug.com/40272746) Add additional functions.
};

static llvm::DenseMap<llvm::StringRef, llvm::DenseMap<llvm::StringRef, uint8_t>>
    g_member_function_annotations = {
        {
            "std::vector",
            {
                {"append_range", Annotation::kInvalidateAll},
                {"assign", Annotation::kInvalidateAll},
                {"assign_range", Annotation::kInvalidateAll},
                {"back", Annotation::kNone},
                {"begin", Annotation::kReturnIterator},
                {"capacity", Annotation::kNone},
                {"cbegin", Annotation::kReturnIterator},
                {"cend", Annotation::kReturnEndIterator},
                {"clear", Annotation::kInvalidateAll},
                {"crbegin", Annotation::kReturnIterator},
                {"crend", Annotation::kReturnIterator},
                {"data", Annotation::kNone},
                {"emplace",
                 Annotation::kReturnIterator | Annotation::kInvalidateAll},
                {"emplace_back", Annotation::kInvalidateAll},
                {"empty", Annotation::kNone},
                {"end", Annotation::kReturnEndIterator},
                {"erase",
                 Annotation::kReturnIterator | Annotation::kInvalidateAll},
                {"front", Annotation::kNone},
                {"insert",
                 Annotation::kInvalidateAll | Annotation::kReturnIterator},
                {"insert_range",
                 Annotation::kInvalidateAll | Annotation::kReturnIterator},
                {"max_size", Annotation::kNone},
                {"pop_back", Annotation::kInvalidateAll},
                {"push_back", Annotation::kInvalidateAll},
                {"rbegin", Annotation::kReturnIterator},
                {"rend", Annotation::kReturnIterator},
                {"reserve", Annotation::kInvalidateAll},
                {"resize", Annotation::kInvalidateAll},
                {"shrink_to_fit", Annotation::kInvalidateAll},
                {"size", Annotation::kNone},
                {"swap", Annotation::kNone},
            },
        },
        {
            "std::unordered_set",
            {
                {"begin", Annotation::kReturnIterator},
                {"cbegin", Annotation::kReturnIterator},
                {"end", Annotation::kReturnEndIterator},
                {"cend", Annotation::kReturnEndIterator},
                {"clear", Annotation::kInvalidateAll},
                {"insert",
                 Annotation::kInvalidateAll | Annotation::kReturnIteratorPair},
                {"emplace",
                 Annotation::kInvalidateAll | Annotation::kReturnIteratorPair},
                {"emplace_hint",
                 Annotation::kInvalidateAll | Annotation::kReturnIterator},
                {"erase",
                 Annotation::kInvalidateArgs | Annotation::kReturnIterator},
                {"extract", Annotation::kInvalidateArgs},
                {"find", Annotation::kReturnIterator},
                // TODO(crbug.com/40272746) Add additional functions.
            },
        },
        {
            "WTF::Vector",
            {
                {"begin", Annotation::kReturnIterator},
                {"rbegin", Annotation::kReturnIterator},
                {"end", Annotation::kReturnEndIterator},
                {"rend", Annotation::kReturnEndIterator},
                {"clear", Annotation::kInvalidateAll},
                {"shrink_to_fit", Annotation::kInvalidateAll},
                {"push_back", Annotation::kInvalidateAll},
                {"emplace_back", Annotation::kInvalidateAll},
                {"insert", Annotation::kInvalidateAll},
                {"InsertAt", Annotation::kInvalidateAll},
                {"InsertVector", Annotation::kInvalidateAll},
                {"push_front", Annotation::kInvalidateAll},
                {"PrependVector", Annotation::kInvalidateAll},
                {"EraseAt", Annotation::kInvalidateAll},
                {"erase",
                 Annotation::kInvalidateAll | Annotation::kReturnIterator},
                // `pop_back` invalidates only the iterator pointed to the last
                // element, but we have no way to track it.
                {"pop_back", Annotation::kNone},
                // TODO(crbug.com/40272746) Add additional functions.
            },
        },
        {
            "std::deque",
            {
                {"begin", Annotation::kReturnIterator},
                {"cbegin", Annotation::kReturnIterator},
                {"rbegin", Annotation::kReturnIterator},
                {"end", Annotation::kReturnEndIterator},
                {"cend", Annotation::kReturnEndIterator},
                {"rend", Annotation::kReturnEndIterator},
                {"clear", Annotation::kInvalidateAll},
                {"shrink_to_fit", Annotation::kInvalidateAll},
                {"insert",
                 Annotation::kInvalidateAll | Annotation::kReturnIterator},
                {"emplace",
                 Annotation::kInvalidateAll | Annotation::kReturnIterator},
                {"erase",
                 Annotation::kInvalidateAll | Annotation::kReturnIterator},
                {"push_back", Annotation::kInvalidateAll},
                {"emplace_back", Annotation::kInvalidateAll},
                {"push_front", Annotation::kInvalidateAll},
                {"emplace_front", Annotation::kInvalidateAll},
                // TODO(crbug.com/40272746) Add additional functions.
            },
        },
};

llvm::raw_ostream& DebugStream() {
  // Updates to llvm::outs() to get debugs logs.
  return llvm::nulls();
}

llvm::raw_ostream& InfoStream() {
  // Updates to llvm::outs() to get info logs.
  return llvm::nulls();
}

// In DataflowAnalysis, we associate to every C++ prvalue:
//
// - A RecordStorageLocation:
//   This will be used to reference the actual location of the values being used
//   during the analysis. For example, in `auto it = std::begin(cont)`, it will
//   be assigned a RecordStorageLocation.
//
// - Some fields:
//   Those are just one-to-one mapping with the actual record type being
//   modeled.
//
// - Some synthetic fields:
//   Those are the essence of how dataflow analysis work. Those fields are not
//   actually mapped to existing fields in the record type, but are ones that we
//   use in order to perform the analysis. For instance, in this analysis, those
//   fields are:
//   - `is_valid` - This field is used to store the iterator validity.
//   - `is_end` - Stores whether the current iterator points to the end
//   iterator.
//   - `container` - Stores the container which the iterator refers to.
//
// We also keep track of the `iterator` -> `container` mapping in order to
// invalidate iterators when necessary.

clang::dataflow::Value* GetSyntheticFieldWithName(
    llvm::StringRef name,
    const clang::dataflow::Environment& env,
    const clang::dataflow::RecordStorageLocation& loc) {
  return env.getValue(loc.getSyntheticField(name));
}

clang::dataflow::BoolValue* GetIsValid(
    const clang::dataflow::Environment& env,
    const clang::dataflow::RecordStorageLocation& loc) {
  return clang::cast_or_null<clang::dataflow::BoolValue>(
      GetSyntheticFieldWithName("is_valid", env, loc));
}

clang::dataflow::BoolValue* GetIsEnd(
    const clang::dataflow::Environment& env,
    const clang::dataflow::RecordStorageLocation& loc) {
  return clang::cast_or_null<clang::dataflow::BoolValue>(
      GetSyntheticFieldWithName("is_end", env, loc));
}

void SetSyntheticFieldWithName(
    llvm::StringRef name,
    clang::dataflow::Environment& env,
    const clang::dataflow::RecordStorageLocation& loc,
    clang::dataflow::Value& res) {
  env.setValue(loc.getSyntheticField(name), res);
}

void SetIsValid(clang::dataflow::Environment& env,
                const clang::dataflow::RecordStorageLocation& loc,
                clang::dataflow::BoolValue& res) {
  SetSyntheticFieldWithName("is_valid", env, loc, res);
}

void SetIsEnd(clang::dataflow::Environment& env,
              const clang::dataflow::RecordStorageLocation& loc,
              clang::dataflow::BoolValue& res) {
  SetSyntheticFieldWithName("is_end", env, loc, res);
}

const clang::dataflow::Formula& ForceBoolValue(
    clang::dataflow::Environment& env,
    const clang::Expr& expr) {
  auto* value = env.get<clang::dataflow::BoolValue>(expr);
  if (value != nullptr) {
    return value->formula();
  }

  value = &env.makeAtomicBoolValue();
  env.setValue(expr, *value);
  return value->formula();
}

// We don't use DataflowAnalysis lattices. Hence why the NoopLattice. Instead,
// we use the WatchedLiteralsSolver and populate different `Environment` with
// `Values`. The DataFlowAnalysis will iterate up until it can't make new
// deductions:
// - The `transfer` function updates an environment after executing one more
//   instructions.
class InvalidIteratorAnalysis
    : public clang::dataflow::DataflowAnalysis<InvalidIteratorAnalysis,
                                               clang::dataflow::NoopLattice> {
 public:
  InvalidIteratorAnalysis(const clang::FunctionDecl* func,
                          clang::DiagnosticsEngine& diagnostic)
      : DataflowAnalysis(func->getASTContext()), diagnostic_(diagnostic) {}

  // Used by DataflowAnalysis template.
  clang::dataflow::NoopLattice initialElement() const {
    return clang::dataflow::NoopLattice();
  }

  // Used by DataflowAnalysis template.
  void transfer(const clang::CFGElement& elt,
                clang::dataflow::NoopLattice& state,
                clang::dataflow::Environment& env) {
    if (auto cfg_stmt = elt.getAs<clang::CFGStmt>()) {
      Transfer(*cfg_stmt->getStmt(), env);
    }
  }

  llvm::StringMap<clang::QualType> GetSyntheticFields(clang::QualType Type) {
    return llvm::StringMap<clang::QualType>{
        {"is_valid", getASTContext().BoolTy},
        {"is_end", getASTContext().BoolTy},
        // Currently this field is not modeled as a Record because we just need
        // a symbolic value (so BoolTy is a workaround)
        {"container", getASTContext().BoolTy},
    };
  }

 private:
  // Stmt: https://clang.llvm.org/doxygen/classclang_1_1Stmt.html
  void Transfer(const clang::Stmt& stmt, clang::dataflow::Environment& env) {
    if (auto* decl_stmt = clang::dyn_cast<clang::DeclStmt>(&stmt)) {
      Transfer(*decl_stmt, env);
      return;
    }

    if (auto* value_stmt = clang::dyn_cast<clang::ValueStmt>(&stmt)) {
      Transfer(*value_stmt, env);
      return;
    }
  }

  // DeclStmt: https://clang.llvm.org/doxygen/classclang_1_1DeclStmt.html
  void Transfer(const clang::DeclStmt& declaration_statement,
                clang::dataflow::Environment& env) {
    for (auto* decl : declaration_statement.decls()) {
      if (auto* var_decl = clang::dyn_cast<clang::VarDecl>(decl)) {
        Transfer(*var_decl, env);
      }
    }
  }

  // VarDecl: https://clang.llvm.org/doxygen/classclang_1_1VarDecl.html
  void Transfer(const clang::VarDecl& var_decl,
                clang::dataflow::Environment& env) {}

  // ValueStmt: https://clang.llvm.org/doxygen/classclang_1_1ValueStmt.html
  void Transfer(const clang::ValueStmt& value_stmt,
                clang::dataflow::Environment& env) {
    if (auto* expr = clang::dyn_cast<clang::Expr>(&value_stmt)) {
      Transfer(*expr, env);
    }
  }

  // Expr: https://clang.llvm.org/doxygen/classclang_1_1Expr.html
  void Transfer(const clang::Expr& expr, clang::dataflow::Environment& env) {
    if (auto* call_expr = clang::dyn_cast<clang::CallExpr>(&expr)) {
      Transfer(*call_expr, env);
      return;
    }

    if (auto* ctor = clang::dyn_cast<clang::CXXConstructExpr>(&expr)) {
      Transfer(*ctor, env);
      return;
    }

    if (auto* cast_expr = clang::dyn_cast<clang::CastExpr>(&expr)) {
      Transfer(*cast_expr, env);
      return;
    }

    //  TODO(crbug.com/40272746): Add support for operator[]
    //  (ArraySubscriptExpr)
  }

  void Transfer(const clang::CXXConstructExpr& expr,
                clang::dataflow::Environment& env) {
    if (!IsIterator(expr.getType().getCanonicalType())) {
      return;
    }

    const clang::CXXConstructorDecl* ctor = expr.getConstructor();
    assert(ctor != nullptr);

    if (ctor->isCopyOrMoveConstructor()) {
      auto* it = UnwrapAsIterator(expr.getArg(0), env);
      assert(it);

      // TODO(crbug.com/40272746): Add support for copy and move constructor
    }
  }

  // CallExpr: https://clang.llvm.org/doxygen/classclang_1_1CallExpr.html
  void Transfer(const clang::CallExpr& callexpr,
                clang::dataflow::Environment& env) {
    TransferCallExprCommon(callexpr, env);

    if (auto* expr = clang::dyn_cast<clang::CXXMemberCallExpr>(&callexpr)) {
      Transfer(*expr, env);
      return;
    }

    if (auto* expr = clang::dyn_cast<clang::CXXOperatorCallExpr>(&callexpr)) {
      Transfer(*expr, env);
      return;
    }
  }

  void TransferCallExprCommon(const clang::CallExpr& expr,
                              clang::dataflow::Environment& env) {
    auto* callee = expr.getDirectCallee();
    if (!callee) {
      return;
    }

    // If the function is known to return an iterator and we can associate it
    // with a known container, then we deduce the resulting expression is itself
    // an iterator:
    std::string callee_name = callee->getQualifiedNameAsString();
    auto it = g_functions_annotations.find(callee_name);
    if (it == g_functions_annotations.end()) {
      return;
    }

    if (!(it->second & Annotation::kReturnIterator) &&
        !(it->second & Annotation::kReturnEndIterator)) {
      return;
    }

    bool is_end = (it->second & Annotation::kReturnEndIterator) != 0;

    // In order to get the container value, we look for it:
    // 1. if there is an iterator tied to the first argument expression, in the
    // iterator itself
    // 2. otherwise, in the argument expression itself
    clang::dataflow::RecordStorageLocation* iterator =
        UnwrapAsIterator(expr.getArg(0), env);
    clang::dataflow::Value* container = nullptr;

    if (iterator) {
      container = GetContainerValue(env, *iterator);
    } else {
      auto* loc =
          clang::dyn_cast_or_null<clang::dataflow::RecordStorageLocation>(
              env.getStorageLocation(*expr.getArg(0)));

      if (loc) {
        container = GetContainerValue(env, *loc);
      }
    }

    if (!iterator && !container) {
      return;
    }

    TransferCallReturningIterator(
        &expr, *container,
        is_end ? env.getBoolLiteralValue(false) : env.makeAtomicBoolValue(),
        is_end ? env.getBoolLiteralValue(true) : env.makeAtomicBoolValue(),
        env);
  }

  void TransferCallReturningIterator(const clang::CallExpr* expr,
                                     clang::dataflow::Value& container,
                                     clang::dataflow::BoolValue& is_valid,
                                     clang::dataflow::BoolValue& is_end,
                                     clang::dataflow::Environment& env) {
    clang::dataflow::RecordStorageLocation* loc = nullptr;
    if (expr->isPRValue()) {
      loc = &env.getResultObjectLocation(*expr);
    } else {
      loc = env.get<clang::dataflow::RecordStorageLocation>(*expr);
      if (loc == nullptr) {
        loc = &clang::cast<clang::dataflow::RecordStorageLocation>(
            env.createStorageLocation(*expr));
        env.setStorageLocation(*expr, *loc);
      }
    }
    assert(loc);
    PopulateIteratorValue(loc, container, is_valid, is_end, env);
  }

  // CXXMemberCallExpr:
  // https://clang.llvm.org/doxygen/classclang_1_1CXXMemberCallExpr.html
  void Transfer(const clang::CXXMemberCallExpr& callexpr,
                clang::dataflow::Environment& env) {
    auto* callee = callexpr.getDirectCallee();
    if (!callee) {
      return;
    }

    const std::string callee_type = clang::cast<clang::CXXMethodDecl>(callee)
                                        ->getParent()
                                        ->getQualifiedNameAsString();
    auto container_annotations =
        g_member_function_annotations.find(callee_type);
    if (container_annotations == g_member_function_annotations.end()) {
      return;
    }

    const std::string callee_name = callee->getNameAsString();
    auto method_annotation = container_annotations->second.find(callee_name);
    if (method_annotation == container_annotations->second.end()) {
      return;
    }

    const uint8_t annotation = method_annotation->second;
    assert(!(annotation & Annotation::kReturnIterator) ||
           !(annotation & Annotation::kReturnIteratorPair));

    clang::dataflow::Value* container = nullptr;

    if (!callexpr.getImplicitObjectArgument()->getType()->isRecordType()) {
      container = env.getValue(*callexpr.getImplicitObjectArgument());
    } else {
      clang::dataflow::RecordStorageLocation* loc =
          env.get<clang::dataflow::RecordStorageLocation>(
              *callexpr.getImplicitObjectArgument());
      container = GetContainerValue(env, *loc);
    }

    if (!container) {
      return;
    }

    if (annotation & Annotation::kInvalidateArgs) {
      bool found_iterator = false;

      // TODO(crbug.com/40272746): Invalid every arguments.
      for (unsigned i = 0; i < callexpr.getNumArgs(); i++) {
        if (auto* iterator = UnwrapAsIterator(callexpr.getArg(i), env)) {
          InfoStream() << "INVALIDATING ONE: " << DebugString(env, *iterator)
                       << '\n';
          InvalidateIterator(env, *iterator);
          found_iterator = true;
        }
      }

      if (!found_iterator) {
        // If we cannot get the iterator from the argument, then let's
        // invalidate everything instead:
        InfoStream() << "INVALIDATING MANY: Container: " << container << '\n';
        InvalidateContainer(env, *container);
      }
    }

    if (annotation & Annotation::kInvalidateAll) {
      InfoStream() << "INVALIDATING MANY: Container: " << container << '\n';
      InvalidateContainer(env, *container);
    }

    if (annotation & Annotation::kReturnIterator ||
        annotation & Annotation::kReturnEndIterator) {
      TransferCallReturningIterator(&callexpr, *container,
                                    annotation & Annotation::kReturnEndIterator
                                        ? env.getBoolLiteralValue(false)
                                        : env.makeAtomicBoolValue(),
                                    annotation & Annotation::kReturnEndIterator
                                        ? env.getBoolLiteralValue(true)
                                        : env.makeAtomicBoolValue(),
                                    env);
    }

    if (annotation & Annotation::kReturnIteratorPair) {
      //  TODO(crbug.com/40272746): Iterator pair are not yet supported.
    }
  }

  // CXXOperatorCallExpr:
  // https://clang.llvm.org/doxygen/classclang_1_1CXXOperatorCallExpr.html
  void Transfer(const clang::CXXOperatorCallExpr& expr,
                clang::dataflow::Environment& env) {
    // Those are operations of the form:
    //   - `*it`
    //   - `it->`
    if (expr.getOperator() == clang::OverloadedOperatorKind::OO_Star ||
        expr.getOperator() == clang::OverloadedOperatorKind::OO_Arrow) {
      assert(expr.getNumArgs() >= 1);
      TransferExpressionAccessForDeref(expr.getArg(0), env);
      return;
    }

    // Those are operations of the form:
    //   - `it += [integer]`
    //   - `it -= [integer]`
    if (expr.getOperator() == clang::OverloadedOperatorKind::OO_PlusEqual ||
        expr.getOperator() == clang::OverloadedOperatorKind::OO_MinusEqual) {
      assert(expr.getNumArgs() == 2);

      // Once all the features are developed, this should really be a
      // TransferExpressionAccessForDeref here, but the current error rate
      // would be too high as for now.
      TransferExpressionAccessForCheck(expr.getArg(0), env);

      // The result of this operation is another iterator.
      if (auto* iterator = UnwrapAsIterator(expr.getArg(0), env)) {
        CloneIterator(&expr, *iterator, env);
      }
      return;
    }

    // Those are operations of the form:
    //   - `it + [integer]`
    //   - `it - [integer]`
    //   - `[integer] + it`
    //   - `[integer] - it`
    if (expr.getOperator() == clang::OverloadedOperatorKind::OO_Plus ||
        expr.getOperator() == clang::OverloadedOperatorKind::OO_Minus) {
      // This can happen for classes representing numerical values for example.
      // e.g. const Decimal d = 3; -d;
      if (expr.getNumArgs() < 2) {
        return;
      }

      // Once all the features are developed, this should really be a
      // TransferExpressionAccessForDeref here, but the current error rate
      // would be too high as for now.
      TransferExpressionAccessForCheck(expr.getArg(0), env);
      TransferExpressionAccessForCheck(expr.getArg(1), env);

      // Adding/Substracing one iterator with an integer results in a new
      // iterator expression of the same type.
      auto deduce_return_value = [&](const clang::Expr* a,
                                     const clang::Expr* b) {
        clang::dataflow::RecordStorageLocation* iterator =
            UnwrapAsIterator(a, env);
        if (!iterator || !b->getType()->isIntegerType()) {
          return;
        }

        CloneIterator(&expr, *iterator, env);
      };

      deduce_return_value(expr.getArg(0), expr.getArg(1));
      deduce_return_value(expr.getArg(1), expr.getArg(0));
      return;
    }

    // Those are operations of the form:
    //   - `it = [expr]`
    if (expr.getOperator() == clang::OverloadedOperatorKind::OO_Equal) {
      // Just record the potentially new iterator.
      auto* lhs = UnwrapAsIterator(&expr, env);
      auto* rhs = UnwrapAsIterator(expr.getArg(1), env);

      if (lhs) {
        assert(rhs);
        SetContainerValue(env, *lhs, *GetContainerValue(env, *rhs));
      }
      return;
    }

    // Those are operations of the form:
    //   - `it != [expr]`
    //   - `it == [expr]`
    if (expr.getOperator() == clang::OverloadedOperatorKind::OO_EqualEqual ||
        expr.getOperator() == clang::OverloadedOperatorKind::OO_ExclaimEqual) {
      assert(expr.getNumArgs() >= 2);

      TransferExpressionAccessForCheck(expr.getArg(0), env);
      TransferExpressionAccessForCheck(expr.getArg(1), env);
      clang::dataflow::RecordStorageLocation* lhs_it =
          UnwrapAsIterator(expr.getArg(0), env);
      clang::dataflow::RecordStorageLocation* rhs_it =
          UnwrapAsIterator(expr.getArg(1), env);
      if (!lhs_it || !rhs_it) {
        return;
      }
      DebugStream() << DebugString(env, *lhs_it) << '\n';
      DebugStream() << DebugString(env, *rhs_it) << '\n';
      if (GetContainerValue(env, *lhs_it) != GetContainerValue(env, *rhs_it)) {
        Report(kInvalidIteratorComparison, expr);
      }
      const auto& formula = ForceBoolValue(env, expr);
      auto& arena = env.arena();
      if (expr.getOperator() == clang::OverloadedOperatorKind::OO_EqualEqual) {
        TransferIteratorsEquality(env, formula, lhs_it, rhs_it);
        TransferIteratorsInequality(env, arena.makeNot(formula), lhs_it,
                                    rhs_it);
      } else {
        TransferIteratorsInequality(env, formula, lhs_it, rhs_it);
        TransferIteratorsEquality(env, arena.makeNot(formula), lhs_it, rhs_it);
      }
      return;
    }

    // Those are operations of the form:
    //   - `it--`
    //   - `it++`
    if (expr.getOperator() == clang::OverloadedOperatorKind::OO_PlusPlus ||
        expr.getOperator() == clang::OverloadedOperatorKind::OO_MinusMinus) {
      assert(expr.getNumArgs());
      TransferExpressionAccessForDeref(expr.getArg(0), env);

      // The result of this operation is another iterator.
      if (auto* iterator = UnwrapAsIterator(expr.getArg(0), env)) {
        CloneIterator(&expr, *iterator, env);
      }

      return;
    }
    // TODO(crbug.com/40272746) Handle other kinds of operators.
  }

  // CastExpr: https://clang.llvm.org/doxygen/classclang_1_1CastExpr.html
  void Transfer(const clang::CastExpr& value_stmt,
                clang::dataflow::Environment& env) {
    if (auto* expr = clang::dyn_cast<clang::ImplicitCastExpr>(&value_stmt)) {
      Transfer(*expr, env);
    }
  }

  // ImplicitCastExpr:
  // https://clang.llvm.org/doxygen/classclang_1_1ImplicitCastExpr.html
  void Transfer(const clang::ImplicitCastExpr& expr,
                clang::dataflow::Environment& env) {
    if (expr.getCastKind() == clang::CastKind::CK_LValueToRValue) {
      TransferExpressionAccessForDeref(expr.getSubExpr(), env);
    }
  }

  void TransferIteratorsEquality(clang::dataflow::Environment& env,
                                 const clang::dataflow::Formula& formula,
                                 clang::dataflow::RecordStorageLocation* lhs,
                                 clang::dataflow::RecordStorageLocation* rhs) {
    auto& arena = env.arena();
    // If we know that lhs and rhs are equal, we can imply that:
    // 1. lhs->is_valid == rhs->is_valid
    // 2. lhs->is_end == rhs->is_end
    // Indeed, in the following scenario:
    //   if (it == std::end(vec)) {}
    // entering the `if` block means that it is the end iterator as well.
    env.assume(arena.makeImplies(
        formula, arena.makeEquals(GetIsValid(env, *lhs)->formula(),
                                  GetIsValid(env, *rhs)->formula())));
    env.assume(arena.makeImplies(
        formula, arena.makeEquals(GetIsEnd(env, *lhs)->formula(),
                                  GetIsEnd(env, *rhs)->formula())));
  }

  void TransferIteratorsInequality(
      clang::dataflow::Environment& env,
      const clang::dataflow::Formula& formula,
      clang::dataflow::RecordStorageLocation* lhs,
      clang::dataflow::RecordStorageLocation* rhs) {
    auto& arena = env.arena();
    // This is a bit trickier, because inequality doesn't really give us
    // generic information on the validities of the iterators, except:
    // 1. lhs->is_end => rhs->is_valid
    // 2. rhs->is_end => lhs->is_valid
    env.assume(arena.makeImplies(
        arena.makeAnd(formula, GetIsEnd(env, *lhs)->formula()),
        GetIsValid(env, *rhs)->formula()));
    env.assume(arena.makeImplies(
        arena.makeAnd(formula, GetIsEnd(env, *rhs)->formula()),
        GetIsValid(env, *lhs)->formula()));
  }

  // This validates that the iterator at `expr` is allowed to be "checked"
  // against. If not, we issue an error.
  void TransferExpressionAccessForCheck(const clang::Expr* expr,
                                        clang::dataflow::Environment& env) {
    clang::dataflow::RecordStorageLocation* iterator =
        UnwrapAsIterator(expr, env);
    if (!iterator) {
      return;
    }

    // If the iterator was never invalidated in any of the parent environments,
    // then we allow it to be checked against another iterator, since it means
    // the iterator is still potentially valid.
    if (env.allows(GetIsValid(env, *iterator)->formula())) {
      return;
    }

    // We always allow the end iterator to be checked, otherwise we wouldn't be
    // able to make iterators valid.
    if (env.proves(GetIsEnd(env, *iterator)->formula())) {
      return;
    }

    TransferExpressionAccessForDeref(expr, env);
  }

  // This validates that the iterator at `expr` is allowed to be dereferenced.
  // In other words, the iterator **must** be valid or we issue an error.
  void TransferExpressionAccessForDeref(const clang::Expr* expr,
                                        clang::dataflow::Environment& env) {
    clang::dataflow::RecordStorageLocation* iterator =
        UnwrapAsIterator(expr, env);
    if (!iterator) {
      return;
    }

    bool is_valid = env.proves(GetIsValid(env, *iterator)->formula());

    DebugStream() << "[ACCESS] " << DebugString(env, *iterator) << '\n';

    if (is_valid) {
      return;
    }

    Report(kInvalidIteratorUsage, *expr);
  }

  // This invalidates all the iterators previously created by this container in
  // the current environment.
  void InvalidateContainer(clang::dataflow::Environment& env,
                           clang::dataflow::Value& container) {
    for (auto& p : iterator_to_container_) {
      if (p.second != &container) {
        continue;
      }
      auto* value = GetContainerValue(env, *p.first);
      if (!value) {
        continue;
      }
      DebugStream() << DebugString(env, *p.first) << '\n';

      SetIsValid(env, *p.first, env.getBoolLiteralValue(false));
    }
  }

  // This invalidates the iterator `iterator` in the current environment.
  void InvalidateIterator(clang::dataflow::Environment& env,
                          clang::dataflow::RecordStorageLocation& iterator) {
    SetIsValid(env, iterator, env.getBoolLiteralValue(false));
  }

  void PopulateIteratorValue(clang::dataflow::RecordStorageLocation* iterator,
                             clang::dataflow::Value& container,
                             clang::dataflow::BoolValue& is_valid,
                             clang::dataflow::BoolValue& is_end,
                             clang::dataflow::Environment& env) {
    iterator_types_mapping_.insert(iterator->getType().getCanonicalType());

    SetContainerValue(env, *iterator, container);
    SetIsValid(env, *iterator, is_valid);
    SetIsEnd(env, *iterator, is_end);
  }

  void CloneIterator(const clang::CallExpr* expr,
                     clang::dataflow::RecordStorageLocation& iterator,
                     clang::dataflow::Environment& env) {
    auto* container = GetContainerValue(env, iterator);
    TransferCallReturningIterator(expr, *container, env.makeAtomicBoolValue(),
                                  env.makeAtomicBoolValue(), env);
  }

  const clang::Expr* Unwrap(const clang::Expr* E) {
    if (auto* implicitcast = clang::dyn_cast<clang::ImplicitCastExpr>(E)) {
      return implicitcast->getSubExpr();  // Is this an iterator implicit cast?
    }

    if (auto* construct = clang::dyn_cast<clang::CXXConstructExpr>(E)) {
      // If the iterator is default constructed, we do not track it since we
      // can't link it to a container or anything. However, if it gets copy
      // assigned from an actually tracked iterator, we'll be able to track it
      // back.
      if (construct->getNumArgs()) {
        // Is this an iterator constructor being invoked?
        return construct->getArg(0);
      }
    }

    return nullptr;
  }

  // This method walks the given expression and tries to find an iterator tied
  // to it.
  clang::dataflow::RecordStorageLocation* UnwrapAsIterator(
      const clang::Expr* expr,
      const clang::dataflow::Environment& env) {
    while (expr) {
      clang::dataflow::RecordStorageLocation* loc = nullptr;

      if (expr->isGLValue()) {
        loc = clang::dyn_cast_or_null<clang::dataflow::RecordStorageLocation>(
            env.getStorageLocation(*expr));
      } else if (expr->isPRValue() && expr->getType()->isRecordType()) {
        loc = &env.getResultObjectLocation(*expr);
      }

      if (loc) {
        if (IsIterator(loc->getType().getCanonicalType())) {
          return loc;
        }
      }

      expr = Unwrap(expr);
    }
    return nullptr;
  }

  // Gets the container value for the given iterator location.
  clang::dataflow::Value* GetContainerValue(
      const clang::dataflow::Environment& env,
      const clang::dataflow::RecordStorageLocation& loc) {
    return GetSyntheticFieldWithName("container", env, loc);
  }

  void SetContainerValue(clang::dataflow::Environment& env,
                         clang::dataflow::RecordStorageLocation& loc,
                         clang::dataflow::Value& res) {
    iterator_to_container_[&loc] = &res;
    SetSyntheticFieldWithName("container", env, loc, res);
  }

  // Returns whether the currently handled value is an iterator.
  bool IsIterator(clang::QualType type) {
    return iterator_types_mapping_.count(type.getCanonicalType()) != 0;
  }

  // Dumps some debugging information about the iterator. Caller is responsible
  // of ensuring `iterator` is actually an iterator.
  std::string DebugString(
      const clang::dataflow::Environment& env,
      const clang::dataflow::RecordStorageLocation& iterator) {
    auto* container = GetContainerValue(env, iterator);
    std::string res;
    const auto& formula = GetIsValid(env, iterator)->formula();
    const bool is_valid = env.proves(formula);
    const bool is_invalid = env.proves(env.arena().makeNot(formula));
    llvm::StringRef status = is_valid     ? "VALID"
                             : is_invalid ? "INVALID"
                                          : "MAYBE_INVALID";

    llvm::raw_string_ostream(res) << &iterator << " (container: " << container
                                  << " status: " << status << ")";
    return res;
  }

  template <size_t N>
  void Report(const char (&error_message)[N], const clang::Expr& expr) {
    clang::SourceLocation location = expr.getSourceRange().getBegin();

    // Avoid the same error to be reported twice:
    if (reported_source_locations_.count({location, error_message})) {
      return;
    }
    reported_source_locations_.insert({location, error_message});

    diagnostic_.Report(
        location, diagnostic_.getCustomDiagID(
                      clang::DiagnosticsEngine::Level::Error, error_message));
  }

  // The diagnostic engine that will issue potential errors.
  clang::DiagnosticsEngine& diagnostic_;

  // The iterator types found along the way.
  // This part is kind of tricky for now, because we'd like to hard code these.
  // Unfortunately, since we aim at handling multiple iterator types, we can't
  // really do it statically, so we need to store the types while we encounter
  // them.
  llvm::DenseSet<clang::QualType> iterator_types_mapping_;

  // Iterator to container map. This allows us to invalidate all iterators in
  // case this is needed.
  llvm::DenseMap<clang::dataflow::RecordStorageLocation*,
                 clang::dataflow::Value*>
      iterator_to_container_;

  // The set of reported errors' location. This is used to avoid submitting
  // twice the same error during Clang DataFlowAnalysis iterations.
  llvm::DenseSet<std::pair<clang::SourceLocation, clang::StringRef>>
      reported_source_locations_;
};

class IteratorInvalidationCheck
    : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  // The checks will performed on every function implemented in the main file.
  void Register(clang::ast_matchers::MatchFinder& finder) {
    using namespace clang::ast_matchers;
    finder.addMatcher(
        functionDecl(isExpansionInMainFile(), isDefinition(), hasBody(stmt()))
            .bind("fun"),
        this);
  }

  // clang::ast_matchers::MatchFinder::MatchCallback implementation:
  void run(const clang::ast_matchers::MatchFinder::MatchResult& result) final {
    if (result.SourceManager->getDiagnostics().hasUncompilableErrorOccurred()) {
      return;
    }

    const auto* func = result.Nodes.getNodeAs<clang::FunctionDecl>("fun");
    assert(func);
    if (!Supported(*func)) {
      return;
    }

    InfoStream() << "[FUNCTION] " << func->getQualifiedNameAsString() << '\n';
    auto control_flow_context = clang::dataflow::AdornedCFG::build(
        *func, *func->getBody(), *result.Context);
    if (!control_flow_context) {
      llvm::report_fatal_error(control_flow_context.takeError());
      return;
    }

    auto solver = std::make_unique<clang::dataflow::WatchedLiteralsSolver>();
    clang::dataflow::DataflowAnalysisContext analysis_context(
        std::move(solver));
    clang::dataflow::Environment environment(analysis_context, *func);

    InvalidIteratorAnalysis analysis(func,
                                     result.SourceManager->getDiagnostics());

    analysis_context.setSyntheticFieldCallback(
        std::bind(&InvalidIteratorAnalysis::GetSyntheticFields, &analysis,
                  std::placeholders::_1));

    auto analysis_result =
        runDataflowAnalysis(*control_flow_context, analysis, environment);
    if (!analysis_result) {
      // just ignore that for now!
      handleAllErrors(analysis_result.takeError(),
                      [](const llvm::StringError& E) {});
    }
  }

  bool Supported(const clang::FunctionDecl& func) {
    if (func.isTemplated()) {
      return false;
    }

    if (auto* method = clang::dyn_cast<clang::CXXMethodDecl>(&func)) {
      return Supported(*method);
    }

    return true;
  }

  bool Supported(const clang::CXXMethodDecl& method) {
    const clang::CXXRecordDecl* record_declaration = method.getParent();
    if (record_declaration && record_declaration->isLambda()) {
      return false;
    }

    if (method.isStatic()) {
      return true;
    }

    if (method.getThisType()->isDependentType()) {
      return false;
    }

    if (method.getParent()->isTemplateDecl()) {
      return false;
    }

    if (method.getThisType()->isUnionType()) {
      return false;
    }

    // Ignore methods of unions and structs that contain an union.
    std::vector<clang::QualType> type_stack;
    type_stack.push_back(method.getThisType());
    while (!type_stack.empty()) {
      clang::QualType type = type_stack.back();
      type_stack.pop_back();

      if (type->isUnionType()) {
        return false;
      }

      if (clang::CXXRecordDecl* cpp_record = type->getAsCXXRecordDecl()) {
        for (auto f : cpp_record->fields()) {
          type_stack.push_back(f->getType());
        }
      }
    }

    return true;
  }
};

class IteratorInvalidationConsumer : public clang::ASTConsumer {
 public:
  IteratorInvalidationConsumer(clang::CompilerInstance& instance) {}

  void HandleTranslationUnit(clang::ASTContext& context) final {
    llvm::TimeTraceScope TimeScope(
        "IteratorInvalidationConsumer::HandleTranslationUnit");

    IteratorInvalidationCheck checker;
    clang::ast_matchers::MatchFinder match_finder;
    checker.Register(match_finder);
    match_finder.matchAST(context);
  }
};

class IteratorInvalidationPluginAction : public clang::PluginASTAction {
 public:
  IteratorInvalidationPluginAction() = default;

 private:
  // clang::PluginASTAction implementation:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& instance,
      llvm::StringRef ref) final {
    llvm::EnablePrettyStackTrace();
    return std::make_unique<IteratorInvalidationConsumer>(instance);
  }

  PluginASTAction::ActionType getActionType() final {
    return CmdlineBeforeMainAction;
  }

  bool ParseArgs(const clang::CompilerInstance&,
                 const std::vector<std::string>& args) final {
    return true;
  }
};

static clang::FrontendPluginRegistry::Add<IteratorInvalidationPluginAction> X(
    "iterator-checker",
    "Check c++ iterator misuse");

}  // namespace
