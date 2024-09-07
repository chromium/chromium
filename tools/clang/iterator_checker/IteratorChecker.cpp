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
#include "clang/Analysis/FlowSensitive/Models/ChromiumCheckModel.h"
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

const char kIteratorMismatch[] =
    "[iterator-checker] Potentially iterator mismatch.";

// To understand C++ code, we need a way to encode what is an iterator and what
// are the functions that might invalidate them.
enum AnnotationType : uint8_t {
  kNone = 0,

  // Annotate function declarations, return and argument types specifying to
  // which container value they belong.
  kContainer = 1 << 0,
  kEndContainer = 1 << 1,

  // Annotate function declarations and argument types specifying which
  // container or iterator values to invalidate.
  kInvalidate = 1 << 2,

  // Annotate function returning a pair of iterators.
  // TODO(crbug.com/40272746) Not yet implemented.
  kIteratorPair = 1 << 3,

  // Annotate functions and argument types specifying
  // which container or iterator values to swap.
  kSwap = 1 << 4,
};

// Represents a single annotation, defined by its:
//  - `type`: Specifies the kind of annotation.
//  - `identifier`: If applicable, a symbolic name that specifies which
//  container the annotation is referring to.
struct Annotation {
  Annotation(AnnotationType type, llvm::StringRef identifier)
      : type(type), identifier(identifier) {}

  Annotation(AnnotationType type) : type(type) {}

  AnnotationType type;
  std::string identifier;
};

// TODO(crbug.com/40272746): Use a set instead, because having duplicated
// annotations doesn't make sense.
using Annotations = std::vector<Annotation>;

// Represents the aggregation of all annotations assignable to a function.
struct GroupedFunctionAnnotation {
  Annotations function_annotations;
  Annotations return_annotations;
  std::vector<Annotations> args_annotations;

  GroupedFunctionAnnotation() = default;

  GroupedFunctionAnnotation& Function(const Annotation& annotation) {
    function_annotations.push_back(annotation);
    return *this;
  }

  GroupedFunctionAnnotation& Return(const Annotation& annotation) {
    return_annotations.push_back(annotation);
    return *this;
  }

  GroupedFunctionAnnotation& Arg(const Annotations& annotations) {
    args_annotations.push_back(annotations);
    return *this;
  }
};

// Find the first annotation in `annotations` of the specified `types`.
Annotations::const_iterator FindAnnotation(const Annotations& annotations,
                                           const uint8_t types) {
  return std::find_if(
      annotations.begin(), annotations.end(),
      [&types](Annotation annotation) { return annotation.type & types; });
}

// Find the first annotation in `annotations` of the specified `types` and
// `identifier`.
Annotations::const_iterator FindAnnotation(const Annotations& annotations,
                                           const uint8_t types,
                                           const llvm::StringRef& identifier) {
  return std::find_if(annotations.begin(), annotations.end(),
                      [&types, &identifier](Annotation annotation) {
                        return (annotation.type & types) &&
                               (annotation.identifier == identifier);
                      });
}

// Merge two different `GroupedFunctionAnnotation`.
GroupedFunctionAnnotation MergeGroupedFunctionAnnotations(
    GroupedFunctionAnnotation first,
    const GroupedFunctionAnnotation& second) {
  first.function_annotations.insert(first.function_annotations.end(),
                                    second.function_annotations.begin(),
                                    second.function_annotations.end());

  first.return_annotations.insert(first.return_annotations.end(),
                                  second.return_annotations.begin(),
                                  second.return_annotations.end());

  for (size_t i = 0; i < second.args_annotations.size(); i++) {
    if (i < first.args_annotations.size()) {
      first.args_annotations[i].insert(first.args_annotations[i].end(),
                                       second.args_annotations[i].begin(),
                                       second.args_annotations[i].end());
    } else {
      first.args_annotations.push_back(second.args_annotations[i]);
    }
  }

  return first;
}

// Mapping between identifiers of source-level annotations and the related
// annotation type (e.g. [[clang::annotate("container")]]).
static llvm::DenseMap<llvm::StringRef, AnnotationType> g_annotations = {
    {"container", AnnotationType::kContainer},
    {"end_container", AnnotationType::kEndContainer},
    {"invalidate", AnnotationType::kInvalidate},
    {"swap", AnnotationType::kSwap},
};

// Hardcoded types annotations.
static llvm::DenseMap<llvm::StringRef, Annotations> g_types_annotations = {
    {"__normal_iterator", {Annotation(AnnotationType::kContainer)}},
    {"__wrap_iter", {Annotation(AnnotationType::kContainer)}},
    {"reverse_iterator", {Annotation(AnnotationType::kContainer)}},
};

// Hardcoded function annotations.
static llvm::DenseMap<llvm::StringRef, GroupedFunctionAnnotation>
    g_functions_annotations = {
        {
            "std::begin",
            {},
        },
        {
            "std::cbegin",
            {},
        },
        {
            "std::end",
            GroupedFunctionAnnotation().Return(
                Annotation(AnnotationType::kEndContainer)),
        },
        {
            "std::rend",
            GroupedFunctionAnnotation().Return(
                Annotation(AnnotationType::kEndContainer)),
        },
        {
            "std::cend",
            GroupedFunctionAnnotation().Return(
                Annotation(AnnotationType::kEndContainer)),
        },
        {
            "std::next",
            {},
        },
        {
            "std::prev",
            {},
        },
        {
            "std::find",
            {},
        },
        {
            "std::search",
            {},
        },
        {
            "std::swap",
            GroupedFunctionAnnotation()
                .Arg({Annotation(AnnotationType::kSwap)})
                .Arg({Annotation(AnnotationType::kSwap)}),
        },
        // TODO(crbug.com/40272746) Add additional functions.
};

// Hardcoded member function annotations.
static llvm::DenseMap<
    llvm::StringRef,
    llvm::DenseMap<llvm::StringRef, GroupedFunctionAnnotation>>
    g_member_function_annotations = {
        {
            "std::vector",
            {
                {
                    "append_range",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "assign",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "assign_range",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "back",
                    {},
                },
                {
                    "begin",
                    {},
                },
                {
                    "capacity",
                    {},
                },
                {
                    "cbegin",
                    {},
                },
                {
                    "cend",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "clear",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "crbegin",
                    {},
                },
                {
                    "crend",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "data",
                    {},
                },
                {
                    "emplace",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "emplace_back",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "empty",
                    {},
                },
                {
                    "end",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {"erase",
                 GroupedFunctionAnnotation()
                     .Function(Annotation(AnnotationType::kInvalidate))
                     .Function(Annotation(AnnotationType::kContainer, "a"))
                     .Arg({Annotation(AnnotationType::kContainer, "a")})
                     .Return(Annotation(AnnotationType::kEndContainer))},
                {
                    "front",
                    {},
                },
                {
                    "insert",
                    GroupedFunctionAnnotation()
                        .Function(Annotation(AnnotationType::kInvalidate))
                        .Function(Annotation(AnnotationType::kContainer)),
                },
                {
                    "insert_range",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "max_size",
                    {},
                },
                {
                    "pop_back",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "push_back",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "rbegin",
                    {},
                },
                {
                    "rend",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "reserve",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "resize",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "shrink_to_fit",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "size",
                    {},
                },
                {
                    "swap",
                    GroupedFunctionAnnotation()
                        .Function(Annotation(AnnotationType::kSwap))
                        .Arg({Annotation(AnnotationType::kSwap)}),
                },
            },
        },
        {
            "std::unordered_set",
            {
                {
                    "begin",
                    {},
                },
                {
                    "cbegin",
                    {},
                },
                {
                    "end",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "cend",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "clear",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "insert",
                    GroupedFunctionAnnotation()
                        .Function(Annotation(AnnotationType::kInvalidate))
                        .Return(Annotation(AnnotationType::kIteratorPair)),
                },
                {
                    "emplace",
                    GroupedFunctionAnnotation()
                        .Function(Annotation(AnnotationType::kInvalidate))
                        .Return(Annotation(AnnotationType::kIteratorPair)),
                },
                {
                    "emplace_hint",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "erase",
                    GroupedFunctionAnnotation().Arg(
                        {Annotation(AnnotationType::kInvalidate)}),
                },
                {
                    "extract",
                    GroupedFunctionAnnotation().Arg(
                        {Annotation(AnnotationType::kInvalidate)}),
                },
                {
                    "find",
                    {},
                },
                // TODO(crbug.com/40272746) Add additional functions.
            },
        },
        {
            "WTF::Vector",
            {
                {
                    "begin",
                    {},
                },
                {
                    "rbegin",
                    {},
                },
                {
                    "end",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "rend",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "clear",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "shrink_to_fit",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "push_back",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "emplace_back",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "insert",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "InsertAt",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "InsertVector",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "push_front",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "PrependVector",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "EraseAt",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "erase",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                // `pop_back` invalidates only the iterator pointed to the last
                // element, but we have no way to track it.
                {
                    "pop_back",
                    {},
                },
                // TODO(crbug.com/40272746) Add additional functions.
            },
        },
        {
            "std::deque",
            {
                {
                    "begin",
                    {},
                },
                {
                    "cbegin",
                    {},
                },
                {
                    "rbegin",
                    {},
                },
                {
                    "end",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "cend",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "rend",
                    GroupedFunctionAnnotation().Return(
                        Annotation(AnnotationType::kEndContainer)),
                },
                {
                    "clear",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "shrink_to_fit",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "insert",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "emplace",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "erase",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "push_back",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "emplace_back",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "push_front",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
                {
                    "emplace_front",
                    GroupedFunctionAnnotation().Function(
                        Annotation(AnnotationType::kInvalidate)),
                },
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

void SwapSyntheticFieldWithName(
    llvm::StringRef name,
    clang::dataflow::Environment& env,
    const clang::dataflow::RecordStorageLocation& loc_a,
    const clang::dataflow::RecordStorageLocation& loc_b) {
  auto* prev_value = env.getValue(loc_a.getSyntheticField(name));

  env.setValue(loc_a.getSyntheticField(name),
               *env.getValue(loc_b.getSyntheticField(name)));
  env.setValue(loc_b.getSyntheticField(name), *prev_value);
}

void SetIsValid(clang::dataflow::Environment& env,
                const clang::dataflow::RecordStorageLocation& loc,
                clang::dataflow::BoolValue& res) {
  SetSyntheticFieldWithName("is_valid", env, loc, res);
}

void SwapIsValid(clang::dataflow::Environment& env,
                 const clang::dataflow::RecordStorageLocation& loc_a,
                 const clang::dataflow::RecordStorageLocation& loc_b) {
  SwapSyntheticFieldWithName("is_valid", env, loc_a, loc_b);
}

void SetIsEnd(clang::dataflow::Environment& env,
              const clang::dataflow::RecordStorageLocation& loc,
              clang::dataflow::BoolValue& res) {
  SetSyntheticFieldWithName("is_end", env, loc, res);
}

void SwapIsEnd(clang::dataflow::Environment& env,
               const clang::dataflow::RecordStorageLocation& loc_a,
               const clang::dataflow::RecordStorageLocation& loc_b) {
  SwapSyntheticFieldWithName("is_end", env, loc_a, loc_b);
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
    check_model_.transfer(elt, env);

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

    if (ctor->isCopyOrMoveConstructor() ||
        ctor->isConvertingConstructor(false)) {
      auto* it = UnwrapAsIterator(expr.getArg(0), env);
      assert(it);

      CloneIterator(&expr, *it, env);
    }
  }

  // CallExpr: https://clang.llvm.org/doxygen/classclang_1_1CallExpr.html
  void Transfer(const clang::CallExpr& callexpr,
                clang::dataflow::Environment& env) {
    // This handles both member and non-member call expressions.
    TransferCallExprCommon(callexpr, env);

    if (auto* expr = clang::dyn_cast<clang::CXXOperatorCallExpr>(&callexpr)) {
      Transfer(*expr, env);
      return;
    }
  }

  void TransferCallExprCommon(const clang::CallExpr& expr,
                              clang::dataflow::Environment& env) {
    std::optional<GroupedFunctionAnnotation> grouped_annotation =
        GetFunctionAnnotation(expr);

    if (!grouped_annotation) {
      return;
    }

    ProcessAnnotationInvalidate(expr, grouped_annotation.value(), env);
    ProcessAnnotationReturnIterator(expr, grouped_annotation.value(), env);
    ProcessAnnotationSwap(expr, grouped_annotation.value(), env);
    ProcessAnnotationRequireSameContainer(expr, grouped_annotation.value(),
                                          env);
  }

  void ProcessAnnotationInvalidate(
      const clang::CallExpr& expr,
      const GroupedFunctionAnnotation& grouped_annotation,
      clang::dataflow::Environment& env) {
    // In order to invalidate iterators and containers, we have to look for
    // invalidation annotations inside:
    // 1. Arguments types annotations.
    // 2. Function annotations.

    ProcessAnnotationInvalidateArgs(expr, grouped_annotation, env);
    ProcessAnnotationInvalidateContainer(expr, grouped_annotation, env);
  }

  void ProcessAnnotationInvalidateArgs(
      const clang::CallExpr& expr,
      const GroupedFunctionAnnotation& grouped_annotation,
      clang::dataflow::Environment& env) {
    // Looking inside arguments types annotations.
    for (size_t i = 0; i < grouped_annotation.args_annotations.size(); i++) {
      Annotations args_annotation = grouped_annotation.args_annotations[i];

      auto invalidate_arg_annotation =
          FindAnnotation(args_annotation, AnnotationType::kInvalidate);

      if (invalidate_arg_annotation == args_annotation.end()) {
        continue;
      }

      clang::dataflow::RecordStorageLocation* iterator =
          UnwrapAsIterator(expr.getArg(i), env);

      if (iterator) {
        // If we get an iterator from the argument, we just invalidate that
        // iterator.
        InfoStream() << "INVALIDATING ONE: " << DebugString(env, *iterator)
                     << '\n';
        InvalidateIterator(env, *iterator);
      } else {
        // If we cannot get the iterator from the argument, then let's
        // invalidate everything instead.
        clang::dataflow::Value* container =
            GetContainerFromArg(env, *expr.getArg(i));

        if (container) {
          InfoStream() << "INVALIDATING MANY: Container: " << container << '\n';
          InvalidateContainer(env, *container);
        }
      }
    }
  }

  void ProcessAnnotationInvalidateContainer(
      const clang::CallExpr& expr,
      const GroupedFunctionAnnotation& grouped_annotation,
      clang::dataflow::Environment& env) {
    // Looking inside function annotations.
    auto invalidate_function_annotation = FindAnnotation(
        grouped_annotation.function_annotations, AnnotationType::kInvalidate);

    if (invalidate_function_annotation ==
        grouped_annotation.function_annotations.end()) {
      return;
    }

    // Container to be invalidated.
    clang::dataflow::Value* container = GetContainerFromImplicitArg(env, expr);

    if (container) {
      InfoStream() << "INVALIDATING MANY: Container: " << container << '\n';
      InvalidateContainer(env, *container);
    }
  }

  void ProcessAnnotationReturnIterator(
      const clang::CallExpr& expr,
      const GroupedFunctionAnnotation& grouped_annotation,
      clang::dataflow::Environment& env) {
    // In order to return the iterator, we first have to look if there is a
    // container annotation inside the return type annotations.
    // If there is, we then have to look for container annotations inside:
    // 1. Function annotations.
    // 2. Arguments annotations.

    // Looking inside return type annotations.
    auto container_return_annotation = FindAnnotation(
        grouped_annotation.return_annotations,
        AnnotationType::kContainer | AnnotationType::kEndContainer);

    if (container_return_annotation ==
        grouped_annotation.return_annotations.end()) {
      return;
    }

    // Container of the iterator to be returned.
    clang::dataflow::Value* container = nullptr;

    // Looking inside arguments types annotations.
    for (size_t i = 0; i < grouped_annotation.args_annotations.size(); i++) {
      Annotations args_annotation = grouped_annotation.args_annotations[i];

      auto container_arg_annotation =
          FindAnnotation(args_annotation, AnnotationType::kContainer,
                         container_return_annotation->identifier);

      if (container_arg_annotation != args_annotation.end()) {
        // We stop looking for the args annotations as soon as we found one.
        container = GetContainerFromArg(env, *expr.getArg(i));
        break;
      }
    }

    // If we don't find the container of the iterator to be returned in the
    // arguments, we assume that :
    // - if it's a member call, it must belong to the implicit argument.
    // - otherwise, it must belong to the first argument
    if (!container) {
      if (clang::isa<clang::CXXMemberCallExpr>(expr)) {
        container = GetContainerFromImplicitArg(env, expr);
      } else {
        container = GetContainerFromArg(env, *expr.getArg(0));
      }
    }

    if (container) {
      bool is_end =
          container_return_annotation->type == AnnotationType::kEndContainer;

      TransferCallReturningIterator(
          &expr, *container,
          is_end ? env.getBoolLiteralValue(false) : env.makeAtomicBoolValue(),
          is_end ? env.getBoolLiteralValue(true) : env.makeAtomicBoolValue(),
          env);
    }
  }

  void ProcessAnnotationSwap(
      const clang::CallExpr& expr,
      const GroupedFunctionAnnotation& grouped_annotation,
      clang::dataflow::Environment& env) {
    llvm::DenseMap<llvm::StringRef,
                   std::vector<clang::dataflow::RecordStorageLocation*>>
        id_to_locations;

    // Looking inside function annotations.
    auto swap_function_annotation = FindAnnotation(
        grouped_annotation.function_annotations, AnnotationType::kSwap);

    if (swap_function_annotation !=
        grouped_annotation.function_annotations.end()) {
      assert(clang::isa<clang::CXXMemberCallExpr>(&expr));
      auto* member_call = clang::cast<clang::CXXMemberCallExpr>(&expr);

      id_to_locations[swap_function_annotation->identifier].push_back(
          clang::dyn_cast_or_null<clang::dataflow::RecordStorageLocation>(
              env.getStorageLocation(
                  *member_call->getImplicitObjectArgument())));
    }

    // Looking inside arguments types annotations.
    for (size_t i = 0; i < grouped_annotation.args_annotations.size(); i++) {
      Annotations args_annotation = grouped_annotation.args_annotations[i];

      auto swap_arg_annotation =
          FindAnnotation(args_annotation, AnnotationType::kSwap);

      if (swap_arg_annotation != args_annotation.end()) {
        id_to_locations[swap_arg_annotation->identifier].push_back(
            clang::dyn_cast_or_null<clang::dataflow::RecordStorageLocation>(
                env.getStorageLocation(*expr.getArg(i))));
      }
    }

    for (const auto& [id, locations] : id_to_locations) {
      assert(locations.size() == 2);

      if (!locations[0] || !locations[1]) {
        continue;
      }

      if (IsIterator(locations[0]->getType().getCanonicalType()) &&
          IsIterator(locations[1]->getType().getCanonicalType())) {
        SwapIterators(env, locations[0], locations[1]);
      } else {
        SwapContainers(env, GetContainerValue(env, *locations[0]),
                       GetContainerValue(env, *locations[1]));
      }
    }
  }

  void ProcessAnnotationRequireSameContainer(
      const clang::CallExpr& expr,
      const GroupedFunctionAnnotation& grouped_annotation,
      clang::dataflow::Environment& env) {
    // In order to compare container values and eventually report an error, we
    // need to save both `clang::Expr` and its related `clang::dataflow::Value`.
    llvm::DenseMap<
        llvm::StringRef,
        std::vector<std::pair<const clang::Expr*, clang::dataflow::Value*>>>
        id_to_containers;

    // Looking inside function annotations.
    auto container_annotation = FindAnnotation(
        grouped_annotation.function_annotations, AnnotationType::kContainer);

    if (container_annotation != grouped_annotation.function_annotations.end()) {
      id_to_containers[container_annotation->identifier].emplace_back(
          &expr, GetContainerFromImplicitArg(env, expr));
    }

    // Looking inside arguments types annotations.
    for (size_t i = 0; i < grouped_annotation.args_annotations.size(); i++) {
      Annotations args_annotation = grouped_annotation.args_annotations[i];

      auto container_arg_annotation =
          FindAnnotation(args_annotation, AnnotationType::kContainer);

      if (container_arg_annotation != args_annotation.end()) {
        id_to_containers[container_arg_annotation->identifier].emplace_back(
            expr.getArg(i), GetContainerFromArg(env, *expr.getArg(i)));
      }
    }

    for (const auto& [id, values] : id_to_containers) {
      // We want to perform this kind of check just for group of iterators that
      // have explicit identifiers.
      if (id == "") {
        continue;
      }

      const clang::dataflow::Value* baseline = values[0].second;

      for (size_t i = 1; i < values.size(); i++) {
        if (values[i].second != baseline) {
          Report(kIteratorMismatch, *values[i].first);
        }
      }
    }
  }

  clang::dataflow::Value* GetContainerFromImplicitArg(
      const clang::dataflow::Environment& env,
      const clang::CallExpr& expr) {
    const clang::CXXMemberCallExpr* member_call_expression =
        clang::cast<clang::CXXMemberCallExpr>(&expr);

    clang::Expr* callee = member_call_expression->getImplicitObjectArgument();

    if (callee->getType()->isRecordType()) {
      auto* callee_location =
          env.get<clang::dataflow::RecordStorageLocation>(*callee);

      return callee_location ? GetContainerValue(env, *callee_location)
                             : nullptr;
    }

    clang::dataflow::Value* container = env.getValue(*callee);

    // The `RecordStorageLocation` of a container can be accessed by its pointer
    // using the related `PointerValue`.
    if (auto* pointer_value =
            clang::dyn_cast_or_null<clang::dataflow::PointerValue>(container)) {
      if (auto* pointee_location =
              clang::dyn_cast<clang::dataflow::RecordStorageLocation>(
                  &pointer_value->getPointeeLoc())) {
        return GetContainerValue(env, *pointee_location);
      }
    }

    return container;
  }

  clang::dataflow::Value* GetContainerFromArg(
      const clang::dataflow::Environment& env,
      const clang::Expr& arg) {
    clang::dataflow::RecordStorageLocation* iterator =
        UnwrapAsIterator(&arg, env);
    clang::dataflow::Value* container = nullptr;

    if (iterator) {
      container = GetContainerValue(env, *iterator);
    } else {
      auto* loc =
          clang::dyn_cast_or_null<clang::dataflow::RecordStorageLocation>(
              env.getStorageLocation(arg));

      if (loc) {
        container = GetContainerValue(env, *loc);
      }
    }

    return container;
  }

  // Return annotations related to the specified call expression if present,
  // otherwise return `std::nullopt`.
  std::optional<GroupedFunctionAnnotation> GetFunctionAnnotation(
      const clang::CallExpr& expr) {
    auto* callee = expr.getDirectCallee();
    if (!callee) {
      return std::nullopt;
    }

    GroupedFunctionAnnotation annotated_grouped_annotation =
        GetAnnotatedFunctionAnnotation(*callee);

    GroupedFunctionAnnotation hardcoded_grouped_annotation =
        GetHardcodedFunctionAnnotation(
            *callee, clang::isa<clang::CXXMemberCallExpr>(expr));

    auto merged_grouped_annotation = MergeGroupedFunctionAnnotations(
        annotated_grouped_annotation, hardcoded_grouped_annotation);

    ApplyIdentifiersFromTemplate(merged_grouped_annotation, callee);

    return merged_grouped_annotation;
  }

  GroupedFunctionAnnotation GetHardcodedFunctionAnnotation(
      const clang::FunctionDecl& callee,
      const bool is_member_function) {
    GroupedFunctionAnnotation grouped_annotation;

    // Get hardcoded functions annotations.
    if (is_member_function) {
      const std::string callee_type = clang::cast<clang::CXXMethodDecl>(callee)
                                          .getParent()
                                          ->getQualifiedNameAsString();
      auto container_annotations =
          g_member_function_annotations.find(callee_type);
      if (container_annotations != g_member_function_annotations.end()) {
        const std::string callee_name = callee.getNameAsString();
        auto it = container_annotations->second.find(callee_name);
        if (it != container_annotations->second.end()) {
          grouped_annotation = it->second;
        }
      }
    } else {
      std::string callee_name = callee.getQualifiedNameAsString();
      auto it = g_functions_annotations.find(callee_name);
      if (it != g_functions_annotations.end()) {
        grouped_annotation = it->second;
      }
    }

    // Get hardcoded types annotations from the return type and arguments types.
    auto* decl = clang::dyn_cast_or_null<clang::TypeDecl>(
        callee.getReturnType()->getAsRecordDecl());

    if (decl) {
      auto it = g_types_annotations.find(decl->getNameAsString());

      if (it != g_types_annotations.end()) {
        grouped_annotation.return_annotations.insert(
            grouped_annotation.return_annotations.end(), it->second.begin(),
            it->second.end());
      }
    }

    for (size_t i = 0; i < callee.getNumParams(); i++) {
      auto* decl = clang::dyn_cast_or_null<clang::TypeDecl>(
          callee.getParamDecl(i)->getType()->getAsRecordDecl());

      if (!decl) {
        continue;
      }

      auto it = g_types_annotations.find(decl->getNameAsString());

      if (it != g_types_annotations.end()) {
        if (i < grouped_annotation.args_annotations.size()) {
          grouped_annotation.args_annotations[i].insert(
              grouped_annotation.args_annotations[i].end(), it->second.begin(),
              it->second.end());
        } else {
          grouped_annotation.args_annotations.push_back(it->second);
        }
      }
    }

    return grouped_annotation;
  }

  GroupedFunctionAnnotation GetAnnotatedFunctionAnnotation(
      const clang::FunctionDecl& callee) {
    // Get annotations from function declaration.
    Annotations function_annotations = ExtractAnnotationsFromDecl(callee);

    // Get types annotations from the function context.
    llvm::DenseMap<llvm::StringRef, Annotations> context_types_annotations;
    GetAnnotationsFromContext(context_types_annotations, callee.getParent());

    // Get annotations from return type, using also the function
    // context.
    Annotations return_annotations;
    if (auto function_type_loc = callee.getFunctionTypeLoc()) {
      return_annotations = ExtractAnnotationsFromTypeLoc(
          function_type_loc.getReturnLoc(), context_types_annotations);
    }

    // Get annotations from arguments types, using also the function
    // context.
    std::vector<Annotations> arguments_annotations;
    for (size_t i = 0; i < callee.getNumParams(); i++) {
      auto* param = callee.getParamDecl(i);

      Annotations arg_annotations;

      if (auto type_source = param->getTypeSourceInfo()) {
        arg_annotations = ExtractAnnotationsFromTypeLoc(
            type_source->getTypeLoc(), context_types_annotations);
      }

      arguments_annotations.emplace_back(arg_annotations);
    }

    return GroupedFunctionAnnotation{function_annotations, return_annotations,
                                     arguments_annotations};
  }

  void ApplyIdentifiersFromTemplate(
      GroupedFunctionAnnotation& grouped_annotation,
      const clang::FunctionDecl* callee) {
    clang::FunctionTemplateDecl* templ = callee->getPrimaryTemplate();

    if (!templ) {
      return;
    }

    const clang::FunctionDecl* callee_decl = templ->getTemplatedDecl();

    for (size_t i = 0; i < callee_decl->getNumParams(); i++) {
      auto* param = callee_decl->getParamDecl(i);

      // We are only interested to parameters that actually belong to the
      // template.
      if (!clang::isa<clang::TemplateTypeParmType>(param->getType())) {
        continue;
      }

      // We want to apply template identifiers just for annotations that already
      // exist.
      if (grouped_annotation.args_annotations.size() <= i) {
        break;
      }

      std::string identifier = param->getType().getAsString();

      for (auto& annotation : grouped_annotation.args_annotations[i]) {
        // The template identifier is applied only if the annotation is of type
        // `kContainer` and if it doesn't have an identifier yet.
        if (annotation.type != AnnotationType::kContainer ||
            annotation.identifier != "") {
          continue;
        }

        annotation.identifier = identifier;
      }
    }
  }

  // Retrieve types annotations from the context and save them in
  // `context_types_annotations`. In this way it is possible to annotate a type
  // when it is declared just once, avoiding to annotate the same multiple
  // times.
  void GetAnnotationsFromContext(
      llvm::DenseMap<llvm::StringRef, Annotations>& context_types_annotations,
      const clang::DeclContext* context) {
    for (auto decl : context->decls()) {
      if (auto* type_decl = clang::dyn_cast<clang::TypeDecl>(decl)) {
        Annotations annotations;

        for (auto* attr : decl->attrs()) {
          if (auto* annotate_attr =
                  clang::dyn_cast<clang::AnnotateAttr>(attr)) {
            auto it = g_annotations.find(annotate_attr->getAnnotation());

            if (it == g_annotations.end()) {
              continue;
            }

            llvm::StringRef identifier =
                GetIdentifierFromAnnotation(annotate_attr);
            annotations.emplace_back(it->second, identifier);
          }
        }

        if (!annotations.empty()) {
          context_types_annotations[type_decl->getName()] = annotations;
        }
      }
    }
  }

  llvm::StringRef GetIdentifierFromAnnotation(
      const clang::AnnotateAttr* annotate_attr) {
    llvm::StringRef identifier;

    if (annotate_attr->args_size() > 0) {
      const auto* string_literal =
          GetStringLiteral(annotate_attr->args_begin()[0]);

      // We assume that the first argument must be always a string literal.
      assert(string_literal);

      identifier = string_literal->getString();
    }

    return identifier;
  }

  llvm::StringRef GetIdentifierFromAnnotation(
      const clang::AnnotateTypeAttr* annotate_type_attr) {
    llvm::StringRef identifier;

    if (annotate_type_attr->args_size() > 0) {
      const auto* string_literal =
          GetStringLiteral(annotate_type_attr->args_begin()[0]);

      // We assume that the first argument must always be a string literal.
      assert(string_literal);

      identifier = string_literal->getString();
    }

    return identifier;
  }

  const clang::StringLiteral* GetStringLiteral(const clang::Expr* expr) {
    using clang::ast_matchers::constantExpr;
    using clang::ast_matchers::hasDescendant;
    using clang::ast_matchers::match;
    using clang::ast_matchers::selectFirst;
    using clang::ast_matchers::stringLiteral;

    return selectFirst<clang::StringLiteral>(
        "str", match(constantExpr(hasDescendant(stringLiteral().bind("str"))),
                     *expr, getASTContext()));
  }

  Annotations ExtractAnnotationsFromDecl(const clang::Decl& decl) {
    Annotations annotations;

    if (decl.hasAttrs()) {
      for (auto attr : decl.attrs()) {
        llvm::StringRef annotation;
        llvm::StringRef identifier;

        if (auto* annotate_attr = clang::dyn_cast<clang::AnnotateAttr>(attr)) {
          annotation = annotate_attr->getAnnotation();
          identifier = GetIdentifierFromAnnotation(annotate_attr);
        } else if (auto* annotate_type_attr =
                       clang::dyn_cast<clang::AnnotateTypeAttr>(attr)) {
          annotation = annotate_type_attr->getAnnotation();
          identifier = GetIdentifierFromAnnotation(annotate_type_attr);
        }

        auto it = g_annotations.find(annotation);

        if (it != g_annotations.end()) {
          annotations.emplace_back(it->second, identifier);
        }
      }
    }

    return annotations;
  }

  Annotations ExtractAnnotationsFromTypeLoc(
      clang::TypeLoc type_loc,
      const llvm::DenseMap<llvm::StringRef, Annotations>&
          context_types_annotations) {
    Annotations attrs;

    // First, get type annotations from the context using
    // `context_types_annotations`.
    std::string type_name = type_loc.getType()
                                .getNonReferenceType()
                                .getUnqualifiedType()
                                .getDesugaredType(getASTContext())
                                .getAsString();

    auto it = context_types_annotations.find(type_name);
    if (it != context_types_annotations.end()) {
      attrs.insert(attrs.end(), it->second.begin(), it->second.end());
    }

    // Then, get type annotations by searching them in the type attributes.
    // Because it is possible to specify multiple attributes for a type, we
    // have to traverse them one by one.
    while (true) {
      auto attributed_loc = type_loc.getAs<clang::AttributedTypeLoc>();

      if (attributed_loc.isNull()) {
        break;
      }

      auto* attr = attributed_loc.getAttrAs<clang::AnnotateTypeAttr>();

      if (attr) {
        auto annotation = attr->getAnnotation();

        auto it = g_annotations.find(annotation);

        if (it != g_annotations.end()) {
          llvm::StringRef identifier = GetIdentifierFromAnnotation(attr);
          attrs.emplace_back(it->second, identifier);
        }
      }

      type_loc = type_loc.getNextTypeLoc();
    }

    return attrs;
  }

  void TransferCallReturningIterator(const clang::Expr* expr,
                                     clang::dataflow::Value& container,
                                     clang::dataflow::BoolValue& is_valid,
                                     clang::dataflow::BoolValue& is_end,
                                     clang::dataflow::Environment& env) {
    clang::dataflow::RecordStorageLocation* loc = nullptr;
    if (expr->isPRValue() && expr->getType()->isRecordType()) {
      loc = &env.getResultObjectLocation(*expr);
    } else {
      loc = env.get<clang::dataflow::RecordStorageLocation>(*expr);
      if (loc == nullptr) {
        loc = &clang::cast<clang::dataflow::RecordStorageLocation>(
            env.createStorageLocation(*expr));
        env.setStorageLocation(*expr, *loc);
      }
    }

    // We need to traverse the AST backwards to catch if the returning iterator
    // belongs to a `VarDecl`. It is necessary because, in case of implicit
    // casts, we need to keep track of the declared target type.
    const clang::VarDecl* var_decl = nullptr;
    auto parents = getASTContext().getParents(*expr);
    while (!parents.empty() && !var_decl) {
      if (auto* decl = parents[0].get<clang::VarDecl>()) {
        var_decl = decl;
      }

      parents = getASTContext().getParents(parents[0]);
    }

    if (var_decl) {
      iterator_types_mapping_.insert(var_decl->getType().getCanonicalType());
    }

    assert(loc);
    PopulateIteratorValue(loc, container, is_valid, is_end, env);
  }

  void SwapContainers(clang::dataflow::Environment& env,
                      clang::dataflow::Value* container_a,
                      clang::dataflow::Value* container_b) {
    // In order to update container values, we need to find the right
    // `RecordStorageLocation`s by iterating over the `iterator_to_container_`
    // map.
    // Updating container values, which is performed by `SetContainerValue`,
    // changes the values in the map.
    // To avoid changing the values in the same map we are iterating over, a
    // copy of it is used instead.
    llvm::DenseMap<clang::dataflow::RecordStorageLocation*,
                   clang::dataflow::Value*>
        map = iterator_to_container_;

    for (auto& [iterator_location, container] : map) {
      if (container == container_a) {
        SetContainerValue(env, *iterator_location, *container_b);
      }
      if (container == container_b) {
        SetContainerValue(env, *iterator_location, *container_a);
      }
    }
  }

  void SwapIterators(clang::dataflow::Environment& env,
                     clang::dataflow::RecordStorageLocation* iterator_a,
                     clang::dataflow::RecordStorageLocation* iterator_b) {
    SwapContainerValue(env, *iterator_a, *iterator_b);
    SwapIsEnd(env, *iterator_a, *iterator_b);
    SwapIsValid(env, *iterator_a, *iterator_b);
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

      // The result of this operation "resets" the current iterator state and
      // returns another one.
      if (auto* iterator = UnwrapAsIterator(expr.getArg(0), env)) {
        SetIsValid(env, *iterator, env.makeAtomicBoolValue());
        SetIsEnd(env, *iterator, env.makeAtomicBoolValue());

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

      // The result of this operation "resets" the current iterator state and
      // returns another one.
      if (auto* iterator = UnwrapAsIterator(expr.getArg(0), env)) {
        SetIsValid(env, *iterator, env.makeAtomicBoolValue());
        SetIsEnd(env, *iterator, env.makeAtomicBoolValue());

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

  void CloneIterator(const clang::Expr* expr,
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

  void SwapContainerValue(clang::dataflow::Environment& env,
                          clang::dataflow::RecordStorageLocation& loc_a,
                          clang::dataflow::RecordStorageLocation& loc_b) {
    iterator_to_container_[&loc_a] = GetContainerValue(env, loc_b);
    iterator_to_container_[&loc_b] = GetContainerValue(env, loc_a);
    SwapSyntheticFieldWithName("container", env, loc_a, loc_b);
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

  // The check model that will handle Chromium's `CHECK` macros.
  clang::dataflow::ChromiumCheckModel check_model_;

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
