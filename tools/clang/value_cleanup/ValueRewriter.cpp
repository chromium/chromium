// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ValueRewriter.h"

#include <utility>

using namespace clang::ast_matchers;

ValueRewriter::ListValueCallback::ListValueCallback(
    std::string method,
    std::string replacement,
    std::set<clang::tooling::Replacement>* replacements)
    : method_(std::move(method)),
      replacement_(std::move(replacement)),
      replacements_(replacements) {}

void ValueRewriter::ListValueCallback::run(
    const MatchFinder::MatchResult& result) {
  auto* callExpr = result.Nodes.getNodeAs<clang::CXXMemberCallExpr>(method());

  clang::CharSourceRange call_range =
      clang::CharSourceRange::getTokenRange(callExpr->getExprLoc());
  replacements_->emplace(*result.SourceManager, call_range, replacement());
}

ValueRewriter::ValueRewriter(
    std::set<clang::tooling::Replacement>* replacements)
    : list_value_callbacks_({
          {"::base::ListValue::Clear", "GetList().clear", replacements},
          {"::base::ListValue::GetSize", "GetList().size", replacements},
          {"::base::ListValue::empty", "GetList().empty", replacements},
          {"::base::ListValue::Reserve", "GetList().reserve", replacements},
          {"::base::ListValue::AppendBoolean", "GetList().emplace_back",
           replacements},
          {"::base::ListValue::AppendInteger", "GetList().emplace_back",
           replacements},
          {"::base::ListValue::AppendDouble", "GetList().emplace_back",
           replacements},
          {"::base::ListValue::AppendString", "GetList().emplace_back",
           replacements},
      }) {}

void ValueRewriter::RegisterMatchers(MatchFinder* match_finder) {
  for (auto& callback : list_value_callbacks_) {
    match_finder->addMatcher(
        callExpr(callee(functionDecl(hasName(callback.method()))))
            .bind(callback.method()),
        &callback);
  }
}
