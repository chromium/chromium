// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

static const char s[] = "hi";
static char s2[] = {'a'};
static int f[] = {1};

int main() {
  base::span{s};      // Error.
  base::span{s2};     // OK. Not a string literal.
  base::span{"wee"};  // Error.
  base::span{f};      // OK. Not a string literal.
}

struct S {
  S()
      : field1("hi"),  // Error.
        field2(s),     // Error.
        field3(s2)     // OK. Not a string literal.
  {}

  base::span<const char> field1;
  base::span<const char> field2;
  base::span<char> field3;
  base::span<const char> field4{"hi"};  // Error.
  base::span<const char> field5{s};     // Error.
  base::span<char> field6{s2};          // OK. Not a string literal.
};

struct Nested {
  struct span {
    span(const char*) {}
  };
};

void dont_crash_on_nested_span_class() {
  Nested::span("hi");
}
