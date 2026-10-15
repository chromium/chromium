// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The rewrite of an increment spells its operand twice, e.g. `s.ptr++` becomes
// `s.ptr = s.ptr.subspan(1u)`.

#include <utility>
#include <vector>

#include "base/containers/span.h"

struct S {
  // Expected rewrite:
  // base::span<int> ptr;
  base::span<int> ptr;
};

// Expected rewrite:
// int MemberOperand(S& s, base::span<int> buf) {
int MemberOperand(S& s, base::span<int> buf) {
  // Gives S::ptr a size, so that it can be rewritten.
  s.ptr = buf;
  // Expected rewrite:
  // s.ptr = s.ptr.subspan(1u);
  s.ptr = s.ptr.subspan(1u);
  // Expected rewrite:
  // return std::exchange(s.ptr, s.ptr.subspan(1u))[0];
  return std::exchange(s.ptr, s.ptr.subspan(1u))[0];
}

struct Item {
  // No rewrite expected: `items[i++].ptr` has side effects, which the rewrite
  // would run twice.
  int* ptr;
};

void OperandWithSideEffects(std::vector<Item>& items, int* buf) {
  // Gives Item::ptr a size: only the side effects prevent its rewrite.
  items[0].ptr = buf;
  int i = 0;
  items[i++].ptr++;
}

// Expected rewrite:
// int ParenthesizedOperand(base::span<int> ptr) {
int ParenthesizedOperand(base::span<int> ptr) {
  // Expected rewrite:
  // (ptr) = ptr.subspan(1u);
  (ptr) = ptr.subspan(1u);
  // Expected rewrite:
  // (ptr = ptr.subspan(1u));
  (ptr = ptr.subspan(1u));
  // Expected rewrite:
  // int a = std::exchange((ptr), ptr.subspan(1u))[0];
  int a = std::exchange((ptr), ptr.subspan(1u))[0];
  // Expected rewrite:
  // int b = ((ptr = ptr.subspan(1u))[0]);
  int b = ((ptr = ptr.subspan(1u))[0]);
  return a + b;
}

// A C-style cast has higher precedence than `=`, so the rewrite is
// parenthesized or a call, unless the operand is already parenthesized.
// Expected rewrite:
// void DiscardedByCast(base::span<int> ptr) {
void DiscardedByCast(base::span<int> ptr) {
  // Expected rewrite:
  // (void)std::exchange(ptr, ptr.subspan(1u));
  (void)std::exchange(ptr, ptr.subspan(1u));
  // Expected rewrite:
  // (void)(ptr = ptr.subspan(1u));
  (void)(ptr = ptr.subspan(1u));
  // Expected rewrite:
  // (void)(ptr = ptr.subspan(1u));
  (void)(ptr = ptr.subspan(1u));
  // Expected rewrite:
  // static_cast<void>(ptr = ptr.subspan(1u));
  static_cast<void>(ptr = ptr.subspan(1u));
}
