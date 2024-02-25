// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_UTIL_H_
#define TOOLS_CLANG_PLUGINS_UTIL_H_

#include <string>
#include <type_traits>

#include "clang/AST/DeclBase.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"

// Utility method for subclasses to determine the namespace of the
// specified record, if any. Unnamed namespaces will be identified as
// "<anonymous namespace>".
std::string GetNamespace(const clang::Decl* record);

// Attempts to determine the filename for the given SourceLocation.
// Returns an empty string if the filename could not be determined.
std::string GetFilename(const clang::SourceManager& instance,
                        clang::SourceLocation location);

// Utility method to obtain a "representative" source location polymorphically.
// We sometimes use a source location to determine a code owner has legitimate
// justification not to fix the issue found out by the plugin (e.g. the issue
// being inside system headers). Among several options to obtain a location,
// this utility aims to provide the best location which represents the node's
// essential token.
inline clang::SourceLocation getRepresentativeLocation(
    const clang::Stmt& node) {
  // clang::Stmt has T::getBeginLoc() and T::getEndLoc().
  // Usually the former one does better represent the location.
  //
  // e.g. clang::IfStmt
  // if (foo) {} else {}
  // ^                 ^
  // |                 getEndLoc()
  // getBeginLoc()
  //
  // e.g. clang::CastExpr
  // int x = static_cast<int>(123ll);
  //         ^                     ^
  //         |                     getEndLoc()
  //         getBeginLoc()
  return node.getBeginLoc();
}
inline clang::SourceLocation getRepresentativeLocation(
    const clang::TypeLoc& node) {
  // clang::TypeLoc has T::getBeginLoc() and T::getEndLoc().
  // As the former may refer to modifiers, we use the latter one.
  return node.getEndLoc();
}
inline clang::SourceLocation getRepresentativeLocation(
    const clang::Decl& node) {
  // Unlike other nodes, clang::Decl provides T::getLocation().
  // Usually, this provides more "representative" location.
  //
  // e.g. clang::FieldDecl
  //   int* field = nullptr;
  //   ^    ^       ^
  //   |    |       getEndLoc()
  //   |    getLocation()
  //   getBeginLoc()
  return node.getLocation();
}

#endif  // TOOLS_CLANG_PLUGINS_UTIL_H_
