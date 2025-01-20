// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <tuple>

// No expected rewrite:
// We don't handle global C arrays.
// TODO(364338808) Handle this case.
struct {
  int val;
} globalBuffer[4];

// No expected rewrite:
// We don't handle global C arrays.
// TODO(364338808) Handle this case.
struct GlobalHasName {
  int val;
} globalNamedBuffer[4];

// No expected rewrite:
// We don't handle global C arrays.
// TODO(364338808) Handle this case.
GlobalHasName globalNamedBufferButNotInline[4];

void fct() {
  // Expected rewrite
  // struct FuncBuffer {
  //   int val;
  // };
  // std::array<FuncBuffer, 4> func_buffer;
  struct FuncBuffer {
    int val;
  };
  std::array<FuncBuffer, 4> func_buffer;

  // Expected rewrite:
  // struct TestCases {
  //   int val;
  // };
  // const std::array<TestCases, 4> kTestCases = {{{1}, {2}, {3}, {4}}};
  struct TestCases {
    int val;
  };
  const std::array<TestCases, 4> kTestCases = {{{1}, {2}, {3}, {4}}};
  std::ignore = kTestCases[2].val;  // Unsafe access to trigger spanification.

  // Expected rewrite:
  // struct GTestCases {
  //   int val;
  // };
  // const std::array<GTestCases, 4> gTestCases = {{{1}, {2}, {3}, {4}}};
  struct GTestCases {
    int val;
  };
  const std::array<GTestCases, 4> gTestCases = {{{1}, {2}, {3}, {4}}};
  std::ignore = gTestCases[2].val;  // Unsafe access to trigger spanification.

  // Expected rewrite:
  // struct Knights {
  //   int val;
  // };
  // const std::array<Knights, 4> knights = {{{1}, {2}, {3}, {4}}};
  struct Knights {
    int val;
  };
  const std::array<Knights, 4> knights = {{{1}, {2}, {3}, {4}}};
  std::ignore = knights[2].val;  // Unsafe access to trigger spanification.

  // Expected rewrite:
  // struct funcHasName {
  //   int val;
  // };
  // std::array<funcHasName, 4> funcNamedBuffer;
  struct funcHasName {
    int val;
  };
  std::array<funcHasName, 4> funcNamedBuffer;

  // Expected rewrite:
  // std::array<funcHasName, 4> funcNamedBufferButNotInline;
  std::array<funcHasName, 4> funcNamedBufferButNotInline;

  // Expected rewrite:
  // struct FuncBuffer2 {
  //   int val;
  // };
  // static const auto func_buffer2 =
  //     std::to_array<FuncBuffer2>({{1}, {2}, {3}, {4}});
  struct FuncBuffer2 {
    int val;
  };
  static const auto func_buffer2 =
      std::to_array<FuncBuffer2>({{1}, {2}, {3}, {4}});

  // Expected rewrite
  // struct FuncBufferWithComment {
  //   int val; // Comment
  // };
  // std::array<FuncBuffer, 4> funcBufferWithComment;
  struct FuncBufferWithComment {
    int val;  // Comment
  };
  std::array<FuncBufferWithComment, 4> funcBufferWithComment;

  // Classes can also be used in a similar way.
  // Expected rewrite
  // class UnnamedClassBuffer {
  //  public:
  //   int val;
  // };
  // std::array<UnnamedClassBuffer, 4> unnamedClassBuffer;
  class UnnamedClassBuffer {
   public:
    int val;
  };
  std::array<UnnamedClassBuffer, 4> unnamedClassBuffer;

  // Unions can also be used in a similar way.
  // Expected rewrite
  // union UnnamedUnionBuffer {
  //   int val;
  //   float fval;
  // };
  // std::array<UnnamedUnionBuffer, 4> unnamedUnionBuffer;
  union UnnamedUnionBuffer {
    int val;
    float fval;
  };
  std::array<UnnamedUnionBuffer, 4> unnamedUnionBuffer;

  // Expected rewrite
  // struct NestedStructBuffer {
  //   struct {
  //     int val;
  //   } inner;
  // };
  // std::array<NestedStructBuffer, 3> nestedStructBuffer;
  struct NestedStructBuffer {
    struct {
      int val;
    } inner;
  };
  std::array<NestedStructBuffer, 4> nestedStructBuffer;

  // Buffer accesses to trigger spanification.
  func_buffer[2].val = 3;
  globalBuffer[2].val = 3;
  funcNamedBuffer[2].val = 3;
  globalNamedBuffer[2].val = 3;
  globalNamedBufferButNotInline[2].val = 3;
  funcNamedBufferButNotInline[3].val = 3;
  (void)func_buffer2[2].val;
  funcBufferWithComment[2].val = 3;
  unnamedClassBuffer[2].val = 3;
  unnamedUnionBuffer[2].val = 3;
  nestedStructBuffer[2].inner.val = 3;
}
