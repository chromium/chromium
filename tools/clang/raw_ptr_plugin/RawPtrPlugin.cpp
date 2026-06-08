// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "RawPtrPlugin.h"

#include <algorithm>

#include "FindBadRawPtrPatterns.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/TimeProfiler.h"
#include "tools/clang/plugins/FilteredASTConsumer.h"

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

// Name of a cmdline parameter that can be used to specify a file with
// config options.
//
// See also:
// - RawPtrPluginConfig
const char kRawPtrPluginConfigPathArgPrefix[] = "config-path=";

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

namespace raw_ptr_plugin {

namespace {

class PluginConsumer : public FilteredASTConsumer {
 public:
  PluginConsumer(CompilerInstance* instance,
                 const Options& options,
                 const RawPtrPluginConfig& config)
      : options_(options), config_(config), instance_(*instance) {}

  void HandleTranslationUnit(clang::ASTContext& context) override {
    llvm::TimeTraceScope TimeScope("HandleTranslationUnit for raw-ptr plugin");
    ApplyFilter(context);

    if (options_.check_bad_raw_ptr_cast || options_.check_raw_ptr_fields ||
        options_.check_raw_ref_fields || config_.container_config.enabled ||
        (options_.check_raw_ptr_to_stack_allocated &&
         !options_.disable_check_raw_ptr_to_stack_allocated_error) ||
        options_.check_span_fields) {
      FindBadRawPtrPatterns(options_, config_, context, instance_);
    }
  }

 private:
  // Options.
  const Options options_;
  const RawPtrPluginConfig config_;

  clang::CompilerInstance& instance_;
};

}  // namespace

RawPtrPlugin::RawPtrPlugin() {}

std::unique_ptr<ASTConsumer> RawPtrPlugin::CreateASTConsumer(
    CompilerInstance& instance,
    llvm::StringRef ref) {
  return std::make_unique<PluginConsumer>(&instance, options_, config_);
}

bool LoadPluginConfig(const clang::CompilerInstance& instance,
                      llvm::StringRef file_path,
                      ParsedRawPtrPluginConfig& config) {
  auto buffer = llvm::MemoryBuffer::getFile(file_path);

  if (!buffer) {
    unsigned diag_id = instance.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "raw-ptr-plugin: Could not open config file %0: %1");
    instance.getDiagnostics().Report(diag_id)
        << file_path << buffer.getError().message();
    return false;
  }

  llvm::yaml::Input yin(buffer.get()->getBuffer());
  yin >> config;

  if (yin.error()) {
    unsigned diag_id = instance.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "raw-ptr-plugin: YAML error in %0: %1");
    instance.getDiagnostics().Report(diag_id)
        << file_path << yin.error().message();
    return false;
  }

  return true;
}

bool RawPtrPlugin::ParseArgs(const CompilerInstance& instance,
                             const std::vector<std::string>& args) {
  bool config_path_provided = false;
  std::string config_path;
  bool check_containers = false;
  for (llvm::StringRef arg : args) {
    if (arg.starts_with(kExcludeFieldsArgPrefix)) {
      options_.exclude_fields_file =
          arg.substr(strlen(kExcludeFieldsArgPrefix)).str();
    } else if (arg.starts_with(kRawPtrExcludePathArgPrefix)) {
      options_.raw_ptr_paths_to_exclude_lines.push_back(
          arg.substr(strlen(kRawPtrExcludePathArgPrefix)).str());
    } else if (arg.starts_with(kRawPtrPluginConfigPathArgPrefix)) {
      config_path_provided = true;
      config_path = arg.substr(strlen(kRawPtrPluginConfigPathArgPrefix)).str();
    } else if (arg.starts_with(kCheckBadRawPtrCastExcludeFuncArgPrefix)) {
      options_.check_bad_raw_ptr_cast_exclude_funcs.push_back(
          arg.substr(strlen(kCheckBadRawPtrCastExcludeFuncArgPrefix)).str());
    } else if (arg.starts_with(kBadRawPtrCastExcludePathArgPrefix)) {
      options_.check_bad_raw_ptr_cast_exclude_paths.push_back(
          arg.substr(strlen(kBadRawPtrCastExcludePathArgPrefix)).str());
    } else if (arg == "check-bad-raw-ptr-cast") {
      options_.check_bad_raw_ptr_cast = true;
    } else if (arg == "check-raw-ptr-fields") {
      options_.check_raw_ptr_fields = true;
    } else if (arg == "check-raw-ptr-to-stack-allocated") {
      options_.check_raw_ptr_to_stack_allocated = true;
    } else if (arg == "disable-check-raw-ptr-to-stack-allocated-error") {
      options_.disable_check_raw_ptr_to_stack_allocated_error = true;
    } else if (arg == "check-raw-ref-fields") {
      options_.check_raw_ref_fields = true;
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
    } else if (arg == "no-special-treatment-for-oilpanized-paths") {
      options_.explicitly_ignore_oilpanized_paths = false;
    } else if (arg == "check-containers") {
      check_containers = true;
    } else {
      llvm::errs() << "Unknown clang plugin argument: " << arg << "\n";
      return false;
    }
  }

  if (config_path_provided) {
    ParsedRawPtrPluginConfig parsed_config;
    if (!LoadPluginConfig(instance, config_path, parsed_config)) {
      return false;
    }
    config_ = RawPtrPluginConfig::Default();
    std::string err = config_.Merge(parsed_config);
    if (!err.empty()) {
      unsigned diag_id = instance.getDiagnostics().getCustomDiagID(
          clang::DiagnosticsEngine::Error, "raw-ptr-plugin: %0");
      instance.getDiagnostics().Report(diag_id) << err;
      return false;
    }
  } else if (check_containers) {
    config_ = RawPtrPluginConfig::Default();
  }

  return true;
}

}  // namespace raw_ptr_plugin

static FrontendPluginRegistry::Add<raw_ptr_plugin::RawPtrPlugin> X(
    "raw-ptr-plugin",
    "Check pointers for safety");
