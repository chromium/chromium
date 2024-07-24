// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "base/containers/span.h"

struct S {
  char get_1st() {
    // Expected rewrite:
    // return member[0];
    return member[0];
  }

  char get_3rd() { return member[2]; }

  // Expected rewrite:
  // base::span<char> member;
  base::span<char> member;
};

// Expected rewrite:
// void fct(base::span<char> param)
void fct(base::span<char> param) {
  // Expected rewrite:
  // param[0] = 'a';
  param[0] = 'a';

  // This leads param to be rewritten.
  param[1] = 'b';
}

// Expected rewrite:
// base::span<char> get(int index = 0)
base::span<char> get(int index = 0) {
  // Expected rewrite:
  // return {};
  return {};
}

void fct2() {
  std::vector<char> buf;

  // Expected rewrite:
  // S obj{buf};
  S obj{buf};

  (void)obj;
  // Expected rewrite:
  // fct(buf);
  fct(buf);

  base::span<char> ptr = get();
  // Buffer expression leading ptr and get return type to be rewritten.
  ptr[3] = 'c';

  // Expected rewrite:
  // get()[0] = 'a'
  get()[0] = 'a';

  int index = 0;
  // Expected rewrite:
  // get(index)[0] = 'x';
  get(index)[0] = 'x';
}
