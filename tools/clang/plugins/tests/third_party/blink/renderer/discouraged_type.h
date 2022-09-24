// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_THIRD_PARTY_BLINK_RENDERER_DISCOURAGED_TYPE_H_
#define TOOLS_CLANG_PLUGINS_TESTS_THIRD_PARTY_BLINK_RENDERER_DISCOURAGED_TYPE_H_

#include <vector>

namespace cc {
using CcVector = std::vector<double>;
}

namespace blink {

namespace nested {
using CcVector = cc::CcVector;
}

struct Foo {
  Foo(std::vector<int> v);

  // Not allowed.
  std::vector<int> v1;

  using FloatVector = std::vector<float>;
  // Not allowed.
  FloatVector v2;

  // Not allowed.
  std::vector<char> v_array[4][4];

  // cc::CcVector is not under the blink:: namespace, the checker should
  // ignore it and allow the use. In real world, this will be OK as long as
  // audit_non_blink_usages.py allows cc::CcVector.
  cc::CcVector v3;

  // This is a type alias that ultimately refers to cc::CcVector. Since the
  // underlying type is not under the blink:: namespace, the checker should
  // ignore it and allow the use.
  nested::CcVector v4;

  std::vector<int> v5 __attribute__((annotate("other1")))
  __attribute__((annotate("other2"), annotate("allow_discouraged_type")));

  using VectorAllowed __attribute__((annotate("allow_discouraged_type"))) =
      std::vector<int>;
  VectorAllowed v6;
};

// Function params of discouraged types are allowed.
inline Foo::Foo(std::vector<int> v) {
  // This is OK.
  std::vector<int> vv(v);

  struct {
    // Not allowed.
    std::vector<int> v;
  } sv = {vv};

  v1 = sv.v;
}

template <typename T>
class Template {
  // Not allowed.
  std::vector<T> v1;

  std::vector<T> v2 __attribute__((annotate("allow_discouraged_type")));
};

}  // namespace blink

#endif  // TOOLS_CLANG_PLUGINS_TESTS_THIRD_PARTY_BLINK_RENDERER_DISCOURAGED_TYPE_H_
