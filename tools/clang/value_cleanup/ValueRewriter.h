// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Handles the rewriting of base::Value::GetType() to base::Value::type().

#ifndef TOOLS_CLANG_VALUE_CLEANUP_VALUE_REWRITER_H_
#define TOOLS_CLANG_VALUE_CLEANUP_VALUE_REWRITER_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/Refactoring.h"

class ValueRewriter {
 public:
  explicit ValueRewriter(std::set<clang::tooling::Replacement>* replacements);

  void RegisterMatchers(clang::ast_matchers::MatchFinder* match_finder);

 private:
  class ListValueCallback
      : public clang::ast_matchers::MatchFinder::MatchCallback {
   public:
    ListValueCallback(std::string method,
                      std::string replacement,
                      std::set<clang::tooling::Replacement>* replacements);

    void run(
        const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    const std::string& method() const { return method_; }
    const std::string& replacement() const { return replacement_; }

   private:
    const std::string method_;
    const std::string replacement_;
    std::set<clang::tooling::Replacement>* const replacements_;
  };

  std::vector<ListValueCallback> list_value_callbacks_;
};

#endif  // TOOLS_CLANG_VALUE_CLEANUP_VALUE_REWRITER_H_
