// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

int no_body(int);  // No annotation

int foo(int, char) {
  return 13;
}

namespace testnamespace {

namespace nestednamespace {

int fo0(int bar, char faz) {
  int baz = bar + 10;
  return baz;
}

}  // namespace nestednamespace

template <typename T>
T twice(T x) {
  return 2 * x;
}

}  // namespace testnamespace

class Aclass {
 public:
  Aclass() {}   // Constructor should not be annotated.
  ~Aclass() {}  // Destructor should not be annotated.
  int furt(int par1, char par2) { return par1 + par2; }

  // Should not be annotated (or =default exchanged for a body)
  bool operator==(const Aclass&) const = default;
};

template <typename T>
class TemplatedClass {
 public:
  int fun();
};

template <typename T>
class Specialized {
 public:
  int f() { return 1; }
};

template <>
class Specialized<double> {
 public:
  int f() { return 1; }
};

namespace double_fun {

template <typename T, typename S>
class DoubleTemplate {
 public:
  int fun() { return 0; }
};

template <typename S>
class DoubleTemplate<int, S> {
 public:
  int fun() { return 0; }
};

template <typename T>
class DoubleTemplate<T, int> {
 public:
  int fun() { return 0; }
};

template <>
class DoubleTemplate<int, int> {
 public:
  int fun() { return 0; }
};

}  // namespace double_fun

int main(int argc, char* argv[]) {
  int four = testnamespace::twice<int>(1);
  double two = testnamespace::twice<double>(1.0);

  TemplatedClass<int> itc;
  TemplatedClass<double> dtc;
  int zero = itc.fun() + dtc.fun();
  foo(1, 'a');

  Specialized<int> si;
  zero += si.f();

  double_fun::DoubleTemplate<char, char> dtcc;
  double_fun::DoubleTemplate<char, int> dtci;
  double_fun::DoubleTemplate<int, char> dtic;
  double_fun::DoubleTemplate<int, int> dtii;
  int funny_zero = dtcc.fun() + dtci.fun() + dtic.fun() + dtii.fun();

  std::vector<int> v = {3, 1, 4, 1, 5, 9};
  std::sort(v.begin(), v.end(), [](int a, int b) { return a < b; });

  return 0;
}

template <typename T>
int TemplatedClass<T>::fun() {
  return 0;
}
