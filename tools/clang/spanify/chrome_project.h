// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_CHROME_PROJECT_H_
#define TOOLS_CLANG_SPANIFY_CHROME_PROJECT_H_

#include <string>
#include <vector>

#include "RawPtrHelpers.h"
#include "SeparateRepositoryPaths.h"
#include "SpanifyManualPathsToIgnore.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "project.h"

class ChromeProject : public Project {
 public:
  constexpr ChromeProject() = default;

 private:
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

  bool IsExcludedFromProject(const clang::Decl& Node) const override {
    static const raw_ptr_plugin::FilterFile excluded_paths([] {
      std::vector<std::string> paths_to_exclude_lines;
      paths_to_exclude_lines.insert(paths_to_exclude_lines.end(),
                                    kSpanifyManualPathsToIgnoreChrome.begin(),
                                    kSpanifyManualPathsToIgnoreChrome.end());

      paths_to_exclude_lines.insert(paths_to_exclude_lines.end(),
                                    kSeparateRepositoryPaths.begin(),
                                    kSeparateRepositoryPaths.end());
      return paths_to_exclude_lines;
    }());

    const clang::SourceManager& source_manager =
        Node.getASTContext().getSourceManager();

    if (raw_ptr_plugin::isNodeInThirdPartyLocation(Node, source_manager)) {
      return true;
    }

    return excluded_paths.ContainsSubstringOf(raw_ptr_plugin::GetFilename(
        source_manager, raw_ptr_plugin::getRepresentativeLocation(Node),
        raw_ptr_plugin::FilenameLocationType::kSpellingLoc));
  }
};

#endif  // TOOLS_CLANG_SPANIFY_CHROME_PROJECT_H_
