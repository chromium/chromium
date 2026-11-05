// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/file_format.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils_portable.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

namespace caspian {
namespace {

std::vector<char> Gzip(std::string_view data) {
  std::vector<char> ret(data.size() + 1024);
  uLongf compressed_size = ret.size();
  int result = zlib_internal::GzipCompressHelper(
      reinterpret_cast<Bytef*>(ret.data()), &compressed_size,
      reinterpret_cast<const Bytef*>(data.data()), data.size(), nullptr,
      nullptr);
  EXPECT_EQ(Z_OK, result);
  ret.resize(compressed_size);
  return ret;
}

TEST(FileFormatTest, ParseSizeInfo) {
  const std::string json = R"({"has_padding": true})";
  std::string contents = "# Created by //tools/binary_size\n";
  contents += "Size File Format v1\n";
  contents += base::NumberToString(json.size()) + "\n" + json + "\n";
  // Paths: (object_path, [source_path]).
  contents += "2\n";
  contents += "obj/a.o\tsrc/a.cc\n";
  contents += "obj/b.o\n";
  // Sections and their symbol counts (.rodata is empty).
  contents += ".text\t.rodata\t.data\n";
  contents += "4\t0\t2\n";
  // Addresses (delta-encoded).
  contents += "100 -10 30 0\n\n5 5\n";
  // Sizes.
  contents += "1 2 3 4\n\n7 8\n";
  // Paddings.
  contents += "0 0 0 0\n\n0 0\n";
  // Path indices (delta-encoded).
  contents += "0 1 -1 0\n\n1 0\n";
  // Symbols: "<full_name>[\t0<num_aliases_hex>][\t<flags_hex>]".
  contents += "foo_deadbeef\n";  // Name that ends with hex characters.
  contents += "baz\t1a\n";       // Flags only.
  contents += "qux\t02\t4\n";    // Aliases and flags.
  contents += "qux2\n";          // Second member of the alias group.
  contents += "d1\t02\n";        // Aliases only.
  contents += "d2";              // No trailing newline.

  std::vector<char> gzipped = Gzip(contents);
  SizeInfo info;
  ParseSizeInfo(gzipped.data(), gzipped.size(), &info);

  ASSERT_EQ(6u, info.raw_symbols.size());
  const std::vector<Symbol>& syms = info.raw_symbols;

  EXPECT_EQ("foo_deadbeef", syms[0].full_name_);
  EXPECT_EQ("baz", syms[1].full_name_);
  EXPECT_EQ("qux", syms[2].full_name_);
  EXPECT_EQ("qux2", syms[3].full_name_);
  EXPECT_EQ("d1", syms[4].full_name_);
  EXPECT_EQ("d2", syms[5].full_name_);

  EXPECT_EQ(0, syms[0].flags_);
  EXPECT_EQ(0x1a, syms[1].flags_);
  EXPECT_EQ(4, syms[2].flags_);
  EXPECT_EQ(0, syms[3].flags_);
  EXPECT_EQ(0, syms[4].flags_);

  EXPECT_EQ(1, syms[0].NumAliases());
  EXPECT_EQ(1, syms[1].NumAliases());
  EXPECT_EQ(2, syms[2].NumAliases());
  EXPECT_EQ(2, syms[3].NumAliases());
  EXPECT_EQ(syms[2].aliases_, syms[3].aliases_);
  EXPECT_EQ(2, syms[4].NumAliases());
  EXPECT_EQ(2, syms[5].NumAliases());

  EXPECT_EQ(100, syms[0].address_);
  EXPECT_EQ(90, syms[1].address_);
  EXPECT_EQ(120, syms[2].address_);
  EXPECT_EQ(120, syms[3].address_);
  EXPECT_EQ(5, syms[4].address_);
  EXPECT_EQ(10, syms[5].address_);

  EXPECT_EQ(1, syms[0].size_);
  EXPECT_EQ(8, syms[5].size_);

  EXPECT_EQ(std::string_view("obj/a.o"), syms[0].object_path_);
  EXPECT_EQ(std::string_view("src/a.cc"), syms[0].source_path_);
  EXPECT_EQ(std::string_view("obj/b.o"), syms[1].object_path_);
  EXPECT_EQ(std::string_view(""), syms[1].source_path_);
  EXPECT_EQ(std::string_view("obj/a.o"), syms[2].object_path_);
  EXPECT_EQ(std::string_view("obj/b.o"), syms[4].object_path_);
  EXPECT_EQ(std::string_view("obj/b.o"), syms[5].object_path_);

  EXPECT_EQ(SectionId::kText, syms[0].section_id_);
  EXPECT_EQ(SectionId::kData, syms[4].section_id_);
}

}  // namespace
}  // namespace caspian
