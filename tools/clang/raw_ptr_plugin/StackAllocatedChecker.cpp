// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "StackAllocatedChecker.h"

#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Frontend/CompilerInstance.h"

namespace raw_ptr_plugin {

namespace {

const char kStackAllocatedFieldError[] =
    "Non-stack-allocated type '%0' has a field '%1' which is a stack-allocated "
    "type, pointer/reference to a stack-allocated type, or template "
    "instantiation with a stack-allocated type as template parameter.";

const clang::Type* StripReferences(const clang::Type* type) {
  while (type) {
    if (type->isArrayType()) {
      type = type->getPointeeOrArrayElementType();
    } else if (type->isPointerType() || type->isReferenceType()) {
      type = type->getPointeeType().getTypePtrOrNull();
    } else {
      break;
    }
  }
  return type;
}

}  // namespace

bool StackAllocatedPredicate::IsStackAllocated(
    const clang::CXXRecordDecl* record) const {
  if (!record) {
    return false;
  }
  auto iter = cache_.find(record);
  if (iter != cache_.end()) {
    return iter->second;
  }

  bool stack_allocated = false;

  // Check member fields
  for (clang::Decl* decl : record->decls()) {
    clang::TypeAliasDecl* alias = clang::dyn_cast<clang::TypeAliasDecl>(decl);
    if (!alias) {
      continue;
    }
    if (alias->getName() == "IsStackAllocatedTypeMarker") {
      stack_allocated = true;
      break;
    }
  }

  // Check base classes
  if (record->hasDefinition()) {
    for (clang::CXXRecordDecl::base_class_const_iterator it =
             record->bases_begin();
         !stack_allocated && it != record->bases_end(); ++it) {
      clang::CXXRecordDecl* parent_record =
          it->getType().getTypePtr()->getAsCXXRecordDecl();
      stack_allocated = IsStackAllocated(parent_record);
    }
  }

  // If we don't create a cache record now, it's possible to get into infinite
  // mutual recursion between the base class check (above) and the template
  // parameter check (below).
  iter = cache_.insert({record, stack_allocated}).first;

  // Check template parameters. This is aggressive and can cause false positives
  // -- a templated class doesn't necessarily store instances of its type
  // parameters, in which case it need not be stack-allocated. In practice,
  // though, this kind of false positive is rare; and conservatively marking
  // this type as stack-allocated will catch cases where a type parameter
  // doesn't have a full type definition in the translation unit.
  if (auto* field_record_template =
          clang::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
    const auto& template_args = field_record_template->getTemplateArgs();
    for (unsigned i = 0; i < template_args.size(); i++) {
      if (template_args[i].getKind() == clang::TemplateArgument::Type) {
        const auto* type =
            StripReferences(template_args[i].getAsType().getTypePtrOrNull());
        if (type && IsStackAllocated(type->getAsCXXRecordDecl())) {
          stack_allocated = true;
        }
      }
    }
  }

  iter->second = stack_allocated;
  return stack_allocated;
}

StackAllocatedChecker::StackAllocatedChecker(clang::CompilerInstance& compiler)
    : compiler_(compiler),
      stack_allocated_field_error_signature_(
          compiler.getDiagnostics().getCustomDiagID(
              clang::DiagnosticsEngine::Error,
              kStackAllocatedFieldError)) {}

void StackAllocatedChecker::Check(clang::CXXRecordDecl* record) {
  if (!record->isCompleteDefinition()) {
    return;
  }
  // If this type is stack allocated, no need to check fields.
  if (predicate_.IsStackAllocated(record)) {
    return;
  }
  for (clang::RecordDecl::field_iterator it = record->field_begin();
       it != record->field_end(); ++it) {
    clang::FieldDecl* field = *it;
    bool ignore = false;
    for (auto annotation : field->specific_attrs<clang::AnnotateAttr>()) {
      if (annotation->getAnnotation() == "stack_allocated_ignore") {
        ignore = true;
        break;
      }
    }
    if (ignore) {
      continue;
    }
    const clang::Type* type =
        StripReferences(field->getType().getTypePtrOrNull());
    if (!type) {
      continue;
    }

    auto* field_record = type->getAsCXXRecordDecl();
    if (!field_record) {
      continue;
    }

    if (predicate_.IsStackAllocated(field_record)) {
      compiler_.getDiagnostics().Report(field->getLocation(),
                                        stack_allocated_field_error_signature_)
          << record->getName() << field->getNameAsString();
    }
  }
}

}  // namespace raw_ptr_plugin
