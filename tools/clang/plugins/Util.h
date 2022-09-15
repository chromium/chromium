// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_UTIL_H_
#define TOOLS_CLANG_PLUGINS_UTIL_H_

#include <string>

#include "clang/AST/DeclBase.h"

// Utility method for subclasses to determine the namespace of the
// specified record, if any. Unnamed namespaces will be identified as
// "<anonymous namespace>".
std::string GetNamespace(const clang::Decl* record);

#endif  // TOOLS_CLANG_PLUGINS_UTIL_H_
