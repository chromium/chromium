// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Arrays declared together with other declarators are not rewritten: the edit
// starts at the shared type and would delete the siblings.

#include <array>

unsigned UnsafeIndex();

void SiblingLocals() {
  // No rewrite expected: rewriting `max` would delete `mu` and `min`.
  int mu[3], min[3], max[3];
  mu[UnsafeIndex()] = 0;
  min[UnsafeIndex()] = 0;
  max[UnsafeIndex()] = 0;
}

void LoneLocal() {
  // Expected rewrite:
  // std::array<int, 3> only;
  std::array<int, 3> only;
  only[UnsafeIndex()] = 0;
}

struct SiblingFields {
  // No rewrite expected: rewriting `refs` would delete `head` and `tail`.
  int head, tail, refs[4];
};

void UseSiblingFields(SiblingFields& s) {
  s.refs[UnsafeIndex()] = 0;
}

struct LoneField {
  // Expected rewrite:
  // std::array<int, 4> refs;
  std::array<int, 4> refs;
};

void UseLoneField(LoneField& s) {
  s.refs[UnsafeIndex()] = 0;
}
