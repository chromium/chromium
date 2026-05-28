// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

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
static const struct TestSet {
  const Curve* tests;
} testSets[] = {
    {testSet0},
};

// Expected rewrite:
// class UnnamedClasses {
//  public:
//   SkSpan<const Curve> tests;
// };
// static const auto unnamedClasses = std::to_array<UnnamedClasses>({
//     {testSet0},
// });
static const class {
 public:
  const Curve* tests;
} unnamedClasses[] = {
    {testSet0},
};

void fct() {
  const TestSet* actTest = testSets + 0;
  const Curve* ptr = actTest->tests + 0;

  const Curve* another_ptr = unnamedClasses[UnsafeIndex()].tests + 0;
}
