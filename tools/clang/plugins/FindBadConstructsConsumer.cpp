// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "FindBadConstructsConsumer.h"

#include "Util.h"
#include "clang/AST/Attr.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace chrome_checker {

namespace {

// Returns the underlying Type for |type| by expanding typedefs and removing
// any namespace qualifiers. This is similar to desugaring, except that for
// ElaboratedTypes, desugar will unwrap too much.
const Type* UnwrapType(const Type* type) {
  if (const ElaboratedType* elaborated = dyn_cast<ElaboratedType>(type))
    return UnwrapType(elaborated->getNamedType().getTypePtr());
  if (const TypedefType* typedefed = dyn_cast<TypedefType>(type))
    return UnwrapType(typedefed->desugar().getTypePtr());
  return type;
}

bool InTestingNamespace(const Decl* record) {
  return GetNamespace(record).find("testing") != std::string::npos;
}

bool IsGtestTestFixture(const CXXRecordDecl* decl) {
  return decl->getQualifiedNameAsString() == "testing::Test";
}

bool IsMethodInTestingNamespace(const CXXMethodDecl* method) {
  for (auto* overridden : method->overridden_methods()) {
    if (IsMethodInTestingNamespace(overridden) ||
        // Provide an exception for ::testing::Test. gtest itself uses some
        // magic to try to make sure SetUp()/TearDown() aren't capitalized
        // incorrectly, but having the plugin enforce override is also nice.
        (InTestingNamespace(overridden) &&
         !IsGtestTestFixture(overridden->getParent()))) {
      return true;
    }
  }

  return false;
}

bool IsGmockObject(const CXXRecordDecl* decl) {
  // If |record| has member variables whose types are in the "testing" namespace
  // (which is how gmock works behind the scenes), there's a really high chance
  // that |record| is a gmock object.
  for (auto* field : decl->fields()) {
    CXXRecordDecl* record_type = field->getTypeSourceInfo()
                                     ->getTypeLoc()
                                     .getTypePtr()
                                     ->getAsCXXRecordDecl();
    if (record_type) {
      if (InTestingNamespace(record_type)) {
        return true;
      }
    }
  }
  return false;
}

bool IsPodOrTemplateType(const CXXRecordDecl& record) {
  return record.isPOD() ||
         record.getDescribedClassTemplate() ||
         record.getTemplateSpecializationKind() ||
         record.isDependentType();
}

// Use a local RAV implementation to simply collect all FunctionDecls marked for
// late template parsing. This happens with the flag -fdelayed-template-parsing,
// which is on by default in MSVC-compatible mode.
std::set<FunctionDecl*> GetLateParsedFunctionDecls(TranslationUnitDecl* decl) {
  struct Visitor : public RecursiveASTVisitor<Visitor> {
    bool VisitFunctionDecl(FunctionDecl* function_decl) {
      if (function_decl->isLateTemplateParsed())
        late_parsed_decls.insert(function_decl);
      return true;
    }

    std::set<FunctionDecl*> late_parsed_decls;
  } v;
  v.TraverseDecl(decl);
  return v.late_parsed_decls;
}

std::string GetAutoReplacementTypeAsString(QualType type,
                                           StorageClass storage_class) {
  QualType non_reference_type = type.getNonReferenceType();
  if (!non_reference_type->isPointerType())
    return storage_class == SC_Static ? "static auto" : "auto";

  std::string result = GetAutoReplacementTypeAsString(
      non_reference_type->getPointeeType(), storage_class);
  result += "*";
  if (non_reference_type.isConstQualified())
    result += " const";
  if (non_reference_type.isVolatileQualified())
    result += " volatile";
  if (type->isReferenceType() && !non_reference_type.isConstQualified()) {
    if (type->isLValueReferenceType())
      result += "&";
    else if (type->isRValueReferenceType())
      result += "&&";
  }
  return result;
}

}  // namespace

FindBadConstructsConsumer::FindBadConstructsConsumer(CompilerInstance& instance,
                                                     const Options& options)
    : ChromeClassTester(instance, options) {
  if (options.check_ipc) {
    ipc_visitor_.reset(new CheckIPCVisitor(instance));
  }

  // Messages for virtual methods.
  diag_method_requires_override_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] Overriding method must be marked with 'override' or "
      "'final'.");
  diag_redundant_virtual_specifier_ = diagnostic().getCustomDiagID(
      getErrorLevel(), "[chromium-style] %0 is redundant; %1 implies %0.");
  diag_will_be_redundant_virtual_specifier_ = diagnostic().getCustomDiagID(
      getErrorLevel(), "[chromium-style] %0 will be redundant; %1 implies %0.");
  // http://llvm.org/bugs/show_bug.cgi?id=21051 has been filed to make this a
  // Clang warning.
  diag_base_method_virtual_and_final_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] The virtual method does not override anything and is "
      "final; consider making it non-virtual.");
  diag_virtual_with_inline_body_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] virtual methods with non-empty bodies shouldn't be "
      "declared inline.");

  // Messages for constructors.
  diag_no_explicit_ctor_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] Complex class/struct needs an explicit out-of-line "
      "constructor.");
  diag_no_explicit_copy_ctor_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] Complex class/struct needs an explicit out-of-line "
      "copy constructor.");
  diag_inline_complex_ctor_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] Complex constructor has an inlined body.");

  // Messages for destructors.
  diag_no_explicit_dtor_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] Complex class/struct needs an explicit out-of-line "
      "destructor.");
  diag_inline_complex_dtor_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] Complex destructor has an inline body.");

  // Messages for refcounted objects.
  diag_refcounted_needs_explicit_dtor_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] Classes that are ref-counted should have explicit "
      "destructors that are declared protected or private.");
  diag_refcounted_with_public_dtor_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] Classes that are ref-counted should have "
      "destructors that are declared protected or private.");
  diag_refcounted_with_protected_non_virtual_dtor_ =
      diagnostic().getCustomDiagID(
          getErrorLevel(),
          "[chromium-style] Classes that are ref-counted and have non-private "
          "destructors should declare their destructor virtual.");

  // Miscellaneous messages.
  diag_weak_ptr_factory_order_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] WeakPtrFactory members which refer to their outer "
      "class must be the last member in the outer class definition.");
  diag_bad_enum_max_value_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] kMaxValue enumerator does not match max value %0 of "
      "other enumerators");
  diag_enum_max_value_unique_ = diagnostic().getCustomDiagID(
      getErrorLevel(),
      "[chromium-style] kMaxValue enumerator should not have a unique value: "
      "it should share the value of the highest enumerator");
  diag_auto_deduced_to_a_pointer_type_ =
      diagnostic().getCustomDiagID(getErrorLevel(),
                                   "[chromium-style] auto variable type "
                                   "must not deduce to a raw pointer "
                                   "type.");

  // Registers notes to make it easier to interpret warnings.
  diag_note_inheritance_ = diagnostic().getCustomDiagID(
      DiagnosticsEngine::Note, "[chromium-style] %0 inherits from %1 here");
  diag_note_implicit_dtor_ = diagnostic().getCustomDiagID(
      DiagnosticsEngine::Note,
      "[chromium-style] No explicit destructor for %0 defined");
  diag_note_public_dtor_ = diagnostic().getCustomDiagID(
      DiagnosticsEngine::Note,
      "[chromium-style] Public destructor declared here");
  diag_note_protected_non_virtual_dtor_ = diagnostic().getCustomDiagID(
      DiagnosticsEngine::Note,
      "[chromium-style] Protected non-virtual destructor declared here");
}

void FindBadConstructsConsumer::Traverse(ASTContext& context) {
  if (ipc_visitor_) {
    ipc_visitor_->set_context(&context);
    ParseFunctionTemplates(context.getTranslationUnitDecl());
  }
  RecursiveASTVisitor::TraverseDecl(context.getTranslationUnitDecl());
  if (ipc_visitor_) ipc_visitor_->set_context(nullptr);
}

bool FindBadConstructsConsumer::TraverseDecl(Decl* decl) {
  if (ipc_visitor_) ipc_visitor_->BeginDecl(decl);
  bool result = RecursiveASTVisitor::TraverseDecl(decl);
  if (ipc_visitor_) ipc_visitor_->EndDecl();
  return result;
}

bool FindBadConstructsConsumer::VisitEnumDecl(clang::EnumDecl* decl) {
  CheckEnumMaxValue(decl);
  return true;
}

bool FindBadConstructsConsumer::VisitTagDecl(clang::TagDecl* tag_decl) {
  if (tag_decl->isCompleteDefinition())
    CheckTag(tag_decl);
  return true;
}

bool FindBadConstructsConsumer::VisitTemplateSpecializationType(
    TemplateSpecializationType* spec) {
  if (ipc_visitor_) ipc_visitor_->VisitTemplateSpecializationType(spec);
  return true;
}

bool FindBadConstructsConsumer::VisitCallExpr(CallExpr* call_expr) {
  if (ipc_visitor_) ipc_visitor_->VisitCallExpr(call_expr);
  return true;
}

bool FindBadConstructsConsumer::VisitVarDecl(clang::VarDecl* var_decl) {
  CheckVarDecl(var_decl);
  return true;
}

void FindBadConstructsConsumer::CheckChromeClass(LocationType location_type,
                                                 SourceLocation record_location,
                                                 CXXRecordDecl* record) {
  bool implementation_file = InImplementationFile(record_location);

  if (!implementation_file) {
    // Only check for "heavy" constructors/destructors in header files;
    // within implementation files, there is no performance cost.

    // If this is a POD or a class template or a type dependent on a
    // templated class, assume there's no ctor/dtor/virtual method
    // optimization that we should do.
    if (!IsPodOrTemplateType(*record))
      CheckCtorDtorWeight(record_location, record);
  }

  bool warn_on_inline_bodies = !implementation_file;
  // Check that all virtual methods are annotated with override or final.
  // Note this could also apply to templates, but for some reason Clang
  // does not always see the "override", so we get false positives.
  // See http://llvm.org/bugs/show_bug.cgi?id=18440 and
  //     http://llvm.org/bugs/show_bug.cgi?id=21942
  if (!IsPodOrTemplateType(*record))
    CheckVirtualMethods(record_location, record, warn_on_inline_bodies);

  // TODO(dcheng): This is needed because some of the diagnostics for refcounted
  // classes use DiagnosticsEngine::Report() directly, and there are existing
  // violations in Blink. This should be removed once the checks are
  // modularized.
  if (location_type != LocationType::kBlink)
    CheckRefCountedDtors(record_location, record);

  CheckWeakPtrFactoryMembers(record_location, record);
}

void FindBadConstructsConsumer::CheckEnumMaxValue(EnumDecl* decl) {
  if (!decl->isScoped())
    return;

  clang::EnumConstantDecl* max_value = nullptr;
  std::set<clang::EnumConstantDecl*> max_enumerators;
  llvm::APSInt max_seen;
  for (clang::EnumConstantDecl* enumerator : decl->enumerators()) {
    if (enumerator->getName() == "kMaxValue")
      max_value = enumerator;

    llvm::APSInt current_value = enumerator->getInitVal();
    if (max_enumerators.empty()) {
      max_enumerators.emplace(enumerator);
      max_seen = current_value;
      continue;
    }

    assert(max_seen.isSigned() == current_value.isSigned());

    if (current_value < max_seen)
      continue;

    if (current_value == max_seen) {
      max_enumerators.emplace(enumerator);
      continue;
    }

    assert(current_value > max_seen);
    max_enumerators.clear();
    max_enumerators.emplace(enumerator);
    max_seen = current_value;
  }

  if (!max_value)
    return;

  if (max_enumerators.find(max_value) == max_enumerators.end()) {
    ReportIfSpellingLocNotIgnored(max_value->getLocation(),
                                  diag_bad_enum_max_value_)
        << max_seen.toString(10);
  } else if (max_enumerators.size() < 2) {
    ReportIfSpellingLocNotIgnored(decl->getLocation(),
                                  diag_enum_max_value_unique_);
  }
}

void FindBadConstructsConsumer::CheckCtorDtorWeight(
    SourceLocation record_location,
    CXXRecordDecl* record) {
  // We don't handle anonymous structs. If this record doesn't have a
  // name, it's of the form:
  //
  // struct {
  //   ...
  // } name_;
  if (record->getIdentifier() == NULL)
    return;

  // We don't handle unions.
  if (record->isUnion())
    return;

  // Skip records that derive from ignored base classes.
  if (HasIgnoredBases(record))
    return;

  // Count the number of templated base classes as a feature of whether the
  // destructor can be inlined.
  int templated_base_classes = 0;
  for (CXXRecordDecl::base_class_const_iterator it = record->bases_begin();
       it != record->bases_end();
       ++it) {
    if (it->getTypeSourceInfo()->getTypeLoc().getTypeLocClass() ==
        TypeLoc::TemplateSpecialization) {
      ++templated_base_classes;
    }
  }

  // Count the number of trivial and non-trivial member variables.
  int trivial_member = 0;
  int non_trivial_member = 0;
  int templated_non_trivial_member = 0;
  for (RecordDecl::field_iterator it = record->field_begin();
       it != record->field_end();
       ++it) {
    CountType(it->getType().getTypePtr(),
              &trivial_member,
              &non_trivial_member,
              &templated_non_trivial_member);
  }

  // Check to see if we need to ban inlined/synthesized constructors. Note
  // that the cutoffs here are kind of arbitrary. Scores over 10 break.
  int dtor_score = 0;
  // Deriving from a templated base class shouldn't be enough to trigger
  // the ctor warning, but if you do *anything* else, it should.
  //
  // TODO(erg): This is motivated by templated base classes that don't have
  // any data members. Somehow detect when templated base classes have data
  // members and treat them differently.
  dtor_score += templated_base_classes * 9;
  // Instantiating a template is an insta-hit.
  dtor_score += templated_non_trivial_member * 10;
  // The fourth normal class member should trigger the warning.
  dtor_score += non_trivial_member * 3;

  int ctor_score = dtor_score;
  // You should be able to have 9 ints before we warn you.
  ctor_score += trivial_member;

  if (ctor_score >= 10) {
    if (!record->hasUserDeclaredConstructor()) {
      ReportIfSpellingLocNotIgnored(record_location, diag_no_explicit_ctor_);
    } else {
      // Iterate across all the constructors in this file and yell if we
      // find one that tries to be inline.
      for (CXXRecordDecl::ctor_iterator it = record->ctor_begin();
           it != record->ctor_end();
           ++it) {
        // The current check is buggy. An implicit copy constructor does not
        // have an inline body, so this check never fires for classes with a
        // user-declared out-of-line constructor.
        if (it->hasInlineBody()) {
          if (it->isCopyConstructor() &&
              !record->hasUserDeclaredCopyConstructor()) {
            // In general, implicit constructors are generated on demand.  But
            // in the Windows component build, dllexport causes instantiation of
            // the copy constructor which means that this fires on many more
            // classes. For now, suppress this on dllexported classes.
            // (This does mean that windows component builds will not emit this
            // warning in some cases where it is emitted in other configs, but
            // that's the better tradeoff at this point).
            // TODO(dcheng): With the RecursiveASTVisitor, these warnings might
            // be emitted on other platforms too, reevaluate if we want to keep
            // surpressing this then http://crbug.com/467288
            if (!record->hasAttr<DLLExportAttr>())
              ReportIfSpellingLocNotIgnored(record_location,
                                            diag_no_explicit_copy_ctor_);
          } else {
            // See the comment in the previous branch about copy constructors.
            // This does the same for implicit move constructors.
            bool is_likely_compiler_generated_dllexport_move_ctor =
                it->isMoveConstructor() &&
                !record->hasUserDeclaredMoveConstructor() &&
                record->hasAttr<DLLExportAttr>();
            if (!is_likely_compiler_generated_dllexport_move_ctor)
              ReportIfSpellingLocNotIgnored(it->getInnerLocStart(),
                                            diag_inline_complex_ctor_);
          }
        } else if (it->isInlined() && !it->isInlineSpecified() &&
                   !it->isDeleted() && (!it->isCopyOrMoveConstructor() ||
                                        it->isExplicitlyDefaulted())) {
          // isInlined() is a more reliable check than hasInlineBody(), but
          // unfortunately, it results in warnings for implicit copy/move
          // constructors in the previously mentioned situation. To preserve
          // compatibility with existing Chromium code, only warn if it's an
          // explicitly defaulted copy or move constructor.
          ReportIfSpellingLocNotIgnored(it->getInnerLocStart(),
                                        diag_inline_complex_ctor_);
        }
      }
    }
  }

  // The destructor side is equivalent except that we don't check for
  // trivial members; 20 ints don't need a destructor.
  if (dtor_score >= 10 && !record->hasTrivialDestructor()) {
    if (!record->hasUserDeclaredDestructor()) {
      ReportIfSpellingLocNotIgnored(record_location, diag_no_explicit_dtor_);
    } else if (CXXDestructorDecl* dtor = record->getDestructor()) {
      if (dtor->isInlined() && !dtor->isInlineSpecified() &&
          !dtor->isDeleted()) {
        ReportIfSpellingLocNotIgnored(dtor->getInnerLocStart(),
                                      diag_inline_complex_dtor_);
      }
    }
  }
}

SuppressibleDiagnosticBuilder
FindBadConstructsConsumer::ReportIfSpellingLocNotIgnored(
    SourceLocation loc,
    unsigned diagnostic_id) {
  LocationType type =
      ClassifyLocation(instance().getSourceManager().getSpellingLoc(loc));
  bool ignored = type == LocationType::kThirdParty;
  if (type == LocationType::kBlink) {
    if (diagnostic_id == diag_no_explicit_ctor_ ||
        diagnostic_id == diag_no_explicit_copy_ctor_ ||
        diagnostic_id == diag_inline_complex_ctor_ ||
        diagnostic_id == diag_no_explicit_dtor_ ||
        diagnostic_id == diag_inline_complex_dtor_ ||
        diagnostic_id == diag_refcounted_with_protected_non_virtual_dtor_ ||
        diagnostic_id == diag_virtual_with_inline_body_) {
      // Certain checks are ignored in Blink for historical reasons.
      // TODO(dcheng): Make this list smaller.
      ignored = true;
    }
  }
  return SuppressibleDiagnosticBuilder(&diagnostic(), loc, diagnostic_id,
                                       ignored);
}

// Checks that virtual methods are correctly annotated, and have no body in a
// header file.
void FindBadConstructsConsumer::CheckVirtualMethods(
    SourceLocation record_location,
    CXXRecordDecl* record,
    bool warn_on_inline_bodies) {
  if (IsGmockObject(record)) {
    if (!options_.check_gmock_objects)
      return;
    warn_on_inline_bodies = false;
  }

  for (CXXRecordDecl::method_iterator it = record->method_begin();
       it != record->method_end();
       ++it) {
    if (it->isCopyAssignmentOperator() || isa<CXXConstructorDecl>(*it)) {
      // Ignore constructors and assignment operators.
    } else if (isa<CXXDestructorDecl>(*it) &&
               !record->hasUserDeclaredDestructor()) {
      // Ignore non-user-declared destructors.
    } else if (!it->isVirtual()) {
      continue;
    } else {
      CheckVirtualSpecifiers(*it);
      if (warn_on_inline_bodies)
        CheckVirtualBodies(*it);
    }
  }
}

// Makes sure that virtual methods use the most appropriate specifier. If a
// virtual method overrides a method from a base class, only the override
// specifier should be used. If the method should not be overridden by derived
// classes, only the final specifier should be used.
void FindBadConstructsConsumer::CheckVirtualSpecifiers(
    const CXXMethodDecl* method) {
  bool is_override = method->size_overridden_methods() > 0;
  bool has_virtual = method->isVirtualAsWritten();
  OverrideAttr* override_attr = method->getAttr<OverrideAttr>();
  FinalAttr* final_attr = method->getAttr<FinalAttr>();

  if (IsMethodInTestingNamespace(method))
    return;

  SourceManager& manager = instance().getSourceManager();
  const LangOptions& lang_opts = instance().getLangOpts();

  // Grab the stream of tokens from the beginning of the method
  bool remove_virtual = false;
  bool add_override = false;

  // Complain if a method is annotated virtual && (override || final).
  if (has_virtual && (override_attr || final_attr))
    remove_virtual = true;

  // Complain if a method is an override and is not annotated with override or
  // final.
  if (is_override && !override_attr && !final_attr) {
    add_override = true;
    // Also remove the virtual in the same fixit if currently present.
    if (has_virtual)
      remove_virtual = true;
  }

  if (final_attr && override_attr) {
    ReportIfSpellingLocNotIgnored(override_attr->getLocation(),
                                  diag_redundant_virtual_specifier_)
        << override_attr << final_attr
        << FixItHint::CreateRemoval(override_attr->getRange());
  }

  if (!remove_virtual && !add_override)
    return;

  // Deletion of virtual and insertion of override are tricky. The AST does not
  // expose the location of `virtual` or `=`: the former is useful when trying
  // to remove `virtual, while the latter is useful when trying to insert
  // `override`. Iterate over the tokens from |method->getBeginLoc()| until:
  // 1. A `{` not nested inside parentheses is found or
  // 2. A `=` not nested inside parentheses is found or
  // 3. A `;` not nested inside parentheses is found or
  // 4. The end of the file is found.
  SourceLocation virtual_loc;
  SourceLocation override_insertion_loc;
  // Attempt to set up the lexer in raw mode.
  std::pair<FileID, unsigned> decomposed_start =
      manager.getDecomposedLoc(method->getBeginLoc());
  bool invalid = false;
  StringRef buffer = manager.getBufferData(decomposed_start.first, &invalid);
  if (!invalid) {
    int nested_parentheses = 0;
    Lexer lexer(manager.getLocForStartOfFile(decomposed_start.first), lang_opts,
                buffer.begin(), buffer.begin() + decomposed_start.second,
                buffer.end());
    Token token;
    while (!lexer.LexFromRawLexer(token)) {
      // Found '=', ';', or '{'. No need to scan any further, since an override
      // fixit hint won't be inserted after any of these tokens.
      if ((token.is(tok::equal) || token.is(tok::semi) ||
           token.is(tok::l_brace)) &&
          nested_parentheses == 0) {
        override_insertion_loc = token.getLocation();
        break;
      }
      if (token.is(tok::l_paren)) {
        ++nested_parentheses;
      } else if (token.is(tok::r_paren)) {
        --nested_parentheses;
      } else if (token.is(tok::raw_identifier)) {
        // TODO(dcheng): Unclear if this needs to check for nested parentheses
        // as well?
        if (token.getRawIdentifier() == "virtual")
          virtual_loc = token.getLocation();
      }
    }
  }

  if (add_override && override_insertion_loc.isValid()) {
    ReportIfSpellingLocNotIgnored(override_insertion_loc,
                                  diag_method_requires_override_)
        << FixItHint::CreateInsertion(override_insertion_loc, " override");
  }
  if (remove_virtual && virtual_loc.isValid()) {
    ReportIfSpellingLocNotIgnored(
        virtual_loc, add_override ? diag_will_be_redundant_virtual_specifier_
                                  : diag_redundant_virtual_specifier_)
        << "'virtual'"
        // Slightly subtle: the else case handles both the currently and the
        // will be redundant case for override. Doing the check this way also
        // lets the plugin prioritize keeping 'final' over 'override' when both
        // are present.
        << (final_attr ? "'final'" : "'override'")
        << FixItHint::CreateRemoval(
               CharSourceRange::getTokenRange(SourceRange(virtual_loc)));
  }
}

void FindBadConstructsConsumer::CheckVirtualBodies(
    const CXXMethodDecl* method) {
  // Virtual methods should not have inline definitions beyond "{}". This
  // only matters for header files.
  if (method->hasBody() && method->hasInlineBody()) {
    if (CompoundStmt* cs = dyn_cast<CompoundStmt>(method->getBody())) {
      if (cs->size()) {
        SourceLocation loc = cs->getLBracLoc();
        // CR_BEGIN_MSG_MAP_EX and BEGIN_SAFE_MSG_MAP_EX try to be compatible
        // to BEGIN_MSG_MAP(_EX).  So even though they are in chrome code,
        // we can't easily fix them, so explicitly whitelist them here.
        bool emit = true;
        if (loc.isMacroID()) {
          SourceManager& manager = instance().getSourceManager();
          LocationType type = ClassifyLocation(manager.getSpellingLoc(loc));
          if (type == LocationType::kThirdParty || type == LocationType::kBlink)
            emit = false;
          else {
            StringRef name = Lexer::getImmediateMacroName(
                loc, manager, instance().getLangOpts());
            if (name == "CR_BEGIN_MSG_MAP_EX" ||
                name == "BEGIN_SAFE_MSG_MAP_EX")
              emit = false;
          }
        }
        if (emit)
          ReportIfSpellingLocNotIgnored(loc, diag_virtual_with_inline_body_);
      }
    }
  }
}

void FindBadConstructsConsumer::CountType(const Type* type,
                                          int* trivial_member,
                                          int* non_trivial_member,
                                          int* templated_non_trivial_member) {
  switch (type->getTypeClass()) {
    case Type::Record: {
      auto* record_decl = type->getAsCXXRecordDecl();
      // Simplifying; the whole class isn't trivial if the dtor is, but
      // we use this as a signal about complexity.
      // Note that if a record doesn't have a definition, it doesn't matter how
      // it's counted, since the translation unit will fail to build. In that
      // case, just count it as a trivial member to avoid emitting warnings that
      // might be spurious.
      if (!record_decl->hasDefinition() || record_decl->hasTrivialDestructor())
        (*trivial_member)++;
      else
        (*non_trivial_member)++;
      break;
    }
    case Type::TemplateSpecialization: {
      TemplateName name =
          dyn_cast<TemplateSpecializationType>(type)->getTemplateName();
      bool whitelisted_template = false;

      // HACK: I'm at a loss about how to get the syntax checker to get
      // whether a template is externed or not. For the first pass here,
      // just do simple string comparisons.
      if (TemplateDecl* decl = name.getAsTemplateDecl()) {
        std::string base_name = decl->getNameAsString();
        if (base_name == "basic_string")
          whitelisted_template = true;
      }

      if (whitelisted_template)
        (*non_trivial_member)++;
      else
        (*templated_non_trivial_member)++;
      break;
    }
    case Type::Elaborated: {
      CountType(dyn_cast<ElaboratedType>(type)->getNamedType().getTypePtr(),
                trivial_member,
                non_trivial_member,
                templated_non_trivial_member);
      break;
    }
    case Type::Typedef: {
      while (const TypedefType* TT = dyn_cast<TypedefType>(type)) {
        if (auto* decl = TT->getDecl()) {
          const std::string name = decl->getNameAsString();
          auto* context = decl->getDeclContext();
          if (name == "atomic_int" && context->isStdNamespace()) {
            (*trivial_member)++;
            return;
          }
          type = decl->getUnderlyingType().getTypePtr();
        }
      }
      CountType(type,
                trivial_member,
                non_trivial_member,
                templated_non_trivial_member);
      break;
    }
    default: {
      // Stupid assumption: anything we see that isn't the above is a POD
      // or reference type.
      (*trivial_member)++;
      break;
    }
  }
}

// Check |record| for issues that are problematic for ref-counted types.
// Note that |record| may not be a ref-counted type, but a base class for
// a type that is.
// If there are issues, update |loc| with the SourceLocation of the issue
// and returns appropriately, or returns None if there are no issues.
// static
FindBadConstructsConsumer::RefcountIssue
FindBadConstructsConsumer::CheckRecordForRefcountIssue(
    const CXXRecordDecl* record,
    SourceLocation& loc) {
  if (!record->hasUserDeclaredDestructor()) {
    loc = record->getLocation();
    return ImplicitDestructor;
  }

  if (CXXDestructorDecl* dtor = record->getDestructor()) {
    if (dtor->getAccess() == AS_public) {
      loc = dtor->getInnerLocStart();
      return PublicDestructor;
    }
  }

  return None;
}

// Returns true if |base| specifies one of the Chromium reference counted
// classes (base::RefCounted / base::RefCountedThreadSafe).
bool FindBadConstructsConsumer::IsRefCounted(
    const CXXBaseSpecifier* base,
    CXXBasePath& path) {
  const TemplateSpecializationType* base_type =
      dyn_cast<TemplateSpecializationType>(
          UnwrapType(base->getType().getTypePtr()));
  if (!base_type) {
    // Base-most definition is not a template, so this cannot derive from
    // base::RefCounted. However, it may still be possible to use with a
    // scoped_refptr<> and support ref-counting, so this is not a perfect
    // guarantee of safety.
    return false;
  }

  TemplateName name = base_type->getTemplateName();
  if (TemplateDecl* decl = name.getAsTemplateDecl()) {
    std::string base_name = decl->getNameAsString();

    // Check for both base::RefCounted and base::RefCountedThreadSafe.
    if (base_name.compare(0, 10, "RefCounted") == 0 &&
        GetNamespace(decl) == "base") {
      return true;
    }
  }

  return false;
}

// Returns true if |base| specifies a class that has a public destructor,
// either explicitly or implicitly.
// static
bool FindBadConstructsConsumer::HasPublicDtorCallback(
    const CXXBaseSpecifier* base,
    CXXBasePath& path,
    void* user_data) {
  // Only examine paths that have public inheritance, as they are the
  // only ones which will result in the destructor potentially being
  // exposed. This check is largely redundant, as Chromium code should be
  // exclusively using public inheritance.
  if (path.Access != AS_public)
    return false;

  CXXRecordDecl* record =
      dyn_cast<CXXRecordDecl>(base->getType()->getAs<RecordType>()->getDecl());
  SourceLocation unused;
  return None != CheckRecordForRefcountIssue(record, unused);
}

// Outputs a C++ inheritance chain as a diagnostic aid.
void FindBadConstructsConsumer::PrintInheritanceChain(const CXXBasePath& path) {
  for (CXXBasePath::const_iterator it = path.begin(); it != path.end(); ++it) {
    diagnostic().Report(it->Base->getBeginLoc(), diag_note_inheritance_)
        << it->Class << it->Base->getType();
  }
}

unsigned FindBadConstructsConsumer::DiagnosticForIssue(RefcountIssue issue) {
  switch (issue) {
    case ImplicitDestructor:
      return diag_refcounted_needs_explicit_dtor_;
    case PublicDestructor:
      return diag_refcounted_with_public_dtor_;
    case None:
      assert(false && "Do not call DiagnosticForIssue with issue None");
      return 0;
  }
  assert(false);
  return 0;
}

// Check |record| to determine if it has any problematic refcounting
// issues and, if so, print them as warnings/errors based on the current
// value of getErrorLevel().
//
// If |record| is a C++ class, and if it inherits from one of the Chromium
// ref-counting classes (base::RefCounted / base::RefCountedThreadSafe),
// ensure that there are no public destructors in the class hierarchy. This
// is to guard against accidentally stack-allocating a RefCounted class or
// sticking it in a non-ref-counted container (like std::unique_ptr<>).
void FindBadConstructsConsumer::CheckRefCountedDtors(
    SourceLocation record_location,
    CXXRecordDecl* record) {
  // Skip anonymous structs.
  if (record->getIdentifier() == NULL)
    return;

  // Determine if the current type is even ref-counted.
  CXXBasePaths refcounted_path;
  if (!record->lookupInBases(
          [this](const CXXBaseSpecifier* base, CXXBasePath& path) {
            return IsRefCounted(base, path);
          },
          refcounted_path)) {
    return;  // Class does not derive from a ref-counted base class.
  }

  // Easy check: Check to see if the current type is problematic.
  SourceLocation loc;
  RefcountIssue issue = CheckRecordForRefcountIssue(record, loc);
  if (issue != None) {
    diagnostic().Report(loc, DiagnosticForIssue(issue));
    PrintInheritanceChain(refcounted_path.front());
    return;
  }
  if (CXXDestructorDecl* dtor =
          refcounted_path.begin()->back().Class->getDestructor()) {
    if (dtor->getAccess() == AS_protected && !dtor->isVirtual()) {
      loc = dtor->getInnerLocStart();
      ReportIfSpellingLocNotIgnored(
          loc, diag_refcounted_with_protected_non_virtual_dtor_);
      return;
    }
  }

  // Long check: Check all possible base classes for problematic
  // destructors. This checks for situations involving multiple
  // inheritance, where the ref-counted class may be implementing an
  // interface that has a public or implicit destructor.
  //
  // struct SomeInterface {
  //   virtual void DoFoo();
  // };
  //
  // struct RefCountedInterface
  //    : public base::RefCounted<RefCountedInterface>,
  //      public SomeInterface {
  //  private:
  //   friend class base::Refcounted<RefCountedInterface>;
  //   virtual ~RefCountedInterface() {}
  // };
  //
  // While RefCountedInterface is "safe", in that its destructor is
  // private, it's possible to do the following "unsafe" code:
  //   scoped_refptr<RefCountedInterface> some_class(
  //       new RefCountedInterface);
  //   // Calls SomeInterface::~SomeInterface(), which is unsafe.
  //   delete static_cast<SomeInterface*>(some_class.get());
  if (!options_.check_base_classes)
    return;

  // Find all public destructors. This will record the class hierarchy
  // that leads to the public destructor in |dtor_paths|.
  CXXBasePaths dtor_paths;
  if (!record->lookupInBases(
          [](const CXXBaseSpecifier* base, CXXBasePath& path) {
            // TODO(thakis): Inline HasPublicDtorCallback() after clang roll.
            return HasPublicDtorCallback(base, path, nullptr);
          },
          dtor_paths)) {
    return;
  }

  for (CXXBasePaths::const_paths_iterator it = dtor_paths.begin();
       it != dtor_paths.end();
       ++it) {
    // The record with the problem will always be the last record
    // in the path, since it is the record that stopped the search.
    const CXXRecordDecl* problem_record = dyn_cast<CXXRecordDecl>(
        it->back().Base->getType()->getAs<RecordType>()->getDecl());

    issue = CheckRecordForRefcountIssue(problem_record, loc);

    if (issue == ImplicitDestructor) {
      diagnostic().Report(record_location,
                          diag_refcounted_needs_explicit_dtor_);
      PrintInheritanceChain(refcounted_path.front());
      diagnostic().Report(loc, diag_note_implicit_dtor_) << problem_record;
      PrintInheritanceChain(*it);
    } else if (issue == PublicDestructor) {
      diagnostic().Report(record_location, diag_refcounted_with_public_dtor_);
      PrintInheritanceChain(refcounted_path.front());
      diagnostic().Report(loc, diag_note_public_dtor_);
      PrintInheritanceChain(*it);
    }
  }
}

// Check for any problems with WeakPtrFactory class members. This currently
// only checks that any WeakPtrFactory<T> member of T appears as the last
// data member in T. We could consider checking for bad uses of
// WeakPtrFactory to refer to other data members, but that would require
// looking at the initializer list in constructors to see what the factory
// points to.
// Note, if we later add other unrelated checks of data members, we should
// consider collapsing them in to one loop to avoid iterating over the data
// members more than once.
void FindBadConstructsConsumer::CheckWeakPtrFactoryMembers(
    SourceLocation record_location,
    CXXRecordDecl* record) {
  // Skip anonymous structs.
  if (record->getIdentifier() == NULL)
    return;

  // Iterate through members of the class.
  RecordDecl::field_iterator iter(record->field_begin()),
      the_end(record->field_end());
  SourceLocation weak_ptr_factory_location;  // Invalid initially.
  for (; iter != the_end; ++iter) {
    const TemplateSpecializationType* template_spec_type =
        iter->getType().getTypePtr()->getAs<TemplateSpecializationType>();
    bool param_is_weak_ptr_factory_to_self = false;
    if (template_spec_type) {
      const TemplateDecl* template_decl =
          template_spec_type->getTemplateName().getAsTemplateDecl();
      if (template_decl && template_spec_type->getNumArgs() == 1) {
        if (template_decl->getNameAsString().compare("WeakPtrFactory") == 0 &&
            GetNamespace(template_decl) == "base") {
          // Only consider WeakPtrFactory members which are specialized for the
          // owning class.
          const TemplateArgument& arg = template_spec_type->getArg(0);
          if (arg.getAsType().getTypePtr()->getAsCXXRecordDecl() ==
              record->getTypeForDecl()->getAsCXXRecordDecl()) {
            if (!weak_ptr_factory_location.isValid()) {
              // Save the first matching WeakPtrFactory member for the
              // diagnostic.
              weak_ptr_factory_location = iter->getLocation();
            }
            param_is_weak_ptr_factory_to_self = true;
          }
        }
      }
    }
    // If we've already seen a WeakPtrFactory<OwningType> and this param is not
    // one of those, it means there is at least one member after a factory.
    if (weak_ptr_factory_location.isValid() &&
        !param_is_weak_ptr_factory_to_self) {
      ReportIfSpellingLocNotIgnored(weak_ptr_factory_location,
                                    diag_weak_ptr_factory_order_);
    }
  }
}

// Copied from BlinkGCPlugin, see crrev.com/1135333007
void FindBadConstructsConsumer::ParseFunctionTemplates(
    TranslationUnitDecl* decl) {
  if (!instance().getLangOpts().DelayedTemplateParsing)
    return;  // Nothing to do.

  std::set<FunctionDecl*> late_parsed_decls = GetLateParsedFunctionDecls(decl);
  clang::Sema& sema = instance().getSema();

  for (const FunctionDecl* fd : late_parsed_decls) {
    assert(fd->isLateTemplateParsed());

    if (instance().getSourceManager().isInSystemHeader(
            instance().getSourceManager().getSpellingLoc(fd->getLocation())))
      continue;

    // Parse and build AST for yet-uninstantiated template functions.
    clang::LateParsedTemplate* lpt = sema.LateParsedTemplateMap[fd].get();
    sema.LateTemplateParser(sema.OpaqueParser, *lpt);
  }
}

void FindBadConstructsConsumer::CheckVarDecl(clang::VarDecl* var_decl) {
  // Lambda init-captures should be ignored.
  if (var_decl->isInitCapture())
    return;

  // Check whether auto deduces to a raw pointer.
  QualType non_reference_type = var_decl->getType().getNonReferenceType();
  // We might have a case where the type is written as auto*, but the actual
  // type is deduced to be an int**. For that reason, keep going down the
  // pointee type until we get an 'auto' or a non-pointer type.
  for (;;) {
    const clang::AutoType* auto_type =
        non_reference_type->getAs<clang::AutoType>();
    if (auto_type) {
      if (auto_type->isDeduced()) {
        QualType deduced_type = auto_type->getDeducedType();
        if (!deduced_type.isNull() && deduced_type->isPointerType() &&
            !deduced_type->isFunctionPointerType()) {
          // Check if we should even be considering this type (note that there
          // should be fewer auto types than banned namespace/directory types,
          // so check this last.
          LocationType location_type =
              ClassifyLocation(var_decl->getBeginLoc());
          if (location_type != LocationType::kThirdParty) {
            // The range starts from |var_decl|'s loc start, which is the
            // beginning of the full expression defining this |var_decl|. It
            // ends, however, where this |var_decl|'s type loc ends, since
            // that's the end of the type of |var_decl|.
            // Note that the beginning source location of type loc omits cv
            // qualifiers, which is why it's not a good candidate to use for the
            // start of the range.
            clang::SourceRange range(
                var_decl->getBeginLoc(),
                var_decl->getTypeSourceInfo()->getTypeLoc().getEndLoc());
            ReportIfSpellingLocNotIgnored(range.getBegin(),
                                          diag_auto_deduced_to_a_pointer_type_)
                << FixItHint::CreateReplacement(
                       range,
                       GetAutoReplacementTypeAsString(
                           var_decl->getType(), var_decl->getStorageClass()));
          }
        }
      }
    } else if (non_reference_type->isPointerType()) {
      non_reference_type = non_reference_type->getPointeeType();
      continue;
    }
    break;
  }
}

}  // namespace chrome_checker
