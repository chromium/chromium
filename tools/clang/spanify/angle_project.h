// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_ANGLE_PROJECT_H_
#define TOOLS_CLANG_SPANIFY_ANGLE_PROJECT_H_

#include <string>
#include <vector>

#include "RawPtrHelpers.h"
#include "Util.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "project.h"

class AngleProject : public Project {
 public:
  constexpr AngleProject() = default;
  std::string_view GetSpanIncludePath() const override {
    return "common/span.h";
  }
  std::string_view GetSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "angle::Span";
  }
  std::string_view GetRawSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "RAW_SPAN_NOT_AVAILABLE";
  }
  std::string_view GetSpanFromRefRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "angle::span_from_ref";
  }
  std::string_view GetAsByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "angle::as_byte_span";
  }
  std::string_view GetAsWritableByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "angle::as_writable_byte_span";
  }
  std::string_view GetSafeConversionsIncludePath() const override {
    return "common/base/anglebase/numerics/safe_conversions.h";
  }
  std::string_view GetRawSpanIncludePath() const override {
    return "PATH_TO_RAW_SPAN_H_NOT_AVAILABLE";
  }
  std::string_view GetAutoSpanificationHelperIncludePath() const override {
    return "NOT_AVAILABLE base/containers/auto_spanification_helper.h";
  }
  const std::vector<FuncMapping>& GetFuncMappingTable() const override {
    static const std::vector<FuncMapping> kFuncMappingTable = {};
    return kFuncMappingTable;
  }
  bool IsExcludedFromProject(const clang::Decl& Node) const override {
    const clang::SourceManager& sm = Node.getASTContext().getSourceManager();

    std::string filename = raw_ptr_plugin::GetFilename(
        sm, raw_ptr_plugin::getRepresentativeLocation(Node),
        raw_ptr_plugin::FilenameLocationType::kSpellingLoc);

    return filename.find("third_party/angle/third_party") !=
               std::string::npos ||
           filename.find("third_party/angle/src/third_party") !=
               std::string::npos;
  }
};

#endif  // TOOLS_CLANG_SPANIFY_ANGLE_PROJECT_H_
