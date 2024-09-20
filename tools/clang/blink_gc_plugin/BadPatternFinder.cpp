// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "BadPatternFinder.h"

#include <clang/AST/Decl.h>
#include <clang/AST/RecordLayout.h>

#include <algorithm>

#include "BlinkGCPluginOptions.h"
#include "Config.h"
#include "DiagnosticsReporter.h"
#include "RecordInfo.h"
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

TypeMatcher MemberType() {
  auto has_member_base = hasCanonicalType(hasDeclaration(
      classTemplateSpecializationDecl(
          hasName("::cppgc::internal::BasicMember"),
          hasAnyTemplateArgument(
              refersToType(hasCanonicalType(hasDeclaration(anyOf(
                  cxxRecordDecl(hasName("::cppgc::internal::StrongMemberTag")),
                  cxxRecordDecl(
                      hasName("::cppgc::internal::WeakMemberTag"))))))))
          .bind("member")));
  return anyOf(has_member_base,
               hasCanonicalType(arrayType(hasElementType(has_member_base))));
}

TypeMatcher TraceableType() {
  auto has_gc_base = hasCanonicalType(hasDeclaration(
      cxxRecordDecl(
          hasMethod(cxxMethodDecl(
              hasName("Trace"), isConst(), parameterCountIs(1),
              hasParameter(
                  0, parmVarDecl(hasType(pointerType(pointee(hasCanonicalType(
                         hasDeclaration(cxxRecordDecl(isSameOrDerivedFrom(
                             hasName("cppgc::Visitor")))))))))))))
          .bind("traceable")));
  return anyOf(has_gc_base,
               hasCanonicalType(arrayType(hasElementType(has_gc_base))));
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

bool IsOnStack(const clang::Decl* decl, RecordCache& record_cache) {
  if (dyn_cast<const clang::VarDecl>(decl)) {
    return true;
  }
  const clang::FieldDecl* field_decl = dyn_cast<const clang::FieldDecl>(decl);
  assert(field_decl);
  const clang::CXXRecordDecl* parent_decl =
      dyn_cast<const clang::CXXRecordDecl>(field_decl->getParent());
  assert(parent_decl);
  return record_cache.Lookup(parent_decl)->IsStackAllocated();
}

class OptionalOrRawPtrToGCedMatcher : public MatchFinder::MatchCallback {
 public:
  OptionalOrRawPtrToGCedMatcher(DiagnosticsReporter& diagnostics,
                                RecordCache& record_cache)
      : diagnostics_(diagnostics), record_cache_(record_cache) {}

  void Register(MatchFinder& match_finder) {
    // Matches fields and new-expressions of type std::optional or
    // absl::optional where the template argument is known to refer to a
    // garbage-collected type.
    auto optional_gced_type = hasType(
        classTemplateSpecializationDecl(
            hasAnyName("::absl::optional", "::std::optional", "::base::raw_ptr",
                       "::base::raw_ref"),
            hasTemplateArgument(0, refersToType(anyOf(GarbageCollectedType(),
                                                      TraceableType()))))
            .bind("type"));
    auto optional_field = fieldDecl(optional_gced_type).bind("bad_decl");
    auto optional_var = varDecl(optional_gced_type).bind("bad_decl");
    auto optional_new_expression =
        cxxNewExpr(has(cxxConstructExpr(optional_gced_type))).bind("bad_new");
    match_finder.addDynamicMatcher(optional_field, this);
    match_finder.addDynamicMatcher(optional_var, this);
    match_finder.addDynamicMatcher(optional_new_expression, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("type");
    bool is_optional = (type->getName() == "optional");
    auto* arg_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    bool is_gced = arg_type;
    if (!arg_type) {
      arg_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("traceable");
    }
    assert(arg_type);
    if (auto* bad_decl = result.Nodes.getNodeAs<clang::Decl>("bad_decl")) {
      if (Config::IsIgnoreAnnotated(bad_decl)) {
        return;
      }
      // Optionals of non-GCed traceable or GCed collections are allowed on
      // stack.
      if (is_optional &&
          (!is_gced || Config::IsGCCollection(arg_type->getName())) &&
          IsOnStack(bad_decl, record_cache_)) {
        return;
      }
      if (is_optional) {
        diagnostics_.OptionalDeclUsedWithGC(bad_decl, type, arg_type);
      } else {
        diagnostics_.RawPtrOrRefDeclUsedWithGC(bad_decl, type, arg_type);
      }
    } else {
      auto* bad_new = result.Nodes.getNodeAs<clang::Expr>("bad_new");
      assert(bad_new);
      if (is_optional) {
        diagnostics_.OptionalNewExprUsedWithGC(bad_new, type, arg_type);
      } else {
        diagnostics_.RawPtrOrRefNewExprUsedWithGC(bad_new, type, arg_type);
      }
    }
  }

 private:
  DiagnosticsReporter& diagnostics_;
  RecordCache& record_cache_;
};

class OptionalMemberMatcher : public MatchFinder::MatchCallback {
 public:
  OptionalMemberMatcher(DiagnosticsReporter& diagnostics,
                        RecordCache& record_cache)
      : diagnostics_(diagnostics), record_cache_(record_cache) {}

  void Register(MatchFinder& match_finder) {
    // Matches fields and new-expressions of type std::optional or
    // absl::optional where the template argument is known to refer to a
    // garbage-collected type.
    auto optional_gced_type =
        hasType(classTemplateSpecializationDecl(
                    hasAnyName("::absl::optional", "::std::optional"),
                    hasTemplateArgument(0, refersToType(MemberType())))
                    .bind("type"));
    // On stack optional<Member> is safe, so we don't need to find variables
    // here.Matching fields should suffice.
    auto optional_field = fieldDecl(optional_gced_type).bind("bad_decl");
    auto optional_new_expression =
        cxxNewExpr(has(cxxConstructExpr(optional_gced_type))).bind("bad_new");
    match_finder.addDynamicMatcher(optional_field, this);
    match_finder.addDynamicMatcher(optional_new_expression, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("type");
    auto* member = result.Nodes.getNodeAs<clang::CXXRecordDecl>("member");
    if (auto* bad_decl = result.Nodes.getNodeAs<clang::Decl>("bad_decl")) {
      if (Config::IsIgnoreAnnotated(bad_decl) ||
          IsOnStack(bad_decl, record_cache_)) {
        return;
      }
      diagnostics_.OptionalDeclUsedWithMember(bad_decl, type, member);
    } else {
      auto* bad_new = result.Nodes.getNodeAs<clang::Expr>("bad_new");
      assert(bad_new);
      diagnostics_.OptionalNewExprUsedWithMember(bad_new, type, member);
    }
  }

 private:
  DiagnosticsReporter& diagnostics_;
  RecordCache& record_cache_;
};

class CollectionOfGarbageCollectedMatcher : public MatchFinder::MatchCallback {
 public:
  explicit CollectionOfGarbageCollectedMatcher(DiagnosticsReporter& diagnostics,
                                               RecordCache& record_cache)
      : diagnostics_(diagnostics), record_cache_(record_cache) {}

  void Register(MatchFinder& match_finder) {
    auto gced_ptr_or_ref =
        anyOf(GarbageCollectedType(),
              pointerType(pointee(GarbageCollectedType())).bind("ptr"),
              referenceType(pointee(GarbageCollectedType())).bind("ptr"));
    auto gced_ptr_ref_or_pair =
        anyOf(gced_ptr_or_ref,
              hasCanonicalType(hasDeclaration((classTemplateSpecializationDecl(
                  hasName("::std::pair"),
                  hasAnyTemplateArgument(refersToType(gced_ptr_or_ref)))))));
    auto member_ptr_or_ref =
        anyOf(MemberType(), pointerType(pointee(MemberType())),
              referenceType(pointee(MemberType())));
    auto member_ptr_ref_or_pair =
        anyOf(member_ptr_or_ref,
              hasCanonicalType(hasDeclaration((classTemplateSpecializationDecl(
                  hasName("::std::pair"),
                  hasAnyTemplateArgument(refersToType(member_ptr_or_ref)))))));
    auto gced_or_member = anyOf(gced_ptr_ref_or_pair, member_ptr_ref_or_pair);
    auto has_wtf_collection_name = hasAnyName(
        "::WTF::Vector", "::WTF::Deque", "::WTF::HashSet",
        "::WTF::LinkedHashSet", "::WTF::HashCountedSet", "::WTF::HashMap");
    auto has_std_collection_name =
        hasAnyName("::std::vector", "::std::map", "::std::unordered_map",
                   "::std::set", "::std::unordered_set", "::std::array");
    auto partition_allocator = hasCanonicalType(
        hasDeclaration(cxxRecordDecl(hasName("::WTF::PartitionAllocator"))));
    auto wtf_collection_decl =
        classTemplateSpecializationDecl(
            has_wtf_collection_name,
            hasAnyTemplateArgument(refersToType(gced_or_member)),
            hasAnyTemplateArgument(refersToType(partition_allocator)))
            .bind("collection");
    auto std_collection_decl =
        classTemplateSpecializationDecl(
            has_std_collection_name,
            hasAnyTemplateArgument(refersToType(gced_or_member)))
            .bind("collection");
    auto any_collection = hasType(hasCanonicalType(
        hasDeclaration(anyOf(wtf_collection_decl, std_collection_decl))));
    auto collection_field = fieldDecl(any_collection).bind("bad_decl");
    auto collection_var = varDecl(any_collection).bind("bad_decl");
    auto collection_new_expression =
        cxxNewExpr(has(cxxConstructExpr(any_collection))).bind("bad_new");
    match_finder.addDynamicMatcher(collection_field, this);
    match_finder.addDynamicMatcher(collection_var, this);
    match_finder.addDynamicMatcher(collection_new_expression, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* collection =
        result.Nodes.getNodeAs<clang::CXXRecordDecl>("collection");
    auto* gc_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    auto* member = result.Nodes.getNodeAs<clang::CXXRecordDecl>("member");
    assert(gc_type || member);
    if (auto* bad_decl = result.Nodes.getNodeAs<clang::Decl>("bad_decl")) {
      if (Config::IsIgnoreAnnotated(bad_decl)) {
        return;
      }
      if (collection->getNameAsString() == "array") {
        if (member || Config::IsGCCollection(gc_type->getName())) {
          // std::array of Members is fine as long as it is traced (which is
          // enforced by another checker).
          return;
        }
        if (result.Nodes.getNodeAs<clang::Type>("ptr") &&
            IsOnStack(bad_decl, record_cache_)) {
          // On stack std::array of raw pointers to GCed type is allowed.
          // Note: this may miss cases of std::array<std::pair<GCed, GCed*>>,
          // but such cases don't currently exist in the codebase.
          return;
        }
      }
      if (gc_type) {
        diagnostics_.CollectionOfGCed(bad_decl, collection, gc_type);
      } else {
        assert(member);
        diagnostics_.CollectionOfMembers(bad_decl, collection, member);
      }
    } else {
      auto* bad_new = result.Nodes.getNodeAs<clang::Expr>("bad_new");
      assert(bad_new);
      if (gc_type) {
        diagnostics_.CollectionOfGCed(bad_new, collection, gc_type);
      } else {
        assert(member);
        diagnostics_.CollectionOfMembers(bad_new, collection, member);
      }
    }
  }

 private:
  DiagnosticsReporter& diagnostics_;
  RecordCache& record_cache_;
};

// For the absl::variant checker, we need to match the inside of a variadic
// template class, which doesn't seem easy with the built-in matchers: define
// a custom matcher to go through the template parameter list.
AST_MATCHER_P(clang::TemplateArgument,
              parameterPackHasAnyElement,
              // Clang exports other instantiations of Matcher via
              // using-declarations in public headers, e.g. `using TypeMatcher
              // = Matcher<QualType>`.
              //
              // Once https://reviews.llvm.org/D89920, a Clang patch adding a
              // similar alias for template arguments, lands, this can be
              // changed to TemplateArgumentMatcher and won't need to use the
              // internal namespace any longer.
              clang::ast_matchers::internal::Matcher<clang::TemplateArgument>,
              InnerMatcher) {
  if (Node.getKind() != clang::TemplateArgument::Pack) {
    return false;
  }
  return llvm::any_of(Node.pack_elements(),
                      [&](const clang::TemplateArgument& Arg) {
                        return InnerMatcher.matches(Arg, Finder, Builder);
                      });
}

// Prevents the use of garbage collected objects in `absl::variant`.
// That's because `absl::variant` doesn't work well with concurrent marking.
// Oilpan uses an object's type to know how to trace it. If the type stored in
// an `absl::variant` changes while the object is concurrently being marked,
// Oilpan might fail to find a matching pair of element type and reference.
// This in turn can lead to UAFs and other memory corruptions.
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
                            hasAnyName("::absl::variant", "::std::variant"),
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
    auto class_member_variable_matcher =
        varDecl(hasType(MemberType())).bind("var");
    match_finder.addDynamicMatcher(class_member_variable_matcher, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* member = result.Nodes.getNodeAs<clang::VarDecl>("var");
    if (Config::IsIgnoreAnnotated(member)) {
      return;
    }
    diagnostics_.MemberOnStack(member);
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

class WeakPtrToGCedMatcher : public MatchFinder::MatchCallback {
 public:
  explicit WeakPtrToGCedMatcher(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    // Matches declarations of type base::WeakPtr and base::WeakPtrFactory
    // where the template argument is known to refer to a garbage-collected
    // type.
    auto weak_ptr_type = hasType(
        classTemplateSpecializationDecl(
            hasAnyName("::base::WeakPtr", "::base::WeakPtrFactory"),
            hasTemplateArgument(0, refersToType(GarbageCollectedType())))
            .bind("weak_ptr"));
    auto weak_ptr_field = fieldDecl(weak_ptr_type).bind("bad_decl");
    auto weak_ptr_var = varDecl(weak_ptr_type).bind("bad_decl");
    auto weak_ptr_new_expression =
        cxxNewExpr(has(cxxConstructExpr(weak_ptr_type))).bind("bad_decl");
    match_finder.addDynamicMatcher(weak_ptr_field, this);
    match_finder.addDynamicMatcher(weak_ptr_var, this);
    match_finder.addDynamicMatcher(weak_ptr_new_expression, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* decl = result.Nodes.getNodeAs<clang::Decl>("bad_decl");
    if (Config::IsIgnoreAnnotated(decl)) {
      return;
    }
    auto* weak_ptr = result.Nodes.getNodeAs<clang::CXXRecordDecl>("weak_ptr");
    auto* gc_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    diagnostics_.WeakPtrToGCed(decl, weak_ptr, gc_type);
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
    if (auto* base = base_spec.getType()->getAsCXXRecordDecl()) {
      if (matches(*base, Finder, Builder)) {
        return true;
      }
    }
  }

  return false;
}

size_t RoundUp(size_t value, size_t align) {
  assert((align & (align - 1)) == 0);
  return (value + align - 1) & ~(align - 1);
}

// Very approximate way of calculating size of a record based on fields.
// Doesn't take into account alignment of base subobjects, but only its own
// fields.
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
        cxxRecordDecl(has(fieldDecl(hasType(MemberType())).bind("field")),
                      isDisallowedNewClass())
            .bind("record");
    match_finder.addMatcher(member_field_matcher, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* class_decl = result.Nodes.getNodeAs<clang::RecordDecl>("record");
    if (class_decl->isDependentType() || class_decl->isUnion()) {
      return;
    }

    if (auto* member_decl = result.Nodes.getNodeAs<clang::FieldDecl>("field");
        member_decl && Config::IsIgnoreAnnotated(member_decl)) {
      return;
    }

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

class GCedVarOrField : public MatchFinder::MatchCallback {
 public:
  explicit GCedVarOrField(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    auto gced_field =
        fieldDecl(hasType(GarbageCollectedType())).bind("bad_field");
    // As opposed to definitions, declarations of function templates with
    // unfulfilled requires-clauses get instantiated and as such are
    // observable in the clang AST (as functions without a body). If such a
    // method takes a GCed type as a parameter, we should not alert on it
    // since the method is never actually used. This is common in Blink due to
    // the implementation of the variant CHECK_* and DCHECK_* macros
    // (specifically the DEFINE_CHECK_OP_IMPL macro).
    auto unimplemented_template_instance_parameter =
        parmVarDecl(hasAncestor(functionDecl(hasParent(functionTemplateDecl()),
                                             unless(hasBody(stmt())))));
    auto gced_var = varDecl(hasType(GarbageCollectedType()),
                            unless(unimplemented_template_instance_parameter))
                        .bind("bad_var");
    match_finder.addDynamicMatcher(gced_field, this);
    match_finder.addDynamicMatcher(gced_var, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const auto* gctype = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    assert(gctype);
    if (Config::IsGCCollection(gctype->getName())) {
      return;
    }
    const auto* field = result.Nodes.getNodeAs<clang::FieldDecl>("bad_field");
    if (field) {
      if (Config::IsIgnoreAnnotated(field)) {
        return;
      }
      diagnostics_.GCedField(field, gctype);
    } else {
      const auto* var = result.Nodes.getNodeAs<clang::VarDecl>("bad_var");
      assert(var);
      if (Config::IsIgnoreAnnotated(var)) {
        return;
      }
      diagnostics_.GCedVar(var, gctype);
    }
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

}  // namespace

void FindBadPatterns(clang::ASTContext& ast_context,
                     DiagnosticsReporter& diagnostics,
                     RecordCache& record_cache,
                     const BlinkGCPluginOptions& options) {
  MatchFinder match_finder;

  UniquePtrGarbageCollectedMatcher unique_ptr_gc(diagnostics);
  unique_ptr_gc.Register(match_finder);

  OptionalOrRawPtrToGCedMatcher optional_or_rawptr_gc(diagnostics,
                                                      record_cache);
  optional_or_rawptr_gc.Register(match_finder);

  CollectionOfGarbageCollectedMatcher collection_of_gc(diagnostics,
                                                       record_cache);
  if (options.enable_off_heap_collections_of_gced_check) {
    collection_of_gc.Register(match_finder);
  }

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

  WeakPtrToGCedMatcher weak_ptr_to_gced(diagnostics);
  weak_ptr_to_gced.Register(match_finder);

  GCedVarOrField gced_var_or_field(diagnostics);
  gced_var_or_field.Register(match_finder);

  OptionalMemberMatcher optional_member(diagnostics, record_cache);
  optional_member.Register(match_finder);

  match_finder.matchAST(ast_context);
}
