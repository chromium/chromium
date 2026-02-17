// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

// Expected rewrite:
// struct GlobalBuffer {
//   int val;
// };
// std::array<GlobalBuffer, 4> globalBuffer;
struct {
  int val;
} globalBuffer[4];

// Expected rewrite:
// struct GlobalHasName {
//   int val;
// };
// std::array<GlobalHasName, 4> globalNamedBuffer;
struct GlobalHasName {
  int val;
} globalNamedBuffer[4];

// Expected rewrite:
// std::array<GlobalHasName, 4> globalNamedBufferButNotInline;
GlobalHasName globalNamedBufferButNotInline[4];

int UnsafeIndex();  // This function might return an out-of-bound index.

void fct() {
  // Expected rewrite:
  // struct FuncBuffer {
  //   int val;
  // };
  // std::array<FuncBuffer, 4> func_buffer;
  struct {
    int val;
  } func_buffer[4];

  // Expected rewrite:
  // struct TestCases {
  //   int val;
  // };
  // const std::array<TestCases, 4> kTestCases = {{{1}, {2}, {3}, {4}}};
  const struct {
    int val;
  } kTestCases[4] = {{1}, {2}, {3}, {4}};
  std::ignore = kTestCases[UnsafeIndex()].val;  // Trigger spanification.

  // Expected rewrite:
  // struct GTestCases {
  //   int val;
  // };
  // const std::array<GTestCases, 4> gTestCases = {{{1}, {2}, {3}, {4}}};
  const struct {
    int val;
  } gTestCases[4] = {{1}, {2}, {3}, {4}};
  std::ignore = gTestCases[UnsafeIndex()].val;  // Trigger spanification.

  // Expected rewrite:
  // struct Knights {
  //   int val;
  // };
  // const std::array<Knights, 4> knights = {{{1}, {2}, {3}, {4}}};
  const struct {
    int val;
  } knights[4] = {{1}, {2}, {3}, {4}};
  std::ignore = knights[UnsafeIndex()].val;  // Trigger spanification.

  // Expected rewrite:
  // struct funcHasName {
  //   int val;
  // };
  // std::array<funcHasName, 4> funcNamedBuffer;
  struct funcHasName {
    int val;
  } funcNamedBuffer[4];

  // Expected rewrite:
  // std::array<funcHasName, 4> funcNamedBufferButNotInline;
  funcHasName funcNamedBufferButNotInline[4];

  // Expected rewrite:
  // struct FuncBuffer2 {
  //   int val;
  // };
  // static const auto func_buffer2 =
  //     std::to_array<FuncBuffer2>({{1}, {2}, {3}, {4}});
  static const struct {
    int val;
  } func_buffer2[] = {{1}, {2}, {3}, {4}};

  // Expected rewrite:
  // struct FuncBufferWithComment {
  //   int val; // Comment
  // };
  // std::array<FuncBuffer, 4> funcBufferWithComment;
  struct {
    int val;  // Comment
  } funcBufferWithComment[4];

  // Classes can also be used in a similar way.
  // Expected rewrite:
  // class UnnamedClassBuffer {
  //  public:
  //   int val;
  // };
  // std::array<UnnamedClassBuffer, 4> unnamedClassBuffer;
  class {
   public:
    int val;
  } unnamedClassBuffer[4];

  // Unions can also be used in a similar way.
  // Expected rewrite:
  // union UnnamedUnionBuffer {
  //   int val;
  //   float fval;
  // };
  // std::array<UnnamedUnionBuffer, 4> unnamedUnionBuffer;
  union {
    int val;
    float fval;
  } unnamedUnionBuffer[4];

  // Expected rewrite:
  // struct NestedStructBuffer {
  //   struct {
  //     int val;
  //   } inner;
  // };
  // std::array<NestedStructBuffer, 3> nestedStructBuffer;
  struct {
    struct {
      int val;
    } inner;
  } nestedStructBuffer[4];

  // Buffer accesses to trigger spanification.
  func_buffer[UnsafeIndex()].val = 3;
  globalBuffer[UnsafeIndex()].val = 3;
  funcNamedBuffer[UnsafeIndex()].val = 3;
  globalNamedBuffer[UnsafeIndex()].val = 3;
  globalNamedBufferButNotInline[UnsafeIndex()].val = 3;
  funcNamedBufferButNotInline[UnsafeIndex()].val = 3;
  (void)func_buffer2[UnsafeIndex()].val;
  funcBufferWithComment[UnsafeIndex()].val = 3;
  unnamedClassBuffer[UnsafeIndex()].val = 3;
  unnamedUnionBuffer[UnsafeIndex()].val = 3;
  nestedStructBuffer[UnsafeIndex()].inner.val = 3;
}

// `const` makes the decl have internal linkage, so this should be rewritten
// regardless of crbug.com/364338808.
// Expected rewrite:
// struct MyGlobalStruct1 {
//   int val;
// };
// const std::array<MyGlobalStruct1, 1 + 2> my_global_struct1 = {{
//     {1},
//     {2},
//     {3},
// }};
const struct MyGlobalStruct1 {
  int val;
} my_global_struct1[1 + 2] = {
    {1},
    {2},
    {3},
};

namespace {

// Anonymous namespace makes the decl have internal linkage, so this should be
// rewritten regardless of crbug.com/364338808.
// Expected rewrite:
// struct MyGlobalStruct2 {
//   int val;
// };
// auto my_global_struct2 = std::to_array<MyGlobalStruct2>({
//     {1},
//     {2},
//     {3},
// });
struct MyGlobalStruct2 {
  int val;
} my_global_struct2[] = {
    {1},
    {2},
    {3},
};

}  // namespace

void named_global_struct_with_var_decl() {
  // Buffer accesses to trigger spanification for the global structs above.
  std::ignore = my_global_struct1[UnsafeIndex()].val;
  std::ignore = my_global_struct2[UnsafeIndex()].val;
}
