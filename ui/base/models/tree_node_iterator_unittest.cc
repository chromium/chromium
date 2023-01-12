// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/tree_node_iterator.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/tree_node_model.h"

namespace ui {

namespace {

using TestNode = TreeNodeWithValue<int>;

bool PruneOdd(TestNode* node) {
  return node->value % 2;
}

bool PruneEven(TestNode* node) {
  return !PruneOdd(node);
}

TEST(TreeNodeIteratorTest, Basic) {
  TestNode root;
  root.Add(std::make_unique<TestNode>(), 0);
  root.Add(std::make_unique<TestNode>(), 1);
  TestNode* f3 = root.Add(std::make_unique<TestNode>(), 2);
  TestNode* f4 = f3->Add(std::make_unique<TestNode>(), 0);
  f4->Add(std::make_unique<TestNode>(), 0);

  TreeNodeIterator<TestNode> iterator(&root);
  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.children()[0].get(), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.children()[1].get(), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.children()[2].get(), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(f4, iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(f4->children()[0].get(), iterator.Next());

  ASSERT_FALSE(iterator.has_next());
}

// The tree used for testing:
// * + 1
//   + 2
//   + 3 + 4 + 5
//       + 7
TEST(TreeNodeIteratorTest, Prune) {
  TestNode root;
  root.Add(std::make_unique<TestNode>(1), 0);
  root.Add(std::make_unique<TestNode>(2), 1);
  TestNode* f3 = root.Add(std::make_unique<TestNode>(3), 2);
  TestNode* f4 = f3->Add(std::make_unique<TestNode>(4), 0);
  f4->Add(std::make_unique<TestNode>(5), 0);
  f3->Add(std::make_unique<TestNode>(7), 1);

  TreeNodeIterator<TestNode> odd_iterator(&root,
                                          base::BindRepeating(&PruneOdd));
  ASSERT_TRUE(odd_iterator.has_next());
  ASSERT_EQ(2, odd_iterator.Next()->value);
  ASSERT_FALSE(odd_iterator.has_next());

  TreeNodeIterator<TestNode> even_iterator(&root,
                                           base::BindRepeating(&PruneEven));
  ASSERT_TRUE(even_iterator.has_next());
  ASSERT_EQ(1, even_iterator.Next()->value);
  ASSERT_TRUE(even_iterator.has_next());
  ASSERT_EQ(3, even_iterator.Next()->value);
  ASSERT_TRUE(even_iterator.has_next());
  ASSERT_EQ(7, even_iterator.Next()->value);
  ASSERT_FALSE(even_iterator.has_next());
}

}  // namespace

}  // namespace ui
