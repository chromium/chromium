// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

struct S {
  char get_1st() {
    // Expected rewrite:
    // return member[0];
    return *member;
  }

  char get_3rd() { return member[2]; }

  // Expected rewrite:
  // base::span<char> member;
  char* member;
};

// Expected rewrite:
// void fct(base::span<char> param)
void fct(char* param) {
  // Expected rewrite:
  // param[0] = 'a';
  *param = 'a';

  // This leads param to be rewritten.
  param[1] = 'b';
}

// Expected rewrite:
// base::span<char> get(int index = 0)
char* get(int index = 0) {
  // Expected rewrite:
  // return {};
  return nullptr;
}

void fct2() {
  std::vector<char> buf;

  // Expected rewrite:
  // S obj{buf};
  S obj{buf.data()};

  (void)obj;
  // Expected rewrite:
  // fct(buf);
  fct(buf.data());

  char* ptr = get();
  // Buffer expression leading ptr and get return type to be rewritten.
  ptr[3] = 'c';

  // Expected rewrite:
  // get()[0] = 'a'
  *get() = 'a';

  int index = 0;
  // Expected rewrite:
  // get(index)[0] = 'x';
  *get(index) = 'x';
}
