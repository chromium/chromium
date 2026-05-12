// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_DIAGNOSTICCONSUMER_H_
#define TOOLS_CLANG_PLUGINS_DIAGNOSTICCONSUMER_H_

#include "clang/Basic/Diagnostic.h"

namespace chrome_checker {

// A base class for forwarding diagnostic consumers that wrap another consumer
// and need to sync error/warning counts.
class ForwardingDiagnosticConsumer : public clang::DiagnosticConsumer {
 protected:
  ForwardingDiagnosticConsumer(clang::DiagnosticConsumer* next) : next_(next) {}

  ~ForwardingDiagnosticConsumer() override {
    if (next_) {
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  void clear() override {
    if (next_) {
      next_->clear();
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  void BeginSourceFile(const clang::LangOptions& opts,
                       const clang::Preprocessor* pp) override {
    if (next_) {
      next_->BeginSourceFile(opts, pp);
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  void EndSourceFile() override {
    if (next_) {
      next_->EndSourceFile();
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  bool IncludeInDiagnosticCounts() const override {
    return next_ && next_->IncludeInDiagnosticCounts();
  }

  void PassthroughDiagnostic(clang::DiagnosticsEngine::Level level,
                             const clang::Diagnostic& diag) {
    if (next_) {
      next_->HandleDiagnostic(level, diag);
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  clang::DiagnosticConsumer* next_;
};

}  // namespace chrome_checker

#endif  // TOOLS_CLANG_PLUGINS_DIAGNOSTICCONSUMER_H_
