// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "base/memory/raw_ptr.h"

template <typename Functor, typename... Args>
void BindOnce(Functor f, Args&&...) {
  (void)f;
}

template <typename Functor, typename... Args>
void BindRepeating(Functor f, Args&&...) {
  (void)f;
}

struct s {
  // Expected rewrite: (const std::vector<raw_ptr<int>>& arg)
  void do_something(const std::vector<raw_ptr<int>>& arg) {
    // Expected rewrite: for(int* i : arg)
    for (int* i : arg) {
      (void)i;
    }
  }

  void fct() { BindOnce(&s::do_something, this, member); }

  // Expected rewrite: std::vector<raw_ptr<int>> member;
  std::vector<raw_ptr<int>> member;
};

struct obj {
  // Expected rewrite: std::vector<raw_ptr<const int>> member;
  std::vector<raw_ptr<const int>> member;
};

int main() {
  obj o;
  // Expected rewrite: (const std::vector<raw_ptr<const int>>& arg)
  BindOnce(
      [](const std::vector<raw_ptr<const int>>& arg) {
        // Expected rewrite: for(const int* i : arg)
        for (const int* i : arg) {
          (void)i;
        }
      },
      o.member);

  BindOnce(
      [](const std::vector<raw_ptr<const int>>& arg) {
        // Expected rewrite: for(const int* i : arg)
        for (const int* i : arg) {
          (void)i;
        }
      },
      std::ref(o.member));

  BindOnce(
      [](const std::vector<raw_ptr<const int>>& arg) {
        // Expected rewrite: for(const int* i : arg)
        for (const int* i : arg) {
          (void)i;
        }
      },
      std::cref(o.member));

  BindRepeating(
      [](const std::vector<raw_ptr<const int>>& arg) {
        // Expected rewrite: for(const int* i : arg)
        for (const int* i : arg) {
          (void)i;
        }
      },
      std::cref(o.member));

  return 0;
}
