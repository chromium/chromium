// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "FindBadConstructsAction.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include "FindBadConstructsConsumer.h"

using namespace clang;

namespace {

// Name of a cmdline parameter that can be used to specify a file listing fields
// that should not be rewritten to use raw_ptr<T>.
//
// See also:
// - OutputSectionHelper
// - FilterFile
const char kExcludeFieldsArgPrefix[] = "exclude-fields=";

// Name of a cmdline parameter that can be used to add a regular expressions
// that matches paths that should be excluded from the raw pointer usage checks.
const char kRawPtrExcludePathArgPrefix[] = "raw-ptr-exclude-path=";

// Name of a cmdline parameter that can be used to add a regular expressions
// that matches paths that should be excluded from the bad raw_ptr casts checks.
const char kBadRawPtrCastExcludePathArgPrefix[] =
    "check-bad-raw-ptr-cast-exclude-path=";

// Name of a cmdline parameter that can be used to add a regular expressions
// that matches function names that should be excluded from the bad raw_ptr cast
// checks. All implicit casts in CallExpr to the specified functions are
// excluded from the check. Use if you know that function does not break a
// reference count.
const char kCheckBadRawPtrCastExcludeFuncArgPrefix[] =
    "check-bad-raw-ptr-cast-exclude-func=";

}  // namespace

namespace chrome_checker {

namespace {

class PluginConsumer : public ASTConsumer {
 public:
  PluginConsumer(CompilerInstance* instance, const Options& options)
      : visitor_(*instance, options) {}

  void HandleTranslationUnit(clang::ASTContext& context) override {
    visitor_.Traverse(context);
  }

 private:
  FindBadConstructsConsumer visitor_;
};

}  // namespace

FindBadConstructsAction::FindBadConstructsAction() {
}

std::unique_ptr<ASTConsumer> FindBadConstructsAction::CreateASTConsumer(
    CompilerInstance& instance,
    llvm::StringRef ref) {
  return std::make_unique<PluginConsumer>(&instance, options_);
}

bool FindBadConstructsAction::ParseArgs(const CompilerInstance& instance,
                                        const std::vector<std::string>& args) {
  for (llvm::StringRef arg : args) {
    if (arg.startswith(kExcludeFieldsArgPrefix)) {
      options_.exclude_fields_file =
          arg.substr(strlen(kExcludeFieldsArgPrefix)).str();
    } else if (arg.startswith(kRawPtrExcludePathArgPrefix)) {
      options_.raw_ptr_paths_to_exclude_lines.push_back(
          arg.substr(strlen(kRawPtrExcludePathArgPrefix)).str());
    } else if (arg.startswith(kCheckBadRawPtrCastExcludeFuncArgPrefix)) {
      options_.check_bad_raw_ptr_cast_exclude_funcs.push_back(
          arg.substr(strlen(kCheckBadRawPtrCastExcludeFuncArgPrefix)).str());
    } else if (arg.startswith(kBadRawPtrCastExcludePathArgPrefix)) {
      options_.check_bad_raw_ptr_cast_exclude_paths.push_back(
          arg.substr(strlen(kBadRawPtrCastExcludePathArgPrefix)).str());
    } else if (arg == "check-base-classes") {
      // TODO(rsleevi): Remove this once http://crbug.com/123295 is fixed.
      options_.check_base_classes = true;
    } else if (arg == "check-blink-data-member-type") {
      options_.check_blink_data_member_type = true;
    } else if (arg == "check-ipc") {
      options_.check_ipc = true;
    } else if (arg == "check-layout-object-methods") {
      options_.check_layout_object_methods = true;
    } else if (arg == "raw-ref-template-as-trivial-member") {
      options_.raw_ref_template_as_trivial_member = true;
    } else if (arg == "check-bad-raw-ptr-cast") {
      options_.check_bad_raw_ptr_cast = true;
    } else if (arg == "check-raw-ptr-fields") {
      options_.check_raw_ptr_fields = true;
    } else if (arg == "check-raw-ptr-to-stack-allocated") {
      options_.check_raw_ptr_to_stack_allocated = true;
    } else if (arg == "check-stack-allocated") {
      options_.check_stack_allocated = true;
    } else if (arg == "check-raw-ref-fields") {
      options_.check_raw_ref_fields = true;
    } else if (arg == "raw-ptr-fix-crbug-1449812") {
      // TODO(mikt): Now enabled by default. Remove this path.
    } else {
      llvm::errs() << "Unknown clang plugin argument: " << arg << "\n";
      return false;
    }
  }

  return true;
}

}  // namespace chrome_checker

static FrontendPluginRegistry::Add<chrome_checker::FindBadConstructsAction> X(
    "find-bad-constructs",
    "Finds bad C++ constructs");
