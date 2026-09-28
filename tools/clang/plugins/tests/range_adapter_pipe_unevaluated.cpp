// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The only call of a std::ranges::operator| in this file is in an unevaluated
// operand, to a function that is only declared.

namespace std::ranges {
struct closure {};
int operator|(int, closure);
}  // namespace std::ranges

// Warning expected.
using Type = decltype(1 | std::ranges::closure());
