// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "RawPtrHelpers.h"

#include "StackAllocatedChecker.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "llvm/Support/LineIterator.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace clang::ast_matchers;

FilterFile::FilterFile(const std::vector<std::string>& lines) {
  for (const auto& line : lines) {
    file_lines_.insert(line);
  }
}

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

clang::ast_matchers::internal::Matcher<clang::Decl> ImplicitFieldDeclaration() {
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

clang::ast_matchers::internal::Matcher<clang::QualType> StackAllocatedQualType(
    const chrome_checker::StackAllocatedPredicate* checker) {
  return qualType(recordType(hasDeclaration(
                      cxxRecordDecl(isStackAllocated(*checker)))))
      .bind("pointeeQualType");
}

// These represent the common conditions to skip the rewrite for reference and
// pointer decls. This includes decls that are:
// - listed in the --exclude-fields cmdline param or located in paths
//   matched by --exclude-paths cmdline param
// - "implicit" (i.e. field decls that are not explicitly present in
//   the source code)
// - located in Extern C context, in generated code or annotated with
// RAW_PTR_EXCLUSION
// - located under third_party/ except under third_party/blink as Blink
// is part of chromium git repo.
clang::ast_matchers::internal::Matcher<clang::NamedDecl> PtrAndRefExclusions(
    const RawPtrAndRefExclusionsOptions& options) {
  if (!options.fix_crbug_1449812) {
    // Before fix for crbug.com/1449812
    // - File exclusion is performed based on the `SourceLocation` obtained
    //   via `getBeginLoc()`.
    // - System header check is based on expansion location.
    if (!options.should_exclude_stack_allocated_records) {
      return anyOf(
          isExpansionInSystemHeader(), isInExternCContext(),
          isRawPtrExclusionAnnotated(), isBeginInThirdPartyLocation(),
          isBeginInGeneratedLocation(),
          isBeginInLocationListedInFilterFile(options.paths_to_exclude),
          isFieldDeclListedInFilterFile(options.fields_to_exclude),
          ImplicitFieldDeclaration(), isObjCSynthesize());
    } else {
      return anyOf(
          isExpansionInSystemHeader(), isInExternCContext(),
          isRawPtrExclusionAnnotated(), isBeginInThirdPartyLocation(),
          isBeginInGeneratedLocation(),
          isBeginInLocationListedInFilterFile(options.paths_to_exclude),
          isFieldDeclListedInFilterFile(options.fields_to_exclude),
          ImplicitFieldDeclaration(), isObjCSynthesize(),
          hasDescendant(
              StackAllocatedQualType(options.stack_allocated_predicate)));
    }
  } else {
    // After fix for crbug.com/1449812
    // - File exclusion is performed based on the `SourceLocation` obtained
    //   via `getLocation()`.
    // - System header check is based on spelling location.
    if (!options.should_exclude_stack_allocated_records) {
      return anyOf(isSpellingInSystemHeader(), isInExternCContext(),
                   isRawPtrExclusionAnnotated(), isInThirdPartyLocation(),
                   isInGeneratedLocation(), isNotSpelledInSource(),
                   isInLocationListedInFilterFile(options.paths_to_exclude),
                   isFieldDeclListedInFilterFile(options.fields_to_exclude),
                   ImplicitFieldDeclaration(), isObjCSynthesize());
    } else {
      return anyOf(isSpellingInSystemHeader(), isInExternCContext(),
                   isRawPtrExclusionAnnotated(), isInThirdPartyLocation(),
                   isInGeneratedLocation(), isNotSpelledInSource(),
                   isInLocationListedInFilterFile(options.paths_to_exclude),
                   isFieldDeclListedInFilterFile(options.fields_to_exclude),
                   ImplicitFieldDeclaration(), isObjCSynthesize(),
                   hasDescendant(StackAllocatedQualType(
                       options.stack_allocated_predicate)));
    }
  }
}

// These represent the common conditions to skip the check on existing
// |raw_ptr<T>| and |raw_ref<T>|. This includes decls that are:
// - located in system headers.
// - located under third_party/ except under third_party/blink as Blink
// is part of chromium git repo.
clang::ast_matchers::internal::Matcher<clang::TypeLoc>
PtrAndRefTypeLocExclusions() {
  return anyOf(isSpellingInSystemHeader(), isInThirdPartyLocation());
}

static const auto unsupported_pointee_types =
    pointee(hasUnqualifiedDesugaredType(
        anyOf(functionType(), memberPointerType(), arrayType())));

clang::ast_matchers::internal::Matcher<clang::Decl> AffectedRawPtrFieldDecl(
    const RawPtrAndRefExclusionsOptions& options) {
  // Supported pointer types =========
  // Given
  //   struct MyStruct {
  //     int* int_ptr;
  //     int i;
  //     int (*func_ptr)();
  //     int (MyStruct::* member_func_ptr)(char);
  //     int (*ptr_to_array_of_ints)[123];
  //   };
  // matches |int*|, but not the other types.
  auto supported_pointer_types_matcher =
      pointerType(unless(unsupported_pointee_types));

  // TODO(crbug.com/1381955): Skipping const char pointers as it likely points
  // to string literals where raw_ptr isn't necessary. Remove when we have
  // implement const char support.
  auto const_char_pointer_matcher =
      fieldDecl(hasType(pointerType(pointee(qualType(allOf(
          isConstQualified(), hasUnqualifiedDesugaredType(anyCharType())))))));

  if (!options.fix_crbug_1449812) {
    auto field_decl_matcher =
        fieldDecl(allOf(hasType(supported_pointer_types_matcher),
                        unless(anyOf(const_char_pointer_matcher,
                                     isBeginInScratchSpace(),
                                     PtrAndRefExclusions(options)))))
            .bind("affectedFieldDecl");
    return field_decl_matcher;
  } else {
    // `isBeginInScratchSpace()` check is done inside `PtrAndRefExclusions`.
    auto field_decl_matcher =
        fieldDecl(allOf(hasType(supported_pointer_types_matcher),
                        unless(anyOf(const_char_pointer_matcher,
                                     PtrAndRefExclusions(options)))))
            .bind("affectedFieldDecl");
    return field_decl_matcher;
  }
}

clang::ast_matchers::internal::Matcher<clang::Decl> AffectedRawRefFieldDecl(
    const RawPtrAndRefExclusionsOptions& options) {
  // Supported reference types =========
  // Given
  //   struct MyStruct {
  //     int& int_ref;
  //     int i;
  //     int (&func_ref)();
  //     int (&ref_to_array_of_ints)[123];
  //   };
  // matches |int&|, but not the other types.
  auto supported_ref_types_matcher =
      referenceType(unless(unsupported_pointee_types));

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
                      hasType(supported_ref_types_matcher),
                      unless(PtrAndRefExclusions(options))))
          .bind("affectedFieldDecl");

  return field_decl_matcher;
}

clang::ast_matchers::internal::Matcher<clang::TypeLoc>
RawPtrToStackAllocatedTypeLoc(
    const chrome_checker::StackAllocatedPredicate* predicate) {
  // Given
  //   class StackAllocatedType { STACK_ALLOCATED(); };
  //   class StackAllocatedSubType : public StackAllocatedType {};
  //   class NonStackAllocatedType {};
  //
  //   struct MyStruct {
  //     raw_ptr<StackAllocatedType> a;
  //     raw_ptr<StackAllocatedSubType> b;
  //     raw_ptr<NonStackAllocatedType> c;
  //     raw_ptr<some_container<StackAllocatedType>> d;
  //     raw_ptr<some_container<StackAllocatedSubType>> e;
  //     raw_ptr<some_container<NonStackAllocatedType>> f;
  //     some_container<raw_ptr<StackAllocatedType>> g;
  //     some_container<raw_ptr<StackAllocatedSubType>> h;
  //     some_container<raw_ptr<NonStackAllocatedType>> i;
  //   };
  // matches fields a,b,d,e,g,h, and not c,f,i.
  // Similarly, given
  //   void my_func() {
  //     raw_ptr<StackAllocatedType> a;
  //     raw_ptr<StackAllocatedSubType> b;
  //     raw_ptr<NonStackAllocatedType> c;
  //     raw_ptr<some_container<StackAllocatedType>> d;
  //     raw_ptr<some_container<StackAllocatedSubType>> e;
  //     raw_ptr<some_container<NonStackAllocatedType>> f;
  //     some_container<raw_ptr<StackAllocatedType>> g;
  //     some_container<raw_ptr<StackAllocatedSubType>> h;
  //     some_container<raw_ptr<NonStackAllocatedType>> i;
  //   }
  // matches variables a,b,d,e,g,h, and not c,f,i.

  // Matches records |raw_ptr| or |raw_ref|.
  auto pointer_record =
      cxxRecordDecl(hasAnyName("base::raw_ptr", "base::raw_ref"))
          .bind("pointerRecordDecl");

  // Matches qual types having a record with |isStackAllocated| = true.
  auto pointee_type =
      qualType(StackAllocatedQualType(predicate)).bind("pointeeQualType");

  // Matches type locs like |raw_ptr<StackAllocatedType>| or
  // |raw_ref<StackAllocatedType>|.
  auto stack_allocated_rawptr_type_loc =
      templateSpecializationTypeLoc(
          allOf(unless(PtrAndRefTypeLocExclusions()),
                loc(templateSpecializationType(hasDeclaration(
                    allOf(pointer_record,
                          classTemplateSpecializationDecl(hasTemplateArgument(
                              0, refersToType(pointee_type)))))))))
          .bind("stackAllocatedRawPtrTypeLoc");
  return stack_allocated_rawptr_type_loc;
}

// If |field_decl| declares a field in an implicit template specialization, then
// finds and returns the corresponding FieldDecl from the template definition.
// Otherwise, just returns the original |field_decl| argument.
const clang::FieldDecl* GetExplicitDecl(const clang::FieldDecl* field_decl) {
  if (field_decl->isAnonymousStructOrUnion()) {
    return field_decl;  // Safe fallback - |field_decl| is not a pointer field.
  }

  const clang::CXXRecordDecl* record_decl =
      clang::dyn_cast<clang::CXXRecordDecl>(field_decl->getParent());
  if (!record_decl) {
    return field_decl;  // Non-C++ records are never template instantiations.
  }

  const clang::CXXRecordDecl* pattern_decl =
      record_decl->getTemplateInstantiationPattern();
  if (!pattern_decl) {
    return field_decl;  // |pattern_decl| is not a template instantiation.
  }

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
    if (!pattern_param->isParameterPack()) {
      continue;
    }

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
