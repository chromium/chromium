// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_SPANIFY_PARTITION_ALLOC_PROJECT_H_
#define TOOLS_CLANG_SPANIFY_PARTITION_ALLOC_PROJECT_H_

#include <algorithm>
#include <string>
#include <vector>

#include "RawPtrHelpers.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/Support/Casting.h"
#include "project.h"

class PartitionAllocProject : public Project {
 public:
  constexpr PartitionAllocProject() = default;
  std::string_view GetSpanIncludePath() const override {
    return "partition_alloc/partition_alloc_base/containers/span.h";
  }
  std::string_view GetSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    static constexpr std::string_view kSpanFullPaths =
        "partition_alloc::internal::base::span";
    size_t index = CommonNamespaceLength(result);
    return kSpanFullPaths.substr(index);
  }
  std::string_view GetRawSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    static constexpr std::string_view kRawSpan =
        "partition_alloc::internal::base::raw_span";
    size_t index = CommonNamespaceLength(result);
    return kRawSpan.substr(index);
  }
  std::string_view GetSpanFromRefRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    static constexpr std::string_view kSpanFromRef =
        "partition_alloc::internal::base::span_from_ref";
    size_t index = CommonNamespaceLength(result);
    return kSpanFromRef.substr(index);
  }
  std::string_view GetAsByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    static constexpr std::string_view kAsByteSpan =
        "partition_alloc::internal::base::as_byte_span";
    size_t index = CommonNamespaceLength(result);
    return kAsByteSpan.substr(index);
  }
  std::string_view GetAsWritableByteSpanRelativePath(
      const clang::ast_matchers::MatchFinder::MatchResult& result)
      const override {
    static constexpr std::string_view kAsWritableByteSpan =
        "partition_alloc::internal::base::as_writable_byte_span";
    size_t index = CommonNamespaceLength(result);
    return kAsWritableByteSpan.substr(index);
  }
  std::string_view GetSafeConversionsIncludePath() const override {
    return "partition_alloc/partition_alloc_base/numerics/safe_conversions.h";
  }
  std::string_view GetRawSpanIncludePath() const override {
    return "partition_alloc/partition_alloc_base/memory/raw_span.h";
  }
  std::string_view GetAutoSpanificationHelperIncludePath() const override {
    return "partition_alloc/partition_alloc_base/containers/"
           "auto_spanification_helper.h";
  }

  const std::vector<FuncMapping>& GetFuncMappingTable() const override {
    static const std::vector<FuncMapping> kFuncMappingTable = {};
    return kFuncMappingTable;
  }

  bool IsExcludedFromProject(const clang::Decl& Node) const override {
    // No known dependencies of partition_alloc to exclude at this time.
    return false;
  }

 private:
  size_t CommonNamespaceLength(
      const clang::ast_matchers::MatchFinder::MatchResult& result) const {
    const clang::DeclContext* context = nullptr;
    for (auto const& [name, node] : result.Nodes.getMap()) {
      if (const auto* d = node.get<clang::Decl>()) {
        context = d->getDeclContext();
        break;
      }
    }

    if (!context) {
      return 0;  // Default to full qualification.
    }

    std::vector<std::string> namespaces;
    const clang::DeclContext* current = context;
    while (current) {
      if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(current)) {
        if (!ns->isAnonymousNamespace() && !ns->isInline()) {
          namespaces.push_back(ns->getNameAsString());
        }
      }
      current = current->getParent();
    }
    std::reverse(namespaces.begin(), namespaces.end());

    static const std::vector<std::string> kTargetNamespaces = {
        "partition_alloc", "internal", "base"};
    size_t total_length = 0;
    auto current_namespace_it = namespaces.begin();
    auto target_namespace_it = kTargetNamespaces.begin();
    while (current_namespace_it != namespaces.end() &&
           target_namespace_it != kTargetNamespaces.end()) {
      if (*current_namespace_it == *target_namespace_it) {
        // We match, add the size.
        total_length += current_namespace_it->size();
        ++current_namespace_it;
        ++target_namespace_it;
        // for each namespace it is need to add +2 for the '::' separating
        // namespaces. It is also true for the last namespace because it
        // separates namespace from the type. For example:
        // base::span
        //  ^     ^
        //  ns    type
        total_length += 2;
      } else {
        // We no longer match so we've found our target length.
        break;
      }
    }
    return total_length;
  }
};

#endif  // TOOLS_CLANG_SPANIFY_PARTITION_ALLOC_PROJECT_H_
