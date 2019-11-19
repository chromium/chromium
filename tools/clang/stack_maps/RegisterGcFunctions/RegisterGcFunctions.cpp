// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/Analysis/CallGraph.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

static unsigned kGCAddressSpace = 1;

void MaybeStatepointFunction(Function* F) {
  if (F->hasFnAttribute("statepoint")) {
    if (F->hasFnAttribute("no-statepoint"))
      return;

    F->setGC("statepoint-example");
  }
}

namespace {
struct RegisterGcFunctions : public ModulePass {
  static char ID;
  RegisterGcFunctions() : ModulePass(ID) {}

  bool runOnModule(Module& M) override {
    auto GA = M.getNamedGlobal("llvm.global.annotations");
    if (GA) {
      auto a = cast<ConstantArray>(GA->getOperand(0));
      for (int i = 0; i < a->getNumOperands(); i++) {
        auto e = cast<ConstantStruct>(a->getOperand(i));

        if (auto F = dyn_cast<Function>(e->getOperand(0)->getOperand(0))) {
          auto Anno = cast<ConstantDataArray>(
                          cast<GlobalVariable>(e->getOperand(1)->getOperand(0))
                              ->getOperand(0))
                          ->getAsCString();
          F->addFnAttr(Anno);
        }
      }
    }

    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      MaybeStatepointFunction(&*F);
    }

    return false;
  }
};

char RegisterGcFunctions::ID;
}  // end of anonymous namespace

static RegisterPass<RegisterGcFunctions> X("register-gc-fns",
                                           "Register GC Functions",
                                           false /* Only looks at CFG */,
                                           true /* Analysis Pass */);

static RegisterStandardPasses Y(PassManagerBuilder::EP_ModuleOptimizerEarly,
                                [](const PassManagerBuilder& Builder,
                                   legacy::PassManagerBase& PM) {
                                  PM.add(new RegisterGcFunctions());
                                });
