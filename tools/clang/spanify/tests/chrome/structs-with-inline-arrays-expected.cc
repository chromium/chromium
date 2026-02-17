// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <tuple>

// Expected rewrite:
// struct GlobalBuffer {
//   int val;
// };
// std::array<GlobalBuffer, 4> globalBuffer;
struct GlobalBuffer {
  int val;
};
std::array<GlobalBuffer, 4> globalBuffer;

// Expected rewrite:
// struct GlobalHasName {
//   int val;
// };
// std::array<GlobalHasName, 4> globalNamedBuffer;
struct GlobalHasName {
  int val;
};
std::array<GlobalHasName, 4> globalNamedBuffer;

// Expected rewrite:
// std::array<GlobalHasName, 4> globalNamedBufferButNotInline;
std::array<GlobalHasName, 4> globalNamedBufferButNotInline;

int UnsafeIndex();  // This function might return an out-of-bound index.

void fct() {
  // Expected rewrite:
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
  std::ignore = kTestCases[UnsafeIndex()].val;  // Trigger spanification.

  // Expected rewrite:
  // struct GTestCases {
  //   int val;
  // };
  // const std::array<GTestCases, 4> gTestCases = {{{1}, {2}, {3}, {4}}};
  struct GTestCases {
    int val;
  };
  const std::array<GTestCases, 4> gTestCases = {{{1}, {2}, {3}, {4}}};
  std::ignore = gTestCases[UnsafeIndex()].val;  // Trigger spanification.

  // Expected rewrite:
  // struct Knights {
  //   int val;
  // };
  // const std::array<Knights, 4> knights = {{{1}, {2}, {3}, {4}}};
  struct Knights {
    int val;
  };
  const std::array<Knights, 4> knights = {{{1}, {2}, {3}, {4}}};
  std::ignore = knights[UnsafeIndex()].val;  // Trigger spanification.

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

  // Expected rewrite:
  // struct FuncBufferWithComment {
  //   int val; // Comment
  // };
  // std::array<FuncBuffer, 4> funcBufferWithComment;
  struct FuncBufferWithComment {
    int val;  // Comment
  };
  std::array<FuncBufferWithComment, 4> funcBufferWithComment;

  // Classes can also be used in a similar way.
  // Expected rewrite:
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
  // Expected rewrite:
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

  // Expected rewrite:
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
struct MyGlobalStruct1 {
  int val;
};
const std::array<MyGlobalStruct1, 1 + 2> my_global_struct1 = {{
    {1},
    {2},
    {3},
}};

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
};
auto my_global_struct2 = std::to_array<MyGlobalStruct2>({
    {1},
    {2},
    {3},
});

}  // namespace

void named_global_struct_with_var_decl() {
  // Buffer accesses to trigger spanification for the global structs above.
  std::ignore = my_global_struct1[UnsafeIndex()].val;
  std::ignore = my_global_struct2[UnsafeIndex()].val;
}
