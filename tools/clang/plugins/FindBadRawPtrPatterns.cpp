// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "FindBadRawPtrPatterns.h"
#include <memory>

#include "RawPtrHelpers.h"
#include "RawPtrManualPathsToIgnore.h"
#include "StackAllocatedChecker.h"
#include "Util.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Attr.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/TypeLoc.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace chrome_checker {

constexpr char kBadCastDiagnostic[] =
    "[chromium-style] casting '%0' to '%1 is not allowed.";
constexpr char kBadCastDiagnosticNoteExplanation[] =
    "[chromium-style] '%0' manages BackupRefPtr refcounts; bypassing its C++ "
    "interface or treating it as a POD will lead to memory safety errors.";
constexpr char kBadCastDiagnosticNoteType[] =
    "[chromium-style] '%0' manages BackupRefPtr or its container here.";

class BadCastMatcher : public MatchFinder::MatchCallback {
 public:
  explicit BadCastMatcher(clang::CompilerInstance& compiler)
      : compiler_(compiler) {
    error_bad_cast_signature_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, kBadCastDiagnostic);
    note_bad_cast_signature_explanation_ =
        compiler_.getDiagnostics().getCustomDiagID(
            clang::DiagnosticsEngine::Note, kBadCastDiagnosticNoteExplanation);
    note_bad_cast_signature_type_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Note, kBadCastDiagnosticNoteType);
  }

  void Register(MatchFinder& match_finder) {
    // Matches anything contains |raw_ptr<T>| / |raw_ref<T>|.
    auto src_type =
        type(isCastingUnsafe(casting_unsafe_predicate_)).bind("srcType");
    auto dst_type =
        type(isCastingUnsafe(casting_unsafe_predicate_)).bind("dstType");
    // Matches |static_cast| on pointers, all |bit_cast|
    // and all |reinterpret_cast|.
    auto cast_kind = castExpr(anyOf(
        hasCastKind(CK_BitCast), hasCastKind(CK_LValueBitCast),
        hasCastKind(CK_LValueToRValueBitCast),
        hasCastKind(CK_PointerToIntegral), hasCastKind(CK_IntegralToPointer)));
    // Implicit/explicit casting from/to |raw_ptr<T>| matches.
    // Both casting direction is unsafe.
    //   https://godbolt.org/z/zqKMzcKfo
    auto cast_matcher =
        castExpr(
            allOf(anyOf(hasSourceExpression(hasType(src_type)),
                        implicitCastExpr(hasImplicitDestinationType(dst_type)),
                        explicitCastExpr(hasDestinationType(dst_type))),
                  cast_kind))
            .bind("castExpr");
    match_finder.addMatcher(cast_matcher, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const clang::CastExpr* cast_expr =
        result.Nodes.getNodeAs<clang::CastExpr>("castExpr");
    assert(cast_expr && "matcher should bind 'castExpr'");

    const clang::SourceManager& source_manager = *result.SourceManager;
    clang::SourceLocation loc = cast_expr->getSourceRange().getBegin();
    std::string file_path = GetFilename(source_manager, loc);

    // Using raw_ptr<T> in a stdlib collection will cause a cast.
    // e.g.
    // https://source.chromium.org/chromium/chromium/src/+/main:components/feed/core/v2/xsurface_datastore.h;drc=a0ff03edcace35ec020edd235f4d9e9735fc9690;l=107
    // |__bit/bit_cast.h| header is excluded to perform checking on
    // |std::bit_cast<T>|.
    if (file_path.find("buildtools/third_party/libc++") != std::string::npos &&
        file_path.find("__bit/bit_cast.h") == std::string::npos) {
      return;
    }

    // Exclude casts via "unsafe_raw_ptr_*_cast".
    if (file_path.find(
            "base/allocator/partition_allocator/pointers/raw_ptr_cast.h") !=
        std::string::npos) {
      return;
    }

    clang::PrintingPolicy printing_policy(result.Context->getLangOpts());
    const std::string src_name =
        cast_expr->getSubExpr()->getType().getAsString(printing_policy);
    const std::string dst_name =
        cast_expr->getType().getAsString(printing_policy);

    const auto* src_type = result.Nodes.getNodeAs<clang::Type>("srcType");
    const auto* dst_type = result.Nodes.getNodeAs<clang::Type>("dstType");
    assert((src_type || dst_type) &&
           "matcher should bind 'srcType' or 'dstType'");
    compiler_.getDiagnostics().Report(cast_expr->getEndLoc(),
                                      error_bad_cast_signature_)
        << src_name << dst_name;

    std::shared_ptr<CastingSafety> type_note;
    if (src_type != nullptr) {
      compiler_.getDiagnostics().Report(cast_expr->getEndLoc(),
                                        note_bad_cast_signature_explanation_)
          << src_name;
      type_note = casting_unsafe_predicate_.GetCastingSafety(src_type);
    } else {
      compiler_.getDiagnostics().Report(cast_expr->getEndLoc(),
                                        note_bad_cast_signature_explanation_)
          << dst_name;
      type_note = casting_unsafe_predicate_.GetCastingSafety(dst_type);
    }

    while (type_note) {
      if (type_note->source_loc()) {
        const auto& type_name = clang::QualType::getAsString(
            type_note->type(), {}, printing_policy);
        compiler_.getDiagnostics().Report(*type_note->source_loc(),
                                          note_bad_cast_signature_type_)
            << type_name;
      }
      type_note = type_note->source();
    }
  }

 private:
  clang::CompilerInstance& compiler_;
  CastingUnsafePredicate casting_unsafe_predicate_;
  unsigned error_bad_cast_signature_;
  unsigned note_bad_cast_signature_explanation_;
  unsigned note_bad_cast_signature_type_;
};

const char kNeedRawPtrSignature[] =
    "[chromium-rawptr] Use raw_ptr<T> instead of a raw pointer.";

class RawPtrFieldMatcher : public MatchFinder::MatchCallback {
 public:
  explicit RawPtrFieldMatcher(
      clang::CompilerInstance& compiler,
      const RawPtrAndRefExclusionsOptions& exclusion_options)
      : compiler_(compiler), exclusion_options_(exclusion_options) {
    error_need_raw_ptr_signature_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, kNeedRawPtrSignature);
  }

  void Register(MatchFinder& match_finder) {
    auto field_decl_matcher = AffectedRawPtrFieldDecl(exclusion_options_);
    match_finder.addMatcher(field_decl_matcher, this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const clang::FieldDecl* field_decl =
        result.Nodes.getNodeAs<clang::FieldDecl>("affectedFieldDecl");
    assert(field_decl && "matcher should bind 'fieldDecl'");

    const clang::TypeSourceInfo* type_source_info =
        field_decl->getTypeSourceInfo();
    assert(type_source_info && "assuming |type_source_info| is always present");

    assert(type_source_info->getType()->isPointerType() &&
           "matcher should only match pointer types");

    compiler_.getDiagnostics().Report(field_decl->getEndLoc(),
                                      error_need_raw_ptr_signature_);
  }

 private:
  clang::CompilerInstance& compiler_;
  unsigned error_need_raw_ptr_signature_;
  const RawPtrAndRefExclusionsOptions& exclusion_options_;
};

const char kNeedRawRefSignature[] =
    "[chromium-rawref] Use raw_ref<T> instead of a native reference.";

class RawRefFieldMatcher : public MatchFinder::MatchCallback {
 public:
  explicit RawRefFieldMatcher(
      clang::CompilerInstance& compiler,
      const RawPtrAndRefExclusionsOptions& exclusion_options)
      : compiler_(compiler), exclusion_options_(exclusion_options) {
    error_need_raw_ref_signature_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, kNeedRawRefSignature);
  }

  void Register(MatchFinder& match_finder) {
    auto field_decl_matcher = AffectedRawRefFieldDecl(exclusion_options_);
    match_finder.addMatcher(field_decl_matcher, this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const clang::FieldDecl* field_decl =
        result.Nodes.getNodeAs<clang::FieldDecl>("affectedFieldDecl");
    assert(field_decl && "matcher should bind 'fieldDecl'");

    const clang::TypeSourceInfo* type_source_info =
        field_decl->getTypeSourceInfo();
    assert(type_source_info && "assuming |type_source_info| is always present");

    assert(type_source_info->getType()->isReferenceType() &&
           "matcher should only match reference types");

    compiler_.getDiagnostics().Report(field_decl->getEndLoc(),
                                      error_need_raw_ref_signature_);
  }

 private:
  clang::CompilerInstance& compiler_;
  unsigned error_need_raw_ref_signature_;
  const RawPtrAndRefExclusionsOptions exclusion_options_;
};

const char kNoRawPtrToStackAllocatedSignature[] =
    "[chromium-raw-ptr-to-stack-allocated] Do not use '%0<T>' on a "
    "`STACK_ALLOCATED` object '%1'.";

class RawPtrToStackAllocatedMatcher : public MatchFinder::MatchCallback {
 public:
  explicit RawPtrToStackAllocatedMatcher(clang::CompilerInstance& compiler)
      : compiler_(compiler), stack_allocated_predicate_() {
    error_no_raw_ptr_to_stack_ = compiler_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, kNoRawPtrToStackAllocatedSignature);
  }

  void Register(MatchFinder& match_finder) {
    auto value_decl_matcher =
        RawPtrToStackAllocatedTypeLoc(&stack_allocated_predicate_);
    match_finder.addMatcher(value_decl_matcher, this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const auto* pointer =
        result.Nodes.getNodeAs<clang::CXXRecordDecl>("pointerRecordDecl");
    assert(pointer && "matcher should bind 'pointerRecordDecl'");

    const auto* pointee =
        result.Nodes.getNodeAs<clang::QualType>("pointeeQualType");
    assert(pointee && "matcher should bind 'pointeeQualType'");
    clang::PrintingPolicy printing_policy(result.Context->getLangOpts());
    const std::string pointee_name = pointee->getAsString(printing_policy);

    const auto* type_loc =
        result.Nodes.getNodeAs<clang::TypeLoc>("stackAllocatedRawPtrTypeLoc");
    assert(type_loc && "matcher should bind 'stackAllocatedRawPtrTypeLoc'");

    compiler_.getDiagnostics().Report(type_loc->getEndLoc(),
                                      error_no_raw_ptr_to_stack_)
        << pointer->getNameAsString() << pointee_name;
  }

 private:
  clang::CompilerInstance& compiler_;
  StackAllocatedPredicate stack_allocated_predicate_;
  unsigned error_no_raw_ptr_to_stack_;
};

void FindBadRawPtrPatterns(Options options,
                           clang::ASTContext& ast_context,
                           clang::CompilerInstance& compiler) {
  MatchFinder match_finder;

  BadCastMatcher bad_cast_matcher(compiler);
  if (options.check_bad_raw_ptr_cast)
    bad_cast_matcher.Register(match_finder);

  std::vector<std::string> paths_to_exclude_lines;
  for (auto* const line : kRawPtrManualPathsToIgnore) {
    paths_to_exclude_lines.push_back(line);
  }
  paths_to_exclude_lines.insert(paths_to_exclude_lines.end(),
                                options.raw_ptr_paths_to_exclude_lines.begin(),
                                options.raw_ptr_paths_to_exclude_lines.end());

  FilterFile exclude_fields(options.exclude_fields_file, "exclude-fields");
  FilterFile exclude_lines(paths_to_exclude_lines);
  StackAllocatedPredicate stack_allocated_predicate;
  RawPtrAndRefExclusionsOptions exclusion_options{
      &exclude_fields, &exclude_lines, options.check_raw_ptr_to_stack_allocated,
      &stack_allocated_predicate, options.raw_ptr_fix_crbug_1449812};

  RawPtrFieldMatcher field_matcher(compiler, exclusion_options);
  if (options.check_raw_ptr_fields) {
    field_matcher.Register(match_finder);
  }

  RawRefFieldMatcher ref_field_matcher(compiler, exclusion_options);
  if (options.check_raw_ref_fields) {
    ref_field_matcher.Register(match_finder);
  }

  RawPtrToStackAllocatedMatcher raw_ptr_to_stack(compiler);
  if (options.check_raw_ptr_to_stack_allocated) {
    raw_ptr_to_stack.Register(match_finder);
  }

  match_finder.matchAST(ast_context);
}

}  // namespace chrome_checker
