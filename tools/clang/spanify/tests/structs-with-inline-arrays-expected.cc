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
  // No expected rewrite
  // Unnamed type is hard to add for std::array
  // TODO(362644557) Handle this case.
  struct {
    int val;
  } funcBuffer[4];
  // No expected rewrite:
  // Inline definitions are to hard to add for std::array.
  // TODO(362644557) Handle this case.
  struct funcHasName {
    int val;
  } funcNamedBuffer[4];

  // Expected rewrite:
  // std::array<funcHasName, 4> funcNamedBufferButNotInline;
  std::array<funcHasName, 4> funcNamedBufferButNotInline;

  // Buffer accesses to trigger spanification.
  funcBuffer[2].val = 3;
  globalBuffer[2].val = 3;
  funcNamedBuffer[2].val = 3;
  globalNamedBuffer[2].val = 3;
  globalNamedBufferButNotInline[2].val = 3;
  funcNamedBufferButNotInline[3].val = 3;
}
