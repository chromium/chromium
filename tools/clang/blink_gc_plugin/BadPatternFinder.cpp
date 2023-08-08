// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "BadPatternFinder.h"
#include <clang/AST/Decl.h>
#include <clang/AST/RecordLayout.h>
#include "BlinkGCPluginOptions.h"
#include "Config.h"
#include "DiagnosticsReporter.h"

#include <algorithm>
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"

using namespace clang::ast_matchers;

namespace {

TypeMatcher GarbageCollectedType() {
  auto has_gc_base = hasCanonicalType(hasDeclaration(
      cxxRecordDecl(isDerivedFrom(hasAnyName("::cppgc::GarbageCollected",
                                             "::cppgc::GarbageCollectedMixin")))
          .bind("gctype")));
  return anyOf(has_gc_base,
               hasCanonicalType(arrayType(hasElementType(has_gc_base))));
}

auto MemberType() {
  return hasType(hasCanonicalType(hasDeclaration(cxxRecordDecl(
      isSameOrDerivedFrom(hasName("::cppgc::internal::BasicMember"))))));
}

class UniquePtrGarbageCollectedMatcher : public MatchFinder::MatchCallback {
 public:
  explicit UniquePtrGarbageCollectedMatcher(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    // Matches any application of make_unique where the template argument is
    // known to refer to a garbage-collected type.
    auto make_unique_matcher =
        callExpr(
            callee(functionDecl(
                       hasAnyName("::std::make_unique", "::base::WrapUnique"),
                       hasTemplateArgument(
                           0, refersToType(GarbageCollectedType())))
                       .bind("badfunc")))
            .bind("bad");
    match_finder.addDynamicMatcher(make_unique_matcher, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* bad_use = result.Nodes.getNodeAs<clang::Expr>("bad");
    auto* bad_function = result.Nodes.getNodeAs<clang::FunctionDecl>("badfunc");
    auto* gc_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    diagnostics_.UniquePtrUsedWithGC(bad_use, bad_function, gc_type);
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

class OptionalGarbageCollectedMatcher : public MatchFinder::MatchCallback {
 public:
  explicit OptionalGarbageCollectedMatcher(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    // Matches fields and new-expressions of type absl::optional where the
    // template argument is known to refer to a garbage-collected type.
    auto optional_type = hasType(
        classTemplateSpecializationDecl(
            hasName("::absl::optional"),
            hasTemplateArgument(0, refersToType(GarbageCollectedType())))
            .bind("optional"));
    auto optional_field = fieldDecl(optional_type).bind("bad_field");
    auto optional_new_expression =
        cxxNewExpr(has(cxxConstructExpr(optional_type))).bind("bad_new");
    match_finder.addDynamicMatcher(optional_field, this);
    match_finder.addDynamicMatcher(optional_new_expression, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* optional = result.Nodes.getNodeAs<clang::CXXRecordDecl>("optional");
    auto* gc_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    if (auto* bad_field =
            result.Nodes.getNodeAs<clang::FieldDecl>("bad_field")) {
      diagnostics_.OptionalFieldUsedWithGC(bad_field, optional, gc_type);
    } else {
      auto* bad_new = result.Nodes.getNodeAs<clang::Expr>("bad_new");
      diagnostics_.OptionalNewExprUsedWithGC(bad_new, optional, gc_type);
    }
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

// For the absl::variant checker, we need to match the inside of a variadic
// template class, which doesn't seem easy with the built-in matchers: define a
// custom matcher to go through the template parameter list.
AST_MATCHER_P(clang::TemplateArgument,
              parameterPackHasAnyElement,
              // Clang exports other instantiations of Matcher via
              // using-declarations in public headers, e.g. `using TypeMatcher =
              // Matcher<QualType>`.
              //
              // Once https://reviews.llvm.org/D89920, a Clang patch adding a
              // similar alias for template arguments, lands, this can be
              // changed to TemplateArgumentMatcher and won't need to use the
              // internal namespace any longer.
              clang::ast_matchers::internal::Matcher<clang::TemplateArgument>,
              InnerMatcher) {
  if (Node.getKind() != clang::TemplateArgument::Pack)
    return false;
  return llvm::any_of(Node.pack_elements(),
                      [&](const clang::TemplateArgument& Arg) {
                        return InnerMatcher.matches(Arg, Finder, Builder);
                      });
}

// Prevents the use of garbage collected objects in `absl::variant`.
// That's because `absl::variant` doesn't work well with concurrent marking.
// Oilpan uses an object's type to know how to trace it. If the type stored in
// an `absl::variant` changes while the object is concurrently being marked,
// Oilpan might fail to find a matching pair of element type and reference. This
// in turn can lead to UAFs and other memory corruptions.
class VariantGarbageCollectedMatcher : public MatchFinder::MatchCallback {
 public:
  explicit VariantGarbageCollectedMatcher(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    // Matches any constructed absl::variant where a template argument is
    // known to refer to a garbage-collected type.
    auto variant_construction =
        cxxConstructExpr(
            hasDeclaration(cxxConstructorDecl(
                ofClass(classTemplateSpecializationDecl(
                            hasName("::absl::variant"),
                            hasAnyTemplateArgument(parameterPackHasAnyElement(
                                refersToType(GarbageCollectedType()))))
                            .bind("variant")))))
            .bind("bad");
    match_finder.addDynamicMatcher(variant_construction, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* bad_use = result.Nodes.getNodeAs<clang::Expr>("bad");
    auto* variant = result.Nodes.getNodeAs<clang::CXXRecordDecl>("variant");
    auto* gc_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    diagnostics_.VariantUsedWithGC(bad_use, variant, gc_type);
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

class MemberOnStackMatcher : public MatchFinder::MatchCallback {
 public:
  explicit MemberOnStackMatcher(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    auto class_member_variable_matcher = varDecl(MemberType()).bind("member");
    match_finder.addDynamicMatcher(class_member_variable_matcher, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* member = result.Nodes.getNodeAs<clang::VarDecl>("member");
    if (Config::IsIgnoreAnnotated(member))
      return;
    diagnostics_.MemberOnStack(member);
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

AST_MATCHER(clang::CXXRecordDecl, isDisallowedNewClass) {
  auto& context = Finder->getASTContext();

  auto gc_matcher = GarbageCollectedType();
  if (gc_matcher.matches(context.getTypeDeclType(&Node), Finder, Builder)) {
    // This is a normal GCed class, bail out.
    return false;
  }

  // First, look for methods in this class.
  auto method = std::find_if(
      Node.method_begin(), Node.method_end(), [](const auto& method) {
        return method->getNameAsString() == kNewOperatorName &&
               method->getNumParams() == 1;
      });
  if (method != Node.method_end()) {
    // We found the 'operator new'. Check if it's deleted.
    return method->isDeleted();
  }

  // Otherwise, lookup in the base classes.
  for (auto& base_spec : Node.bases()) {
    if (auto* base = base_spec.getType()->getAsCXXRecordDecl())
      if (matches(*base, Finder, Builder))
        return true;
  }

  return false;
}

size_t RoundUp(size_t value, size_t align) {
  assert((align & (align - 1)) == 0);
  return (value + align - 1) & ~(align - 1);
}

// Very approximate way of calculating size of a record based on fields. Doesn't
// take into account alignment of base subobjects, but only its own fields.
size_t RequiredSizeForFields(const clang::ASTContext& context,
                             size_t current_size,
                             const std::vector<clang::QualType>& field_types) {
  size_t largest_field_alignment = 0;

  for (clang::QualType type : field_types) {
    assert(!type->isDependentType());
    const size_t current_field_alignment = context.getTypeAlign(type);
    current_size = RoundUp(current_size, current_field_alignment);
    current_size += context.getTypeSize(type);
    largest_field_alignment =
        std::max(largest_field_alignment, current_field_alignment);
  }

  current_size = RoundUp(current_size, largest_field_alignment);
  return current_size;
}

class PaddingInGCedMatcher : public MatchFinder::MatchCallback {
 public:
  PaddingInGCedMatcher(clang::ASTContext& context,
                       DiagnosticsReporter& diagnostics)
      : context_(context), diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    auto member_field_matcher =
        cxxRecordDecl(has(fieldDecl(MemberType()).bind("member")),
                      isDisallowedNewClass())
            .bind("record");
    match_finder.addMatcher(member_field_matcher, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* class_decl = result.Nodes.getNodeAs<clang::RecordDecl>("record");
    if (class_decl->isDependentType() || class_decl->isUnion())
      return;

    if (auto* member_decl = result.Nodes.getNodeAs<clang::FieldDecl>("member");
        member_decl && Config::IsIgnoreAnnotated(member_decl))
      return;

    if (auto* cxx_record_decl =
            clang::dyn_cast<clang::CXXRecordDecl>(class_decl)) {
      if (cxx_record_decl->getNumVBases()) {
        // Don't process class with virtual bases.
        return;
      }
    }

    std::vector<clang::QualType> fields;
    for (auto* field : class_decl->fields()) {
      if (field->isBitField()) {
        // Don't process types with bitfields yet.
        return;
      }
      if (field->isZeroSize(context_)) {
        // Don't process types with [[no_unique_address]] on the fields.
        return;
      }
      if (field->hasAttr<clang::AlignedAttr>()) {
        // Ignore classes containing alignas on the fields.
        return;
      }

      fields.push_back(field->getType());
    }
    assert(fields.size() > 0);

    const clang::ASTRecordLayout& layout =
        context_.getASTRecordLayout(class_decl);
    const size_t base_size = layout.getFieldOffset(0);

    const size_t size_before =
        RequiredSizeForFields(context_, base_size, fields);

    std::sort(fields.begin(), fields.end(),
              [this](clang::QualType t1, clang::QualType t2) {
                // Try simply sort by sizes, ignoring alignment.
                return context_.getTypeSize(t1) > context_.getTypeSize(t2);
              });

    const size_t size_after =
        RequiredSizeForFields(context_, base_size, fields);

    if (size_after < size_before) {
      diagnostics_.AdditionalPadding(
          class_decl, (size_before - size_after) / context_.getCharWidth());
    }
  }

 private:
  clang::ASTContext& context_;
  DiagnosticsReporter& diagnostics_;
};

}  // namespace

void FindBadPatterns(clang::ASTContext& ast_context,
                     DiagnosticsReporter& diagnostics,
                     const BlinkGCPluginOptions& options) {
  MatchFinder match_finder;

  UniquePtrGarbageCollectedMatcher unique_ptr_gc(diagnostics);
  unique_ptr_gc.Register(match_finder);

  OptionalGarbageCollectedMatcher optional_gc(diagnostics);
  optional_gc.Register(match_finder);

  VariantGarbageCollectedMatcher variant_gc(diagnostics);
  variant_gc.Register(match_finder);

  MemberOnStackMatcher member_on_stack(diagnostics);
  if (options.enable_members_on_stack_check) {
    member_on_stack.Register(match_finder);
  }

  PaddingInGCedMatcher padding_in_gced(ast_context, diagnostics);
  if (options.enable_extra_padding_check) {
    padding_in_gced.Register(match_finder);
  }

  match_finder.matchAST(ast_context);
}
