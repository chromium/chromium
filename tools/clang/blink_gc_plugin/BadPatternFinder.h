// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class DiagnosticsReporter;

namespace clang {
class ASTContext;
}  // namespace clang

// Detects and reports use of banned patterns, such as applying
// std::make_unique to a garbage-collected type.
void FindBadPatterns(clang::ASTContext& ast_context, DiagnosticsReporter&);
