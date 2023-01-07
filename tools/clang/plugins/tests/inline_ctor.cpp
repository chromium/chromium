// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define MACRO_FROM_CPP INLINE_CTORS_IN_A_MACRO(InlineCtorsInvolvingCppAreOK)

#include "inline_ctor.h"

#include <string>
#include <vector>

// We don't warn on classes that are in CPP files.
class InlineInCPPOK {
 public:
  InlineInCPPOK() {}
  ~InlineInCPPOK() {}

 private:
  std::vector<int> one_;
  std::vector<std::string> two_;
};

INLINE_CTORS_IN_A_MACRO(InlineCtorsBehindAMacroAreOKInCpp);

int main() {
  InlineInCPPOK one;
  InlineCtorsArentOKInHeader two;
  InlineCtorsBehindAMacroArentOKInHeader three;
  InlineCtorsBehindAMacroAreOKInCpp four;
  InlineCtorsInvolvingCppAreOK five;
  return 0;
}
