// Copyright 2014 The Chromium Authors
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
      } else if (arg == "enable-persistent-in-unique-ptr-check") {
        options_.enable_persistent_in_unique_ptr_check = true;
      } else if (arg == "enable-members-on-stack-check") {
        options_.enable_members_on_stack_check = true;
      } else if (arg == "enable-extra-padding-check") {
        options_.enable_extra_padding_check = true;
      } else if (arg == "disable-off-heap-collections-of-gced-check") {
        options_.enable_off_heap_collections_of_gced_check = false;
      } else if (arg == "enable-ptrs-to-traceable-check") {
        options_.enable_ptrs_to_traceable_check = true;
      } else {
        llvm::errs() << "Unknown blink-gc-plugin argument: " << arg << "\n";
        return false;
      }
    }
    return true;
  }

 private:
  BlinkGCPluginOptions options_;
};

static FrontendPluginRegistry::Add<BlinkGCPluginAction> X(
    "blink-gc-plugin",
    "Check Blink GC invariants");
