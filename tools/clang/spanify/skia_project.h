// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_SKIA_PROJECT_H_
#define TOOLS_CLANG_SPANIFY_SKIA_PROJECT_H_

#include <string>

#include "RawPtrHelpers.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/ADT/StringRef.h"
#include "project.h"

class SkiaProject : public Project {
 public:
  constexpr SkiaProject() = default;

 private:
  std::string_view GetSpanIncludePath() const override {
    return "include/core/SkSpan.h";
  }
  std::string_view GetSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "SkSpan";
  }
  std::string_view GetRawSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "RAW_SPAN_NOT_AVAILABLE";
  }
  std::string_view GetSpanFromRefRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "SPAN_FROM_REF_NOT_AVAILABLE";
  }
  std::string_view GetAsByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "AS_BYTE_SPAN_NOT_AVAILABLE";
  }
  std::string_view GetAsWritableByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "AS_WRITABLE_BYTE_SPAN_NOT_AVAILABLE";
  }
  std::string_view GetSafeConversionsIncludePath() const override {
    return "NOT_AVAILABLE base/numerics/safe_conversions.h";
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
    const clang::SourceManager& source_manager =
        Node.getASTContext().getSourceManager();

    std::string filename = raw_ptr_plugin::GetFilename(
        source_manager, raw_ptr_plugin::getRepresentativeLocation(Node),
        raw_ptr_plugin::FilenameLocationType::kSpellingLoc);

    // Running in-place inside Chromium: absolute path contains
    // "third_party/skia". We only want to spanify Skia sources, excluding its
    // own internal third_party.
    llvm::StringRef file(filename);
    return (file.contains("third_party/") &&
            !file.contains("third_party/skia/")) ||
           file.contains("third_party/skia/third_party/");
  }
  bool SupportsStaticExtent() const override { return false; }
};

#endif  // TOOLS_CLANG_SPANIFY_SKIA_PROJECT_H_
