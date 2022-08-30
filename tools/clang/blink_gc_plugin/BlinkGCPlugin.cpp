// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This clang plugin checks various invariants of the Blink garbage
// collection infrastructure.
//
// Errors are described at:
// http://www.chromium.org/developers/blink-gc-plugin-errors

#include "BlinkGCPluginConsumer.h"
#include "BlinkGCPluginOptions.h"
#include "Config.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

using namespace clang;

class BlinkGCPluginAction : public PluginASTAction {
 public:
  BlinkGCPluginAction() {}

 protected:
  // Overridden from PluginASTAction:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& instance,
                                                 llvm::StringRef ref) override {
    return std::make_unique<BlinkGCPluginConsumer>(instance, options_);
  }

  PluginASTAction::ActionType getActionType() override {
    return CmdlineBeforeMainAction;
  }

  bool ParseArgs(const CompilerInstance&,
                 const std::vector<std::string>& args) override {
    for (const auto& arg : args) {
      if (arg == "dump-graph") {
        options_.dump_graph = true;
      } else if (arg == "enable-weak-members-in-unmanaged-classes") {
        options_.enable_weak_members_in_unmanaged_classes = true;
      } else if (arg == "enable-persistent-in-unique-ptr-check") {
        options_.enable_persistent_in_unique_ptr_check = true;
      } else if (arg == "enable-members-on-stack-check") {
        options_.enable_members_on_stack_check = true;
      } else if (arg.find("ignored-paths-for-default-malloc=") == 0) {
        options_.ignored_paths_for_default_malloc = SplitArg(arg);
      } else if (arg.find("allowed-paths-for-default-malloc=") == 0) {
        options_.allowed_paths_for_default_malloc = SplitArg(arg);
      } else {
        llvm::errs() << "Unknown blink-gc-plugin argument: " << arg << "\n";
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<std::string> SplitArg(const std::string& arg) {
    std::vector<std::string> result;
    size_t start = arg.find('=');
    assert(start != std::string::npos);
    start++;
    size_t end = arg.find(',', start);
    while (end != std::string::npos) {
      result.push_back(arg.substr(start, end));
      start = end + 1;
      end = arg.find(',', start);
    }
    result.push_back(arg.substr(start, end));
    return result;
  }

  BlinkGCPluginOptions options_;
};

static FrontendPluginRegistry::Add<BlinkGCPluginAction> X(
    "blink-gc-plugin",
    "Check Blink GC invariants");
