// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Util.h"

#include "clang/AST/Decl.h"
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
