// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/diff.h"

#include <stdint.h>

#include <deque>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

namespace caspian {
namespace {

Symbol MakeSymbol(SectionId section_id,
                  int32_t size,
                  const char* path,
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

  for (Symbol& sym : info->raw_symbols) {
    // Copy path literals into |owned_strings| so that |size_info1_| and
    // |size_info2_| do not share identical const char* path pointers (matching
    // real .size files parsed into separate buffers).
    info->owned_strings.push_back(sym.object_path_);
    sym.object_path_ = info->owned_strings.back().c_str();
    sym.container_ = &info->containers[0];
  }
  return info;
}

void MakeAliasGroup(SizeInfo* info, size_t start, size_t end) {
  static base::NoDestructor<std::deque<std::vector<Symbol*>>> alias_groups;
  alias_groups->emplace_back();
  for (size_t i = start; i < end; i++) {
    info->raw_symbols[i].aliases_ = &alias_groups->back();
    alias_groups->back().push_back(&info->raw_symbols[i]);
  }
}

class DiffTest : public testing::Test {
 public:
  void SetUp() override {
    size_info1_ = CreateSizeInfo();
    size_info2_ = CreateSizeInfo();
  }

 protected:
  std::unique_ptr<SizeInfo> size_info1_;
  std::unique_ptr<SizeInfo> size_info2_;
  std::vector<std::string> removed_sources_;
  std::vector<std::string> added_sources_;
};

TEST_F(DiffTest, TestIdentity) {
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);
  for (const DeltaSymbol& sym : diff.delta_symbols) {
    EXPECT_EQ(sym.Size(), 0);
    EXPECT_EQ(sym.Padding(), 0);
  }
}

TEST_F(DiffTest, TestSimpleAdd) {
  size_info1_->raw_symbols.erase(size_info1_->raw_symbols.begin());
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolPadding(diff));
  EXPECT_EQ(10, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestSimpleDelete) {
  size_info2_->raw_symbols.erase(size_info2_->raw_symbols.begin());
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{5, 0, 0, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolPadding(diff));
  EXPECT_EQ(-10, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestSimpleChange) {
  size_info2_->raw_symbols[0].size_ += 11;
  size_info2_->raw_symbols[0].padding_ += 20;
  size_info2_->raw_symbols.back().size_ += 11;
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{4, 2, 1, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(20, SumOfSymbolPadding(diff));
  EXPECT_EQ(22, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestDontMatchAcrossSections) {
  size_info1_->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 11, "asdf", "Hello"));
  size_info2_->raw_symbols.push_back(
      MakeSymbol(SectionId::kRoData, 11, "asdf", "Hello"));

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{6, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestDontMatchAcrossContainers) {
  size_info1_->containers.emplace_back("C2");
  Container::AssignShortNames(&size_info1_->containers);
  size_info2_->containers.emplace_back("C2");
  Container::AssignShortNames(&size_info2_->containers);

  size_info1_->raw_symbols[0].container_ = &size_info1_->containers[1];

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestAliasesRemove) {
  MakeAliasGroup(size_info1_.get(), 0, 3);
  MakeAliasGroup(size_info2_.get(), 0, 2);

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{3, 3, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestAliasesAdd) {
  MakeAliasGroup(size_info1_.get(), 0, 2);
  MakeAliasGroup(size_info2_.get(), 0, 3);

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{3, 3, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestAliasesChangeGroup) {
  MakeAliasGroup(size_info1_.get(), 0, 2);
  MakeAliasGroup(size_info1_.get(), 2, 5);
  MakeAliasGroup(size_info2_.get(), 0, 3);
  MakeAliasGroup(size_info2_.get(), 3, 5);

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{2, 4, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

// These tests currently fail because _ doesn't rewrite.

TEST_F(DiffTest, TestStarSymbolNormalization) {
  // Matches in pass 2 (numbers stripped).
  SetName(&size_info1_->raw_symbols[0], "* symbol gap 1 (end of section)");
  SetName(&size_info2_->raw_symbols[0], "* symbol gap 2 (end of section)");
  // Suffix present on only one side: matches in pass 3 (star normalization).
  SetName(&size_info1_->raw_symbols[1], "* symbol gap 1");
  SetName(&size_info2_->raw_symbols[1], "* symbol gap 2 (end of section)");
  // Different parenthesized suffixes: matches in pass 3.
  SetName(&size_info1_->raw_symbols[2], "* symbol gap 1 (foo)");
  SetName(&size_info2_->raw_symbols[2], "* symbol gap 2 (bar)");

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{6, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestNumberNormalization) {
  size_info1_->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 11, "a", ".L__unnamed_1193"));
  size_info1_->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 22, "a", ".L__unnamed_1194"));
  size_info1_->raw_symbols.push_back(MakeSymbol(
      SectionId::kText, 33, "a", "SingleCategoryPreferences$3#this$0"));
  size_info1_->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 44, "a", ".L.ref.tmp.2"));

  size_info2_->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 11, "a", ".L__unnamed_2194"));
  size_info2_->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 22, "a", ".L__unnamed_2195"));
  size_info2_->raw_symbols.push_back(MakeSymbol(
      SectionId::kText, 33, "a", "SingleCategoryPreferences$9#this$009"));
  size_info2_->raw_symbols.push_back(
      MakeSymbol(SectionId::kText, 44, "a", ".L.ref.tmp.137"));

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{10, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestDuplicateKeysMatchInOrder) {
  // Three "before" symbols and four "after" symbols share the same key once
  // numbers are stripped (pass 2). They should be paired in order, and the
  // extra "after" symbol should be reported as added.
  const char* kBeforeNames[] = {".L__unnamed_1", ".L__unnamed_2",
                                ".L__unnamed_3"};
  const int32_t kBeforeSizes[] = {11, 12, 13};
  const char* kAfterNames[] = {".L__unnamed_4", ".L__unnamed_5",
                               ".L__unnamed_6", ".L__unnamed_7"};
  const int32_t kAfterSizes[] = {100, 200, 300, 400};
  for (size_t i = 0; i < std::size(kBeforeNames); ++i) {
    size_info1_->raw_symbols.push_back(
        MakeSymbol(SectionId::kText, kBeforeSizes[i], "c", kBeforeNames[i]));
  }
  for (size_t i = 0; i < std::size(kAfterNames); ++i) {
    size_info2_->raw_symbols.push_back(
        MakeSymbol(SectionId::kText, kAfterSizes[i], "c", kAfterNames[i]));
  }

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{6, 3, 1, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  int checked_pairs = 0;
  for (const DeltaSymbol& sym : diff.delta_symbols) {
    const BaseSymbol* before_sym = sym.Before();
    const BaseSymbol* after_sym = sym.After();
    if (!after_sym || after_sym->ObjectPath() != std::string_view("c")) {
      continue;
    }
    if (!before_sym) {
      EXPECT_EQ(400, after_sym->Size());
      continue;
    }
    EXPECT_EQ(after_sym->Size(), (before_sym->Size() - 10) * 100);
    checked_pairs++;
  }
  EXPECT_EQ(3, checked_pairs);
}

TEST_F(DiffTest, TestChangedParams) {
  // Ensure that params changes match up so long as path doesn't change.
  size_info1_->raw_symbols[0].full_name_ = "Foo()";
  size_info1_->raw_symbols[0].name_ = "Foo";
  size_info2_->raw_symbols[0].full_name_ = "Foo(bool)";
  size_info2_->raw_symbols[0].name_ = "Foo";

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{6, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestChangedPathsNative) {
  // Ensure that non-globally-unique symbols are not matched when path changes.
  size_info2_->raw_symbols[1].object_path_ = "asdf";
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestChangedPathsJava) {
  // Ensure that Java symbols are matched up.
  size_info2_->raw_symbols[0].object_path_ = "asdf";
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{6, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestChangedPathsChangedParams) {
  // Ensure that path changes are not matched when params also change.
  size_info1_->raw_symbols[0].full_name_ = "Foo()";
  size_info1_->raw_symbols[0].name_ = "Foo";
  size_info2_->raw_symbols[0].full_name_ = "Foo(bool)";
  size_info2_->raw_symbols[0].name_ = "Foo";
  size_info2_->raw_symbols[0].object_path_ = "asdf";
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestChangedPathsStringLiterals) {
  // Ensure that path changes are not matched when params also change.
  size_info1_->raw_symbols[0].full_name_ = kStringLiteralName;
  size_info1_->raw_symbols[0].name_ = kStringLiteralName;
  size_info2_->raw_symbols[0].full_name_ = kStringLiteralName;
  size_info2_->raw_symbols[0].name_ = kStringLiteralName;
  size_info2_->raw_symbols[0].object_path_ = "asdf";
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(0, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestChangedPathsNamedStringLiteralsSameSize) {
  // Ensure that path changes are not matched when params also change.
  size_info1_->raw_symbols[0].full_name_ = "\"asdf...\"";
  size_info1_->raw_symbols[0].name_ = "\"asdf...\"";
  size_info2_->raw_symbols[0].full_name_ = "\"asdf...\"";
  size_info2_->raw_symbols[0].name_ = "\"asdf...\"";
  size_info2_->raw_symbols[0].object_path_ = "asdf";
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{6, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
}

TEST_F(DiffTest, TestChangedPathsNamedStringLiteralsDifferentSize) {
  // Ensure that path changes are not matched when params also change.
  size_info1_->raw_symbols[0].full_name_ = "\"asdf...\"";
  size_info1_->raw_symbols[0].name_ = "\"asdf...\"";
  size_info2_->raw_symbols[0].full_name_ = "\"asdf...\"";
  size_info2_->raw_symbols[0].name_ = "\"asdf...\"";
  size_info2_->raw_symbols[0].object_path_ = "asdf";
  size_info2_->raw_symbols[0].size_ += 10;
  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  DeltaSizeInfo::Results expected_counts{5, 0, 1, 1};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(10, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestAliasesCount) {
  MakeAliasGroup(size_info1_.get(), 2, 5);
  MakeAliasGroup(size_info2_.get(), 2, 5);

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);

  int checked_symbols = 0;
  for (const DeltaSymbol& sym : diff.delta_symbols) {
    if (sym.Name().find("30") != std::string_view::npos ||
        sym.Name().find("40") != std::string_view::npos ||
        sym.Name().find("50") != std::string_view::npos) {
      ASSERT_NE(nullptr, sym.Before());
      ASSERT_NE(nullptr, sym.After());
      EXPECT_EQ(3, sym.Before()->NumAliases());
      EXPECT_EQ(3, sym.After()->NumAliases());
      checked_symbols++;
    }
  }
  EXPECT_EQ(3, checked_symbols);
}

TEST_F(DiffTest, TestManySymbolsRehash) {
  for (int i = 0; i < 200; ++i) {
    SectionId section = (i % 2 == 0) ? SectionId::kText : SectionId::kRoData;
    std::string name = "Sym_" + base::NumberToString(i);
    Symbol s1 = MakeSymbol(section, 100 + i, "path/a.o", name);
    s1.padding_ = 4;
    s1.container_ = &size_info1_->containers[0];
    size_info1_->owned_strings.push_back(name);
    s1.full_name_ = size_info1_->owned_strings.back();
    s1.template_name_ = s1.full_name_;
    s1.name_ = s1.full_name_;
    size_info1_->raw_symbols.push_back(s1);

    Symbol s2 = s1;
    s2.size_ += 8;
    s2.padding_ = 8;
    s2.container_ = &size_info2_->containers[0];
    size_info2_->raw_symbols.push_back(s2);
  }

  DeltaSizeInfo diff = Diff(size_info1_.get(), size_info2_.get(),
                            &removed_sources_, &added_sources_);
  DeltaSizeInfo::Results expected_counts{6, 200, 2, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());
  EXPECT_EQ(800, SumOfSymbolPadding(diff));
  EXPECT_EQ(1600, SumOfSymbolSizes(diff));
}

TEST_F(DiffTest, TestManySymbolsDeriveNamesOnThreads) {
  // Enough symbols to diff on multiple threads, with names left for
  // Symbol::DeriveNames() so that they match only in pass 3 (Name()). Data
  // races are only detected when run under TSan.
  constexpr int kNumSymbols = 600;
  auto make_size_info = [](const char* params) {
    auto info = std::make_unique<SizeInfo>();
    info->containers.emplace_back("");
    for (int i = 0; i < kNumSymbols; ++i) {
      bool is_java = i % 2 == 0;
      std::string path = "path/" + base::NumberToString(i % 20) + ".o";
      std::string name = (is_java ? "com.Foo#bar" : "ns::Func") +
                         base::NumberToString(i) + params;
      info->owned_strings.push_back(path);
      const char* path_ptr = info->owned_strings.back().c_str();
      info->owned_strings.push_back(name);
      Symbol sym;
      sym.full_name_ = info->owned_strings.back();
      sym.section_id_ = is_java ? SectionId::kDexMethod : SectionId::kText;
      sym.size_ = 10 + i;
      sym.object_path_ = path_ptr;
      sym.container_ = &info->containers[0];
      sym.size_info_ = info.get();
      info->raw_symbols.push_back(sym);
    }
    return info;
  };
  std::unique_ptr<SizeInfo> before = make_size_info("(int)");
  std::unique_ptr<SizeInfo> after = make_size_info("(bool)");

  DeltaSizeInfo diff =
      Diff(before.get(), after.get(), &removed_sources_, &added_sources_);
  DeltaSizeInfo::Results expected_counts{kNumSymbols, 0, 0, 0};
  EXPECT_EQ(expected_counts, diff.CountsByDiffStatus());

  // Derived names must be owned by the SizeInfo of their symbol.
  auto count_strings = [](const SizeInfo& info) {
    size_t ret = 0;
    for (const auto& arena : info.thread_owned_strings) {
      ret += arena.size();
    }
    return ret;
  };
  EXPECT_GT(count_strings(*before), 0u);
  EXPECT_GT(count_strings(*after), 0u);
  EXPECT_EQ("ns::Func1", before->raw_symbols[1].Name());
  EXPECT_EQ("ns::Func1", after->raw_symbols[1].Name());
}

}  // namespace
}  // namespace caspian
