// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/file_format.h"

#include <stdint.h>

#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils_portable.h"
#include "third_party/zstd/src/lib/zstd.h"
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

void AppendU32(std::string* s, uint32_t v) {
  s->append(reinterpret_cast<const char*>(&v), 4);
}

void AppendU64(std::string* s, uint64_t v) {
  s->append(reinterpret_cast<const char*>(&v), 8);
}

std::string SplitU32(std::initializer_list<uint32_t> vals) {
  std::string out(vals.size() * 4, '\0');
  size_t i = 0;
  for (uint32_t v : vals) {
    for (size_t b = 0; b < 4; ++b) {
      out[b * vals.size() + i] = static_cast<char>((v >> (b * 8)) & 0xFF);
    }
    ++i;
  }
  return out;
}

std::string PackV2Blobs(
    uint32_t flags,
    const std::vector<std::pair<std::string, std::string>>& blobs) {
  std::string out(
      "# Created by //tools/binary_size\nSize File Format v2\n\0\0\0", 56);
  AppendU32(&out, flags);
  AppendU32(&out, static_cast<uint32_t>(blobs.size()));

  std::vector<std::string> comp(blobs.size());
  uint64_t offset = 64 + blobs.size() * 40;
  for (size_t i = 0; i < blobs.size(); ++i) {
    comp[i].resize(ZSTD_compressBound(blobs[i].second.size()));
    size_t sz =
        ZSTD_compress(comp[i].data(), comp[i].size(), blobs[i].second.data(),
                      blobs[i].second.size(), 1);
    EXPECT_FALSE(ZSTD_isError(sz));
    comp[i].resize(sz);

    std::string name = blobs[i].first;
    name.resize(16, '\0');
    out.append(name);
    AppendU64(&out, offset);
    AppendU64(&out, comp[i].size());
    AppendU64(&out, blobs[i].second.size());
    offset += comp[i].size();
  }
  for (const auto& c : comp) {
    out.append(c);
  }
  return out;
}

TEST(FileFormatTest, ParseSizeInfoV2AndDiff) {
  std::string meta = R"({
    "build_config": {"arch": "arm64"},
    "containers": [{"name": "base.apk"}, {"name": "feat.apk"}],
    "segments": [[0, ".text", 3], [1, ".data.rel.ro", 1]],
    "total_symbols": 4
  })";
  std::string paths;
  AppendU32(&paths, 2);  // 2 components
  AppendU32(&paths, 2);  // 2 paths
  paths += SplitU32({0, 1});
  constexpr char kPathStrs[] =
      "Blink\0Internals\0obj/foo.o\0src/foo.cc\0obj/t.o\0src/t.cc";
  paths.append(kPathStrs, sizeof(kPathStrs));

  std::string aliases;
  AppendU32(&aliases, 1);
  aliases += SplitU32({0}) + SplitU32({2});

  // Shard 0 (start=0, count=2): perm=[0, 1] (ZigZag deltas: 0, 2),
  // names: "foo::Bar()", "foo::Baz()" (LCP=7, suf="z()").
  std::string names0;
  AppendU32(&names0, 0);
  AppendU32(&names0, 2);
  names0 += SplitU32({0, 2});
  constexpr char kNames0Strs[] =
      "\x00\x07"
      "foo::Bar()\0z()";
  names0.append(kNames0Strs, sizeof(kNames0Strs));

  // Shard 1 (start=2, count=2): sorted order ["Overhead: gap", "kTable"] maps
  // to perm=[1, 0] (ZigZag deltas: 2, 1), LCP=[0, 0].
  std::string names1;
  AppendU32(&names1, 2);
  AppendU32(&names1, 2);
  names1 += SplitU32({2, 1});
  constexpr char kNames1Strs[] =
      "\x00\x00"
      "Overhead: gap\0kTable";
  names1.append(kNames1Strs, sizeof(kNames1Strs));

  std::string disasm;
  AppendU32(&disasm, 1);
  AppendU32(&disasm, 0);
  AppendU32(&disasm, 3);
  disasm.append("ret", 3);

  std::string v2 = PackV2Blobs(
      1u,  // has_padding = true
      {
          {"meta", meta},
          {"paths", paths},
          {"addresses", SplitU32({0x1000, 0, 0x20, 0x1FE0})},
          {"sizes", SplitU32({0x20, 0x20, 0x10, 12})},
          {"paddings", SplitU32({4, 4, 0, 12})},
          {"path_indices", SplitU32({0, 0, 0, 2})},
          {"flags", SplitU32({SymbolFlag::kStartup, 0, 0, SymbolFlag::kRel})},
          {"aliases", aliases},
          {"names_0", names0},
          {"names_1", names1},
          {"disassembly", disasm},
      });
  EXPECT_FALSE(IsDiffSizeInfo(v2.data(), v2.size()));

  SizeInfo loaded;
  ParseSizeInfo(v2.data(), v2.size(), &loaded);
  EXPECT_EQ("arm64", loaded.fields["build_config"]["arch"].asString());
  ASSERT_EQ(2u, loaded.containers.size());
  ASSERT_EQ(4u, loaded.raw_symbols.size());
  EXPECT_EQ("foo::Bar()", loaded.raw_symbols[0].FullName());
  EXPECT_EQ("foo::Baz()", loaded.raw_symbols[1].FullName());
  EXPECT_EQ("kTable", loaded.raw_symbols[2].FullName());
  EXPECT_EQ("Overhead: gap", loaded.raw_symbols[3].FullName());
  EXPECT_EQ(0x20 + 4, loaded.raw_symbols[0].Size());
  EXPECT_EQ(12, loaded.raw_symbols[3].Size());
  EXPECT_EQ(2, loaded.raw_symbols[0].NumAliases());
  EXPECT_EQ("Blink", std::string_view(loaded.raw_symbols[0].Component()));
  EXPECT_EQ("Internals", std::string_view(loaded.raw_symbols[3].Component()));
  ASSERT_NE(nullptr, loaded.raw_symbols[0].Disassembly());
  EXPECT_EQ("ret", *loaded.raw_symbols[0].Disassembly());

  // Also test empty v2 .size and .sizediff wrapping v2 streams.
  std::string empty_v2 = PackV2Blobs(
      0u,
      {{"meta",
        R"({"containers":[{"name":""}],"segments":[],"total_symbols":0})"}});
  std::string diff_json = R"({"version":1,"before_length":)" +
                          base::NumberToString(empty_v2.size()) + "}";
  std::string sizediff = "# Created by //tools/binary_size\nDIFF\n" +
                         base::NumberToString(diff_json.size()) + "\n" +
                         diff_json + "\n" + empty_v2 + v2;
  ASSERT_TRUE(IsDiffSizeInfo(sizediff.data(), sizediff.size()));
  SizeInfo diff_before, diff_after;
  std::vector<std::string> rem, add;
  ParseDiffSizeInfo(sizediff.data(), sizediff.size(), &diff_before, &diff_after,
                    &rem, &add);
  EXPECT_EQ(0u, diff_before.raw_symbols.size());
  EXPECT_EQ("(Default container)",
            diff_before.fields["containers"][0]["name"].asString());
  EXPECT_EQ(4u, diff_after.raw_symbols.size());
}

}  // namespace
}  // namespace caspian
