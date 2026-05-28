// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "include/core/SkSpan.h"

typedef int Curve;

int UnsafeIndex();

static const Curve testSet0[] = {0};

// Expected rewrite:
// struct TestSet {
//   SkSpan<const Curve> tests;
// };
// static const auto testSets = std::to_array<TestSet>({
//     {testSet0},
// });
struct TestSet {
  SkSpan<const Curve> tests;
};
static const auto testSets = std::to_array<TestSet>({
    {testSet0},
});

// Expected rewrite:
// class UnnamedClasses {
//  public:
//   SkSpan<const Curve> tests;
// };
// static const auto unnamedClasses = std::to_array<UnnamedClasses>({
//     {testSet0},
// });
class UnnamedClasses {
 public:
  SkSpan<const Curve> tests;
};
static const auto unnamedClasses = std::to_array<UnnamedClasses>({
    {testSet0},
});

void fct() {
  const TestSet* actTest =
      SkSpan<const struct TestSet>(testSets).subspan(0u).data();
  const Curve* ptr = actTest->tests.subspan(0u).data();

  const Curve* another_ptr =
      unnamedClasses[UnsafeIndex()].tests.subspan(0u).data();
}
