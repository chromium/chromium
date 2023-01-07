// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

static int kGCAddressSpace = 1;

bool IsManaged(AllocaInst* AI) {
  auto* T = AI->getType();

  // If it looks like a Handle, it probably is a Handle. This brittle way of
  // checking for managed on-stack values returns true if a single element
  // struct has a GC address-spaced pointer field.
  if (T->getElementType()->isStructTy()) {
    auto* ST = dyn_cast<StructType>(&*T->getElementType());
    if (ST->getNumElements() == 1 && ST->getElementType(0)->isPointerTy()) {
      if (ST->getElementType(0)->getPointerAddressSpace() == kGCAddressSpace)
        return true;
    }
  }
  return false;
}

namespace {
struct IdentifySafepoints : public FunctionPass {
  static char ID;
  IdentifySafepoints() : FunctionPass(ID) {}

  bool runOnFunction(Function& F) override {
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      if (auto* AI = dyn_cast_or_null<AllocaInst>(&*I)) {
        if (IsManaged(AI)) {
          F.addFnAttr("statepoint");
          return false;
        }
      }
    }
    return false;
  }
};

char IdentifySafepoints::ID;
}  // namespace

static RegisterPass<IdentifySafepoints> X("-identify-safepoints",
                                          "Identify Safepoints",
                                          false /* Only looks at CFG */,
                                          true /* Analysis Pass */);

static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
                                [](const PassManagerBuilder& Builder,
                                   legacy::PassManagerBase& PM) {
                                  PM.add(new IdentifySafepoints());
                                });
