// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "FilteredASTConsumer.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Basic/LLVM.h"

bool FilteredASTConsumer::HandleTopLevelDecl(clang::DeclGroupRef d) {
  // Since HandleTopLevelDecl is only called when a part of the AST is actually
  // read, this allows us to skip checking any parts of the AST that weren't
  // used.
  // eg. With modules, we have a large AST, but only read parts which were
  // actually used.

  for (clang::Decl* decl : d) {
    // If I write code like the following:
    // template <typename T> void foo()
    // foo<int>();
    // Then foo appears twice in the top-level decls - once as a template, and
    // once as a template instantiation.
    // We want to skip the template instantiation for 2 reasons:
    // 1) Performance
    // 2) Coding style should be based on written code, not generated code. For
    //    example, `auto foo = T()` is good code, but if we use the template
    //    instantiotion of `T=int*`, then it would look like
    //    `auto foo = int*()`, which is incorrect (it should be `auto* foo`).
    auto kind = clang::TemplateSpecializationKind::TSK_Undeclared;
    if (auto* fd = clang::dyn_cast<clang::FunctionDecl>(decl)) {
      kind = fd->getTemplateSpecializationKind();
    } else if (auto* crd = clang::dyn_cast<clang::CXXRecordDecl>(decl)) {
      kind = crd->getTemplateSpecializationKind();
    }
    if (!isTemplateInstantiation(kind)) {
      top_level_decls_.push_back(decl);
    }
  }
  return true;
}

void FilteredASTConsumer::ApplyFilter(clang::ASTContext& context) {
  // The traversal scope defaults to the whole AST. When using clang modules,
  // this includes parts of the AST that were available but never read due to
  // PCM files being lazy.

  // Note: This can only run once all top-level decls are complete.
  // TODO(crbug.com/425542181): One optimization we could consider making in
  // the future is to determine the main cc file and main header file, and
  // filter to only scan those.
  context.setTraversalScope(top_level_decls_);
}