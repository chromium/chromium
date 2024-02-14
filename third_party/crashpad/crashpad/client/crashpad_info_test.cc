// Copyright 2024 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "client/crashpad_info.h"

#include <string>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

constexpr uint32_t kTestStreamType = 0x33333;

class CrashpadInfoTest : public testing::Test {
 protected:
  CrashpadInfo& crashpad_info() { return crashpad_info_; }

  // Returns the current head of the list of streams in `crashpad_info_`. Note
  // that the returned pointer is invalidated if a stream is added or updated.
  internal::UserDataMinidumpStreamListEntry* GetCurrentHead() {
    return crashpad_info().GetUserDataMinidumpStreamHeadForTesting();
  }

  // Returns a pointer to the next node in the list after the given `node`.
  internal::UserDataMinidumpStreamListEntry* GetNext(
      internal::UserDataMinidumpStreamListEntry* node) {
    return reinterpret_cast<internal::UserDataMinidumpStreamListEntry*>(
        node->next);
  }

  internal::UserDataMinidumpStreamListEntry* initial_head() {
    return initial_head_;
  }

  internal::UserDataMinidumpStreamListEntry* initial_tail() {
    return initial_tail_;
  }

 private:
  void SetUp() override {
    ASSERT_EQ(nullptr, GetCurrentHead());

    // Create a simple test list with the structure
    // `initial_head_` -> `initial_tail_`.
    initial_tail_ = AddStream(0x11111, kInitialTailData);
    initial_head_ = AddStream(0x22222, kInitialHeadData);

    // Validate the list's contents.
    auto current = GetCurrentHead();
    ASSERT_EQ(initial_head_, current);
    ASSERT_EQ(kInitialHeadData, reinterpret_cast<char*>(current->base_address));
    current = GetNext(current);
    ASSERT_EQ(initial_tail_, current);
    ASSERT_EQ(nullptr, GetNext(current));
  }

  void TearDown() override {
    // Free the list. The list lives until process exit in production, but must
    // be freed in tests as multiple tests run in the same process.
    auto current = GetCurrentHead();
    while (current) {
      auto next = GetNext(current);
      delete current;
      current = next;
    }
  }

  internal::UserDataMinidumpStreamListEntry* AddStream(uint32_t stream_type,
                                                       const char* data) {
    return reinterpret_cast<internal::UserDataMinidumpStreamListEntry*>(
        crashpad_info().AddUserDataMinidumpStream(
            stream_type, data, strlen(data)));
  }

  CrashpadInfo crashpad_info_;

  static constexpr char kInitialHeadData[] = "head";
  static constexpr char kInitialTailData[] = "tail";

  internal::UserDataMinidumpStreamListEntry* initial_head_ = nullptr;
  internal::UserDataMinidumpStreamListEntry* initial_tail_ = nullptr;
};

// Tests that updating the head of the list updates the head pointer, the new
// head contains the updated data, and the updated node points to the next node.
TEST_F(CrashpadInfoTest, UpdateUserDataMinidumpStreamHead) {
  const std::string new_data = "this is a new string";
  const auto new_entry = crashpad_info().UpdateUserDataMinidumpStream(
      initial_head(), kTestStreamType, new_data.data(), new_data.size());
  const auto head = GetCurrentHead();
  EXPECT_EQ(new_entry, head);
  EXPECT_EQ(new_data.data(), reinterpret_cast<char*>(head->base_address));
  EXPECT_EQ(new_data.size(), head->size);
  EXPECT_EQ(kTestStreamType, head->stream_type);
  EXPECT_EQ(initial_tail(), GetNext(head));
}

// Tests that updating the tail of the list results in a tail pointing to
// nullptr, and that the node before the updated node points to it.
TEST_F(CrashpadInfoTest, UpdateUserDataMinidumpStreamTail) {
  const std::string new_data = "new";
  const auto new_entry = crashpad_info().UpdateUserDataMinidumpStream(
      initial_tail(), kTestStreamType, new_data.data(), new_data.size());
  const auto tail = GetNext(GetCurrentHead());
  EXPECT_EQ(new_entry, tail);
  EXPECT_EQ(nullptr, GetNext(tail));
}

// Tests that the handle returned from updating an entry is usable for updating
// the entry again.
TEST_F(CrashpadInfoTest, UpdateUserDataMinidumpStreamMultipleTimes) {
  // Update the entry at the head; the updated entry should become the new head.
  const std::string new_data = "new";
  const auto new_entry_1 = crashpad_info().UpdateUserDataMinidumpStream(
      initial_head(), kTestStreamType, new_data.data(), new_data.size());
  EXPECT_EQ(new_entry_1, GetCurrentHead());

  // Update the updated entry again; another new entry should replace it as
  // head.
  const auto new_entry_2 = crashpad_info().UpdateUserDataMinidumpStream(
      new_entry_1, kTestStreamType, new_data.data(), new_data.size());
  EXPECT_NE(new_entry_1, new_entry_2);
  EXPECT_EQ(new_entry_2, GetCurrentHead());
  EXPECT_EQ(initial_tail(), GetNext(GetCurrentHead()));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
