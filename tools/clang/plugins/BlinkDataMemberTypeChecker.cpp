// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "BlinkDataMemberTypeChecker.h"

#include "Util.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"
#include "clang/Basic/Diagnostic.h"

using namespace clang;

namespace chrome_checker {

BlinkDataMemberTypeChecker::BlinkDataMemberTypeChecker(
    CompilerInstance& instance)
    : instance_(instance),
      diagnostic_(instance.getDiagnostics()),
      discouraged_types_({
          {"GURL", "KURL"},
          {"std::deque", "WTF::Deque"},
          {"std::map", "WTF::HashMap or WTF::LinkedHashSet"},
          {"std::multimap",
           "WTF::HashMap<K, WTF::Vector<V>> or WTF::HashCountedSet<T>"},
          {"std::multiset", "WTF::HashCountedSet<T>"},
          {"std::set", "WTF::HashSet or WTF::LinkedHashSet"},
          {"std::unordered_set", "WTF::HashSet"},
          {"std::unordered_map", "WTF::HashMap"},
          {"std::vector", "WTF::Vector"},
      }),
      included_filenames_regex_("/third_party/blink/renderer/"),
      excluded_filenames_regex_(
          "_(unit|perf)?test\\."
          "|_fuzzer\\."
          "|/testing/"
          "|/tests/"
          "|_test_helpers") {
  auto error_level = diagnostic_.getWarningsAsErrors()
                         ? DiagnosticsEngine::Error
                         : DiagnosticsEngine::Warning;

  diag_disallowed_blink_data_member_type_ = diagnostic_.getCustomDiagID(
      error_level,
      "[blink-style] '%0' is discouraged for data members in blink renderer. "
      "Use %1 if possible. If the usage is necessary, add "
      "ALLOW_DISCOURAGED_TYPE(reason) to the data member or the type alias to "
      "suppress this message.");
}

void BlinkDataMemberTypeChecker::CheckClass(SourceLocation location,
                                            const CXXRecordDecl* record) {
  std::string filename = GetFilename(instance_.getSourceManager(), location,
                                     FilenameLocationType::kSpellingLoc);
  if (!included_filenames_regex_.match(filename))
    return;
  if (excluded_filenames_regex_.match(filename))
    return;

  for (auto* field : record->fields())
    CheckField(field);
}

bool BlinkDataMemberTypeChecker::AllowsDiscouragedType(const Decl* decl) {
  for (auto* attr : decl->attrs()) {
    if (auto* annotate = dyn_cast<AnnotateAttr>(attr)) {
      if (annotate->getAnnotation() == "allow_discouraged_type")
        return true;
    }
  }
  return false;
}

void BlinkDataMemberTypeChecker::CheckField(const FieldDecl* field) {
  if (AllowsDiscouragedType(field))
    return;

  const Type* type = field->getType().getTypePtr();
  while (type) {
    if (auto* array = dyn_cast<ArrayType>(type)) {
      // Find the element type of the array type.
      type = array->getElementType().getTypePtr();
      continue;
    }
    if (auto* elaborated = dyn_cast<ElaboratedType>(type)) {
      // Find the underlying type of the elaborated type. E.g. for
      // |TypeName v;| where |TypeName| is not a built-in type, v's type is an
      // elaborated type enclosing the actual type named |TypeName|. Though
      // getAsCXXRecordDecl() of this type can return the record decl of the
      // root underlying type directly, we want to desugar the types
      // step-by-step to check the intermediate typedef types.
      type = elaborated->getNamedType().getTypePtr();
      continue;
    }

    const NamedDecl* decl = nullptr;
    if (auto* type_def = dyn_cast<TypedefType>(type)) {
      decl = type_def->getDecl();
      // We will either break the loop below if the type name is not under blink
      // namespace, or continue to check the underlying type of typedef/using.
      type = type_def->desugar().getTypePtr();
    } else if (auto* spec = dyn_cast<TemplateSpecializationType>(type)) {
      // Check cases like "std::vector<T> v;" in a template.
      decl = spec->getTemplateName().getAsTemplateDecl();
      // We may continue if the type still have an underlying type, like the
      // typedef case.
      type = spec->isSugared() ? spec->desugar().getTypePtr() : nullptr;
    } else {
      // For other kinds of types, get the root underlying type directly.
      decl = type->getAsCXXRecordDecl();
      // This will break the loop as we have found the root underlying type.
      type = nullptr;
    }

    if (!decl || AllowsDiscouragedType(decl))
      return;

    std::string type_name = decl->getQualifiedNameAsString();
    auto it = discouraged_types_.find(type_name);
    if (it != discouraged_types_.end()) {
      diagnostic_.Report(field->getLocation(),
                         diag_disallowed_blink_data_member_type_)
          << type_name << it->second;
      return;
    }

    // Skip the following conditions if we will break the loop anyway.
    if (!type)
      return;

    // Stop if the underlying type is not under blink namespace, instead of
    // finding the root underlying type. This is to allow the following case:
    // namespace cc {
    //   using LayerList = std::vector<Layer*>;
    // }
    // namespace blink {
    //   class LayerBuilder {
    //     ...
    //     // This is allowed as long as cc::LayerList is allowed in
    //     // audit_non_blink_usages.py.
    //     cc::LayerList layer_list_;
    //   };
    // Finding the root underlying type would disallow the above usage.
    if (type_name.find("blink::") != 0)
      return;

    // Similarly, stop finding the root underlying type if the intermediate
    // type is defined in a file that should not be checked, e.g. in a file
    // under third_party/blink/public/common.
    std::string filename =
        GetFilename(instance_.getSourceManager(), decl->getLocation(),
                    FilenameLocationType::kSpellingLoc);
    if (!included_filenames_regex_.match(filename))
      return;
  }
}

}  // namespace chrome_checker
