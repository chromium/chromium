// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "FilteredASTConsumer.h"

bool FilteredASTConsumer::HandleTopLevelDecl(clang::DeclGroupRef d) {
  // Since HandleTopLevelDecl is only called when a part of the AST is actually
  // read, this allows us to skip checking any parts of the AST that weren't
  // used.
  // eg. With modules, we have a large AST, but only read parts which were
  // actually used.
  for (clang::Decl* decl : d) {
    top_level_decls_.push_back(decl);
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