// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_DAWN_PROJECT_H_
#define TOOLS_CLANG_SPANIFY_DAWN_PROJECT_H_

#include <algorithm>
#include <string>
#include <vector>

#include "RawPtrHelpers.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/Support/Casting.h"
#include "project.h"

class DawnProject : public Project {
 public:
  constexpr DawnProject() = default;
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

  raw_ptr_plugin::FilterFile PathsToExclude() const override {
    return raw_ptr_plugin::FilterFile({});
  }

  bool IsExcludedFromProject(
      const clang::Decl& Node,
      clang::ast_matchers::internal::ASTMatchFinder* Finder,
      clang::ast_matchers::internal::BoundNodesTreeBuilder* Builder,
      const raw_ptr_plugin::FilterFile* excluded_paths) const override {
    const raw_ptr_plugin::FilterFile paths_to_include(
        {"third_party/dawn/src/"});
    using clang::ast_matchers::allOf;
    using clang::ast_matchers::decl;
    using clang::ast_matchers::unless;
    auto matcher =
        decl(allOf(raw_ptr_plugin::isInThirdPartyLocation(),
                   unless(raw_ptr_plugin::isInLocationListedInFilterFile(
                       &paths_to_include))));
    return matcher.matches(Node, Finder, Builder);
  }
};

#endif  // TOOLS_CLANG_SPANIFY_DAWN_PROJECT_H_
