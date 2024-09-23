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
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <regex>
#include <string>
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

const char kRawSpanIncludePath[] = "base/memory/raw_span.h";

// Name of a cmdline parameter that can be used to specify a file listing fields
// that should not be rewritten to use raw_ptr<T>.
//
// See also:
// - OutputSectionHelper
// - raw_ptr_plugin::FilterFile
const char kExcludeFieldsParamName[] = "exclude-fields";

// Name of a cmdline parameter that can be used to specify a file listing
// regular expressions describing paths that should be excluded from the
// rewrite.
//
// See also:
// - PathFilterFile
const char kOverrideExcludePathsParamName[] = "override-exclude-paths";

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
// - raw_ptr_plugin::FilterFile
// - OutputHelper
class OutputSectionHelper {
 public:
  explicit OutputSectionHelper(llvm::StringRef output_delimiter)
      : output_delimiter_(output_delimiter.str()) {}

  OutputSectionHelper(const OutputSectionHelper&) = delete;
  OutputSectionHelper& operator=(const OutputSectionHelper&) = delete;

  void Add(llvm::StringRef output_line,
           llvm::StringRef tag = "",
           llvm::StringRef loc = "") {
    // Look up |tags| associated with |output_line|.  As a side effect of the
    // lookup, |output_line| will be inserted if it wasn't already present in
    // the map.
    llvm::StringSet<>& tags = output_line_to_tags_[output_line];

    if (!tag.empty()) {
      tags.insert(tag);
    }

    // Do the same for source locations.
    llvm::StringSet<>& locs = output_line_to_locs_[output_line];
    if (!loc.empty()) {
      locs.insert(loc);
    }
  }

  void Emit() {
    if (output_line_to_tags_.empty())
      return;

    llvm::outs() << "==== BEGIN " << output_delimiter_ << " ====\n";
    for (const llvm::StringRef& output_line :
         GetSortedKeys(output_line_to_tags_)) {
      llvm::outs() << output_line;

      const llvm::StringSet<>& locs = output_line_to_locs_[output_line];
      if (!locs.empty()) {
        std::vector<llvm::StringRef> sorted_locs = GetSortedKeys(locs);
        std::string locs_comment =
            llvm::join(sorted_locs.begin(), sorted_locs.end(), ", ");
        llvm::outs() << " @ " << locs_comment;
      }

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
  static std::vector<llvm::StringRef> GetSortedKeys(
      const llvm::StringMap<TValue>& map) {
    std::vector<llvm::StringRef> sorted(map.keys().begin(), map.keys().end());
    std::sort(sorted.begin(), sorted.end());
    return sorted;
  }

  std::string output_delimiter_;
  llvm::StringMap<llvm::StringSet<>> output_line_to_tags_;
  llvm::StringMap<llvm::StringSet<>> output_line_to_locs_;
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
    std::string file_path =
        std::filesystem::proximate(replacement.getFilePath().str());
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

  void AddFilteredField(const clang::SourceManager& source_manager,
                        const clang::FieldDecl& field_decl,
                        llvm::StringRef filter_tag) {
    std::string qualified_name = field_decl.getQualifiedNameAsString();

    clang::SourceLocation loc = field_decl.getBeginLoc();
    // Calculate a relative path to the file not to make the output
    // environment-specific.
    std::string loc_str =
        std::filesystem::proximate(source_manager.getFilename(loc).str());
    if (!loc_str.empty()) {
      loc_str +=
          ":" + std::to_string(source_manager.getSpellingLineNumber(loc));
      loc_str +=
          ":" + std::to_string(source_manager.getSpellingColumnNumber(loc));
    }
    field_decl_filter_helper_.Add(qualified_name, filter_tag, loc_str);
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
    auto matcher =
        recordDecl(forEach(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(anyOf(
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

    output_helper_->AddFilteredField(*result.SourceManager, *field_decl,
                                     filter_tag_);
  }

 private:
  OutputHelper* const output_helper_;
  llvm::StringRef filter_tag_;
};

class RawPtrRewriter {
 public:
  RawPtrRewriter(
      OutputHelper* output_helper,
      MatchFinder& finder,
      const raw_ptr_plugin::RawPtrAndRefExclusionsOptions& exclusion_options)
      : match_finder(finder),
        field_decl_rewriter(output_helper, "raw_ptr<{0}> ", kRawPtrIncludePath),
        affected_expr_rewriter(output_helper, getRangeAndText_),
        filtered_addr_of_expr_writer(output_helper, "addr-of"),
        filtered_in_out_ref_arg_writer(output_helper, "in-out-param-ref"),
        overlapping_field_decl_writer(output_helper, "overlapping"),
        macro_field_decl_writer(output_helper, "macro"),
        global_scope_rewriter(output_helper, "global-scope"),
        union_field_decl_writer(output_helper, "union"),
        reinterpret_cast_struct_writer(output_helper,
                                       "reinterpret-cast-trivial-type"),
        exclusion_options_(exclusion_options) {}

  void addMatchers() {
    auto field_decl_matcher = AffectedRawPtrFieldDecl(exclusion_options_);

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
        memberExpr(member(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                       field_decl_matcher))))
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
        affected_expr_matcher,
        raw_ptr_plugin::hasExplicitParmVarDecl(
            hasType(qualType(allOf(referenceType(pointee(pointerType())),
                                   unless(rValueReferenceType())))))));

    match_finder.addMatcher(affected_in_out_ref_arg_matcher,
                            &filtered_in_out_ref_arg_writer);

    // See the doc comment for the overlapsOtherDeclsWithinRecordDecl matcher
    // and the testcases in tests/gen-overlapping-test.cc.
    auto overlapping_field_decl_matcher = fieldDecl(
        allOf(field_decl_matcher, overlapsOtherDeclsWithinRecordDecl()));

    match_finder.addMatcher(overlapping_field_decl_matcher,
                            &overlapping_field_decl_writer);

    // See the doc comment for the isInMacroLocation matcher
    // and the testcases in tests/gen-macros-test.cc.
    auto macro_field_decl_matcher = fieldDecl(
        allOf(field_decl_matcher, raw_ptr_plugin::isInMacroLocation()));

    match_finder.addMatcher(macro_field_decl_matcher, &macro_field_decl_writer);

    // See the testcases in tests/gen-global-scope-test.cc.
    auto global_scope_matcher =
        varDecl(allOf(hasGlobalStorage(),
                      hasType(typeWithEmbeddedFieldDecl(field_decl_matcher))));

    match_finder.addMatcher(global_scope_matcher, &global_scope_rewriter);

    // This is used to exclude unions from certain files that are known to have
    // safe usage of union (i.e. doesn't cause ref count mismatch), such as
    // std::optional and absl::variant.
    files_with_audited_unions =
        std::make_unique<raw_ptr_plugin::FilterFile>(std::vector<std::string>{
            "third_party/libc++/src/include/optional",
            "third_party/abseil-cpp/absl/types/internal/variant.h",
        });
    // Matches fields in unions (both directly rewritable fields as well as
    // union fields that embed a struct that contains a rewritable field).  See
    // also the testcases in tests/gen-unions-test.cc.
    auto union_field_decl_matcher = recordDecl(allOf(
        isUnion(),
        unless(isInLocationListedInFilterFile(files_with_audited_unions.get())),
        forEach(fieldDecl(
            anyOf(field_decl_matcher,
                  hasType(typeWithEmbeddedFieldDecl(field_decl_matcher)))))));

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
  FilteredExprWriter macro_field_decl_writer;
  FilteredExprWriter global_scope_rewriter;
  FilteredExprWriter union_field_decl_writer;
  FilteredExprWriter reinterpret_cast_struct_writer;
  std::unique_ptr<raw_ptr_plugin::FilterFile> files_with_audited_unions;
  const raw_ptr_plugin::RawPtrAndRefExclusionsOptions exclusion_options_;
};

class RawRefRewriter {
 public:
  RawRefRewriter(
      OutputHelper* output_helper,
      MatchFinder& finder,
      const raw_ptr_plugin::RawPtrAndRefExclusionsOptions& exclusion_options)
      : match_finder(finder),
        field_decl_rewriter(output_helper,
                            "const raw_ref<{0}> ",
                            kRawRefIncludePath),
        affected_expr_operator_rewriter(output_helper,
                                        affectedMemberExprOperatorFct_),
        affected_expr_rewriter(output_helper, affectedMemberExprFct_),
        affected_expr_rewriter_with_parentheses(
            output_helper,
            affectedMemberExprWithParenFct_),
        affected_initializer_expr_rewriter(output_helper,
                                           affectedInitializerExprFct_),
        global_scope_rewriter(output_helper, "global-scope"),
        overlapping_field_decl_writer(output_helper, "overlapping"),
        macro_field_decl_writer(output_helper, "macro"),
        exclusion_options_(exclusion_options) {}

  void addMatchers() {
    auto field_decl_matcher = AffectedRawRefFieldDecl(exclusion_options_);

    match_finder.addMatcher(field_decl_matcher, &field_decl_rewriter);

    // Matches expressions of the form |someClass.ref_field.sub_member| which
    // should be rewritten as |someClass.ref_field->sub_member| as we can't
    // overload `operator.` in C++.
    auto affected_member_expr_operator_matcher =
        expr(anyOf(memberExpr(has(memberExpr(
                       member(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                           field_decl_matcher)))))),
                   memberExpr(has(implicitCastExpr(has(memberExpr(
                       member(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                           field_decl_matcher)))))))),
                   cxxDependentScopeMemberExpr(has(memberExpr(
                       member(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                           field_decl_matcher))))))))
            .bind("affectedMemberExprOperator");

    match_finder.addMatcher(affected_member_expr_operator_matcher,
                            &affected_expr_operator_rewriter);

    // Matches expressions that used to have |SomeType&| as return type and
    // became |const raw_ref<SomeType>| after the rewrite.
    auto affected_member_expr = memberExpr(
        memberExpr(
            member(fieldDecl(
                raw_ptr_plugin::hasExplicitFieldDecl(field_decl_matcher))),
            unless(
                anyOf(hasParent(memberExpr()),
                      hasParent(implicitCastExpr(hasParent(memberExpr()))),
                      hasParent(cxxDependentScopeMemberExpr()),
                      hasParent(varDecl(unless(anyOf(
                          hasType(referenceType(pointee(autoType()))),
                          hasParent(declStmt(hasParent(cxxForRangeStmt()))))))),
                      hasAncestor(cxxConstructorDecl(isDefaulted())),
                      hasParent(cxxOperatorCallExpr()),
                      hasParent(unaryOperator(
                          anyOf(hasOperatorName("--"), hasOperatorName("++")))),
                      hasParent(arraySubscriptExpr()),
                      hasParent(callExpr(
                          callee(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                              field_decl_matcher))))))))
            .bind("affectedMemberExpr"),

        unless(anyOf(
            // Exclude memberExpressions appearing inside a constructor
            // initializer of a reference field where we should NOT add
            // operator*.
            hasParent(cxxConstructorDecl(hasAnyConstructorInitializer(
                allOf(withInitializer(
                          memberExpr(equalsBoundNode("affectedMemberExpr"))),
                      forField(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                          field_decl_matcher))))))),
            // Exclude memberExpressions, in initializer lists, that are
            // initializing a reference field that will be rewritten into
            // raw_ref.
            hasParent(initListExpr(raw_ptr_plugin::forEachInitExprWithFieldDecl(
                memberExpr(equalsBoundNode("affectedMemberExpr")),
                raw_ptr_plugin::hasExplicitFieldDecl(field_decl_matcher)))))));

    match_finder.addMatcher(affected_member_expr, &affected_expr_rewriter);

    auto affected_member_expr_matcher =
        memberExpr(member(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                       field_decl_matcher))))
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
    auto affected_member_expr_with_parentheses =
        memberExpr(member(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                       field_decl_matcher))),
                   anyOf(hasParent(cxxOperatorCallExpr()),
                         hasParent(unaryOperator(anyOf(hasOperatorName("--"),
                                                       hasOperatorName("++")))),
                         hasParent(arraySubscriptExpr()),
                         hasParent(callExpr(callee(
                             fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                                 field_decl_matcher)))))))
            .bind("affectedMemberExprWithParentheses");

    match_finder.addMatcher(affected_member_expr_with_parentheses,
                            &affected_expr_rewriter_with_parentheses);

    // for structs/class that don't define a constructor and are initialized
    // using braced list initialization, we need to add raw_ref around the
    // initializing expression since raw_ref's constructor is explicit.
    // Example:
    // struct A{ int& member; }; => struct A{ const raw_ref<int> member;};
    // int num = x;
    // A a{num}; => A a{raw_ref(num)};
    auto init_list_expr_with_raw_ref = initListExpr(
        raw_ptr_plugin::forEachInitExprWithFieldDecl(
            expr(unless(anyOf(
                     materializeTemporaryExpr(),
                     // Exclude member expressions where the member is a
                     // reference field that will be rewritten into raw_ref.
                     memberExpr(
                         member(fieldDecl(raw_ptr_plugin::hasExplicitFieldDecl(
                             field_decl_matcher)))))))
                .bind("initializer_expr"),
            raw_ptr_plugin::hasExplicitFieldDecl(field_decl_matcher)),
        unless(hasParent(cxxConstructExpr())));

    match_finder.addMatcher(init_list_expr_with_raw_ref,
                            &affected_initializer_expr_rewriter);

    // See the doc comment for the overlapsOtherDeclsWithinRecordDecl
    // matcher and the testcases in tests/gen-overlapping-test.cc.
    auto overlapping_field_decl_matcher = fieldDecl(
        allOf(field_decl_matcher, overlapsOtherDeclsWithinRecordDecl()));

    match_finder.addMatcher(overlapping_field_decl_matcher,
                            &overlapping_field_decl_writer);

    // See the doc comment for the isInMacroLocation matcher
    // and the testcases in tests/gen-macros-test.cc.
    auto macro_field_decl_matcher = fieldDecl(
        allOf(field_decl_matcher, raw_ptr_plugin::isInMacroLocation()));

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

  std::function<std::pair<clang::SourceRange, std::string>(
      const MatchFinder::MatchResult&)>
      affectedInitializerExprFct_ = [](const MatchFinder::MatchResult& result)
      -> std::pair<clang::SourceRange, std::string> {
    const clang::SourceManager& source_manager = *result.SourceManager;

    const clang::Expr* initializer_expr =
        result.Nodes.getNodeAs<clang::Expr>("initializer_expr");
    auto source_text = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(
            initializer_expr->getSourceRange()),
        source_manager, result.Context->getLangOpts());

    clang::SourceLocation endLoc =
        initializer_expr->getBeginLoc().getLocWithOffset(source_text.size());

    clang::SourceRange replacement_range(initializer_expr->getBeginLoc(),
                                         endLoc);

    return {replacement_range,
            llvm::formatv("raw_ref({0})",
                          std::string(source_text.begin(), source_text.end()))};
  };

  MatchFinder& match_finder;
  RawRefFieldDeclRewriter field_decl_rewriter;
  AffectedExprRewriter affected_expr_operator_rewriter;
  AffectedExprRewriter affected_expr_rewriter;
  AffectedExprRewriter affected_expr_rewriter_with_parentheses;
  AffectedExprRewriter affected_initializer_expr_rewriter;
  FilteredExprWriter global_scope_rewriter;
  FilteredExprWriter overlapping_field_decl_writer;
  FilteredExprWriter macro_field_decl_writer;
  const raw_ptr_plugin::RawPtrAndRefExclusionsOptions exclusion_options_;
};

class SpanFieldDeclRewriter : public MatchFinder::MatchCallback {
 public:
  explicit SpanFieldDeclRewriter(OutputHelper* output_helper,
                                 const char* include_path)
      : output_helper_(output_helper), include_path_(include_path) {}

  SpanFieldDeclRewriter(const SpanFieldDeclRewriter&) = delete;
  SpanFieldDeclRewriter& operator=(const SpanFieldDeclRewriter&) = delete;

  void run(const MatchFinder::MatchResult& result) override {
    const clang::ASTContext& ast_context = *result.Context;
    const clang::SourceManager& source_manager = *result.SourceManager;
    const auto& lang_opts = ast_context.getLangOpts();
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

    if (result.Nodes.getNodeAs<clang::QualType>("container_type")) {
      HandleContainerArguments(field_decl, result);
      return;
    }

    // Calculate the |replacement_range|.
    //
    // Consider the following example:
    //      const span<> const   field_name_;
    //      ^--------------------^  = |replacement_range|
    //                           ^  = |field_decl->getLocation()|
    //      ^                       = |field_decl->getBeginLoc()|
    //
    // We get the |replacement_range| in a bit clumsy way, because clang docs
    // for QualifiedTypeLoc explicitly say that these objects "intentionally
    // do not provide source location for type qualifiers".
    clang::SourceRange replacement_range(field_decl->getBeginLoc(),
                                         field_decl->getLocation());

    GenerateReplacement(replacement_range, source_manager, lang_opts);
  }

 private:
  clang::SourceRange GetTemplateArgumentSourceRange(
      const clang::TemplateSpecializationTypeLoc& tst_tl,
      unsigned i) {
    // For some reason, the last template argument's end location is marked as
    // being in scratch space. This leads to a wrong size for the replacement.
    // Work around this by using the RAngle's ('>') Location.
    if (i == (tst_tl.getNumArgs() - 1)) {
      return clang::SourceRange(tst_tl.getArgLoc(i).getLocation(),
                                tst_tl.getRAngleLoc());
    }

    return tst_tl.getArgLoc(i).getSourceRange();
  }

  std::optional<clang::TemplateSpecializationTypeLoc>
  GetTemplateSpecializationTypeLoc(clang::TypeLoc loc) {
    // We can have a TemplateSpecializationTypeLoc directly.
    // Example: span<some_type> member;
    if (auto specialization =
            loc.getAs<clang::TemplateSpecializationTypeLoc>()) {
      return specialization;
    }

    // Or an elaboratedTypeLoc, which has a namedTypeLoc (the
    // TemplateSpecializationTypeLoc)
    // Example:
    // base::span<some_type> member;
    //       ^-------------^ => templateSpecializationTypeLoc
    // ^-------------------^ => elaboratedTypeLoc
    if (auto elaborated = loc.getAs<clang::ElaboratedTypeLoc>()) {
      if (auto specialization =
              elaborated.getNamedTypeLoc()
                  .getAs<clang::TemplateSpecializationTypeLoc>()) {
        return specialization;
      }
    }
    return {};
  }

  void HandleContainerArguments(const clang::FieldDecl* decl,
                                const MatchFinder::MatchResult& result) {
    const clang::ASTContext& ast_context = *result.Context;
    const clang::SourceManager& source_manager = *result.SourceManager;
    auto field_type_loc = decl->getTypeSourceInfo()->getTypeLoc();
    const auto& lang_opts = ast_context.getLangOpts();
    auto tstl = GetTemplateSpecializationTypeLoc(field_type_loc);

    // This means that the field type is a typedef to a span type. This is not
    // handled by the rewriter.
    if (!tstl) {
      return;
    }

    unsigned argument_index = 0;
    if (result.Nodes.getNodeAs<clang::TemplateArgument>("template_arg0")) {
      argument_index = 0;
      auto source_range = GetTemplateArgumentSourceRange(*tstl, argument_index);
      GenerateReplacement(source_range, source_manager, lang_opts);
    }

    if (result.Nodes.getNodeAs<clang::TemplateArgument>("template_arg1")) {
      argument_index = 1;
      auto source_range = GetTemplateArgumentSourceRange(*tstl, argument_index);
      GenerateReplacement(source_range, source_manager, lang_opts);
    }
  }

  void GenerateReplacement(const clang::SourceRange& source_range,
                           const clang::SourceManager& source_manager,
                           const clang::LangOptions& lang_opts) {
    std::string initial_text =
        clang::Lexer::getSourceText(
            clang::CharSourceRange::getCharRange(source_range), source_manager,
            lang_opts)
            .str();

    // The span type to rewrite could appear as follows:
    // 1- span<some_type> (used within base namespace)
    // 2- base::span<some_type> or container<base::span<some_type>>
    // 3- container<span<some_type>> (used within base namespace)
    // The statement below inserts `raw_` before the second matched group
    // `span`. std::span is banned in chromium code, so it's not taken into
    // account in the below regex.
    std::string replacement_text = std::regex_replace(
        initial_text, std::regex("(<|base::)?(span<)"), "$1raw_$2");

    // No need to add a replacement if the replacement text is empty or is the
    // same as the initial text. |initial_text| is the same as
    // |replacemenet_text| when the field's type is an alias of base::span,
    // meaning span<T> does not appear in |initial_text| and thus the regex
    // replace does nothing.
    if (replacement_text.empty() || (initial_text == replacement_text)) {
      return;
    }
    // Generate and print a replacement.
    output_helper_->AddReplacement(source_manager, source_range,
                                   replacement_text, include_path_);
  }

  OutputHelper* const output_helper_;
  const char* include_path_;
};

class SpanRewriter {
 public:
  SpanRewriter(
      OutputHelper* output_helper,
      MatchFinder& finder,
      const raw_ptr_plugin::RawPtrAndRefExclusionsOptions& exclusion_options)
      : match_finder(finder),
        field_decl_rewriter(output_helper, kRawSpanIncludePath),
        global_scope_rewriter(output_helper, "global-scope"),
        overlapping_field_decl_writer(output_helper, "overlapping"),
        macro_field_decl_writer(output_helper, "macro"),
        exclusion_options_(exclusion_options) {}

  void addMatchers() {
    auto raw_span = hasTemplateArgument(
        2, refersToType(qualType(hasCanonicalType(qualType(hasDeclaration(
               mapAnyOf(classTemplateSpecializationDecl, classTemplateDecl)
                   .with(hasName("raw_ptr"))))))));

    auto string_literals_span = hasTemplateArgument(
        0, refersToType(qualType(hasCanonicalType(
               anyOf(asString("const char"), asString("const wchar_t"),
                     asString("const char8_t"), asString("const char16_t"),
                     asString("const char32_t"))))));

    auto excluded_spans = anyOf(raw_span, string_literals_span);

    auto span_type = anyOf(
        qualType(hasCanonicalType(
            qualType(hasDeclaration(classTemplateSpecializationDecl(
                hasName("base::span"), unless(excluded_spans)))))),
        // This part of the matcher is needed to handle templates.
        // Example:
        // template<typename T>struct S{ base::span<T> member; };
        // |member| has canonical type templateSpecializationType.
        qualType(hasCanonicalType(qualType(type(templateSpecializationType(
            hasDeclaration(classTemplateDecl(hasName("base::span"))),
            unless(excluded_spans)))))));

    auto optional_span_type = anyOf(
        qualType(
            hasCanonicalType(hasDeclaration(classTemplateSpecializationDecl(
                hasName("optional"),
                hasTemplateArgument(0, refersToType(span_type)))))),
        qualType(hasCanonicalType(qualType(type(templateSpecializationType(
            hasDeclaration(classTemplateDecl(hasName("optional"))),
            hasTemplateArgument(0, refersToType(span_type))))))));

    auto container_methods =
        anyOf(allOf(hasMethod(hasName("push_back")),
                    hasMethod(hasName("pop_back")), hasMethod(hasName("size"))),
              allOf(hasMethod(hasName("insert")), hasMethod(hasName("erase")),
                    hasMethod(hasName("size"))),
              allOf(hasMethod(hasName("push")), hasMethod(hasName("pop")),
                    hasMethod(hasName("size"))));

    auto template_arg0 = hasTemplateArgument(
        0, templateArgument(refersToType(anyOf(span_type, optional_span_type)))
               .bind("template_arg0"));
    auto template_arg1 = hasTemplateArgument(
        1, templateArgument(refersToType(anyOf(span_type, optional_span_type)))
               .bind("template_arg1"));
    // template_arg0 and template_arg1 are necessary to locate the container
    // template arguments that need to be rewritten. The use of allOf is to
    // force the matching of both arguments if both need to be rewritten.
    // Did not use a forEachTemplateArgument instead as we need the template
    // argument's index to get its location using the field's
    // templateSpecializationTypeLoc.
    auto template_arguments = anyOf(allOf(template_arg0, template_arg1),
                                    template_arg0, template_arg1);

    auto container_of_span_type =
        qualType(hasCanonicalType(anyOf(
                     qualType(hasDeclaration(classTemplateSpecializationDecl(
                         container_methods, template_arguments))),
                     qualType(type(templateSpecializationType(
                         hasDeclaration(classTemplateDecl(
                             has(cxxRecordDecl(container_methods)))),
                         template_arguments))))))
            .bind("container_type");

    auto field_decl_matcher =
        traverse(clang::TK_IgnoreUnlessSpelledInSource,
                 fieldDecl(hasType(qualType(anyOf(span_type, optional_span_type,
                                                  container_of_span_type))),
                           unless(PtrAndRefExclusions(exclusion_options_)))
                     .bind("affectedFieldDecl"));

    match_finder.addMatcher(field_decl_matcher, &field_decl_rewriter);

    // See the testcases in tests/gen-global-scope-test.cc.
    auto global_scope_matcher =
        varDecl(allOf(hasGlobalStorage(),
                      hasType(typeWithEmbeddedFieldDecl(field_decl_matcher))));

    match_finder.addMatcher(global_scope_matcher, &global_scope_rewriter);

    // See the doc comment for the isInMacroLocation matcher
    // and the testcases in tests/gen-macros-test.cc.
    auto macro_field_decl_matcher = fieldDecl(
        allOf(field_decl_matcher, raw_ptr_plugin::isInMacroLocation()));

    match_finder.addMatcher(macro_field_decl_matcher, &macro_field_decl_writer);
  }

 private:
  MatchFinder& match_finder;
  SpanFieldDeclRewriter field_decl_rewriter;
  FilteredExprWriter global_scope_rewriter;
  FilteredExprWriter overlapping_field_decl_writer;
  FilteredExprWriter macro_field_decl_writer;
  const raw_ptr_plugin::RawPtrAndRefExclusionsOptions exclusion_options_;
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
  llvm::cl::opt<std::string> override_exclude_paths_param(
      kOverrideExcludePathsParamName, llvm::cl::value_desc("filepath"),
      llvm::cl::desc(
          "override file listing paths to be blocked (not rewritten)"));

  llvm::cl::opt<bool> enable_raw_ref_rewrite(
      "enable_raw_ref_rewrite", llvm::cl::init(false),
      llvm::cl::desc("Rewrite T& into const raw_ref<T>"));

  llvm::cl::opt<bool> enable_raw_ptr_rewrite(
      "enable_raw_ptr_rewrite", llvm::cl::init(false),
      llvm::cl::desc("Rewrite T* into raw_ptr<T>"));

  llvm::cl::opt<bool> exclude_stack_allocated(
      "exclude_stack_allocated", llvm::cl::init(true),
      llvm::cl::desc("Exclude pointers/references to `STACK_ALLOCATED` objects "
                     "from the rewrite"));

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
  raw_ptr_plugin::FilterFile fields_to_exclude(
      exclude_fields_param, exclude_fields_param.ArgStr.str());

  std::unique_ptr<raw_ptr_plugin::FilterFile> paths_to_exclude;
  if (override_exclude_paths_param == "") {
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

  raw_ptr_plugin::StackAllocatedPredicate stack_allocated_checker;
  raw_ptr_plugin::RawPtrAndRefExclusionsOptions exclusion_options{
      &fields_to_exclude, paths_to_exclude.get(), exclude_stack_allocated,
      &stack_allocated_checker, true};

  RawPtrRewriter raw_ptr_rewriter(&output_helper, match_finder,
                                  exclusion_options);
  if (rewrite_raw_ref_and_ptr || enable_raw_ptr_rewrite) {
    raw_ptr_rewriter.addMatchers();
  }

  RawRefRewriter raw_ref_rewriter(&output_helper, match_finder,
                                  exclusion_options);
  if (rewrite_raw_ref_and_ptr || enable_raw_ref_rewrite) {
    raw_ref_rewriter.addMatchers();
  }

  SpanRewriter span_rewriter(&output_helper, match_finder, exclusion_options);
  span_rewriter.addMatchers();

  // Prepare and run the tool.
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder, &output_helper);
  int result = tool.run(factory.get());
  if (result != 0)
    return result;

  return 0;
}
