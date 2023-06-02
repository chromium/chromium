// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Util.h"

#include <algorithm>

#include "clang/AST/Decl.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

namespace {

std::string GetNamespaceImpl(const clang::DeclContext* context,
                             const std::string& candidate) {
  switch (context->getDeclKind()) {
    case clang::Decl::TranslationUnit: {
      return candidate;
    }
    case clang::Decl::Namespace: {
      const auto* decl = llvm::dyn_cast<clang::NamespaceDecl>(context);
      std::string name_str;
      llvm::raw_string_ostream OS(name_str);
      if (decl->isAnonymousNamespace())
        OS << "<anonymous namespace>";
      else
        OS << *decl;
      return GetNamespaceImpl(context->getParent(), OS.str());
    }
    default: { return GetNamespaceImpl(context->getParent(), candidate); }
  }
}

}  // namespace

std::string GetNamespace(const clang::Decl* record) {
  return GetNamespaceImpl(record->getDeclContext(), std::string());
}

std::string GetFilename(const clang::SourceManager& source_manager,
                        clang::SourceLocation location) {
  clang::SourceLocation spelling_location =
      source_manager.getSpellingLoc(location);
  clang::PresumedLoc ploc = source_manager.getPresumedLoc(spelling_location);
  if (ploc.isInvalid()) {
    // If we're in an invalid location, we're looking at things that aren't
    // actually stated in the source.
    return "";
  }

  std::string name = ploc.getFilename();

  // File paths can have separators which differ from ones at this platform.
  // Make them consistent.
  std::replace(name.begin(), name.end(), '\\', '/');
  return name;
}
