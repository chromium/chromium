// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_STACKALLOCATEDCHECKER_H_
#define TOOLS_CLANG_PLUGINS_STACKALLOCATEDCHECKER_H_

#include <map>
#include <vector>

namespace clang {
class CompilerInstance;
class CXXRecordDecl;
class FieldDecl;
}  // namespace clang

namespace chrome_checker {

// This verifies usage of classes annotated with STACK_ALLOCATED().
// Specifically, it ensures that an instance of such a class cannot be used as a
// member variable in a non-STACK_ALLOCATED() class.
class StackAllocatedChecker {
 public:
  explicit StackAllocatedChecker(clang::CompilerInstance& compiler);

  void Check(clang::CXXRecordDecl* record);

 private:
  bool IsStackAllocated(clang::CXXRecordDecl* record);

  clang::CompilerInstance& compiler_;
  unsigned stack_allocated_field_error_signature_;
  std::map<clang::CXXRecordDecl*, bool> cache_;
};

}  // namespace chrome_checker

#endif  // TOOLS_CLANG_PLUGINS_STACKALLOCATEDCHECKER_H_
