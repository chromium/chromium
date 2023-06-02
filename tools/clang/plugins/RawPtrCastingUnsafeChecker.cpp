// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "RawPtrCastingUnsafeChecker.h"
#include <vector>

#include "clang/AST/DeclCXX.h"
#include "llvm/ADT/ScopeExit.h"

// Removes any pointers, references, arrays and aliases.
static const clang::Type* GetBaseType(const clang::Type* type) {
  const clang::Type* last_type = nullptr;
  while (type && type != last_type) {
    last_type = type;

    // Unwrap type aliases.
    type = type->getUnqualifiedDesugaredType();

    // Unwrap arrays and pointers.
    type = type->getPointeeOrArrayElementType();

    // Unwrap references (and pointers).
    const auto pointee = type->getPointeeType();
    if (!pointee.isNull()) {
      type = pointee.getTypePtrOrNull();
    }
  }
  return type;
}

bool CastingUnsafePredicate::IsCastingUnsafe(const clang::Type* type) const {
  return GetCastingSafety(type)->verdict_ == CastingSafety::kUnsafe;
}

std::shared_ptr<CastingSafety> CastingUnsafePredicate::GetCastingSafety(
    const clang::Type* type,
    std::set<const clang::Type*>* visited) const {
  // Retrieve a "base" type because:
  // - A pointer pointing to a casting-unsafe type IS casting-unsafe.
  // - A reference pointing to a casting-unsafe type IS casting-unsafe.
  // - An array having casting-unsafe elements IS casting-unsafe.
  const clang::Type* raw_type = GetBaseType(type);
  if (!raw_type || !raw_type->isRecordType()) {
    // We assume followings ARE NOT casting-unsafe.
    // - function type
    // - enum type
    // - builtin type
    // - complex type
    // - obj-C types
    // We should not have these types here because we desugared.
    // - using type
    // - typeof type
    return std::make_shared<CastingSafety>(type);
  }
  const clang::RecordDecl* decl = raw_type->getAsRecordDecl();
  assert(decl);

  // Use a memoized result if exists.
  auto iter = cache_.find(type);
  if (iter != cache_.end()) {
    return iter->second;
  }

  // This performs DFS on a directed graph composed of |clang::Type*|.
  // Avoid searching for visited nodes by managing |visited|, as this can lead
  // to infinite loops in the presence of self-references and cross-references.
  // Since |type| being casting-unsafe is equivalent to being able to reach node
  // |raw_ptr<T>| or node |raw_ref<T>| from node |type|, there is no need to
  // look up visited nodes again.
  bool root = visited == nullptr;
  if (root) {
    // Will be deleted as a part of |clean_up()|.
    visited = new std::set<const clang::Type*>();
  } else if (visited->count(type)) {
    // This type is already visited but not memoized,
    // therefore this node is reached by following cross-references from
    // ancestors. The safety of this node cannot be determined without waiting
    // for computation in its ancestors.
    return std::make_shared<CastingSafety>(raw_type,
                                           CastingSafety::kUndetermined);
  }
  visited->insert(type);

  auto safety = std::make_shared<CastingSafety>(raw_type);

  // Clean-up: this lambda is called automatically at the scope exit.
  const auto clean_up =
      llvm::make_scope_exit([this, &visited, &raw_type, &root, &safety] {
        if (root) {
          delete visited;
        }
        // Memoize the result if finalized.
        if (safety->verdict_ != CastingSafety::kUndetermined) {
          this->cache_.insert({raw_type, safety});
        }
      });

  const std::string record_name = decl->getQualifiedNameAsString();
  if (record_name == "base::raw_ptr" || record_name == "base::raw_ref") {
    // Base case: |raw_ptr<T>| and |raw_ref<T>| are never safe to cast to/from.
    safety->verdict_ = CastingSafety::kUnsafe;
    return safety;
  }

  // Check member fields
  for (const auto& field : decl->fields()) {
    safety->MergeSubResult(
        GetCastingSafety(field->getType().getTypePtrOrNull(), visited),
        field->getBeginLoc());

    // Verdict finalized: early return.
    if (safety->verdict_ == CastingSafety::kUnsafe) {
      return safety;
    }
  }

  // Check base classes
  const auto* cxx_decl = clang::dyn_cast<clang::CXXRecordDecl>(decl);
  if (cxx_decl && cxx_decl->hasDefinition()) {
    for (const auto& base_specifier : cxx_decl->bases()) {
      safety->MergeSubResult(
          GetCastingSafety(base_specifier.getType().getTypePtr(), visited),
          base_specifier.getBeginLoc());

      // Verdict finalized: early return.
      if (safety->verdict_ == CastingSafety::kUnsafe) {
        return safety;
      }
    }
    for (const auto& base_specifier : cxx_decl->vbases()) {
      safety->MergeSubResult(
          GetCastingSafety(base_specifier.getType().getTypePtr(), visited),
          base_specifier.getBeginLoc());

      // Verdict finalized: early return.
      if (safety->verdict_ == CastingSafety::kUnsafe) {
        return safety;
      }
    }
  }

  // All reachable types have been traversed but the root type has not
  // been marked as unsafe; therefore it must be safe.
  if (root && safety->verdict_ == CastingSafety::kUndetermined) {
    safety->verdict_ = CastingSafety::kSafe;
  }
  return safety;
}
