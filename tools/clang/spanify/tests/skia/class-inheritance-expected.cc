// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "include/core/SkSpan.h"

class SkRawStream {
 public:
  // Expected rewrite:
  // virtual void fct(SkSpan<char> param) = 0;
  virtual void fct(SkSpan<char> param) = 0;
};

class SkRawBufferedStream : public SkRawStream {
 public:
  // Expected rewrite:
  // void fct(SkSpan<char> param)
  void fct(SkSpan<char> param) override {
    // Expected rewrite:
    // param[0] = 'a';
    param[0] = 'a';

    // This leads param to be rewritten.
    param[1] = 'b';
  }
};

class SkSimpleBufferedStream : public SkRawStream {
 public:
  // Expected rewrite:
  // void fct(SkSpan<char> param)
  void fct(SkSpan<char> param) override {}
};

// Expected rewrite:
// SkSpan<char> get(int index = 0)
SkSpan<char> get(int index = 0) {
  // Expected rewrite:
  // return {};
  return {};
}

void fct2() {
  std::vector<char> buf;
  SkRawBufferedStream stream;

  // Expected rewrite:
  // stream.fct(buf);
  stream.fct(buf);

  // Expected rewrite:
  // SkSpan<char> ptr = get();
  SkSpan<char> ptr = get();
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
