// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "BadPatternFinder.h"

#include <clang/AST/Decl.h>
#include <clang/AST/RecordLayout.h>

#include <algorithm>

#include "BlinkGCPluginOptions.h"
#include "DiagnosticsReporter.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"

using namespace clang::ast_matchers;

namespace {

TypeMatcher TypeOrArrayOf(TypeMatcher matcher) {
  return anyOf(matcher, hasCanonicalType(arrayType(hasElementType(matcher))));
}

TypeMatcher GarbageCollectedType() {
  auto has_gc_base = hasCanonicalType(hasDeclaration(
      cxxRecordDecl(isDerivedFrom(hasAnyName("::cppgc::GarbageCollected",
                                             "::cppgc::GarbageCollectedMixin")))
          .bind("gctype")));
  return TypeOrArrayOf(has_gc_base);
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
  return TypeOrArrayOf(has_member_base);
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
  return TypeOrArrayOf(has_gc_base);
}

TypeMatcher StackAllocatedType() {
  auto has_stack_allocated_marker = hasCanonicalType(
      hasDeclaration(cxxRecordDecl(isSameOrDerivedFrom(has(typeAliasDecl(
                                       hasName("IsStackAllocatedTypeMarker")))))
                         .bind("stack_allocated")));
  return TypeOrArrayOf(has_stack_allocated_marker);
}

TypeMatcher StdPairContaining(TypeMatcher matcher) {
  return hasCanonicalType(hasDeclaration(classTemplateSpecializationDecl(
      hasName("::std::pair"), hasAnyTemplateArgument(refersToType(matcher)))));
}

TypeMatcher TypeOrStdPairContaining(TypeMatcher matcher) {
  return anyOf(matcher, StdPairContaining(matcher));
}

TypeMatcher RawPtrOrRef(TypeMatcher matcher) {
  return anyOf(pointerType(pointee(matcher)).bind("ptr"),
               referenceType(pointee(matcher)).bind("ptr"));
}

TypeMatcher TypeOrRawPtrOrRef(TypeMatcher matcher) {
  return anyOf(matcher, RawPtrOrRef(matcher));
}

// As opposed to definitions, declarations of function templates (e.g. with
// unfulfilled requires-clauses or in unevaluated contexts like std::declval)
// and methods of class template specializations get instantiated and as such
// are observable in the clang AST as functions without a body. If such a
// function or method takes or returns a disallowed type, we should not alert on
// it since it is never actually used. Same applies to deleted or implicitly
// generated functions (e.g. a captureless lambda's static __invoke).
DeclarationMatcher UninstantiatedOrDeletedFunction() {
  return functionDecl(
      anyOf(isDeleted(), isImplicit(),
            allOf(unless(hasBody(stmt())),
                  anyOf(hasParent(functionTemplateDecl()),
                        hasAncestor(classTemplateSpecializationDecl())))));
}

auto FieldWithType(TypeMatcher type) {
  return fieldDecl(hasType(type));
}

auto VarWithType(TypeMatcher type) {
  return varDecl(
      hasType(type),
      unless(parmVarDecl(hasAncestor(UninstantiatedOrDeletedFunction()))));
}

auto FunctionReturningType(TypeMatcher type) {
  return functionDecl(returns(type), unless(UninstantiatedOrDeletedFunction()));
}

auto NewExprWithType(TypeMatcher type) {
  return cxxNewExpr(has(cxxConstructExpr(hasType(type))));
}

void RegisterBadDeclAndNewMatchers(MatchFinder& match_finder,
                                   MatchFinder::MatchCallback* callback,
                                   TypeMatcher type) {
  match_finder.addDynamicMatcher(FieldWithType(type).bind("bad_decl"),
                                 callback);
  match_finder.addDynamicMatcher(VarWithType(type).bind("bad_decl"), callback);
  match_finder.addDynamicMatcher(FunctionReturningType(type).bind("bad_decl"),
                                 callback);
  match_finder.addDynamicMatcher(NewExprWithType(type).bind("bad_new"),
                                 callback);
}

bool IsOnStack(const clang::Decl* decl, RecordCache& record_cache) {
  if (llvm::dyn_cast<const clang::VarDecl>(decl) ||
      llvm::dyn_cast<const clang::FunctionDecl>(decl)) {
    return true;
  }
  const clang::FieldDecl* field_decl =
      llvm::dyn_cast<const clang::FieldDecl>(decl);
  assert(field_decl);
  const clang::CXXRecordDecl* parent_decl =
      llvm::dyn_cast<const clang::CXXRecordDecl>(field_decl->getParent());
  assert(parent_decl);
  RecordInfo* record_info = record_cache.Lookup(parent_decl);
  assert(record_info);
  return record_info->IsStackAllocated();
}

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

auto HasTemplateArgWithType(TypeMatcher type) {
  auto arg = refersToType(type);
  return hasAnyTemplateArgument(anyOf(arg, parameterPackHasAnyElement(arg)));
}

class OptionalOrRawPtrOrVariantToGCedMatcher
    : public MatchFinder::MatchCallback {
 public:
  OptionalOrRawPtrOrVariantToGCedMatcher(DiagnosticsReporter& diagnostics,
                                         RecordCache& record_cache)
      : diagnostics_(diagnostics), record_cache_(record_cache) {}

  void Register(MatchFinder& match_finder) {
    // Matches fields, variables, return types, and new-expressions of type
    // std::optional, absl::optional, base::raw_ptr, base::raw_ref,
    // std::variant, absl::variant, or std::unique_ptr where a template argument
    // is known to refer to a garbage-collected, traceable, or Member type.
    // Neither optional nor variant are safe to use with concurrent marking (if
    // the active state or stored type changes while the object is concurrently
    // being marked, Oilpan can fail to trace it properly, leading to UAFs and
    // memory corruptions), and raw_ptr/raw_ref/unique_ptr must not be used to
    // reference Oilpan-managed types.
    // Check GarbageCollectedType() by value before non-GCed or pointer/ref
    // types so that on-stack variants with multiple GC-related alternatives
    // (e.g. std::variant<Member<Base>, Base>) match the disallowed by-value
    // GCed alternative rather than stopping on the allowed one.
    auto non_gced_or_ptr =
        anyOf(MemberType(), TraceableType(),
              RawPtrOrRef(anyOf(GarbageCollectedType(), MemberType(),
                                TraceableType())));
    auto gced_type = hasCanonicalType(hasDeclaration(
        classTemplateSpecializationDecl(
            hasAnyName("::absl::optional", "::std::optional", "::base::raw_ptr",
                       "::base::raw_ref", "::absl::variant", "::std::variant",
                       "::std::unique_ptr"),
            anyOf(HasTemplateArgWithType(GarbageCollectedType()),
                  HasTemplateArgWithType(non_gced_or_ptr)))
            .bind("type")));
    RegisterBadDeclAndNewMatchers(match_finder, this, gced_type);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("type");
    bool is_optional = (type->getName() == "optional");
    bool is_variant = (type->getName() == "variant");
    bool is_unique_ptr = (type->getName() == "unique_ptr");
    auto* arg_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    bool is_gced = arg_type;
    if (!arg_type) {
      arg_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("traceable");
    }
    auto* member = result.Nodes.getNodeAs<clang::CXXRecordDecl>("member");
    assert(arg_type || member);
    bool is_ptr = result.Nodes.getNodeAs<clang::Type>("ptr");
    if (auto* bad_decl = result.Nodes.getNodeAs<clang::Decl>("bad_decl")) {
      if (Config::IsIgnoreAnnotated(bad_decl)) {
        return;
      }
      // Optionals and variants of non-GCed traceable, Members, or
      // pointers/references are allowed on stack.
      if ((is_optional || is_variant) && (!is_gced || is_ptr) &&
          IsOnStack(bad_decl, record_cache_)) {
        return;
      }
      if (is_optional) {
        if (arg_type) {
          diagnostics_.OptionalDeclUsedWithGC(bad_decl, type, arg_type);
        } else {
          diagnostics_.OptionalDeclUsedWithMember(bad_decl, type, member);
        }
      } else if (is_variant) {
        diagnostics_.VariantUsedWithGC(bad_decl, type,
                                       arg_type ? arg_type : member);
      } else if (is_unique_ptr) {
        diagnostics_.UniquePtrUsedWithGC(bad_decl, type,
                                         arg_type ? arg_type : member);
      } else {
        diagnostics_.RawPtrOrRefDeclUsedWithGC(bad_decl, type,
                                               arg_type ? arg_type : member);
      }
    } else {
      auto* bad_new = result.Nodes.getNodeAs<clang::Expr>("bad_new");
      assert(bad_new);
      if (is_optional) {
        if (arg_type) {
          diagnostics_.OptionalNewExprUsedWithGC(bad_new, type, arg_type);
        } else {
          diagnostics_.OptionalNewExprUsedWithMember(bad_new, type, member);
        }
      } else if (is_variant) {
        diagnostics_.VariantUsedWithGC(bad_new, type,
                                       arg_type ? arg_type : member);
      } else if (is_unique_ptr) {
        diagnostics_.UniquePtrUsedWithGC(bad_new, type,
                                         arg_type ? arg_type : member);
      } else {
        diagnostics_.RawPtrOrRefNewExprUsedWithGC(bad_new, type,
                                                  arg_type ? arg_type : member);
      }
    }
  }

 private:
  DiagnosticsReporter& diagnostics_;
  RecordCache& record_cache_;
};

class CollectionOfGarbageCollectedMatcher : public MatchFinder::MatchCallback {
 public:
  CollectionOfGarbageCollectedMatcher(DiagnosticsReporter& diagnostics,
                                      RecordCache& record_cache,
                                      const BlinkGCPluginOptions& options)
      : diagnostics_(diagnostics),
        record_cache_(record_cache),
        options_(options) {}

  void Register(MatchFinder& match_finder, bool check_stack_allocated) {
    auto gced_or_member = TypeOrStdPairContaining(
        TypeOrRawPtrOrRef(anyOf(GarbageCollectedType(), MemberType())));
    auto gced_or_member_ptr = TypeOrStdPairContaining(anyOf(
        TypeOrRawPtrOrRef(GarbageCollectedType()), RawPtrOrRef(MemberType())));
    auto stack_allocated_or_pair =
        TypeOrStdPairContaining(StackAllocatedType());
    auto has_wtf_collection_name =
        hasAnyName("::blink::Vector", "::blink::Deque", "::blink::HashSet",
                   "::blink::LinkedHashSet", "::blink::HashCountedSet",
                   "::blink::HashMap");
    auto has_std_collection_name =
        hasAnyName("::std::vector", "::std::map", "::std::unordered_map",
                   "::std::set", "::std::unordered_set");
    auto has_gced_collection_name = hasAnyName(
        "::blink::BasicHeapVector", "::blink::BasicHeapDeque",
        "::blink::BasicHeapHashSet", "::blink::BasicHeapLinkedHashSet",
        "::blink::BasicHeapHashCountedSet", "::blink::BasicHeapHashMap",
        "::blink::GCedHeapLinkedStack");
    auto partition_allocator = hasCanonicalType(
        hasDeclaration(cxxRecordDecl(hasName("::blink::PartitionAllocator"))));
    // WTF collections already static_assert against stack-allocated types.
    auto wtf_collection_decl =
        classTemplateSpecializationDecl(
            has_wtf_collection_name,
            hasAnyTemplateArgument(refersToType(gced_or_member)),
            hasAnyTemplateArgument(refersToType(partition_allocator)))
            .bind("collection");
    auto std_collection_decl =
        check_stack_allocated
            ? classTemplateSpecializationDecl(
                  has_std_collection_name,
                  anyOf(hasAnyTemplateArgument(refersToType(gced_or_member)),
                        hasAnyTemplateArgument(
                            refersToType(stack_allocated_or_pair))))
                  .bind("collection")
            : classTemplateSpecializationDecl(
                  has_std_collection_name,
                  hasAnyTemplateArgument(refersToType(gced_or_member)))
                  .bind("collection");
    // std::array of Members or stack-allocated types is fine as long as
    // tracing/field rules are satisfied (enforced by another checker). Check
    // by-value GCed types before raw pointers/references to GCed or Member
    // types so that on-stack std::array<std::pair<GCed*, GCed>, N> or
    // std::array<std::pair<Member<GCed>*, GCed>, N> matches the disallowed
    // by-value GCed element rather than stopping on the allowed pointer/ref.
    auto std_array_decl =
        classTemplateSpecializationDecl(
            hasName("::std::array"),
            anyOf(hasAnyTemplateArgument(refersToType(
                      TypeOrStdPairContaining(GarbageCollectedType()))),
                  hasAnyTemplateArgument(
                      refersToType(TypeOrStdPairContaining(RawPtrOrRef(
                          anyOf(GarbageCollectedType(), MemberType())))))))
            .bind("collection");
    auto gced_collection_decl =
        classTemplateSpecializationDecl(
            has_gced_collection_name,
            hasAnyTemplateArgument(refersToType(gced_or_member_ptr)))
            .bind("collection");
    auto any_collection_type = hasCanonicalType(
        hasDeclaration(anyOf(wtf_collection_decl, std_collection_decl,
                             std_array_decl, gced_collection_decl)));
    RegisterBadDeclAndNewMatchers(match_finder, this, any_collection_type);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* collection =
        result.Nodes.getNodeAs<clang::CXXRecordDecl>("collection");
    assert(collection);
    auto* gc_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    auto* member = result.Nodes.getNodeAs<clang::CXXRecordDecl>("member");
    auto* stack_allocated =
        result.Nodes.getNodeAs<clang::CXXRecordDecl>("stack_allocated");
    assert(gc_type || member || stack_allocated);
    if (auto* bad_decl = result.Nodes.getNodeAs<clang::Decl>("bad_decl")) {
      if (Config::IsIgnoreAnnotated(bad_decl)) {
        return;
      }
      if (!options_.enable_off_heap_collections_of_gced_check &&
          !IsOnStack(bad_decl, record_cache_)) {
        assert(options_.enable_off_heap_collections_of_gced_on_stack_check);
        return;
      }
      if (collection->getName() == "array" &&
          result.Nodes.getNodeAs<clang::Type>("ptr") &&
          IsOnStack(bad_decl, record_cache_)) {
        // On stack std::array of raw pointers/references to GCed or Member
        // types is allowed.
        return;
      }
      if (gc_type) {
        diagnostics_.CollectionOfGCed(bad_decl, collection, gc_type);
      } else if (member) {
        diagnostics_.CollectionOfMembers(bad_decl, collection, member);
      } else {
        assert(stack_allocated);
        if (!Config::IsStackAllocatedIgnoreAnnotated(bad_decl)) {
          diagnostics_.CollectionOfStackAllocated(bad_decl, collection,
                                                  stack_allocated);
        }
      }
    } else {
      if (!options_.enable_off_heap_collections_of_gced_check) {
        assert(options_.enable_off_heap_collections_of_gced_on_stack_check);
        return;
      }
      auto* bad_new = result.Nodes.getNodeAs<clang::Expr>("bad_new");
      assert(bad_new);
      if (gc_type) {
        diagnostics_.CollectionOfGCed(bad_new, collection, gc_type);
      } else if (member) {
        diagnostics_.CollectionOfMembers(bad_new, collection, member);
      } else {
        assert(stack_allocated);
        diagnostics_.CollectionOfStackAllocated(bad_new, collection,
                                                stack_allocated);
      }
    }
  }

 private:
  DiagnosticsReporter& diagnostics_;
  RecordCache& record_cache_;
  const BlinkGCPluginOptions& options_;
};

class MemberOnStackMatcher : public MatchFinder::MatchCallback {
 public:
  explicit MemberOnStackMatcher(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    match_finder.addDynamicMatcher(VarWithType(MemberType()).bind("var"), this);
    match_finder.addDynamicMatcher(
        FunctionReturningType(MemberType()).bind("var"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* member = result.Nodes.getNodeAs<clang::NamedDecl>("var");
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
    auto weak_ptr_decl = hasCanonicalType(hasDeclaration(
        classTemplateSpecializationDecl(
            hasAnyName("::base::WeakPtr", "::base::WeakPtrFactory"),
            hasTemplateArgument(0, refersToType(GarbageCollectedType())))
            .bind("weak_ptr")));
    RegisterBadDeclAndNewMatchers(match_finder, this, weak_ptr_decl);
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* weak_ptr = result.Nodes.getNodeAs<clang::CXXRecordDecl>("weak_ptr");
    auto* gc_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    if (auto* decl = result.Nodes.getNodeAs<clang::Decl>("bad_decl")) {
      if (Config::IsIgnoreAnnotated(decl)) {
        return;
      }
      diagnostics_.WeakPtrToGCed(decl, weak_ptr, gc_type);
    } else {
      auto* bad_new = result.Nodes.getNodeAs<clang::Expr>("bad_new");
      assert(bad_new);
      diagnostics_.WeakPtrToGCed(bad_new, weak_ptr, gc_type);
    }
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

AST_MATCHER(clang::CXXRecordDecl, isDisallowedNewClass) {
  auto& context = Finder->getASTContext();

  auto gc_matcher = GarbageCollectedType();
  if (gc_matcher.matches(context.getCanonicalTagType(&Node), Finder, Builder)) {
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
        cxxRecordDecl(has(FieldWithType(MemberType()).bind("field")),
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
    match_finder.addDynamicMatcher(
        FieldWithType(GarbageCollectedType()).bind("bad_field"), this);
    match_finder.addDynamicMatcher(
        VarWithType(GarbageCollectedType()).bind("bad_var"), this);
    match_finder.addDynamicMatcher(
        FunctionReturningType(GarbageCollectedType()).bind("bad_return"), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const auto* gctype = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    assert(gctype);
    if (const auto* field =
            result.Nodes.getNodeAs<clang::FieldDecl>("bad_field")) {
      if (Config::IsIgnoreAnnotated(field)) {
        return;
      }
      diagnostics_.GCedField(field, gctype);
    } else if (const auto* var =
                   result.Nodes.getNodeAs<clang::VarDecl>("bad_var")) {
      if (Config::IsIgnoreAnnotated(var)) {
        return;
      }
      diagnostics_.GCedVar(var, gctype);
    } else {
      const auto* func =
          result.Nodes.getNodeAs<clang::FunctionDecl>("bad_return");
      assert(func);
      if (Config::IsIgnoreAnnotated(func)) {
        return;
      }
      diagnostics_.GCedReturn(func, gctype);
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

  OptionalOrRawPtrOrVariantToGCedMatcher optional_or_rawptr_or_variant_gc(
      diagnostics, record_cache);
  optional_or_rawptr_or_variant_gc.Register(match_finder);

  CollectionOfGarbageCollectedMatcher collection_of_gc(diagnostics,
                                                       record_cache, options);
  if (options.enable_off_heap_collections_of_gced_check ||
      options.enable_off_heap_collections_of_gced_on_stack_check) {
    collection_of_gc.Register(
        match_finder,
        options.enable_off_heap_collections_of_stack_allocated_check);
  }

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

  match_finder.matchAST(ast_context);
}
