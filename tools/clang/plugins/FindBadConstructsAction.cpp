// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "FindBadConstructsAction.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/TimeProfiler.h"

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

}  // namespace

namespace chrome_checker {

namespace {

class PluginConsumer : public ASTConsumer {
 public:
  PluginConsumer(CompilerInstance* instance, const Options& options)
      : visitor_(*instance, options) {}

  void HandleTranslationUnit(clang::ASTContext& context) override {
    llvm::TimeTraceScope TimeScope(
        "HandleTranslationUnit for find-bad-constructs plugin");
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
    if (arg.starts_with(kExcludeFieldsArgPrefix)) {
      options_.exclude_fields_file =
          arg.substr(strlen(kExcludeFieldsArgPrefix)).str();
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
    } else if (arg == "raw-span-template-as-trivial-member") {
      options_.raw_span_template_as_trivial_member = true;
    } else if (arg == "check-stack-allocated") {
      options_.check_stack_allocated = true;
    } else if (arg == "check-ptrs-to-non-string-literals") {
      // Rewriting const char pointers was skipped for performance as they are
      // likely to point to string literals.
      //
      // This exclusion mechanism also wrongly excluded some non-string-literals
      // like `const uint8_t*` and `const int8*`.
      //
      // This flag is added to gradually re-include these types in the
      // enforcement plugin.
      //
      // TODO(https://crbug.com/331840473) Remove this flag
      // once the necessary members are rewritten and the raw_ptr enforcement
      // plugin is up to date.
      options_.check_ptrs_to_non_string_literals = true;
    } else if (arg == "check-span-fields") {
      options_.check_span_fields = true;
    } else if (arg == "enable-match-profiling") {
      options_.enable_match_profiling = true;
    } else if (arg == "span-ctor-from-string-literal") {
      options_.span_ctor_from_string_literal = true;
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
