// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

namespace std::ranges {

// Mocking the basic structure of a range adaptor closure.
struct range_adaptor_closure {};

// Mock operator| to allow the expression to parse in both concrete
// and template contexts.
template <typename T>
void operator|(T&&, range_adaptor_closure) {}

// Mock std::ranges::to as a function template.
// This matches the libc++ AST structure.
template <typename Container, typename Range>
range_adaptor_closure to() {
  return {};
}

// Overload for cases where only the container is specified
template <typename Container>
range_adaptor_closure to() {
  return {};
}

}  // namespace std::ranges

void Test() {
  int x = 0;
  auto closure = std::ranges::to<std::vector<int>>();

  // This usage should trigger the warning because it matches:
  // 1. operator|
  // 2. defined in std::ranges
  x | closure;

  // This should not trigger a warning because operator| is not defined in
  // std::ranges.
  int y = 1;
  [[maybe_unused]] int z = x | y;
}

void TestConcrete() {
  std::vector<int> x;
  x | std::ranges::to<std::vector<int>>();
}

template <typename T>
void TestTemplate(T t) {
  t | std::ranges::to<T>();
}

template <typename T>
void UnresolvedLookupTest(T t) {
  t | std::ranges::to<std::vector<T>>();
}

template <typename T, typename U>
void TestDoubleTemplate(T t, U u) {
  t | u;
}

void Instantiate() {
  TestTemplate(std::vector<int>());
  UnresolvedLookupTest(std::vector<int>());
  TestDoubleTemplate(std::vector<int>(), std::ranges::to<std::vector<int>>());
}
