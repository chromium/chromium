// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/diff.h"

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

namespace caspian {
namespace {

Symbol MakeSymbol(SectionId section_id,
                  int32_t size,
                  const char* path,
                  std::string_view name = "") {
  static std::deque<std::string> symbol_names;
  if (name.empty()) {
    symbol_names.push_back(std::string());
    std::string& s = symbol_names.back();
    s += static_cast<char>(section_id);
    s += "_";
    s += std::to_string(size);
    s += "A";
    name = s;
  }
  Symbol sym;
  sym.full_name_ = name;
  sym.template_name_ = name;
  sym.name_ = name;
  sym.section_id_ = section_id;
  sym.size_ = size;
  sym.object_path_ = path;
  return sym;
}

void SetName(Symbol* symbol, std::string_view full_name) {
  symbol->full_name_ = full_name;
  symbol->template_name_ = full_name;
  symbol->name_ = full_name;
}

int32_t SumOfSymbolSizes(const DeltaSizeInfo& info) {
  int32_t size = 0;
  for (const DeltaSymbol& sym : info.delta_symbols) {
    size += sym.Size();
  }
  return size;
}

int32_t SumOfSymbolPadding(const DeltaSizeInfo& info) {
  int32_t size = 0;
  for (const DeltaSymbol& sym : info.delta_symbols) {
    size += sym.Padding();
  }
  return size;
}

std::unique_ptr<SizeInfo> CreateSizeInfo() {
  std::unique_ptr<SizeInfo> info = std::make_unique<SizeInfo>();
  info->containers.emplace_back("");
  Container::AssignShortNames(&info->containers);
  info->raw_symbols.push_back(
      MakeSymbol(SectionId::kDexMethod, 10, "a", "com.Foo#bar()"));
  info->raw_symbols.push_back(MakeSymbol(SectionId::kText, 20, "a", ".Lfoo"));
  info->raw_symbols.push_back(MakeSymbol(SectionId::kText, 30, "b"));
  info->raw_symbols.push_back(MakeSymbol(SectionId::kText, 40, "b"));
  info->raw_symbols.push_back(MakeSymbol(SectionId::kText, 50, "b"));
  info->raw_symbols.push_back(MakeSymbol(SectionId::kText, 60, ""));

  for (auto sym : info->raw_symbols) {
    sym.container_ = &info->containers[0];
  }
  return info;
}

void MakeAliasGroup(SizeInfo* info, size_t start, size_t end) {
  static std::deque<std::vector<Symbol*>> alias_groups;
  alias_groups.emplace_back();
  for (size_t i = start; i < end; i++) {
    info->raw_symbols[i].aliases_ = &alias_groups.back();
    alias_groups.back().push_back(&info->raw_symbols[i]);
  }
}

TEST(DiffTest, TestIdentity) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());
  for (const DeltaSymbol& sym : diff.delta_symbols) {
    EXPECT_EQ(sym.Size(), 0);
    EXPECT_EQ(sym.Padding(), 0);
  }
}

TEST(DiffTest, TestSimpleAdd) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  size_info1->raw_symbols.erase(size_info1->raw_symbols.begin());
  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolPadding(diff));
  EXPECT_EQ(10, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestSimpleDelete) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  size_info2->raw_symbols.erase(size_info2->raw_symbols.begin());
  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{5, 0, 0, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolPadding(diff));
  EXPECT_EQ(-10, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestSimpleChange) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  size_info2->raw_symbols[0].size_ += 11;
  size_info2->raw_symbols[0].padding_ += 20;
  size_info2->raw_symbols.back().size_ += 11;
  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{4, 2, 1, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(20, SumOfSymbolPadding(diff));
  EXPECT_EQ(22, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestDontMatchAcrossSections) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  size_info1->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 11, "asdf", "Hello"));
  size_info2->raw_symbols.push_back(
      MakeSymbol(SectionId::kRoData, 11, "asdf", "Hello"));

  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{6, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestDontMatchAcrossContainers) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();
  size_info1->containers.emplace_back("C2");
  Container::AssignShortNames(&size_info1->containers);
  size_info2->containers.emplace_back("C2");
  Container::AssignShortNames(&size_info2->containers);

  size_info1->raw_symbols[0].container_ = &size_info1->containers[1];

  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestAliasesRemove) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  MakeAliasGroup(size_info1.get(), 0, 3);
  MakeAliasGroup(size_info2.get(), 0, 2);

  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{3, 3, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestAliasesAdd) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  MakeAliasGroup(size_info1.get(), 0, 2);
  MakeAliasGroup(size_info2.get(), 0, 3);

  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{3, 3, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestAliasesChangeGroup) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  MakeAliasGroup(size_info1.get(), 0, 2);
  MakeAliasGroup(size_info1.get(), 2, 5);
  MakeAliasGroup(size_info2.get(), 0, 3);
  MakeAliasGroup(size_info2.get(), 3, 5);

  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{2, 4, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

// These tests currently fail because _ doesn't rewrite.

TEST(DiffTest, TestStarSymbolNormalization) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  SetName(&size_info1->raw_symbols[0], "* symbol gap 1 (end of section)");
  SetName(&size_info2->raw_symbols[0], "* symbol gap 2 (end of section)");

  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{6, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestNumberNormalization) {
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  size_info1->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 11, "a", ".L__unnamed_1193"));
  size_info1->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 22, "a", ".L__unnamed_1194"));
  size_info1->raw_symbols.push_back(MakeSymbol(
      SectionId::kText, 33, "a", "SingleCategoryPreferences$3#this$0"));
  size_info1->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 44, "a", ".L.ref.tmp.2"));

  size_info2->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 11, "a", ".L__unnamed_2194"));
  size_info2->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 22, "a", ".L__unnamed_2195"));
  size_info2->raw_symbols.push_back(MakeSymbol(
      SectionId::kText, 33, "a", "SingleCategoryPreferences$9#this$009"));
  size_info2->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 44, "a", ".L.ref.tmp.137"));

  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{10, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestChangedParams) {
  // Ensure that params changes match up so long as path doesn't change.
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  size_info1->raw_symbols[0].full_name_ = "Foo()";
  size_info1->raw_symbols[0].name_ = "Foo";
  size_info2->raw_symbols[0].full_name_ = "Foo(bool)";
  size_info2->raw_symbols[0].name_ = "Foo";

  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{6, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestChangedPathsNative) {
  // Ensure that non-globally-unique symbols are not matched when path changes.
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  size_info2->raw_symbols[1].object_path_ = "asdf";
  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestChangedPathsJava) {
  // Ensure that Java symbols are matched up.
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  size_info2->raw_symbols[0].object_path_ = "asdf";
  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{6, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST(DiffTest, TestChangedPathsChangedParams) {
  // Ensure that path changes are not matched when params also change.
  std::unique_ptr<SizeInfo> size_info1 = CreateSizeInfo();
  std::unique_ptr<SizeInfo> size_info2 = CreateSizeInfo();

  size_info1->raw_symbols[0].full_name_ = "Foo()";
  size_info1->raw_symbols[0].name_ = "Foo";
  size_info2->raw_symbols[0].full_name_ = "Foo(bool)";
  size_info2->raw_symbols[0].name_ = "Foo";
  size_info2->raw_symbols[0].object_path_ = "asdf";
  DeltaSizeInfo diff = Diff(size_info1.get(), size_info2.get());

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}
}  // namespace
}  // namespace caspian
