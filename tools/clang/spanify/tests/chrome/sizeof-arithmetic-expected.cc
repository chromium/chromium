// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstring>
#include <tuple>

#include "base/containers/auto_spanification_helper.h"
#include "base/containers/span.h"

struct Mailbox {
  char name[16];
};

void TestSizeofArithmetic(const Mailbox& source, const Mailbox& dest) {
  // Expected rewrite:
  // std::array<char, sizeof(source.name) * 2> buffer;
  std::array<char, sizeof(source.name) * 2> buffer;

  // Test `sizeof(...)` with parentheses.
  // Expected rewrite:
  // memcpy(base::span(buffer).subspan(sizeof(source.name)).data(),
  //        dest.name, sizeof(dest.name));
  memcpy(base::span<char>(buffer).subspan(sizeof(source.name)).data(),
         dest.name, sizeof(dest.name));

  // Test `sizeof ...` without parentheses.
  // Expected rewrite:
  // memcpy(base::span(buffer).subspan(sizeof source.name).name,
  //        dest.name, sizeof dest.name);
  memcpy(base::span<char>(buffer).subspan(sizeof source.name).data(), dest.name,
         sizeof dest.name);

  // Test `sizeof` as part of a more complex expression.
  // Expected rewrite:
  // memcpy(
  //     base::span(buffer)
  //         .subspan(sizeof source.name / sizeof source.name[0])
  //         .data(),
  //     dest.name, sizeof dest.name);
  memcpy(base::span<char>(buffer)
             .subspan(sizeof source.name / sizeof source.name[0])
             .data(),
         dest.name, sizeof dest.name);

  // Test `sizeof` used on array.
  // Expected rewrite:
  // memcpy(base::span<char>(buffer)
  //   .subspan(base::SpanificationSizeofForStdArray(buffer) - 1u)
  //   .data(), dest.name, 1);
  memcpy(base::span<char>(buffer)
             .subspan(base::SpanificationSizeofForStdArray(buffer) - 1u)
             .data(),
         dest.name, 1);
}
