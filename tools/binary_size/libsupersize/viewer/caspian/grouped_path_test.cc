// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/grouped_path.h"

#include <stdint.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

namespace caspian {
namespace {

void CheckParentChain(std::vector<GroupedPath> paths, char sep) {
  for (unsigned int i = 1; i < paths.size(); i++) {
    EXPECT_EQ(paths[i], paths[i - 1].Parent(sep));
  }
}

}  // namespace

TEST(PathTest, TestBasic) {
  std::vector<GroupedPath> paths{{"group", "foo/bar"},
                                 {"group", "foo"},
                                 {"group", ""},
                                 {"", ""},
                                 {"", ""}};
  CheckParentChain(paths, '>');
}

TEST(PathTest, TestEmptyGroup) {
  std::vector<GroupedPath> paths{
      {"", "foo/bar/baz"}, {"", "foo/bar"}, {"", "foo"}, {"", ""}};
  CheckParentChain(paths, '>');
}

TEST(PathTest, TestComponent) {
  std::vector<GroupedPath> paths{
      {"A>B>C", "foo"}, {"A>B>C", ""}, {"A>B", ""}, {"A", ""}, {"", ""}};
  CheckParentChain(paths, '>');
}

TEST(PathTest, TestGroupPaths) {
  std::vector<GroupedPath> paths{
      {"a/b/c", "foo"}, {"a/b/c", ""}, {"a/b", ""}, {"a", ""}, {"", ""}};
  CheckParentChain(paths, '/');
}

TEST(PathTest, TestNoSplitOnAngleBracketInPath) {
  std::vector<GroupedPath> paths{
      {"a/b/c", "operator>"}, {"a/b/c", ""}, {"a/b", ""}, {"a", ""}, {"", ""}};
  CheckParentChain(paths, '/');
}

TEST(PathTest, TestNoSplitOnAngleBracketInGroup) {
  std::vector<GroupedPath> paths{{"operator<>(foo)", ""}, {"", ""}};
  CheckParentChain(paths, '/');
}

TEST(PathTest, TestIsTopLevelPath) {
  EXPECT_TRUE((GroupedPath{"operator<>(foo)", "operator>"}.IsTopLevelPath()));
  EXPECT_FALSE((GroupedPath{"", "a/b"}.IsTopLevelPath()));
  EXPECT_TRUE((GroupedPath{"", "a"}.IsTopLevelPath()));
  EXPECT_FALSE((GroupedPath{"a", "b/c"}.IsTopLevelPath()));

  EXPECT_TRUE((GroupedPath{"foo", ""}.IsTopLevelPath()));
  EXPECT_TRUE((GroupedPath{"", ""}.IsTopLevelPath()));
}

TEST(PathTest, TestComparison) {
  EXPECT_TRUE((GroupedPath{"a", "b/c"}) < (GroupedPath{"a", "b/d"}));
  EXPECT_FALSE((GroupedPath{"a", "b/c"}) < (GroupedPath{"a", "b/b"}));

  EXPECT_FALSE((GroupedPath{"a", "b/c"}) < (GroupedPath{"a", "b/c"}));
  EXPECT_TRUE((GroupedPath{"a", "b/c"}) < (GroupedPath{"a", "b/c/d"}));

  EXPECT_TRUE((GroupedPath{"b", "c/c"}) < (GroupedPath{"c", "b/b"}));
  EXPECT_FALSE((GroupedPath{"b", "a/c"}) < (GroupedPath{"a", "b/b"}));
}

TEST(PathTest, TestShortname) {
  EXPECT_EQ("Blink", (GroupedPath{"Blink", ""}).ShortName('>'));
  EXPECT_EQ("Foo", (GroupedPath{"Blink>Foo", ""}).ShortName('>'));

  EXPECT_EQ("template<>", (GroupedPath{"a/template<>", ""}).ShortName('/'));

  EXPECT_EQ("Bar", (GroupedPath{"Blink>Foo", "Bar"}).ShortName('>'));
  EXPECT_EQ("c", (GroupedPath{"a", "b/c"}).ShortName('>'));
  EXPECT_EQ("c", (GroupedPath{"a", "b/c"}).ShortName('>'));
}

}  // namespace caspian
