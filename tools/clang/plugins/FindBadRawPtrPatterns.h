// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_FINDBADRAWPTRPATTERNS_H_
#define TOOLS_CLANG_PLUGINS_FINDBADRAWPTRPATTERNS_H_

#include "Options.h"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/CompilerInstance.h"

namespace chrome_checker {

void FindBadRawPtrPatterns(Options options,
                           clang::ASTContext& ast_context,
                           clang::CompilerInstance& compiler);

}  // namespace chrome_checker

#endif  // TOOLS_CLANG_PLUGINS_FINDBADRAWPTRPATTERNS_H_
