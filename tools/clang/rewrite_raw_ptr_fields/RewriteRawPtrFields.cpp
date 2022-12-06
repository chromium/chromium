// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is implementation of a clang tool that rewrites raw pointer fields into
// raw_ptr<T>:
//     Pointee* field_
// becomes:
//     raw_ptr<Pointee> field_
//
// Note that the tool always emits two kinds of output:
// 1. Fields to exclude:
//    - FilteredExprWriter
// 2. Edit/replacement directives:
//    - FieldDeclRewriter
//    - AffectedExprRewriter
// The rewriter is expected to be used twice, in two passes:
// 1. Output from the 1st pass should be used to generate fields-to-ignore.txt
//    (or to augment the manually created exclusion list file)
// 2. The 2nd pass should use fields-to-ignore.txt from the first pass as input
//    for the --exclude-fields cmdline parameter.  The output from the 2nd pass
//    can be used to perform the actual rewrite via extract_edits.py and
//    apply_edits.py.
//
// For more details, see the doc here:
// https://docs.google.com/document/d/1chTvr3fSofQNV_PDPEHRyUgcJCQBgTDOOBriW9gIm9M

#include <assert.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "RawPtrHelpers.h"
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
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::ast_matchers;

namespace {

// Include path that needs to be added to all the files where raw_ptr<...>
// replaces a raw pointer.
const char kRawPtrIncludePath[] = "base/memory/raw_ptr.h";

// Include path that needs to be added to all the files where raw_ref<...>
// replaces a raw reference.
const char kRawRefIncludePath[] = "base/memory/raw_ref.h";

// Name of a cmdline parameter that can be used to specify a file listing fields
// that should not be rewritten to use raw_ptr<T>.
//
// See also:
// - OutputSectionHelper
// - FilterFile
const char kExcludeFieldsParamName[] = "exclude-fields";

// Name of a cmdline parameter that can be used to specify a file listing
// regular expressions describing paths that should be excluded from the
// rewrite.
//
// See also:
// - PathFilterFile
const char kExcludePathsParamName[] = "exclude-paths";

// OutputSectionHelper helps gather and emit a section of output.
//
// The section of output is delimited in a way that makes it easy to extract it
// with sed like so:
//    $ DELIM = ...
//    $ cat ~/scratch/rewriter.out \
//        | sed '/^==== BEGIN $DELIM ====$/,/^==== END $DELIM ====$/{//!b};d' \
//        | sort | uniq > ~/scratch/some-out-of-band-output.txt
//    (For DELIM="EDITS", there is also tools/clang/scripts/extract_edits.py.)
//
// Each output line is deduped and may be followed by optional comment tags:
//        Some filter # tag1, tag2
//        Another filter # tag1, tag2, tag3
//        An output line with no comment tags
//
// The output lines are sorted.  This helps provide deterministic output (even
// if AST matchers start firing in a different order after benign clang
// changes).
//
// See also:
// - FilterFile
// - OutputHelper
class OutputSectionHelper {
 public:
  explicit OutputSectionHelper(llvm::StringRef output_delimiter)
      : output_delimiter_(output_delimiter.str()) {}

  OutputSectionHelper(const OutputSectionHelper&) = delete;
  OutputSectionHelper& operator=(const OutputSectionHelper&) = delete;

  void Add(llvm::StringRef output_line, llvm::StringRef tag = "") {
    // Look up |tags| associated with |output_line|.  As a side effect of the
    // lookup, |output_line| will be inserted if it wasn't already present in
    // the map.
    llvm::StringSet<>& tags = output_line_to_tags_[output_line];

    if (!tag.empty())
      tags.insert(tag);
  }

  void Emit() {
    if (output_line_to_tags_.empty())
      return;

    llvm::outs() << "==== BEGIN " << output_delimiter_ << " ====\n";
    for (const llvm::StringRef& output_line :
         GetSortedKeys(output_line_to_tags_)) {
      llvm::outs() << output_line;

      const llvm::StringSet<>& tags = output_line_to_tags_[output_line];
      if (!tags.empty()) {
        std::vector<llvm::StringRef> sorted_tags = GetSortedKeys(tags);
        std::string tags_comment =
            llvm::join(sorted_tags.begin(), sorted_tags.end(), ", ");
        llvm::outs() << "  # " << tags_comment;
      }

      llvm::outs() << "\n";
    }
    llvm::outs() << "==== END " << output_delimiter_ << " ====\n";
  }

 private:
  template <typename TValue>
  std::vector<llvm::StringRef> GetSortedKeys(
      const llvm::StringMap<TValue>& map) {
    std::vector<llvm::StringRef> sorted(map.keys().begin(), map.keys().end());
    std::sort(sorted.begin(), sorted.end());
    return sorted;
  }

  std::string output_delimiter_;
  llvm::StringMap<llvm::StringSet<>> output_line_to_tags_;
};

// Output format is documented in //docs/clang_tool_refactoring.md
class OutputHelper : public clang::tooling::SourceFileCallbacks {
 public:
  OutputHelper()
      : edits_helper_("EDITS"), field_decl_filter_helper_("FIELD FILTERS") {}
  ~OutputHelper() = default;

  OutputHelper(const OutputHelper&) = delete;
  OutputHelper& operator=(const OutputHelper&) = delete;

  void AddReplacement(const clang::SourceManager& source_manager,
                      const clang::SourceRange& replacement_range,
                      std::string replacement_text,
                      const char* include_path = nullptr) {
    clang::tooling::Replacement replacement(
        source_manager, clang::CharSourceRange::getCharRange(replacement_range),
        replacement_text);
    llvm::StringRef file_path = replacement.getFilePath();
    if (file_path.empty())
      return;

    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    std::string replacement_directive = llvm::formatv(
        "r:::{0}:::{1}:::{2}:::{3}", file_path, replacement.getOffset(),
        replacement.getLength(), replacement_text);
    edits_helper_.Add(replacement_directive);

    if (include_path) {
      std::string include_directive = llvm::formatv(
          "include-user-header:::{0}:::-1:::-1:::{1}", file_path, include_path);
      edits_helper_.Add(include_directive);
    }
  }

  void AddFilteredField(const clang::FieldDecl& field_decl,
                        llvm::StringRef filter_tag) {
    std::string qualified_name = field_decl.getQualifiedNameAsString();
    field_decl_filter_helper_.Add(qualified_name, filter_tag);
  }

 private:
  // clang::tooling::SourceFileCallbacks override:
  bool handleBeginSource(clang::CompilerInstance& compiler) override {
    const clang::FrontendOptions& frontend_options = compiler.getFrontendOpts();

    assert((frontend_options.Inputs.size() == 1) &&
           "run_tool.py should invoke the rewriter one file at a time");
    const clang::FrontendInputFile& input_file = frontend_options.Inputs[0];
    assert(input_file.isFile() &&
           "run_tool.py should invoke the rewriter on actual files");

    current_language_ = input_file.getKind().getLanguage();

    return true;  // Report that |handleBeginSource| succeeded.
  }

  // clang::tooling::SourceFileCallbacks override:
  void handleEndSource() override {
    if (ShouldSuppressOutput())
      return;

    edits_helper_.Emit();
    field_decl_filter_helper_.Emit();
  }

  bool ShouldSuppressOutput() {
    switch (current_language_) {
      case clang::Language::Unknown:
      case clang::Language::Asm:
      case clang::Language::LLVM_IR:
      case clang::Language::OpenCL:
      case clang::Language::CUDA:
      case clang::Language::RenderScript:
      case clang::Language::HIP:
      case clang::Language::HLSL:
        // Rewriter can't handle rewriting the current input language.
        return true;

      case clang::Language::C:
      case clang::Language::ObjC:
        // raw_ptr<T> requires C++.  In particular, attempting to #include
        // "base/memory/raw_ptr.h" from C-only compilation units will lead
        // to compilation errors.
        return true;

      case clang::Language::CXX:
      case clang::Language::OpenCLCXX:
      case clang::Language::ObjCXX:
        return false;
    }

    assert(false && "Unrecognized clang::Language");
    return true;
  }

  OutputSectionHelper edits_helper_;
  OutputSectionHelper field_decl_filter_helper_;
  clang::Language current_language_ = clang::Language::Unknown;
};

// Matches CXXRecordDecls that are classified as trivial:
// https://en.cppreference.com/w/cpp/named_req/TrivialType
AST_MATCHER(clang::CXXRecordDecl, isTrivial) {
  return Node.isTrivial();
}

AST_POLYMORPHIC_MATCHER(isInMacroLocation,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(clang::Decl,
                                                        clang::Stmt,
                                                        clang::TypeLoc)) {
  return Node.getBeginLoc().isMacroID();
}

// If |field_decl| declares a field in an implicit template specialization, then
// finds and returns the corresponding FieldDecl from the template definition.
// Otherwise, just returns the original |field_decl| argument.
const clang::FieldDecl* GetExplicitDecl(const clang::FieldDecl* field_decl) {
  if (field_decl->isAnonymousStructOrUnion())
    return field_decl;  // Safe fallback - |field_decl| is not a pointer field.

  const clang::CXXRecordDecl* record_decl =
      clang::dyn_cast<clang::CXXRecordDecl>(field_decl->getParent());
  if (!record_decl)
    return field_decl;  // Non-C++ records are never template instantiations.

  const clang::CXXRecordDecl* pattern_decl =
      record_decl->getTemplateInstantiationPattern();
  if (!pattern_decl)
    return field_decl;  // |pattern_decl| is not a template instantiation.

  if (record_decl->getTemplateSpecializationKind() !=
      clang::TemplateSpecializationKind::TSK_ImplicitInstantiation) {
    return field_decl;  // |field_decl| was in an *explicit* specialization.
  }

  // Find the field decl with the same name in |pattern_decl|.
  clang::DeclContextLookupResult lookup_result =
      pattern_decl->lookup(field_decl->getDeclName());
  assert(!lookup_result.empty());
  const clang::NamedDecl* found_decl = lookup_result.front();
  assert(found_decl);
  field_decl = clang::dyn_cast<clang::FieldDecl>(found_decl);
  assert(field_decl);
  return field_decl;
}

// Given:
//   template <typename T>
//   class MyTemplate {
//     T field;  // This is an explicit field declaration.
//   };
//   void foo() {
//     // This creates implicit template specialization for MyTemplate,
//     // including an implicit |field| declaration.
//     MyTemplate<int> v;
//     v.field = 123;
//   }
// and
//   innerMatcher that will match the explicit |T field| declaration (but not
//   necessarily the implicit template declarations),
// hasExplicitFieldDecl(innerMatcher) will match both explicit and implicit
// field declarations.
//
// For example, |member_expr_matcher| below will match |v.field| in the example
// above, even though the type of |v.field| is |int|, rather than |T| (matched
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

// If |original_param| declares a parameter in an implicit template
// specialization of a function or method, then finds and returns the
// corresponding ParmVarDecl from the template definition.  Otherwise, just
// returns the |original_param| argument.
//
// Note: nullptr may be returned in rare, unimplemented cases.
const clang::ParmVarDecl* GetExplicitDecl(
    const clang::ParmVarDecl* original_param) {
  const clang::FunctionDecl* original_func =
      clang::dyn_cast<clang::FunctionDecl>(original_param->getDeclContext());
  if (!original_func) {
    // |!original_func| may happen when the ParmVarDecl is part of a
    // FunctionType, but not part of a FunctionDecl:
    //     base::RepeatingCallback<void(int parm_var_decl_here)>
    //
    // In theory, |parm_var_decl_here| can also represent an implicit template
    // specialization in this scenario.  OTOH, it should be rare + shouldn't
    // matter for this rewriter, so for now let's just return the
    // |original_param|.
    //
    // TODO: Implement support for this scenario.
    return nullptr;
  }

  const clang::FunctionDecl* pattern_func =
      original_func->getTemplateInstantiationPattern();
  if (!pattern_func) {
    // |original_func| is not a template instantiation - return the
    // |original_param|.
    return original_param;
  }

  // See if |pattern_func| has a parameter that is a template parameter pack.
  bool has_param_pack = false;
  unsigned int index_of_param_pack = std::numeric_limits<unsigned int>::max();
  for (unsigned int i = 0; i < pattern_func->getNumParams(); i++) {
    const clang::ParmVarDecl* pattern_param = pattern_func->getParamDecl(i);
    if (!pattern_param->isParameterPack())
      continue;

    if (has_param_pack) {
      // TODO: Implement support for multiple parameter packs.
      return nullptr;
    }

    has_param_pack = true;
    index_of_param_pack = i;
  }

  // Find and return the corresponding ParmVarDecl from |pattern_func|.
  unsigned int original_index = original_param->getFunctionScopeIndex();
  unsigned int pattern_index = std::numeric_limits<unsigned int>::max();
  if (!has_param_pack) {
    pattern_index = original_index;
  } else {
    // |original_func| has parameters that look like this:
    //     l1, l2, l3, p1, p2, p3, t1, t2, t3
    // where
    //     lN is a leading, non-pack parameter
    //     pN is an expansion of a template parameter pack
    //     tN is a trailing, non-pack parameter
    // Using the knowledge above, let's adjust |pattern_index| as needed.
    unsigned int leading_param_num = index_of_param_pack;  // How many |lN|.
    unsigned int pack_expansion_num =  // How many |pN| above.
        original_func->getNumParams() - pattern_func->getNumParams() + 1;
    if (original_index < leading_param_num) {
      // |original_param| is a leading, non-pack parameter.
      pattern_index = original_index;
    } else if (leading_param_num <= original_index &&
               original_index < (leading_param_num + pack_expansion_num)) {
      // |original_param| is an expansion of a template pack parameter.
      pattern_index = index_of_param_pack;
    } else if ((leading_param_num + pack_expansion_num) <= original_index) {
      // |original_param| is a trailing, non-pack parameter.
      pattern_index = original_index - pack_expansion_num + 1;
    }
  }
  assert(pattern_index < pattern_func->getNumParams());
  return pattern_func->getParamDecl(pattern_index);
}

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

// Returns |true| if and only if:
// 1. |a| and |b| are in the same file (e.g. |false| is returned if any location
//    is within macro scratch space or a similar location;  similarly |false| is
//    returned if |a| and |b| are in different files).
// 2. |a| and |b| overlap.
bool IsOverlapping(const clang::SourceManager& source_manager,
                   const clang::SourceRange& a,
                   const clang::SourceRange& b) {
  clang::FullSourceLoc a1(a.getBegin(), source_manager);
  clang::FullSourceLoc a2(a.getEnd(), source_manager);
  clang::FullSourceLoc b1(b.getBegin(), source_manager);
  clang::FullSourceLoc b2(b.getEnd(), source_manager);

  // Are all locations in a file?
  if (!a1.isFileID() || !a2.isFileID() || !b1.isFileID() || !b2.isFileID())
    return false;

  // Are all locations in the same file?
  if (a1.getFileID() != a2.getFileID() || a2.getFileID() != b1.getFileID() ||
      b1.getFileID() != b2.getFileID()) {
    return false;
  }

  // Check the 2 cases below:
  // 1. A: |============|
  //    B:      |===============|
  //       a1   b1      a2      b2
  // or
  // 2. A: |====================|
  //    B:      |=======|
  //       a1   b1      b2      a2
  bool b1_is_inside_a_range = a1.getFileOffset() <= b1.getFileOffset() &&
                              b1.getFileOffset() <= a2.getFileOffset();

  // Check the 2 cases below:
  // 1. B: |============|
  //    A:      |===============|
  //       b1   a1      b2      a2
  // or
  // 2. B: |====================|
  //    A:      |=======|
  //       b1   a1      a2      b2
  bool a1_is_inside_b_range = b1.getFileOffset() <= a1.getFileOffset() &&
                              a1.getFileOffset() <= b2.getFileOffset();

  return b1_is_inside_a_range || a1_is_inside_b_range;
}

// Matcher for FieldDecl that has a SourceRange that overlaps other declarations
// within the parent RecordDecl.
//
// Given
//   struct MyStruct {
//     int f;
//     int f2, f3;
//     struct S { int x } f4;
//   };
// - doesn't match |f|
// - matches |f2| and |f3| (which overlap each other's location)
// - matches |f4| (which overlaps the location of |S|)
AST_MATCHER(clang::FieldDecl, overlapsOtherDeclsWithinRecordDecl) {
  const clang::FieldDecl& self = Node;
  const clang::SourceManager& source_manager =
      Finder->getASTContext().getSourceManager();

  const clang::RecordDecl* record_decl = self.getParent();
  if (!record_decl)
    return false;

  clang::SourceRange self_range(self.getBeginLoc(), self.getEndLoc());

  auto is_overlapping_sibling = [&](const clang::Decl* other_decl) {
    if (other_decl == &self)
      return false;

    clang::SourceRange other_range(other_decl->getBeginLoc(),
                                   other_decl->getEndLoc());
    return IsOverlapping(source_manager, self_range, other_range);
  };
  bool has_sibling_with_overlapping_location =
      std::any_of(record_decl->decls_begin(), record_decl->decls_end(),
                  is_overlapping_sibling);
  return has_sibling_with_overlapping_location;
}

// Matches clang::Type if
// 1) it represents a RecordDecl with a FieldDecl that matches the InnerMatcher
//    (*all* such FieldDecls will be matched)
// or
// 2) it represents an array or a RecordDecl that nests the case #1
//    (this recurses to any depth).
AST_MATCHER_P(clang::QualType,
              typeWithEmbeddedFieldDecl,
              clang::ast_matchers::internal::Matcher<clang::FieldDecl>,
              InnerMatcher) {
  const clang::Type* type =
      Node.getDesugaredType(Finder->getASTContext()).getTypePtrOrNull();
  if (!type)
    return false;

  if (const clang::CXXRecordDecl* record_decl = type->getAsCXXRecordDecl()) {
    auto matcher = recordDecl(forEach(fieldDecl(hasExplicitFieldDecl(anyOf(
        InnerMatcher, hasType(typeWithEmbeddedFieldDecl(InnerMatcher)))))));
    return matcher.matches(*record_decl, Finder, Builder);
  }

  if (type->isArrayType()) {
    const clang::ArrayType* array_type =
        Finder->getASTContext().getAsArrayType(Node);
    auto matcher = typeWithEmbeddedFieldDecl(InnerMatcher);
    return matcher.matches(array_type->getElementType(), Finder, Builder);
  }

  return false;
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
  if (!type)
    return false;
  const clang::CXXRecordDecl* record_decl = type->getAsCXXRecordDecl();
  if (!record_decl)
    return false;

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
      if (designated_init_expr->size() != 1)
        break;
      expr = designated_init_expr->getInit();
      field_decl = designated_init_expr->getDesignator(0)->getField();
    } else {
      if (i >= field_decls.size())
        break;
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

class FieldDeclRewriter : public MatchFinder::MatchCallback {
 public:
  explicit FieldDeclRewriter(OutputHelper* output_helper,
                             const char* format_string,
                             const char* include_path)
      : output_helper_(output_helper),
        format_string_(format_string),
        include_path_(include_path) {}

  FieldDeclRewriter(const FieldDeclRewriter&) = delete;
  FieldDeclRewriter& operator=(const FieldDeclRewriter&) = delete;

  virtual bool earlyExit(const MatchFinder::MatchResult& result) const = 0;

  void run(const MatchFinder::MatchResult& result) override {
    if (earlyExit(result)) {
      return;
    }
    const clang::ASTContext& ast_context = *result.Context;
    const clang::SourceManager& source_manager = *result.SourceManager;

    const clang::FieldDecl* field_decl =
        result.Nodes.getNodeAs<clang::FieldDecl>("affectedFieldDecl");

    assert(field_decl && "matcher should bind 'fieldDecl'");

    const clang::TypeSourceInfo* type_source_info =
        field_decl->getTypeSourceInfo();
    if (auto* ivar_decl = clang::dyn_cast<clang::ObjCIvarDecl>(field_decl)) {
      // Objective-C @synthesize statements should not be rewritten. They
      // return null for getTypeSourceInfo().
      if (ivar_decl->getSynthesize()) {
        assert(!type_source_info);
        return;
      }
    }
    assert(type_source_info && "assuming |type_source_info| is always present");

    clang::QualType pointer_type = type_source_info->getType();

    // Calculate the |replacement_range|.
    //
    // Consider the following example:
    //      const Pointee* const field_name_;
    //      ^--------------------^  = |replacement_range|
    //                           ^  = |field_decl->getLocation()|
    //      ^                       = |field_decl->getBeginLoc()|
    //                   ^          = PointerTypeLoc::getStarLoc
    //            ^------^          = TypeLoc::getSourceRange
    //
    // We get the |replacement_range| in a bit clumsy way, because clang docs
    // for QualifiedTypeLoc explicitly say that these objects "intentionally
    // do not provide source location for type qualifiers".
    clang::SourceRange replacement_range(field_decl->getBeginLoc(),
                                         field_decl->getLocation());

    // Calculate |replacement_text|.
    std::string replacement_text = GenerateNewText(ast_context, pointer_type);
    if (field_decl->isMutable())
      replacement_text.insert(0, "mutable ");

    // Generate and print a replacement.
    output_helper_->AddReplacement(source_manager, replacement_range,
                                   replacement_text, include_path_);
  }

 private:
  std::string GenerateNewText(const clang::ASTContext& ast_context,
                              const clang::QualType& pointer_type) {
    std::string result;

    clang::QualType pointee_type = pointer_type->getPointeeType();

    // Preserve qualifiers.
    assert(
        !pointer_type.isRestrictQualified() &&
        "|restrict| is a C-only qualifier and raw_ptr<T>/raw_ref<T> need C++");
    if (pointer_type.isConstQualified())
      result += "const ";
    if (pointer_type.isVolatileQualified())
      result += "volatile ";

    // Convert pointee type to string.
    clang::PrintingPolicy printing_policy(ast_context.getLangOpts());
    printing_policy.SuppressScope = 1;  // s/blink::Pointee/Pointee/
    std::string pointee_type_as_string =
        pointee_type.getAsString(printing_policy);
    result += llvm::formatv(format_string_, pointee_type_as_string);

    return result;
  }

  OutputHelper* const output_helper_;
  const char* format_string_;
  const char* include_path_;
};

class AffectedExprRewriter : public MatchFinder::MatchCallback {
 public:
  explicit AffectedExprRewriter(
      OutputHelper* output_helper,
      std::function<std::pair<clang::SourceRange, std::string>(
          const MatchFinder::MatchResult&)> fct)
      : output_helper_(output_helper), getRangeAndText_(fct) {}

  AffectedExprRewriter(const AffectedExprRewriter&) = delete;
  AffectedExprRewriter& operator=(const AffectedExprRewriter&) = delete;

  void run(const MatchFinder::MatchResult& result) override {
    const clang::SourceManager& source_manager = *result.SourceManager;

    auto [replacement_range, text] = getRangeAndText_(result);
    output_helper_->AddReplacement(source_manager, replacement_range,
                                   text.c_str());
  }

 private:
  OutputHelper* const output_helper_;
  std::function<std::pair<clang::SourceRange, std::string>(
      const MatchFinder::MatchResult&)>
      getRangeAndText_;
};

// Emits problematic fields (matched as "affectedFieldDecl") as filtered fields.
class FilteredExprWriter : public MatchFinder::MatchCallback {
 public:
  FilteredExprWriter(OutputHelper* output_helper, llvm::StringRef filter_tag)
      : output_helper_(output_helper), filter_tag_(filter_tag) {}

  FilteredExprWriter(const FilteredExprWriter&) = delete;
  FilteredExprWriter& operator=(const FilteredExprWriter&) = delete;

  void run(const MatchFinder::MatchResult& result) override {
    const clang::FieldDecl* field_decl =
        result.Nodes.getNodeAs<clang::FieldDecl>("affectedFieldDecl");
    assert(field_decl && "matcher should bind 'affectedFieldDecl'");

    output_helper_->AddFilteredField(*field_decl, filter_tag_);
  }

 private:
  OutputHelper* const output_helper_;
  llvm::StringRef filter_tag_;
};

AST_MATCHER(clang::CXXRecordDecl, isAnonymousStructOrUnion) {
  return Node.getName().empty();
}

class RawPtrRewriter {
 public:
  RawPtrRewriter(OutputHelper* output_helper,
                 MatchFinder& finder,
                 FilterFile& fields_to_exclude,
                 FilterFile& paths_to_exclude)
      : match_finder(finder),
        field_decl_rewriter(output_helper, "raw_ptr<{0}> ", kRawPtrIncludePath),
        affected_expr_rewriter(output_helper, getRangeAndText_),
        filtered_addr_of_expr_writer(output_helper, "addr-of"),
        filtered_in_out_ref_arg_writer(output_helper, "in-out-param-ref"),
        overlapping_field_decl_writer(output_helper, "overlapping"),
        constexpr_ctor_field_initializer_writer(
            output_helper,
            "constexpr-ctor-field-initializer"),
        constexpr_var_initializer_writer(output_helper,
                                         "constexpr-var-initializer"),
        macro_field_decl_writer(output_helper, "macro"),
        global_scope_rewriter(output_helper, "global-scope"),
        union_field_decl_writer(output_helper, "union"),
        reinterpret_cast_struct_writer(output_helper,
                                       "reinterpret-cast-trivial-type"),
        fields_to_exclude(fields_to_exclude),
        paths_to_exclude(paths_to_exclude) {}

  void addMatchers() {
    auto field_decl_matcher =
        AffectedRawPtrFieldDecl(&paths_to_exclude, &fields_to_exclude);

    match_finder.addMatcher(field_decl_matcher, &field_decl_rewriter);

    // Matches expressions that used to return a value of type |SomeClass*|
    // but after the rewrite return an instance of |raw_ptr<SomeClass>|.
    // Many such expressions might need additional changes after the rewrite:
    // - Some expressions (printf args, const_cast args, etc.) might need
    // |.get()|
    //   appended.
    // - Using such expressions in specific contexts (e.g. as in-out arguments
    // or
    //   as a return value of a function returning references) may require
    //   additional work and should cause related fields to be emitted as
    //   candidates for the --field-filter-file parameter.
    auto affected_member_expr_matcher =
        memberExpr(member(fieldDecl(hasExplicitFieldDecl(field_decl_matcher))))
            .bind("affectedMemberExpr");
    auto affected_expr_matcher = ignoringImplicit(affected_member_expr_matcher);

    // Places where |.get()| needs to be appended =========
    // Given
    //   void foo(const S& s) {
    //     printf("%p", s.y);
    //     const_cast<...>(s.y)
    //     reinterpret_cast<...>(s.y)
    //   }
    // matches the |s.y| expr if it matches the |affected_expr_matcher| above.
    //
    // See also testcases in tests/affected-expr-original.cc
    auto affected_expr_that_needs_fixing_matcher = expr(allOf(
        affected_expr_matcher,
        hasParent(expr(anyOf(callExpr(callee(functionDecl(isVariadic()))),
                             cxxConstCastExpr(), cxxReinterpretCastExpr())))));

    match_finder.addMatcher(affected_expr_that_needs_fixing_matcher,
                            &affected_expr_rewriter);

    // Affected ternary operator args =========
    // Given
    //   void foo(const S& s) {
    //     cond ? s.y : ...
    //   }
    // binds the |s.y| expr if it matches the |affected_expr_matcher| above.
    //
    // See also testcases in tests/affected-expr-original.cc
    auto affected_ternary_operator_arg_matcher =
        conditionalOperator(eachOf(hasTrueExpression(affected_expr_matcher),
                                   hasFalseExpression(affected_expr_matcher)));
    match_finder.addMatcher(affected_ternary_operator_arg_matcher,
                            &affected_expr_rewriter);

    // Affected string binary operator =========
    // Given
    //   struct S { const char* y; }
    //   void foo(const S& s) {
    //     std::string other;
    //     bool v1 = s.y == other;
    //     std::string v2 = s.y + other;
    //   }
    // binds the |s.y| expr if it matches the |affected_expr_matcher| above.
    //
    // See also testcases in tests/affected-expr-original.cc
    auto std_string_expr_matcher =
        expr(hasType(cxxRecordDecl(hasName("::std::basic_string"))));
    auto affected_string_binary_operator_arg_matcher = cxxOperatorCallExpr(
        hasAnyOverloadedOperatorName("+", "==", "!=", "<", "<=", ">", ">="),
        hasAnyArgument(std_string_expr_matcher),
        forEachArgumentWithParam(affected_expr_matcher, parmVarDecl()));
    match_finder.addMatcher(affected_string_binary_operator_arg_matcher,
                            &affected_expr_rewriter);

    // Calls to templated functions =========
    // Given
    //   struct S { int* y; };
    //   template <typename T>
    //   void templatedFunc(T* arg) {}
    //   void foo(const S& s) {
    //     templatedFunc(s.y);
    //   }
    // binds the |s.y| expr if it matches the |affected_expr_matcher| above.
    //
    // See also testcases in tests/affected-expr-original.cc
    auto templated_function_arg_matcher = forEachArgumentWithParam(
        affected_expr_matcher,
        parmVarDecl(allOf(
            hasType(
                qualType(allOf(findAll(qualType(substTemplateTypeParmType())),
                               unless(referenceType())))),
            unless(hasAncestor(functionDecl(hasName("Unretained")))))));
    match_finder.addMatcher(callExpr(templated_function_arg_matcher),
                            &affected_expr_rewriter);
    // TODO(lukasza): It is unclear why |traverse| below is needed.  Maybe it
    // can be removed if https://bugs.llvm.org/show_bug.cgi?id=46287 is fixed.
    match_finder.addMatcher(
        traverse(clang::TraversalKind::TK_AsIs,
                 cxxConstructExpr(templated_function_arg_matcher)),
        &affected_expr_rewriter);

    // Calls to constructors via an implicit cast =========
    // Given
    //   struct I { I(int*) {} };
    //   void bar(I i) {}
    //   struct S { int* y; };
    //   void foo(const S& s) {
    //     bar(s.y);  // implicit cast from |s.y| to I.
    //   }
    // binds the |s.y| expr if it matches the |affected_expr_matcher| above.
    //
    // See also testcases in tests/affected-expr-original.cc
    auto implicit_ctor_expr_matcher = cxxConstructExpr(
        allOf(anyOf(hasParent(materializeTemporaryExpr()),
                    hasParent(implicitCastExpr())),
              hasDeclaration(cxxConstructorDecl(
                  allOf(parameterCountIs(1), unless(isExplicit())))),
              forEachArgumentWithParam(affected_expr_matcher, parmVarDecl())));
    match_finder.addMatcher(implicit_ctor_expr_matcher,
                            &affected_expr_rewriter);

    // |auto| type declarations =========
    // Given
    //   struct S { int* y; };
    //   void foo(const S& s) {
    //     auto* p = s.y;
    //   }
    // binds the |s.y| expr if it matches the |affected_expr_matcher| above.
    //
    // See also testcases in tests/affected-expr-original.cc
    auto auto_var_decl_matcher = declStmt(forEach(
        varDecl(allOf(hasType(pointerType(pointee(autoType()))),
                      hasInitializer(anyOf(
                          affected_expr_matcher,
                          initListExpr(hasInit(0, affected_expr_matcher))))))));
    match_finder.addMatcher(auto_var_decl_matcher, &affected_expr_rewriter);

    // address-of(affected-expr) =========
    // Given
    //   ... &s.y ...
    // matches the |s.y| expr if it matches the |affected_member_expr_matcher|
    // above.
    //
    // See also the testcases in tests/gen-in-out-arg-test.cc.
    auto affected_addr_of_expr_matcher = expr(allOf(
        affected_expr_matcher, hasParent(unaryOperator(hasOperatorName("&")))));

    match_finder.addMatcher(affected_addr_of_expr_matcher,
                            &filtered_addr_of_expr_writer);

    // in-out reference arg =========
    // Given
    //   struct S { SomeClass* ptr_field; };
    //   void f(SomeClass*& in_out_arg) { ... }
    //   template <typename T> void f2(T&& rvalue_ref_arg) { ... }
    //   template <typename... Ts> void f3(Ts&&... rvalue_ref_args) { ... }
    //   void bar() {
    //     S s;
    //     foo(s.ptr_field)
    //   }
    // matches the |s.ptr_field| expr if it matches the
    // |affected_member_expr_matcher| and is passed as a function argument that
    // has |FooBar*&| type (like |f|, but unlike |f2| and |f3|).
    //
    // See also the testcases in tests/gen-in-out-arg-test.cc.
    auto affected_in_out_ref_arg_matcher = callExpr(forEachArgumentWithParam(
        affected_expr_matcher, hasExplicitParmVarDecl(hasType(qualType(
                                   allOf(referenceType(pointee(pointerType())),
                                         unless(rValueReferenceType())))))));

    match_finder.addMatcher(affected_in_out_ref_arg_matcher,
                            &filtered_in_out_ref_arg_writer);

    // See the doc comment for the overlapsOtherDeclsWithinRecordDecl matcher
    // and the testcases in tests/gen-overlapping-test.cc.
    auto overlapping_field_decl_matcher = fieldDecl(
        allOf(field_decl_matcher, overlapsOtherDeclsWithinRecordDecl()));

    match_finder.addMatcher(overlapping_field_decl_matcher,
                            &overlapping_field_decl_writer);

    // Matches fields initialized with a non-nullptr value in a constexpr
    // constructor.  See also the testcase in tests/gen-constexpr-test.cc.
    auto non_nullptr_expr_matcher =
        expr(unless(ignoringImplicit(cxxNullPtrLiteralExpr())));
    auto constexpr_ctor_field_initializer_matcher = cxxConstructorDecl(
        allOf(isConstexpr(), unless(isImplicit()),
              forEachConstructorInitializer(
                  allOf(forField(field_decl_matcher),
                        withInitializer(non_nullptr_expr_matcher)))));

    match_finder.addMatcher(constexpr_ctor_field_initializer_matcher,
                            &constexpr_ctor_field_initializer_writer);

    // Matches constexpr initializer list expressions that initialize a
    // rewritable field with a non-nullptr value.  For more details and
    // rationale see the testcases in tests/gen-constexpr-test.cc.
    auto constexpr_var_initializer_matcher = varDecl(
        allOf(isConstexpr(),
              hasInitializer(findAll(initListExpr(forEachInitExprWithFieldDecl(
                  non_nullptr_expr_matcher,
                  hasExplicitFieldDecl(field_decl_matcher)))))));

    match_finder.addMatcher(constexpr_var_initializer_matcher,
                            &constexpr_var_initializer_writer);

    // See the doc comment for the isInMacroLocation matcher
    // and the testcases in tests/gen-macros-test.cc.
    auto macro_field_decl_matcher =
        fieldDecl(allOf(field_decl_matcher, isInMacroLocation()));

    match_finder.addMatcher(macro_field_decl_matcher, &macro_field_decl_writer);

    // See the testcases in tests/gen-global-scope-test.cc.
    auto global_scope_matcher =
        varDecl(allOf(hasGlobalStorage(),
                      hasType(typeWithEmbeddedFieldDecl(field_decl_matcher))));

    match_finder.addMatcher(global_scope_matcher, &global_scope_rewriter);

    // Matches fields in unions (both directly rewritable fields as well as
    // union fields that embed a struct that contains a rewritable field).  See
    // also the testcases in tests/gen-unions-test.cc.
    auto union_field_decl_matcher = recordDecl(allOf(
        isUnion(), forEach(fieldDecl(anyOf(field_decl_matcher,
                                           hasType(typeWithEmbeddedFieldDecl(
                                               field_decl_matcher)))))));

    match_finder.addMatcher(union_field_decl_matcher, &union_field_decl_writer);

    // Matches rewritable fields of struct `SomeStruct` if that struct happens
    // to be a destination type of a `reinterpret_cast<SomeStruct*>` cast and is
    // a trivial type (otherwise `reinterpret_cast<SomeStruct*>` wouldn't be
    // valid before the rewrite if it skipped non-trivial constructors).
    auto reinterpret_cast_struct_matcher =
        cxxReinterpretCastExpr(hasDestinationType(pointerType(pointee(
            hasUnqualifiedDesugaredType(recordType(hasDeclaration(cxxRecordDecl(
                allOf(forEach(field_decl_matcher), isTrivial())))))))));

    match_finder.addMatcher(reinterpret_cast_struct_matcher,
                            &reinterpret_cast_struct_writer);
  }

 private:
  // Rewrites |SomeClass* field| (matched as "affectedFieldDecl") into
  // |raw_ptr<SomeClass> field| and for each file rewritten in such way adds an
  // |#include "base/memory/raw_ptr.h"|.
  class RawPtrFieldDeclRewriter : public FieldDeclRewriter {
   public:
    explicit RawPtrFieldDeclRewriter(OutputHelper* output_helper,
                                     const char* format_string,
                                     const char* include_path)
        : FieldDeclRewriter(output_helper, format_string, include_path) {}

    bool earlyExit(const MatchFinder::MatchResult& result) const override {
      return false;
    }
  };
  // Rewrites |my_struct.ptr_field| (matched as "affectedMemberExpr") into
  // |my_struct.ptr_field.get()|.
  std::function<std::pair<clang::SourceRange, std::string>(
      const MatchFinder::MatchResult&)>
      getRangeAndText_ = [](const MatchFinder::MatchResult& result)
      -> std::pair<clang::SourceRange, std::string> {
    const clang::MemberExpr* member_expr =
        result.Nodes.getNodeAs<clang::MemberExpr>("affectedMemberExpr");
    assert(member_expr && "matcher should bind 'affectedMemberExpr'");

    clang::SourceLocation member_name_start = member_expr->getMemberLoc();
    size_t member_name_length = member_expr->getMemberDecl()->getName().size();
    clang::SourceLocation insertion_loc =
        member_name_start.getLocWithOffset(member_name_length);

    clang::SourceRange replacement_range(insertion_loc, insertion_loc);
    return {replacement_range, ".get()"};
  };
  MatchFinder& match_finder;
  RawPtrFieldDeclRewriter field_decl_rewriter;
  AffectedExprRewriter affected_expr_rewriter;
  FilteredExprWriter filtered_addr_of_expr_writer;
  FilteredExprWriter filtered_in_out_ref_arg_writer;
  FilteredExprWriter overlapping_field_decl_writer;
  FilteredExprWriter constexpr_ctor_field_initializer_writer;
  FilteredExprWriter constexpr_var_initializer_writer;
  FilteredExprWriter macro_field_decl_writer;
  FilteredExprWriter global_scope_rewriter;
  FilteredExprWriter union_field_decl_writer;
  FilteredExprWriter reinterpret_cast_struct_writer;
  FilterFile& fields_to_exclude;
  FilterFile& paths_to_exclude;
};

class RawRefRewriter {
 public:
  RawRefRewriter(OutputHelper* output_helper,
                 MatchFinder& finder,
                 FilterFile& fields_to_exclude,
                 FilterFile& paths_to_exclude)
      : match_finder(finder),
        field_decl_rewriter(output_helper,
                            "const raw_ref<{0}> ",
                            kRawRefIncludePath),
        affected_expr_operator_rewriter(output_helper,
                                        affectedMemberExprOperatorFct_),
        affected_expr_rewriter(output_helper, affectedMemberExprFct_),
        affected_expr_rewriter_withParentheses(output_helper,
                                               affectedMemberExprWithParenFct_),
        global_scope_rewriter(output_helper, "global-scope"),
        overlapping_field_decl_writer(output_helper, "overlapping"),
        constexpr_ctor_field_initializer_writer(
            output_helper,
            "constexpr-ctor-field-initializer"),
        constexpr_var_initializer_writer(output_helper,
                                         "constexpr-var-initializer"),
        macro_field_decl_writer(output_helper, "macro"),
        fields_to_exclude(fields_to_exclude),
        paths_to_exclude(paths_to_exclude) {}

  void addMatchers() {
    auto anonymous_struct_field_decl_matcher =
        fieldDecl(hasParent(cxxRecordDecl(isAnonymousStructOrUnion())));

    // Field declarations =========
    // Given
    //   struct S {
    //     int& y;
    //   };
    // matches |int& y|.  Doesn't match:
    // - non-reference types
    // - fields of lambda-supporting classes
    // - fields listed in the --exclude-fields cmdline param or located in paths
    //   matched by --exclude-paths cmdline param
    // - "implicit" fields (i.e. field decls that are not explicitly present in
    //   the source code)
    // - fields in anonymous structs.

    auto field_decl_matcher =
        fieldDecl(allOf(has(referenceTypeLoc().bind("affectedFieldDeclType")),
                        unless(anyOf(
                            isExpansionInSystemHeader(), isInExternCContext(),
                            isInThirdPartyLocation(), isInGeneratedLocation(),
                            isInLocationListedInFilterFile(&paths_to_exclude),
                            isFieldDeclListedInFilterFile(&fields_to_exclude),
                            ImplicitFieldDeclaration(),
                            anonymous_struct_field_decl_matcher))))
            .bind("affectedFieldDecl");

    match_finder.addMatcher(field_decl_matcher, &field_decl_rewriter);

    // Matches expressions of the form |someClass.ref_field.sub_member| which
    // should be rewritten as |someClass.ref_field->sub_member| as we can't
    // overload `operator.` in C++.
    auto affected_member_expr_operator_matcher =
        expr(
            anyOf(memberExpr(has(memberExpr(member(
                      fieldDecl(hasExplicitFieldDecl(field_decl_matcher)))))),
                  memberExpr(has(implicitCastExpr(has(memberExpr(member(
                      fieldDecl(hasExplicitFieldDecl(field_decl_matcher)))))))),
                  cxxDependentScopeMemberExpr(has(memberExpr(member(
                      fieldDecl(hasExplicitFieldDecl(field_decl_matcher))))))))
            .bind("affectedMemberExprOperator");

    match_finder.addMatcher(affected_member_expr_operator_matcher,
                            &affected_expr_operator_rewriter);

    // Matches expressions that used to have |SomeType&| as return type and
    // became |const raw_ref<SomeType>| after the rewrite.
    auto affected_member_expr =
        memberExpr(
            member(fieldDecl(hasExplicitFieldDecl(field_decl_matcher))),
            unless(anyOf(
                hasParent(memberExpr()),
                hasParent(implicitCastExpr(hasParent(memberExpr()))),
                hasParent(cxxDependentScopeMemberExpr()),
                hasParent(varDecl(unless(
                    anyOf(hasType(referenceType(pointee(autoType()))),
                          hasParent(declStmt(hasParent(cxxForRangeStmt()))))))),
                hasAncestor(cxxConstructorDecl(isDefaulted())),
                hasParent(cxxOperatorCallExpr()),
                hasParent(arraySubscriptExpr()),
                hasParent(callExpr(callee(
                    fieldDecl(hasExplicitFieldDecl(field_decl_matcher))))))))
            .bind("affectedMemberExpr");

    match_finder.addMatcher(affected_member_expr, &affected_expr_rewriter);

    auto affected_member_expr_matcher =
        memberExpr(member(fieldDecl(hasExplicitFieldDecl(field_decl_matcher))))
            .bind("affectedMemberExpr");

    // Calls to constructors via an implicit cast =========
    // Given
    //   struct I { I(int&) {} };
    //   void bar(I i) {}
    //   struct S { int& y; };
    //   void foo(const S& s) {
    //     bar(s.y);  // implicit cast from |s.y| to I.
    //   }
    // binds the |s.y| expr if it matches the |affected_expr_matcher| above.
    //
    // See also testcases in tests/affected-expr-original.cc
    auto implicit_ctor_expr_matcher = cxxConstructExpr(allOf(
        anyOf(hasParent(materializeTemporaryExpr()),
              hasParent(implicitCastExpr())),
        hasDeclaration(cxxConstructorDecl(
            allOf(parameterCountIs(1), unless(isExplicit())))),
        forEachArgumentWithParam(affected_member_expr_matcher, parmVarDecl())));
    match_finder.addMatcher(implicit_ctor_expr_matcher,
                            &affected_expr_rewriter);

    // |auto| type declarations =========
    // Given
    //   struct S { int& y; };
    //   void foo(const S& s) {
    //     auto& p = s.y;
    //   }
    // binds the |s.y| expr if it matches the |affected_expr_matcher| above.
    //
    // See also testcases in tests/affected-expr-original.cc
    auto auto_var_decl_matcher = declStmt(forEach(varDecl(
        allOf(hasType(referenceType(pointee(autoType()))),
              hasInitializer(anyOf(
                  affected_member_expr_matcher,
                  initListExpr(hasInit(0, affected_member_expr_matcher))))))));
    match_finder.addMatcher(auto_var_decl_matcher, &affected_expr_rewriter);

    // Matches affected member expressions that need parenthesization.
    auto affected_member_expr_withParentheses =
        memberExpr(member(fieldDecl(hasExplicitFieldDecl(field_decl_matcher))),
                   anyOf(hasParent(cxxOperatorCallExpr()),
                         hasParent(arraySubscriptExpr()),
                         hasParent(callExpr(callee(fieldDecl(
                             hasExplicitFieldDecl(field_decl_matcher)))))))
            .bind("affectedMemberExprWithParentheses");

    match_finder.addMatcher(affected_member_expr_withParentheses,
                            &affected_expr_rewriter_withParentheses);

    // See the doc comment for the overlapsOtherDeclsWithinRecordDecl
    // matcher and the testcases in tests/gen-overlapping-test.cc.
    auto overlapping_field_decl_matcher = fieldDecl(
        allOf(field_decl_matcher, overlapsOtherDeclsWithinRecordDecl()));

    match_finder.addMatcher(overlapping_field_decl_matcher,
                            &overlapping_field_decl_writer);

    // Matches fields initialized with a non-nullptr value in a constexpr
    // constructor.  See also the testcase in tests/gen-constexpr-test.cc.
    auto non_nullptr_expr_matcher =
        expr(unless(ignoringImplicit(cxxNullPtrLiteralExpr())));
    auto constexpr_ctor_field_initializer_matcher = cxxConstructorDecl(
        allOf(isConstexpr(), forEachConstructorInitializer(allOf(
                                 forField(field_decl_matcher),
                                 withInitializer(non_nullptr_expr_matcher)))));

    match_finder.addMatcher(constexpr_ctor_field_initializer_matcher,
                            &constexpr_ctor_field_initializer_writer);

    // Matches constexpr initializer list expressions that initialize a
    // rewritable field with a non-nullptr value.  For more details and
    // rationale see the testcases in tests/gen-constexpr-test.cc.
    auto constexpr_var_initializer_matcher = varDecl(
        allOf(isConstexpr(),
              hasInitializer(findAll(initListExpr(forEachInitExprWithFieldDecl(
                  non_nullptr_expr_matcher,
                  hasExplicitFieldDecl(field_decl_matcher)))))));

    match_finder.addMatcher(constexpr_var_initializer_matcher,
                            &constexpr_var_initializer_writer);

    // See the doc comment for the isInMacroLocation matcher
    // and the testcases in tests/gen-macros-test.cc.
    auto macro_field_decl_matcher =
        fieldDecl(allOf(field_decl_matcher, isInMacroLocation()));

    match_finder.addMatcher(macro_field_decl_matcher, &macro_field_decl_writer);

    // See the testcases in tests/gen-global-scope-test.cc.
    auto global_scope_matcher =
        varDecl(allOf(hasGlobalStorage(),
                      hasType(typeWithEmbeddedFieldDecl(field_decl_matcher))));

    match_finder.addMatcher(global_scope_matcher, &global_scope_rewriter);
  }

 private:
  // Rewrites |SomeClass& field| (matched as "affectedFieldDecl") as
  // |const raw_ref<SomeClass> field| and for each file rewritten in such way
  // adds an
  // |#include "base/memory/raw_ref.h"|.
  class RawRefFieldDeclRewriter : public FieldDeclRewriter {
   public:
    explicit RawRefFieldDeclRewriter(OutputHelper* output_helper,
                                     const char* format_string,
                                     const char* include_path)
        : FieldDeclRewriter(output_helper, format_string, include_path) {}

    bool earlyExit(const MatchFinder::MatchResult& result) const override {
      auto* type = result.Nodes.getNodeAs<clang::LValueReferenceTypeLoc>(
          "affectedFieldDeclType");
      // in this case, it's not an lvalue reference type member => DO NOTHING
      return !type;
    }
  };

  // Rewrites |my_struct.ref_field| (matched as "affectedMemberExpr") as
  // |*my_struct.ref_field|.
  std::function<std::pair<clang::SourceRange, std::string>(
      const MatchFinder::MatchResult&)>
      affectedMemberExprFct_ = [](const MatchFinder::MatchResult& result)
      -> std::pair<clang::SourceRange, std::string> {
    const clang::MemberExpr* member_expr =
        result.Nodes.getNodeAs<clang::MemberExpr>("affectedMemberExpr");
    assert(member_expr && "matcher should bind 'affectedMemberExpr'");
    clang::SourceRange replacement_range(member_expr->getBeginLoc(),
                                         member_expr->getBeginLoc());
    return {replacement_range, "*"};
  };

  // Rewrites |my_struct.ref_field| (matched as
  // "affectedMemberExprWithParentheses") as
  // |(*my_struct.ref_field)|.
  // Examples on why this is needed:
  //  1- std::vector<T>& v; => const raw_ref<std::vector<T>> v;
  //     v[0] => needs to be rewritten as (*v)[0] after the rewrite.
  //  2- key_compare& comp_; => const raw_ref<key_compare> comp_;
  //     comp_(a, b) => needs to be rewritten as (*comp_)(a,b) after the
  //     rewrite.
  std::function<std::pair<clang::SourceRange, std::string>(
      const MatchFinder::MatchResult&)>
      affectedMemberExprWithParenFct_ =
          [](const MatchFinder::MatchResult& result)
      -> std::pair<clang::SourceRange, std::string> {
    const clang::SourceManager& source_manager = *result.SourceManager;

    const clang::MemberExpr* member_expr =
        result.Nodes.getNodeAs<clang::MemberExpr>(
            "affectedMemberExprWithParentheses");
    assert(member_expr &&
           "matcher should bind 'affectedMemberExprWithParentheses'");

    clang::SourceLocation member_name_start = member_expr->getMemberLoc();
    clang::SourceLocation endLoc = member_name_start.getLocWithOffset(
        member_expr->getMemberDecl()->getName().size());

    clang::SourceRange replacement_range(member_expr->getBeginLoc(), endLoc);

    auto source_text = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(member_expr->getSourceRange()),
        source_manager, result.Context->getLangOpts());
    return {replacement_range,
            llvm::formatv("(*{0})",
                          std::string(source_text.begin(), source_text.end()))};
  };

  // Rewrites |my_struct.ptr_field.sub_field| (matched as
  // "affectedMemberExprOperator") into |my_struct.ptr_field->sub_field|.
  std::function<std::pair<clang::SourceRange, std::string>(
      const MatchFinder::MatchResult&)>
      affectedMemberExprOperatorFct_ =
          [](const MatchFinder::MatchResult& result)
      -> std::pair<clang::SourceRange, std::string> {
    const clang::MemberExpr* member_expr =
        result.Nodes.getNodeAs<clang::MemberExpr>("affectedMemberExprOperator");
    const clang::CXXDependentScopeMemberExpr* cxx_dependent_scope_member_expr =
        result.Nodes.getNodeAs<clang::CXXDependentScopeMemberExpr>(
            "affectedMemberExprOperator");
    assert((member_expr || cxx_dependent_scope_member_expr) &&
           "matcher should bind 'affectedMemberExprOperator'");
    if (member_expr) {
      clang::SourceRange replacement_range(member_expr->getOperatorLoc(),
                                           member_expr->getMemberLoc());
      return {replacement_range, "->"};
    }
    clang::SourceRange replacement_range(
        cxx_dependent_scope_member_expr->getOperatorLoc(),
        cxx_dependent_scope_member_expr->getMemberLoc());
    return {replacement_range, "->"};
  };

  MatchFinder& match_finder;
  RawRefFieldDeclRewriter field_decl_rewriter;
  AffectedExprRewriter affected_expr_operator_rewriter;
  AffectedExprRewriter affected_expr_rewriter;
  AffectedExprRewriter affected_expr_rewriter_withParentheses;
  FilteredExprWriter global_scope_rewriter;
  FilteredExprWriter overlapping_field_decl_writer;
  FilteredExprWriter constexpr_ctor_field_initializer_writer;
  FilteredExprWriter constexpr_var_initializer_writer;
  FilteredExprWriter macro_field_decl_writer;
  FilterFile& fields_to_exclude;
  FilterFile& paths_to_exclude;
};

}  // namespace

int main(int argc, const char* argv[]) {
  // TODO(dcheng): Clang tooling should do this itself.
  // http://llvm.org/bugs/show_bug.cgi?id=21627
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::cl::OptionCategory category(
      "rewrite_raw_ptr_fields: changes |T* field_| to |raw_ptr<T> field_|.");
  llvm::cl::opt<std::string> exclude_fields_param(
      kExcludeFieldsParamName, llvm::cl::value_desc("filepath"),
      llvm::cl::desc("file listing fields to be blocked (not rewritten)"));
  llvm::cl::opt<std::string> exclude_paths_param(
      kExcludePathsParamName, llvm::cl::value_desc("filepath"),
      llvm::cl::desc("file listing paths to be blocked (not rewritten)"));

  llvm::cl::opt<bool> enable_raw_ref_rewrite(
      "enable_raw_ref_rewrite", llvm::cl::init(false),
      llvm::cl::desc("Rewrite T& into const raw_ref<T>"));

  llvm::cl::opt<bool> enable_raw_ptr_rewrite(
      "enable_raw_ptr_rewrite", llvm::cl::init(false),
      llvm::cl::desc("Rewrite T* into raw_ptr<T>"));

  llvm::Expected<clang::tooling::CommonOptionsParser> options =
      clang::tooling::CommonOptionsParser::create(argc, argv, category);
  assert(static_cast<bool>(options));  // Should not return an error.
  clang::tooling::ClangTool tool(options->getCompilations(),
                                 options->getSourcePathList());

  // Rewrite both T& and T* into const raw_ref<T> and raw_ptr<T> respectively if
  // no argument is provided.
  bool rewrite_raw_ref_and_ptr =
      !enable_raw_ref_rewrite && !enable_raw_ptr_rewrite;
  MatchFinder match_finder;
  OutputHelper output_helper;
  FilterFile fields_to_exclude(exclude_fields_param,
                               exclude_fields_param.ArgStr.str());
  FilterFile paths_to_exclude(exclude_paths_param,
                              exclude_paths_param.ArgStr.str());

  RawPtrRewriter raw_ptr_rewriter(&output_helper, match_finder,
                                  fields_to_exclude, paths_to_exclude);

  if (rewrite_raw_ref_and_ptr || enable_raw_ptr_rewrite) {
    raw_ptr_rewriter.addMatchers();
  }

  RawRefRewriter raw_ref_rewriter(&output_helper, match_finder,
                                  fields_to_exclude, paths_to_exclude);

  if (rewrite_raw_ref_and_ptr || enable_raw_ref_rewrite) {
    raw_ref_rewriter.addMatchers();
  }
  // Prepare and run the tool.
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder, &output_helper);
  int result = tool.run(factory.get());
  if (result != 0)
    return result;

  return 0;
}
