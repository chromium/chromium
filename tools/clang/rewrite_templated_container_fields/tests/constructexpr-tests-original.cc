// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

struct S {};

class A {
 public:
  // Expected rewrite: A(const std::vector<raw_ptr<S>>& arg);
  A(const std::vector<S*>& arg) : member(arg) {}

  // Expected rewrite: A(const std::vector<raw_ptr<S>>* arg);
  A(const std::vector<S*>* arg) : member(*arg) {}

 private:
  // Expected rewrite: std::vector<raw_ptr<S>> member;
  std::vector<S*> member;
};

struct obj {
  // Expected rewrite: std::vector<raw_ptr<S>> member;
  std::vector<S*> member;
};

void fct() {
  {
    // Expected rewrite: std::vector<raw_ptr<S>> temp;
    std::vector<S*> temp;
    A a(temp);
  }
  {
    // Expected rewrite: std::vector<raw_ptr<S>> temp;
    std::vector<S*> temp;
    A* a = new A(temp);
  }

  {
    // Expected rewrite: std::vector<raw_ptr<S>> temp;
    std::vector<S*> temp;
    A a(&temp);
  }

  {
    // Expected rewrite: std::vector<raw_ptr<S>> temp;
    std::vector<S*> temp;
    A* a = new A(&temp);
  }

  {
    // Expected rewrite: std::vector<raw_ptr<S>> temp;
    std::vector<S*>* temp;
    A a(*temp);
  }

  {
    // Expected rewrite: -> std::vector<raw_ptr<S>> { return {}; };
    auto fct = []() -> std::vector<S*> { return {}; };
    A a(fct());
  }

  {
    // Expected rewrite: -> std::vector<raw_ptr<S>>* { return nullptr; };
    auto fct = []() -> std::vector<S*>* { return nullptr; };
    A a(*fct());
  }

  {
    // Expected rewrite: std::vector<raw_ptr<S>> temp;
    std::vector<S*> temp;
    obj o{temp};
  }

  {
    // Expected rewrite: std::vector<raw_ptr<S>> temp;
    std::vector<S*> temp;
    obj* o = new obj{temp};
  }

  {
    // Expected rewrite: -> std::vector<raw_ptr<S>> { return {}; };
    auto fct = []() -> std::vector<S*> { return {}; };
    obj o{fct()};
  }
}
