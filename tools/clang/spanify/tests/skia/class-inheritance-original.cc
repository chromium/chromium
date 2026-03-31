// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

class SkRawStream {
 public:
  // Expected rewrite:
  // virtual void fct(SkSpan<char> param) = 0;
  virtual void fct(char* param) = 0;
};

class SkRawBufferedStream : public SkRawStream {
 public:
  // Expected rewrite:
  // void fct(SkSpan<char> param)
  void fct(char* param) override {
    // Expected rewrite:
    // param[0] = 'a';
    *param = 'a';

    // This leads param to be rewritten.
    param[1] = 'b';
  }
};

class SkSimpleBufferedStream : public SkRawStream {
 public:
  // Expected rewrite:
  // void fct(SkSpan<char> param)
  void fct(char* param) override {}
};

// Expected rewrite:
// SkSpan<char> get(int index = 0)
char* get(int index = 0) {
  // Expected rewrite:
  // return {};
  return nullptr;
}

void fct2() {
  std::vector<char> buf;
  SkRawBufferedStream stream;

  // Expected rewrite:
  // stream.fct(buf);
  stream.fct(buf.data());

  // Expected rewrite:
  // SkSpan<char> ptr = get();
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
