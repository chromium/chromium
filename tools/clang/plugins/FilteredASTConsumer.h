// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_FILTEREDASTCONSUMER_H_
#define TOOLS_CLANG_PLUGINS_FILTEREDASTCONSUMER_H_

#include <vector>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclGroup.h"

// FilteredASTConsumer should ideally remove dependencies from the parts of the
// AST we consume, leaving us with only the cc file and header file currently
// being compiled.
// In practice, it may not be able to remove all dependencies, but should still
// filter out much of it.
class FilteredASTConsumer : public clang::ASTConsumer {
 public:
  bool HandleTopLevelDecl(clang::DeclGroupRef d) override;

  void ApplyFilter(clang::ASTContext& context);

 private:
  std::vector<clang::Decl*> top_level_decls_;
};

#endif  // TOOLS_CLANG_PLUGINS_FILTEREDASTCONSUMER_H_
