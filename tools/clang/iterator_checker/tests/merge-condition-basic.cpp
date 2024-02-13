// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

bool A();
bool B();

void SameConditionInvalidatesThenValidatesIterator() {
  std::vector<int> container = {1, 2, 3};
  auto it = container.begin() + 1;
  if (it == container.end()) {
    return;
  }

  const bool a = A();

  if (a) {
    container.clear();
  }

  if (a) {
    container.push_back(1);
    container.push_back(2);
    it = container.begin() + 1;
    if (it == std::end(container)) {
      return;
    }
  }

  // This is valid because although the container was invalidated in the first
  // `if(a)` block, the second one reassigns the iterator and ensures that it
  // is different than `end`. Since the same check is made at the very
  // beginning of the function, the iterator is logically valid at this point
  // of the execution.
  *it = 10;
}

void SameConditionUncheckedIterator() {
  std::vector<int> container = {1, 2, 3};
  auto it = container.begin() + 1;
  if (it == std::end(container)) {
    return;
  }

  const bool a = A();

  if (a) {
    container.clear();
  }

  if (!a) {
    container.push_back(1);
    container.push_back(2);
    it = container.begin() + 1;
  }

  // This is invalid because although the iterator is getting reassigned in the
  // second `a` conditional block, it is not checked against the `end` iterator.
  *it = 10;  // Invalid.
}

void DifferentConditionsWithCheckedIterator() {
  std::vector<int> container = {1, 2, 3};
  auto it = container.begin() + 1;
  if (it == std::end(container)) {
    return;
  }

  const bool a = A();
  const bool b = B();

  if (a && b) {
    container.clear();
  }

  if (a || b) {
    container.push_back(1);
    container.push_back(2);
    it = container.begin() + 1;
    if (it == std::end(container)) {
      return;
    }
  }

  // Valid since in all cases it is checked against the `end` iterator.
  *it = 10;
}

void DifferentConditionsWithUncheckedIterator() {
  std::vector<int> container = {1, 2, 3};
  auto it = container.begin() + 1;
  if (it == std::end(container)) {
    return;
  }

  const bool a = A();
  const bool b = B();

  if (a && b) {
    container.clear();
  }

  if (a || b) {
    container.push_back(1);
    container.push_back(2);
    it = container.begin() + 1;
  }

  // Invalid. The difference with `DifferentConditionsWithCheckedIterator` is
  // that we do not check the iterator in the last `if` block, hence we can't
  // ensure that the iterator is valid.
  *it = 10;
}
