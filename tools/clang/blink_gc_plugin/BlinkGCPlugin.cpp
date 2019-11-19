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

  bool ParseArgs(const CompilerInstance&,
                 const std::vector<std::string>& args) override {
    for (const auto& arg : args) {
      if (arg == "dump-graph") {
        options_.dump_graph = true;
      } else if (arg == "enable-weak-members-in-unmanaged-classes") {
        options_.enable_weak_members_in_unmanaged_classes = true;
      } else if (arg == "no-gc-finalized" || arg == "warn-unneeded-finalizer") {
        // TODO(bikineev): Remove after flags are removed from BUILD.gn.
        continue;
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
