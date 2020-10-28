// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace liburlpattern {

TEST(ParseTest, ValidChar) {
  auto result = Parse("/foo/bar");
  EXPECT_TRUE(result.ok());
}

TEST(ParseTest, InvalidChar) {
  auto result = Parse("/foo/ßar");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(result.status().message().find("Invalid character"),
            std::string::npos);
}

}  // namespace liburlpattern
