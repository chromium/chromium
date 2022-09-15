// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/list_model.h"

#include <stddef.h>

#include <memory>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

class FooItem {
 public:
  explicit FooItem(int id) : id_(id) {}

  FooItem(const FooItem&) = delete;
  FooItem& operator=(const FooItem&) = delete;

  int id() const { return id_; }

 private:
  int id_;
};

class ListModelTest : public testing::Test,
                      public ListModelObserver {
 public:
  ListModelTest()
      : added_count_(0),
        removed_count_(0),
        moved_count_(0),
        changed_count_(0) {
  }

  ListModelTest(const ListModelTest&) = delete;
  ListModelTest& operator=(const ListModelTest&) = delete;

  void ExpectCountsEqual(size_t added_count,
                         size_t removed_count,
                         size_t moved_count,
                         size_t changed_count) {
    EXPECT_EQ(added_count, added_count_);
    EXPECT_EQ(removed_count, removed_count_);
    EXPECT_EQ(moved_count, moved_count_);
    EXPECT_EQ(changed_count, changed_count_);
  }

  void ClearCounts() {
    added_count_ = removed_count_ = moved_count_ = changed_count_ = 0;
  }

  // ListModelObserver implementation:
  void ListItemsAdded(size_t start, size_t count) override {
    added_count_ += count;
  }
  void ListItemsRemoved(size_t start, size_t count) override {
    removed_count_ += count;
  }
  void ListItemMoved(size_t index, size_t target_index) override {
    ++moved_count_;
  }
  void ListItemsChanged(size_t start, size_t count) override {
    changed_count_ += count;
  }

 private:
  size_t added_count_;
  size_t removed_count_;
  size_t moved_count_;
  size_t changed_count_;
};

TEST_F(ListModelTest, Add) {
  ListModel<FooItem> model;
  model.AddObserver(this);

  // Append FooItem(0)
  model.Add(std::make_unique<FooItem>(0));
  ExpectCountsEqual(1, 0, 0, 0);

  // Append FooItem(1)
  model.Add(std::make_unique<FooItem>(1));
  ExpectCountsEqual(2, 0, 0, 0);

  // Insert FooItem(2) at position 0
  model.AddAt(0, std::make_unique<FooItem>(2));
  ExpectCountsEqual(3, 0, 0, 0);

  // Total 3 items in model.
  EXPECT_EQ(3U, model.item_count());

  // First one should be FooItem(2), followed by FooItem(0) and FooItem(1)
  EXPECT_EQ(2, model.GetItemAt(0)->id());
  EXPECT_EQ(0, model.GetItemAt(1)->id());
  EXPECT_EQ(1, model.GetItemAt(2)->id());
}

TEST_F(ListModelTest, Remove) {
  ListModel<FooItem> model;
  model.AddObserver(this);

  model.Add(std::make_unique<FooItem>(0));
  model.Add(std::make_unique<FooItem>(1));
  model.Add(std::make_unique<FooItem>(2));

  ClearCounts();

  // Remove item at index 1 from model and release memory.
  model.DeleteAt(1);
  ExpectCountsEqual(0, 1, 0, 0);

  EXPECT_EQ(2U, model.item_count());
  EXPECT_EQ(0, model.GetItemAt(0)->id());
  EXPECT_EQ(2, model.GetItemAt(1)->id());

  // Remove all items from model and delete them.
  model.DeleteAll();
  ExpectCountsEqual(0, 3, 0, 0);
}

TEST_F(ListModelTest, DeleteAll) {
  ListModel<FooItem> model;
  model.AddObserver(this);

  model.Add(std::make_unique<FooItem>(0));
  model.Add(std::make_unique<FooItem>(1));
  model.Add(std::make_unique<FooItem>(2));

  ClearCounts();

  // Delete all items.
  model.DeleteAll();
  ExpectCountsEqual(0, 3, 0, 0);
}

TEST_F(ListModelTest, Move) {
  ListModel<FooItem> model;
  model.AddObserver(this);

  model.Add(std::make_unique<FooItem>(0));
  model.Add(std::make_unique<FooItem>(1));
  model.Add(std::make_unique<FooItem>(2));

  ClearCounts();

  // Moves item at index 0 to index 2.
  model.Move(0, 2);
  ExpectCountsEqual(0, 0, 1, 0);
  EXPECT_EQ(1, model.GetItemAt(0)->id());
  EXPECT_EQ(2, model.GetItemAt(1)->id());
  EXPECT_EQ(0, model.GetItemAt(2)->id());
}

TEST_F(ListModelTest, FakeUpdate) {
  ListModel<FooItem> model;
  model.AddObserver(this);

  model.Add(std::make_unique<FooItem>(0));
  model.Add(std::make_unique<FooItem>(1));
  model.Add(std::make_unique<FooItem>(2));

  ClearCounts();

  model.NotifyItemsChanged(0, 1);
  ExpectCountsEqual(0, 0, 0, 1);

  model.NotifyItemsChanged(1, 2);
  ExpectCountsEqual(0, 0, 0, 3);
}

}  // namespace ui
