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
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "project.h"

class DawnProject : public Project {
 public:
  constexpr DawnProject() = default;

 private:
  std::string_view GetSpanIncludePath() const override {
    return "src/utils/span.h";
  }
  std::string_view GetSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "dawn::Span";
  }
  std::string_view GetRawSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    // TODO(crbug.com/497912213): Add a raw_span class to Dawn.
    return "dawn::Span";
  }
  std::string_view GetSpanFromRefRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "dawn::SpanFromRef";
  }
  std::string_view GetAsByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "dawn::SpanAsBytes";
  }
  std::string_view GetAsWritableByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    return "dawn::SpanAsWritableBytes";
  }
  std::string_view GetSafeConversionsIncludePath() const override {
    return "src/utils/numeric.h";
  }
  CheckedCastReplacement GetCheckedCastReplacement(
      clang::SourceRange range) const override {
    return CheckedCastReplacement{
        .opener = {.range = range.getBegin(),
                   .text = "dawn::checked_cast<size_t>("},
        .closer = {.range = range.getEnd(), .text = ")"}};
  }
  std::string_view GetRawSpanIncludePath() const override {
    return "src/utils/span.h";
  }
  std::string_view GetAutoSpanificationHelperIncludePath() const override {
    return "src/utils/span.h";
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
    // "third_party/dawn". We only want to spanify Dawn sources, excluding its
    // own internal third_party.
    llvm::StringRef file(filename);
    return (file.contains("third_party/") &&
            !file.contains("third_party/dawn/")) ||
           file.contains("third_party/dawn/third_party/");
  }

  bool SupportsStaticExtent() const override {
    // TODO(crbug.com/536893823): add dynamic extent support in dawn::Span
    // and enable its use by spanify here.
    return false;
  }
};

#endif  // TOOLS_CLANG_SPANIFY_DAWN_PROJECT_H_
