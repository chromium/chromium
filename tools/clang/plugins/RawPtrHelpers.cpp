// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "RawPtrHelpers.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "llvm/Support/LineIterator.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace clang::ast_matchers;

bool FilterFile::ContainsLine(llvm::StringRef line) const {
  auto it = file_lines_.find(line);
  return it != file_lines_.end();
}

bool FilterFile::ContainsSubstringOf(llvm::StringRef string_to_match) const {
  if (!inclusion_substring_regex_.has_value()) {
    std::vector<std::string> regex_escaped_inclusion_file_lines;
    std::vector<std::string> regex_escaped_exclusion_file_lines;
    regex_escaped_inclusion_file_lines.reserve(file_lines_.size());
    for (const llvm::StringRef& file_line : file_lines_.keys()) {
      if (file_line.startswith("!")) {
        regex_escaped_exclusion_file_lines.push_back(
            llvm::Regex::escape(file_line.substr(1)));
      } else {
        regex_escaped_inclusion_file_lines.push_back(
            llvm::Regex::escape(file_line));
      }
    }
    std::string inclusion_substring_regex_pattern =
        llvm::join(regex_escaped_inclusion_file_lines.begin(),
                   regex_escaped_inclusion_file_lines.end(), "|");
    inclusion_substring_regex_.emplace(inclusion_substring_regex_pattern);
    std::string exclusion_substring_regex_pattern =
        llvm::join(regex_escaped_exclusion_file_lines.begin(),
                   regex_escaped_exclusion_file_lines.end(), "|");
    exclusion_substring_regex_.emplace(exclusion_substring_regex_pattern);
  }
  return inclusion_substring_regex_->match(string_to_match) &&
         !exclusion_substring_regex_->match(string_to_match);
}

void FilterFile::ParseInputFile(const std::string& filepath,
                                const std::string& arg_name) {
  if (filepath.empty())
    return;

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> file_or_err =
      llvm::MemoryBuffer::getFile(filepath);
  if (std::error_code err = file_or_err.getError()) {
    llvm::errs() << "ERROR: Cannot open the file specified in --" << arg_name
                 << " argument: " << filepath << ": " << err.message() << "\n";
    assert(false);
    return;
  }

  llvm::line_iterator it(**file_or_err, true /* SkipBlanks */, '#');
  for (; !it.is_at_eof(); ++it) {
    llvm::StringRef line = *it;

    // Remove trailing comments.
    size_t comment_start_pos = line.find('#');
    if (comment_start_pos != llvm::StringRef::npos)
      line = line.substr(0, comment_start_pos);
    line = line.trim();

    if (line.empty())
      continue;

    file_lines_.insert(line);
  }
}

clang::ast_matchers::internal::Matcher<clang::FieldDecl>
ImplicitFieldDeclaration() {
  auto implicit_class_specialization_matcher =
      classTemplateSpecializationDecl(isImplicitClassTemplateSpecialization());
  auto implicit_function_specialization_matcher =
      functionDecl(isImplicitFunctionTemplateSpecialization());
  auto implicit_field_decl_matcher = fieldDecl(hasParent(cxxRecordDecl(anyOf(
      isLambda(), implicit_class_specialization_matcher,
      hasAncestor(decl(anyOf(implicit_class_specialization_matcher,
                             implicit_function_specialization_matcher)))))));

  return implicit_field_decl_matcher;
}

// These represent the common conditions to skip the rewrite for reference and
// pointer fields. This includes fields that are:
// - listed in the --exclude-fields cmdline param or located in paths
//   matched by --exclude-paths cmdline param
// - "implicit" (i.e. field decls that are not explicitly present in
//   the source code)
// - located in Extern C context, in generated code or annotated with
// RAW_PTR_EXCLUSION
// - located under third_party/ except under third_party/blink as Blink
// is part of chromium git repo.
auto PtrAndRefExclusions(const FilterFile* paths_to_exclude,
                         const FilterFile* fields_to_exclude) {
  return anyOf(isExpansionInSystemHeader(), isInExternCContext(),
               isRawPtrExclusionAnnotated(), isInThirdPartyLocation(),
               isInGeneratedLocation(),
               isInLocationListedInFilterFile(paths_to_exclude),
               isFieldDeclListedInFilterFile(fields_to_exclude),
               ImplicitFieldDeclaration());
}

clang::ast_matchers::internal::Matcher<clang::Decl> AffectedRawPtrFieldDecl(
    const FilterFile* paths_to_exclude,
    const FilterFile* fields_to_exclude) {
  // Supported pointer types =========
  // Given
  //   struct MyStrict {
  //     int* int_ptr;
  //     int i;
  //     int (*func_ptr)();
  //     int (MyStruct::* member_func_ptr)(char);
  //     int (*ptr_to_array_of_ints)[123]
  //   };
  // matches |int*|, but not the other types.
  auto supported_pointer_types_matcher =
      pointerType(unless(pointee(hasUnqualifiedDesugaredType(
          anyOf(functionType(), memberPointerType(), arrayType())))));

  // TODO(crbug.com/1381955): Skipping const char pointers as it likely points
  // to string literals where raw_ptr isn't necessary. Remove when we have
  // implement const char support.
  auto const_char_pointer_matcher =
      fieldDecl(hasType(pointerType(pointee(qualType(allOf(
          isConstQualified(), hasUnqualifiedDesugaredType(anyCharType())))))));

  // TODO(keishi): Skip field declarations in scratch space for now as we can't
  // tell the correct file path.
  auto field_decl_matcher =
      fieldDecl(
          allOf(hasType(supported_pointer_types_matcher),
                unless(anyOf(
                    const_char_pointer_matcher, isInScratchSpace(),
                    PtrAndRefExclusions(paths_to_exclude, fields_to_exclude)))))
          .bind("affectedFieldDecl");
  return field_decl_matcher;
}

clang::ast_matchers::internal::Matcher<clang::Decl> AffectedRawRefFieldDecl(
    const FilterFile* paths_to_exclude,
    const FilterFile* fields_to_exclude) {
  // Field declarations =========
  // Given
  //   struct S {
  //     int& y;
  //   };
  // matches |int& y|.  Doesn't match:
  // - non-reference types
  // - fields matching criteria elaborated in PtrAndRefExclusions
  auto field_decl_matcher =
      fieldDecl(allOf(has(referenceTypeLoc().bind("affectedFieldDeclType")),
                      unless(PtrAndRefExclusions(paths_to_exclude,
                                                 fields_to_exclude))))
          .bind("affectedFieldDecl");

  return field_decl_matcher;
}