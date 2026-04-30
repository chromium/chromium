// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_WEBRTC_PROJECT_H_
#define TOOLS_CLANG_SPANIFY_WEBRTC_PROJECT_H_

#include <string>

#include "RawPtrHelpers.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "project.h"

class WebrtcProject : public Project {
 public:
  constexpr WebrtcProject() = default;
  std::string_view GetSpanIncludePath() const override { return "<span>"; }
  std::string_view GetSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "std::span";
  }
  std::string_view GetRawSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "std::span";
  }
  std::string_view GetSpanFromRefRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "SPAN_FROM_REF_NOT_AVAILABLE";
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
    return "rtc_base/numerics/safe_conversions.h";
  }
  std::string_view GetRawSpanIncludePath() const override { return "<span>"; }
  std::string_view GetAutoSpanificationHelperIncludePath() const override {
    return "NOT_AVAILABLE rtc_base/containers/auto_spanification_helper.h";
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

    return filename.find("third_party/webrtc/third_party") != std::string::npos;
  }
};

#endif  // TOOLS_CLANG_SPANIFY_WEBRTC_PROJECT_H_
