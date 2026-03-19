// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_CHROME_PROJECT_H_
#define TOOLS_CLANG_SPANIFY_CHROME_PROJECT_H_

#include <string>

#include "RawPtrHelpers.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "project.h"

class ChromeProject : public Project {
 public:
  constexpr ChromeProject() = default;
  std::string_view GetSpanIncludePath() const override {
    return "base/containers/span.h";
  }
  std::string_view GetSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "base::span";
  }
  std::string_view GetRawSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "base::raw_span";
  }
  std::string_view GetSpanFromRefRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "base::span_from_ref";
  }
  std::string_view GetAsByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "base::as_byte_span";
  }
  std::string_view GetAsWritableByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "base::as_writable_byte_span";
  }
  std::string_view GetSafeConversionsIncludePath() const override {
    return "base/numerics/safe_conversions.h";
  }
  std::string_view GetRawSpanIncludePath() const override {
    return "base/memory/raw_span.h";
  }
  std::string_view GetAutoSpanificationHelperIncludePath() const override {
    return "base/containers/auto_spanification_helper.h";
  }
  const std::vector<FuncMapping>& GetFuncMappingTable() const override {
    static const std::vector<FuncMapping> kFuncMappingTable = {
        {"std::begin", "base::SpanificationArrayBegin"},
        {"std::end", "base::SpanificationArrayEnd"},
        {"std::cbegin", "base::SpanificationArrayCBegin"},
        {"std::cend", "base::SpanificationArrayCEnd"},
    };
    return kFuncMappingTable;
  }
  raw_ptr_plugin::FilterFile PathsToExclude() const override {
    std::vector<std::string> paths_to_exclude_lines;
    paths_to_exclude_lines.insert(paths_to_exclude_lines.end(),
                                  kSpanifyManualPathsToIgnoreChrome.begin(),
                                  kSpanifyManualPathsToIgnoreChrome.end());

    paths_to_exclude_lines.insert(paths_to_exclude_lines.end(),
                                  kSeparateRepositoryPaths.begin(),
                                  kSeparateRepositoryPaths.end());
    return raw_ptr_plugin::FilterFile(paths_to_exclude_lines);
  }
  bool IsExcludedFromProject(
      const clang::Decl& Node,
      clang::ast_matchers::internal::ASTMatchFinder* Finder,
      clang::ast_matchers::internal::BoundNodesTreeBuilder* Builder,
      const raw_ptr_plugin::FilterFile* excluded_paths) const override {
    using clang::ast_matchers::anyOf;
    using clang::ast_matchers::decl;
    auto matcher = decl(
        anyOf(raw_ptr_plugin::isInThirdPartyLocation(),
              raw_ptr_plugin::isInLocationListedInFilterFile(excluded_paths)));
    return matcher.matches(Node, Finder, Builder);
  }
};

#endif  // TOOLS_CLANG_SPANIFY_CHROME_PROJECT_H_
