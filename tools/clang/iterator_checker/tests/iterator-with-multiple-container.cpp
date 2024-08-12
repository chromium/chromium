// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

bool Bool();

void MergeWithDifferentContainerValuesIteratorUnchecked() {
  std::vector<int> v1 = {1, 10, 100};
  std::vector<int> v2 = {1, 10, 100};
  std::vector<int> v3 = {1, 10, 100};

  auto it = std::find(v1.begin(), v1.end(), 10);

  if (Bool()) {
    it = std::find(v2.begin(), v2.end(), 10);
  } else {
    it = std::find(v3.begin(), v3.end(), 10);
  }

  v1.clear();

  // As for now, the tool doesn't handle well merges with different container
  // values. see https://crbug.com/1455371.
  // However, since the iterator was never checked against `end`, accessing it
  // is still considered invalid.
  *it = 20;
}

void MergeWithDifferentContainerValueIteratorChecked() {
  std::vector<int> v1 = {1, 10, 100};
  std::vector<int> v2 = {1, 10, 100};
  std::vector<int> v3 = {1, 10, 100};

  auto it = std::find(v1.begin(), v1.end(), 10);

  if (Bool()) {
    it = std::find(v2.begin(), v2.end(), 10);
    if (it == std::end(v2)) {
      return;
    }
  } else {
    it = std::find(v3.begin(), v3.end(), 10);
    if (it == std::end(v3)) {
      return;
    }
  }

  v1.clear();

  // This is in practice valid, because it can only be true here.
  // Although the tool does well on this case, the fact that multiple
  // containers are being used for the same iterator above means that the tool
  // is lucky.
  *it = 20;
}

void MergeWithDifferentContainersInvalidateAll() {
  std::vector<int> v1 = {1, 10, 100};
  std::vector<int> v2 = {1, 10, 100};

  auto it = std::find(v1.begin(), v1.end(), 10);

  if (Bool()) {
    it = std::find(v2.begin(), v2.end(), 10);
    if (it == std::end(v2)) {
      return;
    }
  } else {
    it = std::find(v1.begin(), v1.end(), 10);
    if (it == std::end(v1)) {
      return;
    }
  }

  v1.clear();

  // It is invalid because the iterator could still point to `v1`, which is
  // invalidated by `v1.clear()`.
  *it = 20;
}

void MergeWithSameContainerValueIteratorChecked() {
  std::vector<int> v1 = {1, 10, 100};
  std::vector<int> v2 = {1, 10, 100};
  std::vector<int> v3 = {1, 10, 100};

  auto it = std::find(v1.begin(), v1.end(), 10);

  if (Bool()) {
    it = std::find(v1.begin(), v1.end(), 10);
    if (it == std::end(v1)) {
      return;
    }
  } else {
    it = std::find(v1.begin(), v1.end(), 10);
    if (it == std::end(v1)) {
      return;
    }
  }

  // It is valid here because we would have returned from the function in the
  // previous conditional blocks if not.
  *it;
  v1.clear();

  // It just got invalidated by `v1.clear()`.
  *it = 20;
}
