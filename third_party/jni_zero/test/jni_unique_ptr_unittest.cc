// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/jni_unique_ptr.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace jni_zero {
namespace {

class DestructionTracker {
 public:
  explicit DestructionTracker(bool* destroyed) : destroyed_(destroyed) {
    *destroyed_ = false;
  }
  ~DestructionTracker() { *destroyed_ = true; }

 private:
  bool* destroyed_;
};

}  // namespace

TEST(JniUniquePtrTest, MoveSemantics) {
  bool destroyed = false;
  auto tracker = std::make_unique<DestructionTracker>(&destroyed);
  auto unique_ptr = MakeUnique(std::move(tracker));

  EXPECT_TRUE(unique_ptr);
  EXPECT_NE(0, unique_ptr.deleter_address());

  auto moved_ptr = std::move(unique_ptr);
  EXPECT_FALSE(unique_ptr);
  EXPECT_EQ(0, unique_ptr.deleter_address());
  EXPECT_TRUE(moved_ptr);
  EXPECT_NE(0, moved_ptr.deleter_address());

  DestructionTracker* raw = moved_ptr.release();
  EXPECT_FALSE(moved_ptr);
  EXPECT_FALSE(destroyed);

  delete raw;
  EXPECT_TRUE(destroyed);
}

TEST(JniUniquePtrTest, MoveAssignmentDeletesOldPointee) {
  bool old_destroyed = false;
  bool new_destroyed = false;
  auto old_ptr =
      MakeUnique(std::make_unique<DestructionTracker>(&old_destroyed));
  auto new_ptr =
      MakeUnique(std::make_unique<DestructionTracker>(&new_destroyed));

  old_ptr = std::move(new_ptr);
  EXPECT_TRUE(old_destroyed);
  EXPECT_FALSE(new_destroyed);
  EXPECT_TRUE(old_ptr);
  EXPECT_NE(0, old_ptr.deleter_address());
  EXPECT_EQ(0, new_ptr.deleter_address());
}

TEST(JniUniquePtrTest, DeleterExecution) {
  bool destroyed = false;
  auto tracker = std::make_unique<DestructionTracker>(&destroyed);
  auto unique_ptr = MakeUnique(std::move(tracker));

  jlong deleter_addr = unique_ptr.deleter_address();
  DestructionTracker* raw = unique_ptr.release();

  EXPECT_FALSE(destroyed);
  const auto* deleter = reinterpret_cast<const DeleterBase*>(deleter_addr);
  deleter->Destroy(raw);
  EXPECT_TRUE(destroyed);
}

TEST(JniUniquePtrTest, DeletesOwnedObjectWhenNotReleased) {
  bool destroyed = false;
  {
    auto tracker = std::make_unique<DestructionTracker>(&destroyed);
    auto unique_ptr = MakeUnique(std::move(tracker));
    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

TEST(JniUniquePtrTest, NullPointer) {
  JniUniquePtr<DestructionTracker> null_ptr(nullptr);
  EXPECT_FALSE(null_ptr);
  EXPECT_EQ(0, null_ptr.deleter_address());
  EXPECT_EQ(nullptr, null_ptr.get());
}

}  // namespace jni_zero
