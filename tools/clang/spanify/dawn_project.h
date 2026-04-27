// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_DAWN_PROJECT_H_
#define TOOLS_CLANG_SPANIFY_DAWN_PROJECT_H_

#include <algorithm>
#include <string>
#include <vector>

#include "RawPtrHelpers.h"
#include "Util.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/Support/Casting.h"
#include "project.h"

class DawnProject : public Project {
 public:
  constexpr DawnProject() = default;

 private:
  std::string_view GetSpanIncludePath() const override { return "<span>"; }
  std::string_view GetSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "std::span";
  }
  std::string_view GetRawSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    // TODO(crbug.com/497912213): Add a raw_span class to Dawn.
    return "std::span";
  }
  std::string_view GetSpanFromRefRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "TODO_497912213";
  }
  std::string_view GetAsByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "std::as_bytes";
  }
  std::string_view GetAsWritableByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "std::as_writable_bytes";
  }
  std::string_view GetSafeConversionsIncludePath() const override {
    return "TODO_497912213";
  }
  std::string_view GetRawSpanIncludePath() const override { return "<span>"; }
  std::string_view GetAutoSpanificationHelperIncludePath() const override {
    return "TODO_497912213";
  }

  const std::vector<FuncMapping>& GetFuncMappingTable() const override {
    static const std::vector<FuncMapping> kFuncMappingTable = {};
    return kFuncMappingTable;
  }

  bool IsExcludedFromProject(const clang::Decl& Node) const override {
    const clang::SourceManager& source_manager =
        Node.getASTContext().getSourceManager();

    std::string filename = raw_ptr_plugin::GetFilename(
        source_manager, raw_ptr_plugin::getRepresentativeLocation(Node),
        raw_ptr_plugin::FilenameLocationType::kSpellingLoc);

    return filename.find("third_party/dawn/third_party") != std::string::npos;
  }
};

#endif  // TOOLS_CLANG_SPANIFY_DAWN_PROJECT_H_
