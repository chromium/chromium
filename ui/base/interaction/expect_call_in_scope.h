// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_EXPECT_CALL_IN_SCOPE_H_
#define UI_BASE_INTERACTION_EXPECT_CALL_IN_SCOPE_H_

#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"

// TODO(dfried): move this code to base/test.

// Declares a MockCallback that will cause the current test to fail if called
// before the first EXPECT_CALL or EXPECT_CALL_IN_SCOPE (see below).
#define UNCALLED_MOCK_CALLBACK(callback_type, name) \
  base::MockCallback<callback_type> name;           \
  EXPECT_CALL(name, Run).Times(0)

// Requires that a specific base::MockCallback is called inside a specific block
// of code, or the current test will fail. `Block` may be a single statement or
// a block of code enclosed in {}.
//
// Syntax for the first two arguments is the same as EXPECT_CALL().
#define EXPECT_CALL_IN_SCOPE(Name, Call, Block) \
  EXPECT_CALL(Name, Call).Times(1);             \
  Block;                                        \
  EXPECT_CALL(Name, Run).Times(0)

// As EXPECT_CALL_IN_SCOPE but expects `Name1` and `Name2` to be called in order
// in the specified `Block`.
#define EXPECT_CALLS_IN_SCOPE_2(Name1, Call1, Name2, Call2, Block) \
  {                                                                \
    testing::InSequence in_sequence;                               \
    EXPECT_CALL(Name1, Call1).Times(1);                            \
    EXPECT_CALL(Name2, Call2).Times(1);                            \
  }                                                                \
  Block;                                                           \
  EXPECT_CALL(Name1, Run).Times(0);                                \
  EXPECT_CALL(Name2, Run).Times(0)

// As EXPECT_CALL_IN_SCOPE but expects `Name1`, `Name2`, and `Name3` to be
// called in order in the specified `Block`.
#define EXPECT_CALLS_IN_SCOPE_3(Name1, Call1, Name2, Call2, Name3, Call3, \
                                Block)                                    \
  {                                                                       \
    testing::InSequence in_sequence;                                      \
    EXPECT_CALL(Name1, Call1).Times(1);                                   \
    EXPECT_CALL(Name2, Call2).Times(1);                                   \
    EXPECT_CALL(Name3, Call3).Times(1);                                   \
  }                                                                       \
  Block;                                                                  \
  EXPECT_CALL(Name1, Run).Times(0);                                       \
  EXPECT_CALL(Name2, Run).Times(0);                                       \
  EXPECT_CALL(Name3, Run).Times(0)

#endif  // UI_BASE_INTERACTION_EXPECT_CALL_IN_SCOPE_H_
