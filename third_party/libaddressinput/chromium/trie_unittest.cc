// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/trie.h"

#include <stdint.h>
#include <set>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

std::vector<uint8_t> ToByteArray(const std::string& text) {
  std::vector<uint8_t> result(text.length() + 1, 0);
  result.assign(text.begin(), text.end());
  return result;
}

}  // namespace

TEST(TrieTest, EmptyTrieHasNoData) {
  Trie<std::string> trie;
  std::set<std::string> result;
  trie.FindDataForKeyPrefix(ToByteArray("key"), &result);
  EXPECT_TRUE(result.empty());
}

TEST(TrieTest, CanGetDataByExactKey) {
  Trie<std::string> trie;
  trie.AddDataForKey(ToByteArray("hello"), "world");
  std::set<std::string> result;
  trie.FindDataForKeyPrefix(ToByteArray("hello"), &result);
  std::set<std::string> expected;
  expected.insert("world");
  EXPECT_EQ(expected, result);
}

TEST(TrieTest, CanGetDataByPrefix) {
  Trie<std::string> trie;
  trie.AddDataForKey(ToByteArray("hello"), "world");
  std::set<std::string> result;
  trie.FindDataForKeyPrefix(ToByteArray("he"), &result);
  std::set<std::string> expected;
  expected.insert("world");
  EXPECT_EQ(expected, result);
}

TEST(TrieTest, KeyTooLongNoData) {
  Trie<std::string> trie;
  trie.AddDataForKey(ToByteArray("hello"), "world");
  std::set<std::string> result;
  trie.FindDataForKeyPrefix(ToByteArray("helloo"), &result);
  EXPECT_TRUE(result.empty());
}

TEST(TrieTest, CommonPrefixFindsMultipleData) {
  Trie<std::string> trie;
  trie.AddDataForKey(ToByteArray("hello"), "world");
  trie.AddDataForKey(ToByteArray("howdy"), "buddy");
  trie.AddDataForKey(ToByteArray("foo"), "bar");
  std::set<std::string> results;
  trie.FindDataForKeyPrefix(ToByteArray("h"), &results);
  std::set<std::string> expected;
  expected.insert("world");
  expected.insert("buddy");
  EXPECT_EQ(expected, results);
}

TEST(TrieTest, KeyCanBePrefixOfOtherKey) {
  Trie<std::string> trie;
  trie.AddDataForKey(ToByteArray("hello"), "world");
  trie.AddDataForKey(ToByteArray("helloo"), "woorld");
  trie.AddDataForKey(ToByteArray("hella"), "warld");
  std::set<std::string> results;
  trie.FindDataForKeyPrefix(ToByteArray("hello"), &results);
  std::set<std::string> expected;
  expected.insert("world");
  expected.insert("woorld");
  EXPECT_EQ(expected, results);
}

TEST(TrieTest, AllowMutlipleKeys) {
  Trie<std::string> trie;
  trie.AddDataForKey(ToByteArray("hello"), "world");
  trie.AddDataForKey(ToByteArray("hello"), "woorld");
  std::set<std::string> results;
  trie.FindDataForKeyPrefix(ToByteArray("hello"), &results);
  std::set<std::string> expected;
  expected.insert("world");
  expected.insert("woorld");
  EXPECT_EQ(expected, results);
}

TEST(TrieTest, CanFindVeryLongKey) {
  Trie<std::string> trie;
  static const char kVeryLongKey[] = "1234567890qwertyuioasdfghj";
  trie.AddDataForKey(ToByteArray(kVeryLongKey), "world");
  std::set<std::string> result;
  trie.FindDataForKeyPrefix(ToByteArray(kVeryLongKey), &result);
  std::set<std::string> expected;
  expected.insert("world");
  EXPECT_EQ(expected, result);
}

}  // namespace autofill
