// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>
#include <vector>

int UnsafeIndex();

// Expected rewrite:
// void TakePointerIntoContainer(base::span<int> into_container) {
void TakePointerIntoContainer(int* into_container) {
  into_container[UnsafeIndex()] = 13;
}

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
  TakePointerIntoContainer(&vector[0]);

  // Expected rewrite:
  // TakePointerIntoContainer(base::span<int>(vector).subspan(2u));
  TakePointerIntoContainer(&vector[2]);

  int cached_index = UnsafeIndex();
  // Expected rewrite:
  // TakePointerIntoContainer(base::span<int>(vector).subspan(
  //     base::checked_cast<size_t>(cached_index)));
  TakePointerIntoContainer(&vector[cached_index]);

  // Expected rewrite:
  // TakePointerIntoContainer(base::span<int>(vector).subspan(
  //     base::checked_cast<size_t>(UnsafeIndex())));
  TakePointerIntoContainer(&vector[UnsafeIndex()]);

  // Also test basic rewrite functionality on `std::array`.
  auto array = std::to_array<int>({13, 26, 39, 52});

  // Expected rewrite:
  // TakePointerIntoContainer(array);
  TakePointerIntoContainer(&array[0]);

  // Expected rewrite:
  // TakePointerIntoContainer(base::span<int>(array).subspan(2u));
  TakePointerIntoContainer(&array[2]);

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
