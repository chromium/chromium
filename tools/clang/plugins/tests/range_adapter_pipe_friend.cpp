// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Like range_adapter_pipe.cpp, but every operator| is a hidden friend, and
// one of them is only called in an unevaluated operand.

namespace std::ranges {

struct closure {
  friend int operator|(int, closure) { return 0; }
};

template <typename T>
struct closure_template {
  friend int operator|(int, closure_template) { return 0; }
};

inline namespace v1 {
struct inline_closure {
  friend int operator|(int, inline_closure);
};
}  // namespace v1

}  // namespace std::ranges

namespace other {
struct closure {
  friend int operator|(int, closure) { return 0; }
};
}  // namespace other

void Test() {
  // Warning expected.
  1 | std::ranges::closure();
  // Warning expected.
  1 | std::ranges::closure_template<int>();
  // Warning expected.
  using Type = decltype(1 | std::ranges::inline_closure());
  // No warning expected.
  1 | other::closure();
}
