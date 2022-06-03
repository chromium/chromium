// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "statusor.h"

#include <memory>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include <gtest/gtest.h>

namespace rlwe {
namespace {

class NoDefault {
 public:
  explicit NoDefault(int dummy) { dummy_ = dummy; }

  ~NoDefault() = default;

  int GetDummy() { return dummy_; }

 private:
  NoDefault() = delete;
  int dummy_;
};

class NoDefaultNoCopy {
 public:
  explicit NoDefaultNoCopy(int dummy) { dummy_ = dummy; }

  ~NoDefaultNoCopy() = default;

  int GetDummy() { return dummy_; }

  NoDefaultNoCopy(NoDefaultNoCopy&& other) = default;
  NoDefaultNoCopy& operator=(NoDefaultNoCopy&& other) = default;

 private:
  NoDefaultNoCopy() = delete;
  NoDefaultNoCopy(const NoDefaultNoCopy& other) = delete;
  NoDefaultNoCopy& operator=(const NoDefaultNoCopy& other) = delete;
  int dummy_;
};

TEST(StatusOrTest, StatusUnknown) {
  StatusOr<std::string> statusor;
  EXPECT_FALSE(statusor.ok());
  EXPECT_EQ(absl::UnknownError(""), statusor.status());
}

TEST(StatusOrTest, CopyCtors) {
  NoDefault no_default(42);
  StatusOr<NoDefault> statusor(no_default);
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.value().GetDummy());
  statusor = StatusOr<NoDefault>(no_default);
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.value().GetDummy());
}

TEST(StatusOrTest, MoveCtors) {
  StatusOr<NoDefaultNoCopy> statusor(NoDefaultNoCopy(42));
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.value().GetDummy());
  statusor = NoDefaultNoCopy(42);
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.value().GetDummy());
}

TEST(StatusOrTest, DiesWithNotOkStatus) {
  StatusOr<NoDefault> statusor(absl::CancelledError(""));
  EXPECT_DEATH_IF_SUPPORTED(statusor.value(), "");
}

TEST(StatusOrTest, Pointers) {
  std::unique_ptr<NoDefault> no_default(new NoDefault(42));
  StatusOr<NoDefault*> statusor(no_default.get());
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.value()->GetDummy());
}

TEST(StatusOrTest, TestStatusOrCopyCtors) {
  NoDefault no_default(42);
  StatusOr<NoDefault> statusor(no_default);
  StatusOr<NoDefault> statusor_wrap(statusor);
  EXPECT_TRUE(statusor_wrap.ok());
  EXPECT_EQ(42, statusor_wrap.value().GetDummy());
}

TEST(StatusOrTest, TestStatusOrCopyAssignment) {
  NoDefault no_default(42);
  StatusOr<NoDefault> statusor(no_default);
  StatusOr<NoDefault> statusor_wrap = statusor;
  EXPECT_TRUE(statusor_wrap.ok());
  EXPECT_EQ(42, statusor_wrap.value().GetDummy());
}

}  // namespace
}  // namespace rlwe
