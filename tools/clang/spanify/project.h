// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_PROJECT_H_
#define TOOLS_CLANG_SPANIFY_PROJECT_H_

#include <string_view>
#include <vector>

#include "RawPtrHelpers.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

struct FuncMapping {
  const std::string function_name;
  const std::string replacement_function_name;
};

class Project {
 public:
  constexpr Project() = default;
  virtual ~Project() = default;
  virtual std::string_view GetSpanIncludePath() const = 0;
  virtual std::string_view GetSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result) const = 0;
  virtual std::string_view GetRawSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result) const = 0;
  virtual std::string_view GetSpanFromRefRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result) const = 0;
  virtual std::string_view GetAsByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result) const = 0;
  virtual std::string_view GetAsWritableByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result) const = 0;
  virtual std::string_view GetSafeConversionsIncludePath() const = 0;
  virtual std::string_view GetRawSpanIncludePath() const = 0;
  virtual std::string_view GetAutoSpanificationHelperIncludePath() const = 0;
  virtual const std::vector<FuncMapping>& GetFuncMappingTable() const = 0;
  virtual bool IsExcludedFromProject(const clang::Decl& Node) const = 0;
  virtual bool SupportsStaticExtent() const { return true; }
};
#endif  // TOOLS_CLANG_SPANIFY_PROJECT_H_
