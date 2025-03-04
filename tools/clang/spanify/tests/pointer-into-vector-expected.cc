// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>
#include <vector>

#include "base/containers/span.h"

int UnsafeIndex();

// Expected rewrite:
// void TakePointerIntoContainer(base::span<int> into_container) {
void TakePointerIntoContainer(base::span<int> into_container) {
  into_container[UnsafeIndex()] = 13;
}

// TODO: no rewriting is done here. Investigate.
// Expected rewrite:
// void TakePointerIntoCharContainer(base::span<char> into_container) {
void TakePointerIntoCharContainer(char* into_container) {
  into_container[UnsafeIndex()] = 'a';
}

int main() {
  // Test basic rewrite functionality on `std::vector`, the original
  // motivator for this.
  std::vector<int> vector = {13, 26, 39, 52};

  // Expected rewrite:
  // TakePointerIntoContainer(vector);
  TakePointerIntoContainer(vector);

  // Expected rewrite:
  // TakePointerIntoContainer(base::span<int>(vector).subspan(2));
  TakePointerIntoContainer(base::span<int>(vector).subspan(2));

  int cached_index = UnsafeIndex();
  // Expected rewrite:
  // TakePointerIntoContainer(base::span<int>(vector).subspan(cached_index));
  TakePointerIntoContainer(base::span<int>(vector).subspan(cached_index));

  // Expected rewrite:
  // TakePointerIntoContainer(base::span<int>(vector).subspan(UnsafeIndex()));
  TakePointerIntoContainer(base::span<int>(vector).subspan(UnsafeIndex()));

  // Also test basic rewrite functionality on `std::array`.
  auto array = std::to_array<int>({13, 26, 39, 52});

  // Expected rewrite:
  // TakePointerIntoContainer(array);
  TakePointerIntoContainer(array);

  // Expected rewrite:
  // TakePointerIntoContainer(base::span<int>(array).subspan(2));
  TakePointerIntoContainer(base::span<int>(array).subspan(2));

  // TODO: no rewriting is done here. Investigate.
  // Test basic rewrite functionality on `std::string`.
  std::string string = "Hello there!";

  // TODO: no rewriting is done here. Investigate.
  // Expected rewrite:
  // TakePointerIntoContainer(string);
  TakePointerIntoCharContainer(&string[0]);

  // TODO: no rewriting is done here. Investigate.
  // Expected rewrite:
  // TakePointerIntoContainer(base::span<char>(string).subspan(2));
  TakePointerIntoCharContainer(&string[2]);

  return 0;
}
