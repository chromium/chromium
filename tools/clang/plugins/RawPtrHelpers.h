// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_RAWPTRHELPERS_H_
#define TOOLS_CLANG_PLUGINS_RAWPTRHELPERS_H_

#include <optional>

#include "Util.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/CommandLine.h"

// Represents a filter file specified via cmdline.
//
// Filter file format:
// - '#' character starts a comment (which gets ignored).
// - Blank or whitespace-only or comment-only lines are ignored.
// - Other lines are expected to contain a fully-qualified name of a field
//   like:
//       autofill::AddressField::address1_ # some comment
// - Templates are represented without template arguments, like:
//       WTF::HashTable::table_ # some comment
class FilterFile {
 public:
  explicit FilterFile(const std::string& filepath,
                      const std::string& arg_name) {
    ParseInputFile(filepath, arg_name);
  }
  explicit FilterFile(const std::vector<std::string>& lines);

  FilterFile(const FilterFile&) = delete;
  FilterFile& operator=(const FilterFile&) = delete;

  // Returns true if any of the filter file lines is exactly equal to |line|.
  bool ContainsLine(llvm::StringRef line) const;

  // Returns true if |string_to_match| matches based on the filter file lines.
  // Filter file lines can contain both inclusions and exclusions in the filter.
  // Only returns true if |string_to_match| both matches an inclusion filter and
  // is *not* matched by an exclusion filter.
  bool ContainsSubstringOf(llvm::StringRef string_to_match) const;

 private:
  void ParseInputFile(const std::string& filepath, const std::string& arg_name);

  // Stores all file lines (after stripping comments and blank lines).
  llvm::StringSet<> file_lines_;

  // |file_lines_| is partitioned based on whether the line starts with a !
  // (exclusion line) or not (inclusion line). Inclusion lines specify things to
  // be matched by the filter. The exclusion lines specify what to force exclude
  // from the filter. Lazily-constructed regex that matches strings that contain
  // any of the inclusion lines in |file_lines_|.
  mutable std::optional<llvm::Regex> inclusion_substring_regex_;

  // Lazily-constructed regex that matches strings that contain any of the
  // exclusion lines in |file_lines_|.
  mutable std::optional<llvm::Regex> exclusion_substring_regex_;
};

AST_MATCHER(clang::Type, anyCharType) {
  return Node.isAnyCharacterType();
}

AST_MATCHER(clang::FieldDecl, isInScratchSpace) {
  const clang::SourceManager& source_manager =
      Finder->getASTContext().getSourceManager();
  clang::SourceLocation location = Node.getSourceRange().getBegin();
  if (location.isInvalid())
    return false;
  clang::SourceLocation spelling_location =
      source_manager.getSpellingLoc(location);
  return source_manager.isWrittenInScratchSpace(spelling_location);
}

AST_MATCHER(clang::FieldDecl, isInThirdPartyLocation) {
  std::string filename = GetFilename(Finder->getASTContext().getSourceManager(),
                                     Node.getSourceRange().getBegin());

  // Blink is part of the Chromium git repo, even though it contains
  // "third_party" in its path.
  if (filename.find("/third_party/blink/") != std::string::npos)
    return false;
  // Otherwise, just check if the paths contains the "third_party" substring.
  // We don't want to rewrite content of such paths even if they are in the main
  // Chromium git repository.
  return filename.find("/third_party/") != std::string::npos;
}

AST_MATCHER(clang::FieldDecl, isInGeneratedLocation) {
  std::string filename = GetFilename(Finder->getASTContext().getSourceManager(),
                                     Node.getSourceRange().getBegin());

  return filename.find("/gen/") != std::string::npos ||
         filename.rfind("gen/", 0) == 0;
}

AST_MATCHER_P(clang::FieldDecl,
              isFieldDeclListedInFilterFile,
              const FilterFile*,
              Filter) {
  return Filter->ContainsLine(Node.getQualifiedNameAsString());
}

AST_MATCHER_P(clang::FieldDecl,
              isInLocationListedInFilterFile,
              const FilterFile*,
              Filter) {
  clang::SourceLocation loc = Node.getSourceRange().getBegin();
  if (loc.isInvalid())
    return false;
  std::string file_path =
      GetFilename(Finder->getASTContext().getSourceManager(), loc);
  return Filter->ContainsSubstringOf(file_path);
}

AST_MATCHER(clang::Decl, isInExternCContext) {
  return Node.getLexicalDeclContext()->isExternCContext();
}

// Given:
//   template <typename T, typename T2> class MyTemplate {};  // Node1 and Node4
//   template <typename T2> class MyTemplate<int, T2> {};     // Node2
//   template <> class MyTemplate<int, char> {};              // Node3
//   void foo() {
//     // This creates implicit template specialization (Node4) out of the
//     // explicit template definition (Node1).
//     MyTemplate<bool, double> v;
//   }
// with the following AST nodes:
//   ClassTemplateDecl MyTemplate                                       - Node1
//   | |-CXXRecordDecl class MyTemplate definition
//   | `-ClassTemplateSpecializationDecl class MyTemplate definition    - Node4
//   ClassTemplatePartialSpecializationDecl class MyTemplate definition - Node2
//   ClassTemplateSpecializationDecl class MyTemplate definition        - Node3
//
// Matches AST node 4, but not AST node2 nor node3.
AST_MATCHER(clang::ClassTemplateSpecializationDecl,
            isImplicitClassTemplateSpecialization) {
  return !Node.isExplicitSpecialization();
}

static bool IsAnnotated(const clang::Decl* decl,
                        const std::string& expected_annotation) {
  clang::AnnotateAttr* attr = decl->getAttr<clang::AnnotateAttr>();
  return attr && (attr->getAnnotation() == expected_annotation);
}

AST_MATCHER(clang::Decl, isRawPtrExclusionAnnotated) {
  return IsAnnotated(&Node, "raw_ptr_exclusion");
}

AST_MATCHER(clang::CXXRecordDecl, isAnonymousStructOrUnion) {
  return Node.getName().empty();
}

// Given:
//   template <typename T, typename T2> void foo(T t, T2 t2) {};  // N1 and N4
//   template <typename T2> void foo<int, T2>(int t, T2 t) {};    // N2
//   template <> void foo<int, char>(int t, char t2) {};          // N3
//   void foo() {
//     // This creates implicit template specialization (N4) out of the
//     // explicit template definition (N1).
//     foo<bool, double>(true, 1.23);
//   }
// with the following AST nodes:
//   FunctionTemplateDecl foo
//   |-FunctionDecl 0x191da68 foo 'void (T, T2)'         // N1
//   `-FunctionDecl 0x194bf08 foo 'void (bool, double)'  // N4
//   FunctionTemplateDecl foo
//   `-FunctionDecl foo 'void (int, T2)'                 // N2
//   FunctionDecl foo 'void (int, char)'                 // N3
//
// Matches AST node N4, but not AST nodes N1, N2 nor N3.
AST_MATCHER(clang::FunctionDecl, isImplicitFunctionTemplateSpecialization) {
  switch (Node.getTemplateSpecializationKind()) {
    case clang::TSK_ImplicitInstantiation:
      return true;
    case clang::TSK_Undeclared:
    case clang::TSK_ExplicitSpecialization:
    case clang::TSK_ExplicitInstantiationDeclaration:
    case clang::TSK_ExplicitInstantiationDefinition:
      return false;
  }
}

// Matches field declarations that do not explicitly appear in the source
// code:
// 1. fields of classes generated by the compiler to back capturing lambdas,
// 2. fields within an implicit class or function template specialization
//    (e.g. when a template is instantiated by a bit of code and there's no
//    explicit specialization for it).
clang::ast_matchers::internal::Matcher<clang::FieldDecl>
ImplicitFieldDeclaration();

// Matches raw pointer field declarations that is a candidate for raw_ptr<T>
// conversion.
clang::ast_matchers::internal::Matcher<clang::Decl> AffectedRawPtrFieldDecl(
    const FilterFile* paths_to_exclude,
    const FilterFile* fields_to_exclude);

// Matches raw reference field declarations that are candidates for raw_ref<T>
// conversion.
clang::ast_matchers::internal::Matcher<clang::Decl> AffectedRawRefFieldDecl(
    const FilterFile* paths_to_exclude,
    const FilterFile* fields_to_exclude);

// If `field_decl` declares a field in an implicit template specialization, then
// finds and returns the corresponding FieldDecl from the template definition.
// Otherwise, just returns the original `field_decl` argument.
const clang::FieldDecl* GetExplicitDecl(const clang::FieldDecl* field_decl);

// Given:
//   template <typename T>
//   class MyTemplate {
//     T field;  // This is an explicit field declaration.
//   };
//   void foo() {
//     // This creates implicit template specialization for MyTemplate,
//     // including an implicit `field` declaration.
//     MyTemplate<int> v;
//     v.field = 123;
//   }
// and
//   innerMatcher that will match the explicit `T field` declaration (but not
//   necessarily the implicit template declarations),
// hasExplicitFieldDecl(innerMatcher) will match both explicit and implicit
// field declarations.
//
// For example, `member_expr_matcher` below will match `v.field` in the example
// above, even though the type of `v.field` is `int`, rather than `T` (matched
// by substTemplateTypeParmType()):
//   auto explicit_field_decl_matcher =
//       fieldDecl(hasType(substTemplateTypeParmType()));
//   auto member_expr_matcher = memberExpr(member(fieldDecl(
//       hasExplicitFieldDecl(explicit_field_decl_matcher))))
AST_MATCHER_P(clang::FieldDecl,
              hasExplicitFieldDecl,
              clang::ast_matchers::internal::Matcher<clang::FieldDecl>,
              InnerMatcher) {
  const clang::FieldDecl* explicit_field_decl = GetExplicitDecl(&Node);
  return InnerMatcher.matches(*explicit_field_decl, Finder, Builder);
}

// If `original_param` declares a parameter in an implicit template
// specialization of a function or method, then finds and returns the
// corresponding ParmVarDecl from the template definition.  Otherwise, just
// returns the `original_param` argument.
//
// Note: nullptr may be returned in rare, unimplemented cases.
const clang::ParmVarDecl* GetExplicitDecl(
    const clang::ParmVarDecl* original_param);

AST_MATCHER_P(clang::ParmVarDecl,
              hasExplicitParmVarDecl,
              clang::ast_matchers::internal::Matcher<clang::ParmVarDecl>,
              InnerMatcher) {
  const clang::ParmVarDecl* explicit_param = GetExplicitDecl(&Node);
  if (!explicit_param) {
    // Rare, unimplemented case - fall back to returning "no match".
    return false;
  }

  return InnerMatcher.matches(*explicit_param, Finder, Builder);
}

// forEachInitExprWithFieldDecl matches InitListExpr if it
// 1) evaluates to a RecordType
// 2) has a InitListExpr + FieldDecl pair that matches the submatcher args.
//
// forEachInitExprWithFieldDecl is based on and very similar to the builtin
// forEachArgumentWithParam matcher.
AST_MATCHER_P2(clang::InitListExpr,
               forEachInitExprWithFieldDecl,
               clang::ast_matchers::internal::Matcher<clang::Expr>,
               init_expr_matcher,
               clang::ast_matchers::internal::Matcher<clang::FieldDecl>,
               field_decl_matcher) {
  const clang::InitListExpr& init_list_expr = Node;
  const clang::Type* type = init_list_expr.getType()
                                .getDesugaredType(Finder->getASTContext())
                                .getTypePtrOrNull();
  if (!type) {
    return false;
  }
  const clang::CXXRecordDecl* record_decl = type->getAsCXXRecordDecl();
  if (!record_decl) {
    return false;
  }

  bool is_matching = false;
  clang::ast_matchers::internal::BoundNodesTreeBuilder result;
  const llvm::SmallVector<const clang::FieldDecl*> field_decls(
      record_decl->fields());
  for (unsigned i = 0; i < init_list_expr.getNumInits(); i++) {
    const clang::Expr* expr = init_list_expr.getInit(i);

    const clang::FieldDecl* field_decl = nullptr;
    if (const clang::ImplicitValueInitExpr* implicit_value_init_expr =
            clang::dyn_cast<clang::ImplicitValueInitExpr>(expr)) {
      continue;  // Do not match implicit value initializers.
    } else if (const clang::DesignatedInitExpr* designated_init_expr =
                   clang::dyn_cast<clang::DesignatedInitExpr>(expr)) {
      // Nested designators are unsupported by C++.
      if (designated_init_expr->size() != 1) {
        break;
      }
      expr = designated_init_expr->getInit();
#if defined(LLVM_FORCE_HEAD_REVISION)
      field_decl = designated_init_expr->getDesignator(0)->getFieldDecl();
#else
      field_decl = designated_init_expr->getDesignator(0)->getField();
#endif
    } else {
      if (i >= field_decls.size()) {
        break;
      }
      field_decl = field_decls[i];
    }

    clang::ast_matchers::internal::BoundNodesTreeBuilder field_matches(
        *Builder);
    if (field_decl_matcher.matches(*field_decl, Finder, &field_matches)) {
      clang::ast_matchers::internal::BoundNodesTreeBuilder expr_matches(
          field_matches);
      if (init_expr_matcher.matches(*expr, Finder, &expr_matches)) {
        result.addMatch(expr_matches);
        is_matching = true;
      }
    }
  }

  *Builder = std::move(result);
  return is_matching;
}

AST_POLYMORPHIC_MATCHER(isInMacroLocation,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(clang::Decl,
                                                        clang::Stmt,
                                                        clang::TypeLoc)) {
  return Node.getBeginLoc().isMacroID();
}

#endif  // TOOLS_CLANG_PLUGINS_RAWPTRHELPERS_H_
