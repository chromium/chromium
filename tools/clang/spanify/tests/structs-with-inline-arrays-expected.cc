// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

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
  // static const std::array<FuncBuffer2, 4> func_buffer2 = {{{1}, {2}, {3},
  // {4}}};
  struct FuncBuffer2 {
    int val;
  };
  static const std::array<FuncBuffer2, 4> func_buffer2 = {{{1}, {2}, {3}, {4}}};

  // Buffer accesses to trigger spanification.
  func_buffer[2].val = 3;
  globalBuffer[2].val = 3;
  funcNamedBuffer[2].val = 3;
  globalNamedBuffer[2].val = 3;
  globalNamedBufferButNotInline[2].val = 3;
  funcNamedBufferButNotInline[3].val = 3;
  (void)func_buffer2[2].val;
}
