// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/tree_builder.h"

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/binary_size/libsupersize/viewer/caspian/diff.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

namespace caspian {

namespace {

using FilterList = std::vector<TreeBuilder::FilterFunc>;

std::unique_ptr<SizeInfo> MakeSizeInfo() {
  auto size_info = std::make_unique<SizeInfo>();
  size_info->containers.emplace_back("");
  return size_info;
}

void MakeSymbol(SizeInfo* size_info,
                SectionId section_id,
                int32_t size,
                const char* path,
                const char* component,
                std::string_view name = "") {
  static base::NoDestructor<std::deque<std::string>> symbol_names;
  if (name.empty()) {
    symbol_names->emplace_back();
    std::string& s = symbol_names->back();
    s += static_cast<char>(section_id);
    s += "_";
    s += base::NumberToString(size);
    s += "A";
    name = s;
  }
  Symbol sym;
  sym.full_name_ = name;
  sym.section_id_ = section_id;
  sym.size_ = size;
  sym.source_path_ = path;
  sym.object_path_ = "";
  sym.component_ = component;
  sym.container_ = &size_info->containers[0];
  sym.size_info_ = size_info;
  size_info->raw_symbols.push_back(sym);
}

std::string ShortName(const Json::Value& node) {
  const std::string id_path = node["idPath"].asString();
  const int short_name_index = node["shortNameIndex"].asInt();
  return id_path.substr(short_name_index);
}

void MakeAliasGroup(SizeInfo* info, size_t start, size_t end) {
  auto& group = info->alias_groups.emplace_back();
  for (size_t i = start; i < end; ++i) {
    info->raw_symbols[i].aliases_ = &group;
    group.push_back(&info->raw_symbols[i]);
  }
}

void MakeAliasSymbols(SizeInfo* info,
                      SectionId section_id,
                      int32_t size,
                      const char* path,
                      const char* name_prefix,
                      int start_idx,
                      int count,
                      int32_t relocation_count = 0) {
  for (int i = 0; i < count; ++i) {
    info->owned_strings.push_back(std::string(name_prefix) +
                                  base::NumberToString(start_idx + i));
    MakeSymbol(info, section_id, size, path, "", info->owned_strings.back());
    info->raw_symbols.back().relocation_count_ = relocation_count;
  }
}
}  // namespace

std::unique_ptr<SizeInfo> CreateSizeInfo() {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kText, 20, "a/b/c", "A");
  MakeSymbol(size_info.get(), SectionId::kText, 30, "a/b", "B");
  return size_info;
}

void CheckAllTreeNodesFindable(TreeBuilder& tree, const Json::Value& node) {
  const Json::Value& children = node["children"];
  for (unsigned int i = 0; i < children.size(); i++) {
    // Only recurse on folders, which have type Ct or Dt rather than t.
    if (children[i]["type"].size() == 2) {
      std::string id_path = children[i]["idPath"].asString();
      CheckAllTreeNodesFindable(tree, tree.Open(id_path.c_str()));
    }
  }
}

TEST(TreeBuilderTest, TestIdPathLens) {
  std::unique_ptr<SizeInfo> size_info = CreateSizeInfo();

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));
  EXPECT_EQ("Dt", builder.Open("")["type"].asString());
}

TEST(TreeBuilderTest, TestComponentLens) {
  std::unique_ptr<SizeInfo> size_info = CreateSizeInfo();

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<ComponentLens>(), '>', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));
  EXPECT_EQ("Gt", builder.Open("A")["type"].asString());
  EXPECT_EQ(20, builder.Open("A")["size"].asInt());
  EXPECT_EQ("Gt", builder.Open("B")["type"].asString());
  EXPECT_EQ(30, builder.Open("B")["size"].asInt());
}

TEST(TreeBuilderTest, TestTemplateLens) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kText, 20, "a/b/c", "A",
             "base::internal::Invoker<base::internal::BindState<void "
             "(autofill_assistant::Controller::*)(), "
             "base::WeakPtr<autofill_assistant::Controller> >, void "
             "()>::RunOnce(base::internal::BindStateBase*)");
  MakeSymbol(size_info.get(), SectionId::kText, 30, "a/b", "B",
             "base::internal::Invoker<base::internal::BindState<void "
             "(autofill_assistant::Controller::*)(int), "
             "base::WeakPtr<autofill_assistant::Controller>, unsigned int>, "
             "void ()>::RunOnce(base::internal::BindStateBase*)");

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<TemplateLens>(), '/', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));
  EXPECT_EQ(
      "Gt",
      builder.Open("base::internal::Invoker<>::RunOnce")["type"].asString());
  EXPECT_EQ(50,
            builder.Open("base::internal::Invoker<>::RunOnce")["size"].asInt());
}

TEST(TreeBuilderTest, TestNoNameUnderGroup) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kText, 20, "", "A>B>C", "SymbolName");

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<ComponentLens>(), '>', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));
  EXPECT_EQ("A>B>C/(No path)",
            builder.Open("A>B>C")["children"][0]["idPath"].asString());
}

TEST(TreeBuilderTest, TestJoinDexMethodClasses) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kDex, 30, "a/b/c", "",
             "zL2#foo(int,android.os.Parcel,android.os.Parcel,int): boolean");

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '>', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));

  Json::Value class_symbol = builder.Open("a/b/c/zL2");
  EXPECT_EQ("zL2", ShortName(class_symbol));
  EXPECT_EQ(1u, class_symbol["children"].size());

  Json::Value method_symbol = builder.Open("a/b/c/zL2")["children"][0];
  EXPECT_EQ("foo", ShortName(method_symbol));
  EXPECT_EQ(0u, method_symbol["children"].size());
}

TEST(TreeBuilderTest, TestJoinDexMethodClassesStringLiteral) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kDex, 30, "a/b/c", "",
             "\"start x.y.z end\"");

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '>', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));

  Json::Value class_symbol = builder.Open("a/b/c");
  EXPECT_EQ(1u, class_symbol["children"].size());

  Json::Value string_literal_symbol = class_symbol["children"][0];
  EXPECT_EQ("\"start x.y.z end\"", ShortName(string_literal_symbol));
  EXPECT_EQ(0u, string_literal_symbol["children"].size());
}

TEST(TreeBuilderTest, TestManyPathsRehash) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  std::deque<std::string> paths;
  for (int i = 0; i < 200; ++i) {
    paths.push_back("dir_" + base::NumberToString(i) + "/sub_" +
                    base::NumberToString(i) + "/File" +
                    base::NumberToString(i) + ".java");
    MakeSymbol(size_info.get(), SectionId::kDex, 10 + i, paths.back().c_str(),
               "Comp>Sub", "org.chromium.Cls#method(int): void");
  }

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<ComponentLens>(), '>', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));
  EXPECT_EQ(200u, builder.Open("Comp>Sub")["children"].size());
}

TEST(TreeBuilderTest, TestRelocationsMode) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kText, 100, "a/b/c", "A", "sym1");
  MakeSymbol(size_info.get(), SectionId::kText, 200, "a/b/c", "A", "sym2");
  size_info->raw_symbols[0].relocation_count_ = 5;
  size_info->raw_symbols[1].relocation_count_ = 0;

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/true, /*minimum_size=*/0);
  Json::Value root = builder.Open("");
  EXPECT_FLOAT_EQ(5.0f, root["size"].asFloat());
  Json::Value file_node = builder.Open("a/b/c");
  ASSERT_EQ(1u, file_node["children"].size());
  EXPECT_EQ("sym1", file_node["children"][0]["fullName"].asString());
  EXPECT_FLOAT_EQ(5.0f, file_node["children"][0]["size"].asFloat());
}

TEST(TreeBuilderTest, TestRelocationsDiffModePreservesFileDiffStatus) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  MakeSymbol(before.get(), SectionId::kDataRelRo, 100, "a/b/c", "A", "sym1");
  MakeSymbol(before.get(), SectionId::kDataRelRo, 100, "a/b/c", "A", "sym_sz");
  before->raw_symbols[0].relocation_count_ = 5;
  before->raw_symbols[1].relocation_count_ = 0;

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  MakeSymbol(after.get(), SectionId::kDataRelRo, 100, "a/b/c", "A", "sym1");
  MakeSymbol(after.get(), SectionId::kDataRelRo, 120, "a/b/c", "A", "sym_sz");
  MakeSymbol(after.get(), SectionId::kDataRelRo, 80, "a/b/c", "A", "sym_new");
  after->raw_symbols[0].relocation_count_ = 5;
  after->raw_symbols[1].relocation_count_ = 0;
  after->raw_symbols[2].relocation_count_ = 3;

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/true, /*minimum_size=*/0);

  Json::Value file_node = builder.Open("a/b/c");
  EXPECT_FALSE(file_node.isMember("diffStatus"));
  ASSERT_EQ(1u, file_node["children"].size());
  EXPECT_EQ("sym_new", file_node["children"][0]["fullName"].asString());
  EXPECT_FLOAT_EQ(3.0f, file_node["children"][0]["size"].asFloat());
  EXPECT_FLOAT_EQ(0.0f, file_node["children"][0]["beforeSize"].asFloat());
  EXPECT_EQ(1, file_node["childStats"]["R"]["added"].asInt());
  EXPECT_EQ(0, file_node["childStats"]["R"]["changed"].asInt());
}

TEST(TreeBuilderTest, TestDiffModeFiltersTinyAliasPss) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  // 25-alias 2-byte group (0.08 B each < 0.1) across dir1, dir2, dir3, dir6,
  // dir7, and dir8.
  MakeAliasSymbols(before.get(), SectionId::kText, 2, "dir1/a.cc", "alias_", 0,
                   10);
  MakeAliasSymbols(before.get(), SectionId::kText, 2, "dir2/b.cc", "alias_", 10,
                   10);
  MakeAliasSymbols(before.get(), SectionId::kText, 2, "dir3/c.cc", "alias_", 20,
                   1);
  MakeAliasSymbols(before.get(), SectionId::kText, 2, "dir6/f.cc", "alias_", 21,
                   1);
  // Tiny alias that will be removed from wholly-removed file dir7/g.cc.
  MakeAliasSymbols(before.get(), SectionId::kText, 2, "dir7/g.cc", "alias_", 22,
                   1);
  // Tiny aliases that will be removed from existing file dir8/h.cc while a real
  // symbol is added there (mixed polarity).
  MakeAliasSymbols(before.get(), SectionId::kText, 2, "dir8/h.cc", "alias_", 23,
                   2);
  // 10-alias 2-byte group (0.2 B each) that will split to 20 aliases (-0.1 B).
  MakeAliasSymbols(before.get(), SectionId::kText, 2, "dir5/e.cc", "split_", 0,
                   10);
  MakeSymbol(before.get(), SectionId::kText, 100, "dir1/a.cc", "", "real_sym");
  MakeSymbol(before.get(), SectionId::kText, 30, "dir6/f.cc", "",
             "removed_in_f");
  MakeSymbol(before.get(), SectionId::kText, 35, "dir7/g.cc", "",
             "removed_in_g");
  MakeSymbol(before.get(), SectionId::kText, 45, "dir9/i.cc", "",
             "removed_in_i");
  MakeAliasGroup(before.get(), 0, 25);
  MakeAliasGroup(before.get(), 25, 35);

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  // Alias group changes from 25 to 26 aliases (22 matched, 3 removed, 4 added):
  // matched delta = 2/26 - 2/25 = -0.003, added = +0.077, removed = -0.080;
  // all |delta| < 0.1 and the group's net delta is 0.
  MakeAliasSymbols(after.get(), SectionId::kText, 2, "dir1/a.cc", "alias_", 0,
                   10);
  MakeAliasSymbols(after.get(), SectionId::kText, 2, "dir2/b.cc", "alias_", 10,
                   10);
  MakeAliasSymbols(after.get(), SectionId::kText, 2, "dir3/c.cc", "alias_", 20,
                   1);
  MakeAliasSymbols(after.get(), SectionId::kText, 2, "dir6/f.cc", "alias_", 21,
                   1);
  MakeAliasSymbols(after.get(), SectionId::kText, 2, "dir1/a.cc", "alias_", 25,
                   2);
  MakeAliasSymbols(after.get(), SectionId::kText, 2, "dir4/d.cc", "alias_", 27,
                   1);
  // Tiny alias added to dir9/i.cc while its real symbol is removed (mixed
  // polarity).
  MakeAliasSymbols(after.get(), SectionId::kText, 2, "dir9/i.cc", "alias_", 28,
                   1);
  // Split group grows from 10 to 20 aliases: matched delta = -0.1, added = +0.1
  // (exact 0.1 boundary must be kept).
  MakeAliasSymbols(after.get(), SectionId::kText, 2, "dir5/e.cc", "split_", 0,
                   20);
  MakeSymbol(after.get(), SectionId::kText, 120, "dir1/a.cc", "", "real_sym");
  // Existing file dir3/c.cc gets a real added symbol alongside its tiny alias.
  MakeSymbol(after.get(), SectionId::kText, 40, "dir3/c.cc", "", "added_in_c");
  // Brand-new file dir4/d.cc gets a real added symbol alongside a tiny added
  // alias.
  MakeSymbol(after.get(), SectionId::kText, 50, "dir4/d.cc", "", "added_in_d");
  // Existing file dir8/h.cc loses its tiny aliases and gains a real symbol.
  MakeSymbol(after.get(), SectionId::kText, 60, "dir8/h.cc", "", "added_in_h");
  MakeAliasGroup(after.get(), 0, 26);
  MakeAliasGroup(after.get(), 26, 46);

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));

  // dir2/b.cc only had tiny alias deltas and should be omitted completely.
  EXPECT_EQ(nullptr, builder.Find("dir2"));
  // The zero-net alias group sums to 0, so no aggregate overhead node is
  // emitted.
  EXPECT_EQ(nullptr, builder.Find("(No path)"));

  // dir1/a.cc keeps only real_sym (+20) with consistent size and childStats.
  Json::Value a_node = builder.Open("dir1/a.cc");
  ASSERT_EQ(1u, a_node["children"].size());
  EXPECT_EQ("real_sym", a_node["children"][0]["fullName"].asString());
  EXPECT_FLOAT_EQ(20.0f, a_node["size"].asFloat());
  EXPECT_FLOAT_EQ(20.0f, a_node["childStats"]["t"]["size"].asFloat());
  EXPECT_EQ(1, a_node["childStats"]["t"]["changed"].asInt());
  EXPECT_EQ(0, a_node["childStats"]["t"]["added"].asInt());

  // Existing file dir3/c.cc must not be marked wholly added because it still
  // contains an unchanged-in-practice matched alias from `before`.
  Json::Value c_node = builder.Open("dir3/c.cc");
  ASSERT_EQ(1u, c_node["children"].size());
  EXPECT_FALSE(c_node.isMember("diffStatus"));
  EXPECT_FLOAT_EQ(40.0f, c_node["size"].asFloat());

  // Brand-new file dir4/d.cc must still be marked wholly added even though its
  // tiny added alias was skipped.
  Json::Value d_node = builder.Open("dir4/d.cc");
  ASSERT_EQ(1u, d_node["children"].size());
  EXPECT_EQ(static_cast<int>(DiffStatus::kAdded), d_node["diffStatus"].asInt());
  EXPECT_EQ(1, d_node["childStats"]["t"]["added"].asInt());
  EXPECT_FLOAT_EQ(50.0f, d_node["size"].asFloat());

  // Exact |delta| == 0.1 boundary symbols in dir5/e.cc are kept.
  Json::Value e_node = builder.Open("dir5/e.cc");
  EXPECT_EQ(20u, e_node["children"].size());
  EXPECT_NEAR(0.0f, e_node["size"].asFloat(), 1e-4f);

  // Existing file dir6/f.cc must not be marked wholly removed because it still
  // contains a matched alias from `before`.
  Json::Value f_node = builder.Open("dir6/f.cc");
  ASSERT_EQ(1u, f_node["children"].size());
  EXPECT_FALSE(f_node.isMember("diffStatus"));
  EXPECT_FLOAT_EQ(-30.0f, f_node["size"].asFloat());

  // Wholly removed file dir7/g.cc must still be marked wholly removed even
  // though its tiny removed alias was skipped.
  Json::Value g_node = builder.Open("dir7/g.cc");
  ASSERT_EQ(1u, g_node["children"].size());
  EXPECT_EQ(static_cast<int>(DiffStatus::kRemoved),
            g_node["diffStatus"].asInt());
  EXPECT_EQ(1, g_node["childStats"]["t"]["removed"].asInt());
  EXPECT_FLOAT_EQ(-35.0f, g_node["size"].asFloat());

  // Mixed-polarity files (skipped removal + visible addition in dir8/h.cc, and
  // skipped addition + visible removal in dir9/i.cc) must not be marked wholly
  // added or removed.
  Json::Value h_node = builder.Open("dir8/h.cc");
  ASSERT_EQ(1u, h_node["children"].size());
  EXPECT_FALSE(h_node.isMember("diffStatus"));
  Json::Value i_node = builder.Open("dir9/i.cc");
  ASSERT_EQ(1u, i_node["children"].size());
  EXPECT_FALSE(i_node.isMember("diffStatus"));
}

TEST(TreeBuilderTest, TestDiffModeAggregatesSmallAliasDeltas) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  before->containers[0].name = "c1";
  before->containers.emplace_back("c2");
  // Container c1: 25-alias 64-byte group that grows to 26 aliases (zero net
  // change, but added alias is +2.46 B >= 0.1 while 25 matched aliases are
  // -0.098 B < 0.1).
  MakeAliasSymbols(before.get(), SectionId::kText, 64, "dir_matched/a.cc",
                   "icf_", 0, 25);
  MakeAliasGroup(before.get(), 0, 25);

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  after->containers[0].name = "c1";
  after->containers.emplace_back("c2");
  MakeAliasSymbols(after.get(), SectionId::kText, 64, "dir_matched/a.cc",
                   "icf_", 0, 25);
  MakeAliasSymbols(after.get(), SectionId::kText, 64, "dir_added/b.cc", "icf_",
                   25, 1);
  // Container c2: wholly added 4-byte 50-alias group (+0.08 B each < 0.1).
  size_t c2_start = after->raw_symbols.size();
  MakeAliasSymbols(after.get(), SectionId::kText, 4, "dir_c2/c.cc", "small_", 0,
                   50);
  for (size_t i = c2_start; i < after->raw_symbols.size(); ++i) {
    after->raw_symbols[i].container_ = &after->containers[1];
  }
  MakeAliasGroup(after.get(), 0, 26);
  MakeAliasGroup(after.get(), c2_start, after->raw_symbols.size());

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<ContainerLens>(), '/', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));

  // c1: dir_matched is omitted, dir_added keeps the +2.46 B alias, and
  // (No path) carries the -2.46 B aggregate so c1's net total stays 0.
  EXPECT_EQ(nullptr, builder.Find("c1/dir_matched"));
  Json::Value c1_node = builder.Open("c1");
  EXPECT_NEAR(0.0f, c1_node["size"].asFloat(), 1e-4f);
  EXPECT_NEAR(64.0f, c1_node["beforeSize"].asFloat(), 1e-4f);
  Json::Value c1_no_path = builder.Open("c1/(No path)");
  ASSERT_EQ(1u, c1_no_path["children"].size());
  EXPECT_EQ("Overhead: aggregate small symbol diffs",
            c1_no_path["children"][0]["fullName"].asString());
  EXPECT_EQ("c1", c1_no_path["children"][0]["container"].asString());
  EXPECT_NEAR(-64.0f / 26.0f, c1_no_path["children"][0]["size"].asFloat(),
              1e-4f);

  // c2: all 50 sub-0.1 B symbols collapse into one +4 B aggregate node so the
  // real +4 B growth is preserved.
  EXPECT_EQ(nullptr, builder.Find("c2/dir_c2"));
  Json::Value c2_node = builder.Open("c2");
  EXPECT_NEAR(4.0f, c2_node["size"].asFloat(), 1e-4f);
  Json::Value c2_no_path = builder.Open("c2/(No path)");
  ASSERT_EQ(1u, c2_no_path["children"].size());
  EXPECT_EQ("c2", c2_no_path["children"][0]["container"].asString());
  EXPECT_NEAR(4.0f, c2_no_path["children"][0]["size"].asFloat(), 1e-4f);

  // Root total equals c1 (0) + c2 (+4) = +4 B.
  EXPECT_NEAR(4.0f, builder.Open("")["size"].asFloat(), 1e-4f);

  // Under IdPathLens (empty group), skipped deltas from both containers merge
  // into one aggregate symbol with an empty container name.
  TreeBuilder id_builder(&diff);
  id_builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                   /*relocations_mode=*/false, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(id_builder, id_builder.Open(""));
  Json::Value no_path = id_builder.Open("(No path)");
  ASSERT_EQ(1u, no_path["children"].size());
  EXPECT_EQ("", no_path["children"][0]["container"].asString());
  EXPECT_NEAR(4.0f - 64.0f / 26.0f, no_path["children"][0]["size"].asFloat(),
              1e-4f);
  EXPECT_NEAR(4.0f, id_builder.Open("")["size"].asFloat(), 1e-4f);
}

TEST(TreeBuilderTest, TestDiffModeAggregatesSmallAliasDeltasPerLens) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  // CompB: 25-alias 2-byte group (-0.08 B each when removed) -> -2 B net.
  MakeAliasSymbols(before.get(), SectionId::kText, 2, "comp_b/b.cc", "rem_", 0,
                   25);
  for (Symbol& sym : before->raw_symbols) {
    sym.component_ = "CompB";
  }
  MakeAliasGroup(before.get(), 0, 25);

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  // CompA>Sub: 25-alias 2-byte group (+0.08 B each when added) -> +2 B net.
  // Same magnitude as CompB's removal so pooling across components would cancel
  // both to 0.
  MakeAliasSymbols(after.get(), SectionId::kText, 2, "comp_a/a.cc", "add_", 0,
                   25);
  for (size_t i = 0; i < 25; ++i) {
    after->raw_symbols[i].component_ = "CompA>Sub";
  }

  // Generated Mojo group: 50-alias 4-byte group (+0.08 B each) -> +4 B net.
  size_t mojo_start = after->raw_symbols.size();
  MakeAliasSymbols(after.get(), SectionId::kText, 4, "gen/foo.mojom.cc",
                   "mojo_", 0, 50);
  for (size_t i = mojo_start; i < after->raw_symbols.size(); ++i) {
    after->raw_symbols[i].component_ = "CompMojo";
    after->raw_symbols[i].flags_ = SymbolFlag::kGeneratedSource;
  }

  // Native template group: 50-alias 4-byte group (+0.08 B each) -> +4 B net.
  size_t tmpl_start = after->raw_symbols.size();
  for (int i = 0; i < 50; ++i) {
    after->owned_strings.push_back("MyTemplate<T" + base::NumberToString(i) +
                                   ">()");
    MakeSymbol(after.get(), SectionId::kText, 4, "tmpl/t.cc", "CompT",
               after->owned_strings.back());
  }
  MakeAliasGroup(after.get(), 0, mojo_start);
  MakeAliasGroup(after.get(), mojo_start, tmpl_start);
  MakeAliasGroup(after.get(), tmpl_start, after->raw_symbols.size());

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  FilterList filters;

  // ComponentLens: CompA>Sub (+2) and CompB (-2) stay attributed to their own
  // components rather than cancelling or falling back to "(No component)".
  {
    TreeBuilder builder(&diff);
    builder.Build(std::make_unique<ComponentLens>(), '>', false, filters,
                  /*relocations_mode=*/false, /*minimum_size=*/0);
    CheckAllTreeNodesFindable(builder, builder.Open(""));
    EXPECT_EQ(nullptr, builder.Find("(No component)"));
    EXPECT_NEAR(2.0f, builder.Open("CompA>Sub")["size"].asFloat(), 1e-4f);
    EXPECT_NEAR(2.0f, builder.Open("CompA>Sub/(No path)")["size"].asFloat(),
                1e-4f);
    EXPECT_NEAR(-2.0f, builder.Open("CompB")["size"].asFloat(), 1e-4f);
    EXPECT_NEAR(-2.0f, builder.Open("CompB/(No path)")["size"].asFloat(),
                1e-4f);
  }

  // GeneratedLens: Mojo symbols stay under "Mojo/(No path)", not merged into
  // "Not generated".
  {
    TreeBuilder builder(&diff);
    builder.Build(std::make_unique<GeneratedLens>(), '/', false, filters,
                  /*relocations_mode=*/false, /*minimum_size=*/0);
    CheckAllTreeNodesFindable(builder, builder.Open(""));
    EXPECT_NEAR(4.0f, builder.Open("Mojo")["size"].asFloat(), 1e-4f);
    EXPECT_NEAR(4.0f, builder.Open("Mojo/(No path)")["size"].asFloat(), 1e-4f);
    EXPECT_NEAR(4.0f, builder.Open("Not generated")["size"].asFloat(), 1e-4f);
  }

  // TemplateLens: template small diffs stay under "MyTemplate<>/(No path)",
  // without creating a bogus top-level "Overhead: aggregate small symbol diffs"
  // template group.
  {
    TreeBuilder builder(&diff);
    FilterList tmpl_filters = {
        [](const GroupedPath&, const BaseSymbol& sym) {
          return sym.IsTemplate() && sym.IsNative();
        },
    };
    builder.Build(std::make_unique<TemplateLens>(), '/', false, tmpl_filters,
                  /*relocations_mode=*/false, /*minimum_size=*/0);
    CheckAllTreeNodesFindable(builder, builder.Open(""));
    EXPECT_EQ(nullptr, builder.Find("Overhead: aggregate small symbol diffs"));
    EXPECT_NEAR(4.0f, builder.Open("MyTemplate<>")["size"].asFloat(), 1e-4f);
    Json::Value tmpl_no_path = builder.Open("MyTemplate<>/(No path)");
    ASSERT_EQ(1u, tmpl_no_path["children"].size());
    EXPECT_EQ("Overhead: aggregate small symbol diffs",
              tmpl_no_path["children"][0]["fullName"].asString());
  }
}

TEST(TreeBuilderTest, TestDiffModeFiltersTinyRelocationPss) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  // 21-alias group with 1 relocation (0.0476 each < 0.05) across dir1 & dir2.
  MakeAliasSymbols(before.get(), SectionId::kDataRelRo, 100, "dir1/a.cc",
                   "rel_", 0, 16, /*relocation_count=*/1);
  MakeAliasSymbols(before.get(), SectionId::kDataRelRo, 100, "dir2/b.cc",
                   "rel_", 16, 5, /*relocation_count=*/1);
  // 14-alias group with 10 relocations growing to 15 aliases: added +0.667 is
  // kept, 14 matched -0.0476 (< 0.05) are aggregated so net stays 0.
  size_t asym_before_start = before->raw_symbols.size();
  MakeAliasSymbols(before.get(), SectionId::kDataRelRo, 100, "dir_asym/x.cc",
                   "asym_", 0, 14, /*relocation_count=*/10);
  MakeAliasGroup(before.get(), 0, 21);
  MakeAliasGroup(before.get(), asym_before_start, asym_before_start + 14);

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  // 21 -> 22 aliases: matched delta = 1/22 - 1/21 = -0.0022, added = +0.0455
  // (all |delta| < 0.05 and net = 0).
  MakeAliasSymbols(after.get(), SectionId::kDataRelRo, 100, "dir1/a.cc", "rel_",
                   0, 16, /*relocation_count=*/1);
  MakeAliasSymbols(after.get(), SectionId::kDataRelRo, 100, "dir2/b.cc", "rel_",
                   16, 5, /*relocation_count=*/1);
  MakeAliasSymbols(after.get(), SectionId::kDataRelRo, 100, "dir1/a.cc", "rel_",
                   21, 1, /*relocation_count=*/1);
  // Existing file dir1/a.cc also gains a real relocation symbol so we can check
  // its diffStatus is preserved.
  MakeSymbol(after.get(), SectionId::kDataRelRo, 100, "dir1/a.cc", "",
             "added_rel");
  after->raw_symbols.back().relocation_count_ = 2;
  // Exact 0.05 boundary: wholly added 20-alias group with 1 relocation
  // (+0.05 each) must be kept individually.
  size_t boundary_start = after->raw_symbols.size();
  MakeAliasSymbols(after.get(), SectionId::kDataRelRo, 100, "dir3/c.cc",
                   "bound_", 0, 20, /*relocation_count=*/1);
  // Asymmetric group grows 14 -> 15 aliases (net = 0).
  size_t asym_after_start = after->raw_symbols.size();
  MakeAliasSymbols(after.get(), SectionId::kDataRelRo, 100, "dir_asym/x.cc",
                   "asym_", 0, 14, /*relocation_count=*/10);
  MakeAliasSymbols(after.get(), SectionId::kDataRelRo, 100, "dir_asym_add/y.cc",
                   "asym_", 14, 1, /*relocation_count=*/10);
  MakeAliasGroup(after.get(), 0, 22);
  MakeAliasGroup(after.get(), boundary_start, boundary_start + 20);
  MakeAliasGroup(after.get(), asym_after_start, asym_after_start + 15);

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/true, /*minimum_size=*/0);
  CheckAllTreeNodesFindable(builder, builder.Open(""));

  EXPECT_EQ(nullptr, builder.Find("dir2"));
  EXPECT_EQ(nullptr, builder.Find("dir_asym"));

  // Existing file dir1/a.cc keeps only added_rel and is not marked wholly
  // added because of its skipped matched relocation aliases.
  Json::Value a_node = builder.Open("dir1/a.cc");
  ASSERT_EQ(1u, a_node["children"].size());
  EXPECT_EQ("added_rel", a_node["children"][0]["fullName"].asString());
  EXPECT_FALSE(a_node.isMember("diffStatus"));

  // Exact 0.05 boundary symbols in dir3/c.cc are all kept individually.
  Json::Value c_node = builder.Open("dir3/c.cc");
  EXPECT_EQ(20u, c_node["children"].size());

  // Root total equals added_rel (+2) + boundary (+1) + asymmetric (0) = +3.
  EXPECT_NEAR(3.0f, builder.Open("")["size"].asFloat(), 1e-4f);
}

TEST(TreeBuilderTest, TestSnapshotModeKeepsSmallAliasPss) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeAliasSymbols(size_info.get(), SectionId::kText, 2, "a/b.cc", "alias_", 0,
                   25, /*relocation_count=*/1);
  MakeAliasGroup(size_info.get(), 0, 25);

  {
    TreeBuilder builder(size_info.get());
    FilterList filters;
    builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                  /*relocations_mode=*/false, /*minimum_size=*/0);
    Json::Value file_node = builder.Open("a/b.cc");
    EXPECT_EQ(25u, file_node["children"].size());
    EXPECT_NEAR(2.0f, file_node["size"].asFloat(), 1e-4f);
  }

  {
    TreeBuilder builder(size_info.get());
    FilterList filters;
    builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                  /*relocations_mode=*/true, /*minimum_size=*/0);
    Json::Value file_node = builder.Open("a/b.cc");
    EXPECT_EQ(25u, file_node["children"].size());
    EXPECT_NEAR(1.0f, file_node["size"].asFloat(), 1e-4f);
  }
}

TEST(TreeBuilderTest, TestMethodCountDiffModeKeepsSmallAliasPss) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  MakeAliasSymbols(after.get(), SectionId::kDexMethod, 2, "a/Foo.java", "Foo#m",
                   0, 25);
  MakeAliasGroup(after.get(), 0, 25);

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/',
                /*method_count_mode=*/true, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);

  Json::Value class_node = builder.Open("a/Foo.java/Foo");
  EXPECT_EQ(25u, class_node["children"].size());
  EXPECT_EQ(25, class_node["childStats"]["m"]["count"].asInt());
}

TEST(TreeBuilderTest, TestMinSizeNormalModeSnapshot) {
  // Snapshot mode: filters on `Symbol::Pss()`.
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kText, 40, "a/b.cc", "", "small");
  MakeSymbol(size_info.get(), SectionId::kText, 50, "a/b.cc", "", "exact");
  // 2-alias 60 B group -> `Pss() == 30` (< 50, excluded).
  MakeAliasSymbols(size_info.get(), SectionId::kText, 60, "a/b.cc",
                   "small_alias_", 0, 2);
  // 2-alias 100 B group -> `Pss() == 50` (>= 50, kept).
  MakeAliasSymbols(size_info.get(), SectionId::kText, 100, "a/b.cc",
                   "keep_alias_", 0, 2);
  MakeAliasGroup(size_info.get(), 2, 4);
  MakeAliasGroup(size_info.get(), 4, 6);

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/50);

  Json::Value file_node = builder.Open("a/b.cc");
  ASSERT_EQ(3u, file_node["children"].size());
  EXPECT_EQ(150, file_node["size"].asInt());
}

TEST(TreeBuilderTest, TestMinSizeNormalModeDiff) {
  // Diff mode: filters on `|DeltaSymbol::Pss()|`.
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  MakeSymbol(before.get(), SectionId::kText, 100, "a/b.cc", "", "small_inc");
  MakeSymbol(before.get(), SectionId::kText, 100, "a/b.cc", "", "keep_inc");
  MakeSymbol(before.get(), SectionId::kText, 100, "a/b.cc", "", "keep_dec");
  MakeSymbol(before.get(), SectionId::kText, 100, "a/b.cc", "", "small_dec");

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  MakeSymbol(after.get(), SectionId::kText, 140, "a/b.cc", "", "small_inc");
  MakeSymbol(after.get(), SectionId::kText, 150, "a/b.cc", "", "keep_inc");
  MakeSymbol(after.get(), SectionId::kText, 40, "a/b.cc", "", "keep_dec");
  MakeSymbol(after.get(), SectionId::kText, 60, "a/b.cc", "", "small_dec");
  MakeAliasSymbols(after.get(), SectionId::kText, 80, "a/b.cc", "added_alias_",
                   0, 2);
  MakeAliasGroup(after.get(), 4, 6);

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/50);

  Json::Value file_node = builder.Open("a/b.cc");
  ASSERT_EQ(2u, file_node["children"].size());
  EXPECT_EQ("keep_dec", file_node["children"][0]["fullName"].asString());
  EXPECT_EQ(-60, file_node["children"][0]["size"].asInt());
  EXPECT_EQ("keep_inc", file_node["children"][1]["fullName"].asString());
  EXPECT_EQ(50, file_node["children"][1]["size"].asInt());
}

TEST(TreeBuilderTest, TestMinSizeRelocationsModeSnapshot) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  // Large byte size (1000 B) but only 2 relocations -> excluded.
  MakeSymbol(size_info.get(), SectionId::kDataRelRo, 1000, "a/b.cc", "",
             "big_bytes_few_relocs");
  size_info->raw_symbols.back().relocation_count_ = 2;
  // Small byte size (8 B) with 3 relocations -> kept.
  MakeSymbol(size_info.get(), SectionId::kDataRelRo, 8, "a/b.cc", "",
             "small_bytes_enough_relocs");
  size_info->raw_symbols.back().relocation_count_ = 3;
  // 2-alias group with 4 relocations (`RelocationPss() == 2.0f`) -> excluded.
  MakeAliasSymbols(size_info.get(), SectionId::kDataRelRo, 100, "a/b.cc",
                   "alias_few_", 0, 2, /*relocation_count=*/4);
  // 2-alias group with 6 relocations (`RelocationPss() == 3.0f`) -> kept.
  MakeAliasSymbols(size_info.get(), SectionId::kDataRelRo, 100, "a/b.cc",
                   "alias_keep_", 0, 2, /*relocation_count=*/6);
  MakeAliasGroup(size_info.get(), 2, 4);
  MakeAliasGroup(size_info.get(), 4, 6);

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/true, /*minimum_size=*/3);

  Json::Value file_node = builder.Open("a/b.cc");
  ASSERT_EQ(3u, file_node["children"].size());
  EXPECT_FLOAT_EQ(9.0f, file_node["size"].asFloat());
  EXPECT_EQ("alias_keep_0", file_node["children"][0]["fullName"].asString());
  EXPECT_EQ("alias_keep_1", file_node["children"][1]["fullName"].asString());
  EXPECT_EQ("small_bytes_enough_relocs",
            file_node["children"][2]["fullName"].asString());
  EXPECT_FLOAT_EQ(3.0f, file_node["children"][2]["size"].asFloat());
}

TEST(TreeBuilderTest, TestMinSizeRelocationsModeDiff) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  MakeSymbol(before.get(), SectionId::kDataRelRo, 100, "a/b.cc", "",
             "small_inc");
  before->raw_symbols.back().relocation_count_ = 1;
  MakeSymbol(before.get(), SectionId::kDataRelRo, 100, "a/b.cc", "",
             "keep_inc");
  before->raw_symbols.back().relocation_count_ = 1;
  MakeSymbol(before.get(), SectionId::kDataRelRo, 100, "a/b.cc", "",
             "keep_dec");
  before->raw_symbols.back().relocation_count_ = 5;
  MakeSymbol(before.get(), SectionId::kDataRelRo, 100, "a/b.cc", "",
             "small_dec");
  before->raw_symbols.back().relocation_count_ = 4;

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  // +2 relocations, +200 B -> excluded in relocations mode with
  // `minimum_size=3`.
  MakeSymbol(after.get(), SectionId::kDataRelRo, 300, "a/b.cc", "",
             "small_inc");
  after->raw_symbols.back().relocation_count_ = 3;
  // +3 relocations, +8 B -> kept.
  MakeSymbol(after.get(), SectionId::kDataRelRo, 108, "a/b.cc", "", "keep_inc");
  after->raw_symbols.back().relocation_count_ = 4;
  // -4 relocations (|-4| >= 3) -> kept.
  MakeSymbol(after.get(), SectionId::kDataRelRo, 100, "a/b.cc", "", "keep_dec");
  after->raw_symbols.back().relocation_count_ = 1;
  // -2 relocations (|-2| < 3) -> excluded.
  MakeSymbol(after.get(), SectionId::kDataRelRo, 100, "a/b.cc", "",
             "small_dec");
  after->raw_symbols.back().relocation_count_ = 2;

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/true, /*minimum_size=*/3);

  Json::Value file_node = builder.Open("a/b.cc");
  ASSERT_EQ(2u, file_node["children"].size());
  EXPECT_EQ("keep_dec", file_node["children"][0]["fullName"].asString());
  EXPECT_FLOAT_EQ(-4.0f, file_node["children"][0]["size"].asFloat());
  EXPECT_EQ("keep_inc", file_node["children"][1]["fullName"].asString());
  EXPECT_FLOAT_EQ(3.0f, file_node["children"][1]["size"].asFloat());
  EXPECT_FLOAT_EQ(-1.0f, builder.Open("")["size"].asFloat());
}

TEST(TreeBuilderTest, TestDumpCsvNormalModeSnapshot) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kText, 104, "a/b.cc", "",
             "foo(int, \"bar\")");
  size_info->raw_symbols.back().address_ = 0x1a0;
  size_info->raw_symbols.back().padding_ = 4;

  MakeSymbol(size_info.get(), SectionId::kData, 50, "", "", "obj_sym");
  size_info->raw_symbols.back().address_ = 0x20;
  size_info->raw_symbols.back().object_path_ = "obj/c.o";

  MakeAliasSymbols(size_info.get(), SectionId::kText, 120, "a/d.cc", "alias_",
                   0, 2);
  for (size_t i = 2; i < 4; ++i) {
    size_info->raw_symbols[i].address_ = 0x300;
    size_info->raw_symbols[i].padding_ = 8;
  }
  MakeAliasGroup(size_info.get(), 2, 4);

  MakeSymbol(size_info.get(), SectionId::kRoData, 30, "", "", "no_path");
  size_info->raw_symbols.back().address_ = static_cast<int32_t>(0x80000010u);

  MakeSymbol(size_info.get(), SectionId::kText, 10, "a/b.cc", "",
             "tiny_filtered");

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/20);

  EXPECT_EQ(5u, builder.Open("")["symbolCount"].asUInt());
  EXPECT_FALSE(builder.Open("a/b.cc").isMember("symbolCount"));

  EXPECT_EQ(
      "address,full_name,path,size_without_padding,num_aliases\n"
      "0x1a0,\"foo(int, \"\"bar\"\")\",a/b.cc,100,1\n"
      "0x300,alias_0,a/d.cc,112,2\n"
      "0x300,alias_1,a/d.cc,112,2\n"
      "0x20,obj_sym,obj/c.o,50,1\n"
      "0x80000010,no_path,,30,1\n",
      builder.DumpCsv());

  TreeBuilder empty_builder(size_info.get());
  empty_builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                      /*relocations_mode=*/false, /*minimum_size=*/1000);
  EXPECT_EQ(0u, empty_builder.Open("")["symbolCount"].asUInt());
  EXPECT_EQ("address,full_name,path,size_without_padding,num_aliases\n",
            empty_builder.DumpCsv());
}

TEST(TreeBuilderTest, TestDumpCsvNormalModeDiff) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  MakeSymbol(before.get(), SectionId::kText, 10, "a/b.cc", "", "changed");
  before->raw_symbols.back().padding_ = 2;
  MakeSymbol(before.get(), SectionId::kText, 40, "a/b.cc", "", "unchanged");
  MakeAliasSymbols(before.get(), SectionId::kText, 64, "a/b.cc", "removed_", 0,
                   2);
  for (size_t i = 2; i < 4; ++i) {
    before->raw_symbols[i].padding_ = 4;
  }
  MakeAliasGroup(before.get(), 2, 4);

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  MakeSymbol(after.get(), SectionId::kText, 60, "a/b.cc", "", "changed");
  after->raw_symbols.back().padding_ = 2;
  MakeSymbol(after.get(), SectionId::kText, 40, "a/b.cc", "", "unchanged");
  MakeAliasSymbols(after.get(), SectionId::kText, 44, "a/b.cc", "alias_", 0, 2);
  for (size_t i = 2; i < 4; ++i) {
    after->raw_symbols[i].padding_ = 4;
  }
  MakeAliasGroup(after.get(), 2, 4);

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);

  EXPECT_EQ(5u, builder.Open("")["symbolCount"].asUInt());
  EXPECT_EQ(
      "full_name,path,size_without_padding,num_aliases\n"
      "changed,a/b.cc,50,1\n"
      "removed_0,a/b.cc,-60,2\n"
      "removed_1,a/b.cc,-60,2\n"
      "alias_0,a/b.cc,40,2\n"
      "alias_1,a/b.cc,40,2\n",
      builder.DumpCsv());

  TreeBuilder empty_builder(&diff);
  empty_builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                      /*relocations_mode=*/false, /*minimum_size=*/1000);
  EXPECT_EQ(0u, empty_builder.Open("")["symbolCount"].asUInt());
  EXPECT_EQ("full_name,path,size_without_padding,num_aliases\n",
            empty_builder.DumpCsv());
}

TEST(TreeBuilderTest, TestDumpCsvMethodCountModeSnapshot) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kDexMethod, 100, "a/z.java", "",
             "z_method");
  size_info->raw_symbols.back().address_ = 0x10;
  MakeSymbol(size_info.get(), SectionId::kDexMethod, 10, "a/a.java", "",
             "a_method");
  size_info->raw_symbols.back().address_ = 0x20;
  MakeSymbol(size_info.get(), SectionId::kText, 500, "a/b.cc", "", "native");

  TreeBuilder builder(size_info.get());
  FilterList filters = {
      [](const GroupedPath&, const BaseSymbol& sym) {
        return sym.Section() == SectionId::kDexMethod;
      },
  };
  builder.Build(std::make_unique<IdPathLens>(), '/',
                /*method_count_mode=*/true, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);

  EXPECT_EQ(2u, builder.Open("")["symbolCount"].asUInt());
  EXPECT_EQ(
      "address,full_name,path,method_count,num_aliases\n"
      "0x20,a_method,a/a.java,1,1\n"
      "0x10,z_method,a/z.java,1,1\n",
      builder.DumpCsv());
}

TEST(TreeBuilderTest, TestDumpCsvMethodCountModeDiff) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  MakeSymbol(before.get(), SectionId::kDexMethod, 10, "a/a.java", "",
             "unchanged");
  MakeSymbol(before.get(), SectionId::kDexMethod, 20, "a/b.java", "",
             "removed");

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  MakeSymbol(after.get(), SectionId::kDexMethod, 10, "a/a.java", "",
             "unchanged");
  MakeSymbol(after.get(), SectionId::kDexMethod, 30, "a/a.java", "", "added");

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/',
                /*method_count_mode=*/true, filters,
                /*relocations_mode=*/false, /*minimum_size=*/0);

  EXPECT_EQ(2u, builder.Open("")["symbolCount"].asUInt());
  EXPECT_EQ(
      "full_name,path,method_count,num_aliases\n"
      "added,a/a.java,1,1\n"
      "removed,a/b.java,-1,1\n",
      builder.DumpCsv());
}

TEST(TreeBuilderTest, TestDumpCsvRelocationsModeSnapshot) {
  std::unique_ptr<SizeInfo> size_info = MakeSizeInfo();
  MakeSymbol(size_info.get(), SectionId::kDataRelRo, 1000, "a/b.cc", "",
             "no_relocs");
  MakeSymbol(size_info.get(), SectionId::kDataRelRo, 8, "a/b.cc", "", "single");
  size_info->raw_symbols.back().address_ = 0x10;
  size_info->raw_symbols.back().relocation_count_ = 3;

  MakeAliasSymbols(size_info.get(), SectionId::kDataRelRo, 100, "a/c.cc",
                   "alias_", 0, 2, /*relocation_count=*/10);
  for (size_t i = 2; i < 4; ++i) {
    size_info->raw_symbols[i].address_ = 0x20;
  }
  MakeAliasGroup(size_info.get(), 2, 4);

  TreeBuilder builder(size_info.get());
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/true, /*minimum_size=*/0);

  EXPECT_EQ(3u, builder.Open("")["symbolCount"].asUInt());
  EXPECT_EQ(
      "address,full_name,path,relocation_count,num_aliases\n"
      "0x20,alias_0,a/c.cc,10,2\n"
      "0x20,alias_1,a/c.cc,10,2\n"
      "0x10,single,a/b.cc,3,1\n",
      builder.DumpCsv());
}

TEST(TreeBuilderTest, TestDumpCsvRelocationsModeDiff) {
  std::unique_ptr<SizeInfo> before = MakeSizeInfo();
  MakeSymbol(before.get(), SectionId::kDataRelRo, 100, "a/b.cc", "", "inc");
  before->raw_symbols.back().relocation_count_ = 1;
  MakeSymbol(before.get(), SectionId::kDataRelRo, 100, "a/b.cc", "", "dec");
  before->raw_symbols.back().relocation_count_ = 6;
  MakeSymbol(before.get(), SectionId::kDataRelRo, 100, "a/b.cc", "", "same");
  before->raw_symbols.back().relocation_count_ = 4;

  std::unique_ptr<SizeInfo> after = MakeSizeInfo();
  MakeSymbol(after.get(), SectionId::kDataRelRo, 100, "a/b.cc", "", "inc");
  after->raw_symbols.back().relocation_count_ = 4;
  MakeSymbol(after.get(), SectionId::kDataRelRo, 100, "a/b.cc", "", "dec");
  after->raw_symbols.back().relocation_count_ = 1;
  MakeSymbol(after.get(), SectionId::kDataRelRo, 100, "a/b.cc", "", "same");
  after->raw_symbols.back().relocation_count_ = 4;

  DeltaSizeInfo diff = Diff(before.get(), after.get(), nullptr, nullptr);
  TreeBuilder builder(&diff);
  FilterList filters;
  builder.Build(std::make_unique<IdPathLens>(), '/', false, filters,
                /*relocations_mode=*/true, /*minimum_size=*/0);

  EXPECT_EQ(2u, builder.Open("")["symbolCount"].asUInt());
  EXPECT_EQ(
      "full_name,path,relocation_count,num_aliases\n"
      "dec,a/b.cc,-5,1\n"
      "inc,a/b.cc,3,1\n",
      builder.DumpCsv());
}

}  // namespace caspian
