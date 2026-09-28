// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_FILTEREDASTCONSUMER_H_
#define TOOLS_CLANG_PLUGINS_FILTEREDASTCONSUMER_H_

#include <vector>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclGroup.h"

namespace clang {
class Preprocessor;
}  // namespace clang

// FilteredASTConsumer should ideally remove dependencies from the parts of the
// AST we consume, leaving us with only the cc file and header file currently
// being compiled.
// In practice, it may not be able to remove all dependencies, but should still
// filter out much of it.
class FilteredASTConsumer : public clang::ASTConsumer {
 public:
  bool HandleTopLevelDecl(clang::DeclGroupRef d) override;

  void ApplyFilter(clang::ASTContext& context);

 protected:
  // Also filters out top-level declarations in system headers (libc++ and the
  // SDK: usually around half of the AST). Only for plugins that never report
  // anything for code that is spelled in a system header. Has to be called
  // before parsing starts.
  //
  // Code spelled in a non-system file can be part of a declaration in a system
  // header, if the system header expands a macro that is defined in a
  // non-system file. Such declarations are not filtered out.
  void SkipSystemHeaders(clang::Preprocessor& preprocessor);

 private:
  class NonSystemCodeInSystemHeaderFinder;

  bool IsFilteredOutSystemHeaderDecl(const clang::Decl* decl) const;

  std::vector<clang::Decl*> top_level_decls_;

  // Null unless SkipSystemHeaders() was called. Owned by the preprocessor.
  NonSystemCodeInSystemHeaderFinder* non_system_code_finder_ = nullptr;
};

#endif  // TOOLS_CLANG_PLUGINS_FILTEREDASTCONSUMER_H_
