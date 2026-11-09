// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/viewer/caspian/file_format_v2.h"

#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "third_party/jsoncpp/source/include/json/json.h"
#include "third_party/zstd/src/lib/zstd.h"
#include "tools/binary_size/libsupersize/viewer/caspian/file_format.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/threading.h>
#endif

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
              "Caspian .size v2 requires a little-endian target.");

namespace caspian {
namespace {

const char kSizeV2Magic[] =
    "# Created by //tools/binary_size\nSize File Format v2\n\0\0\0";
constexpr size_t kSizeV2MagicLen = 56;
constexpr size_t kSizeV2HeaderLen = 64;
constexpr size_t kSizeV2TocEntryLen = 40;

void Check(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << msg << std::endl;
    exit(1);
  }
}

template <typename T>
void ByteStreamSplitDecode(const uint8_t* src, size_t count, T* dst_typed) {
  constexpr size_t kWidth = sizeof(T);
  uint8_t* dst = reinterpret_cast<uint8_t*>(dst_typed);
  for (size_t i = 0; i < count; ++i) {
    for (size_t b = 0; b < kWidth; ++b) {
      dst[i * kWidth + b] = src[b * count + i];
    }
  }
}

struct Reader {
  const uint8_t* cur = nullptr;
  const uint8_t* end = nullptr;

  Reader(const uint8_t* data, size_t len) : cur(data), end(data + len) {}
  explicit Reader(const std::vector<char>& buf)
      : cur(reinterpret_cast<const uint8_t*>(buf.data())),
        end(cur + buf.size()) {}

  const uint8_t* ReadBytes(size_t n) {
    Check(static_cast<size_t>(end - cur) >= n, "Truncated v2 blob");
    const uint8_t* p = cur;
    cur += n;
    return p;
  }

  uint32_t ReadU32() {
    uint32_t v = 0;
    std::memcpy(&v, ReadBytes(4), 4);
    return v;
  }

  uint64_t ReadU64() {
    uint64_t v = 0;
    std::memcpy(&v, ReadBytes(8), 8);
    return v;
  }

  const char* ReadStr(size_t* len_out = nullptr) {
    const void* nul = std::memchr(cur, '\0', static_cast<size_t>(end - cur));
    Check(nul != nullptr, "Truncated null-terminated string in v2 blob");
    const char* s = reinterpret_cast<const char*>(cur);
    const uint8_t* nul_p = static_cast<const uint8_t*>(nul);
    if (len_out) {
      *len_out = static_cast<size_t>(nul_p - cur);
    }
    cur = nul_p + 1;
    return s;
  }

  template <typename T>
  void ReadSplit(size_t count, std::vector<T>* out) {
    out->resize(count);
    ByteStreamSplitDecode(ReadBytes(count * sizeof(T)), count, out->data());
  }
};

template <typename Func>
void ParallelFor(size_t num_tasks, Func&& fn) {
  if (num_tasks == 0) {
    return;
  }
  size_t default_threads = 1;
#ifdef __EMSCRIPTEN__
  if (emscripten_has_threading_support()) {
    default_threads = 4;
  }
#else
  default_threads =
      std::min(16u, std::max(1u, std::thread::hardware_concurrency()));
#endif
  size_t num_workers = std::min<size_t>(num_tasks, default_threads);
  if (num_workers <= 1) {
    for (size_t i = 0; i < num_tasks; ++i) {
      fn(i);
    }
    return;
  }

  std::atomic<size_t> next_idx{0};
  auto worker = [&]() {
    while (true) {
      size_t idx = next_idx.fetch_add(1, std::memory_order_relaxed);
      if (idx >= num_tasks) {
        break;
      }
      fn(idx);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(num_workers - 1);
  for (size_t t = 1; t < num_workers; ++t) {
    threads.emplace_back(worker);
  }
  worker();
  for (auto& th : threads) {
    th.join();
  }
}

}  // namespace

bool IsSizeV2(const char* data, unsigned long len) {
  return len >= kSizeV2HeaderLen &&
         std::memcmp(data, kSizeV2Magic, kSizeV2MagicLen) == 0;
}

void ParseSizeInfoV2(const uint8_t* data, unsigned long len, SizeInfo* info) {
  Reader hdr(data, len);
  hdr.ReadBytes(kSizeV2MagicLen);
  uint32_t header_flags = hdr.ReadU32();
  uint32_t num_blobs = hdr.ReadU32();
  bool has_padding = (header_flags & 1u) != 0;
  Check(static_cast<uint64_t>(num_blobs) * kSizeV2TocEntryLen <=
            len - kSizeV2HeaderLen,
        "Truncated v2 TOC");

  struct TocEntry {
    std::string_view name;
    uint64_t offset;
    uint64_t comp_size;
    uint64_t raw_size;
  };
  std::vector<TocEntry> toc(num_blobs);
  for (uint32_t i = 0; i < num_blobs; ++i) {
    const char* name_ptr = reinterpret_cast<const char*>(hdr.ReadBytes(16));
    toc[i].name = std::string_view(name_ptr, strnlen(name_ptr, 16));
    toc[i].offset = hdr.ReadU64();
    toc[i].comp_size = hdr.ReadU64();
    toc[i].raw_size = hdr.ReadU64();
    Check(toc[i].offset <= len && toc[i].comp_size <= len - toc[i].offset &&
              toc[i].raw_size <= SIZE_MAX,
          "Invalid v2 blob bounds");
  }

  std::vector<std::vector<char>> raw_blobs(num_blobs);
  ParallelFor(num_blobs, [&](size_t i) {
    size_t raw_sz = static_cast<size_t>(toc[i].raw_size);
    raw_blobs[i].resize(raw_sz);
    if (raw_sz > 0) {
      size_t res = ZSTD_decompress(raw_blobs[i].data(), raw_sz,
                                   data + toc[i].offset, toc[i].comp_size);
      Check(!ZSTD_isError(res) && res == raw_sz, "ZSTD_decompress failed");
    }
  });

  auto find_blob = [&](std::string_view name) -> std::vector<char>* {
    for (size_t i = 0; i < num_blobs; ++i) {
      if (toc[i].name == name) {
        return &raw_blobs[i];
      }
    }
    return nullptr;
  };
  auto require_blob = [&](std::string_view name) -> std::vector<char>& {
    auto* b = find_blob(name);
    Check(b != nullptr, "Missing required v2 blob");
    return *b;
  };

  Json::Value meta_json;
  {
    const auto& meta_buf = require_blob("meta");
    std::unique_ptr<Json::CharReader> reader(
        Json::CharReaderBuilder().newCharReader());
    std::string errs;
    Check(reader->parse(meta_buf.data(), meta_buf.data() + meta_buf.size(),
                        &meta_json, &errs),
          "Failed to parse v2 meta JSON");
    if (meta_json.isMember("build_config")) {
      info->fields["build_config"] = meta_json["build_config"];
    }
    if (meta_json.isMember("containers")) {
      info->fields["containers"] = meta_json["containers"];
      for (const auto& c : meta_json["containers"]) {
        info->containers.emplace_back(c["name"].asString());
      }
    }
    if (info->containers.empty()) {
      info->containers.emplace_back("");
    }
    if (info->fields["containers"].size() == 1 &&
        info->fields["containers"][0]["name"].asString().empty()) {
      info->fields["containers"][0]["name"] = "(Default container)";
    }
    info->fields["has_padding"] = has_padding;
    info->fields["has_components"] = true;
  }
  uint32_t total_symbols = meta_json["total_symbols"].asUInt();

  std::vector<uint32_t> path_components;
  if (auto* paths_blob = find_blob("paths")) {
    info->raw_decompressed = std::move(*paths_blob);
    Reader pr(info->raw_decompressed);
    uint32_t num_comps = pr.ReadU32();
    uint32_t num_paths = pr.ReadU32();
    pr.ReadSplit(num_paths, &path_components);
    info->components.resize(num_comps);
    for (uint32_t i = 0; i < num_comps; ++i) {
      info->components[i] = pr.ReadStr();
    }
    info->object_paths.resize(num_paths);
    info->source_paths.resize(num_paths);
    for (uint32_t i = 0; i < num_paths; ++i) {
      Check(path_components[i] < num_comps, "Invalid component index");
      info->object_paths[i] = pr.ReadStr();
      info->source_paths[i] = pr.ReadStr();
    }
  }

  if (total_symbols > 0) {
    {
      auto read_col = [&](const char* name, auto* vec) {
        auto& b = require_blob(name);
        Reader(b).ReadSplit(total_symbols, vec);
        std::vector<char>().swap(b);
      };
      std::vector<uint32_t> addresses, path_indices, flags;
      std::vector<int32_t> sizes, paddings;
      read_col("addresses", &addresses);
      read_col("sizes", &sizes);
      if (has_padding) {
        read_col("paddings", &paddings);
      }
      read_col("path_indices", &path_indices);
      read_col("flags", &flags);

      info->raw_symbols.resize(total_symbols);
      size_t sym_idx = 0;
      uint32_t prev_addr = 0;
      int32_t prev_path = 0;
      for (const auto& seg : meta_json["segments"]) {
        uint32_t c_idx = seg[0].asUInt();
        uint32_t seg_count = seg[2].asUInt();
        Check(c_idx < info->containers.size() &&
                  sym_idx + seg_count <= total_symbols,
              "Invalid v2 segment");
        const Container* cur_container = &info->containers[c_idx];
        info->owned_strings.push_back(seg[1].asString());
        const char* sec_name = info->owned_strings.back().c_str();
        SectionId sec_id = info->ShortSectionName(sec_name);
        for (uint32_t i = 0; i < seg_count; ++i, ++sym_idx) {
          Symbol& sym = info->raw_symbols[sym_idx];
          prev_addr += addresses[sym_idx];
          sym.section_id_ = sec_id;
          sym.address_ = static_cast<int32_t>(prev_addr);
          sym.size_ = sizes[sym_idx];
          if (has_padding) {
            sym.padding_ = paddings[sym_idx];
          }
          sym.section_name_ = sec_name;
          sym.flags_ = static_cast<int32_t>(flags[sym_idx]);
          sym.size_info_ = info;
          sym.container_ = cur_container;

          uint32_t zp = path_indices[sym_idx];
          prev_path += static_cast<int32_t>((zp >> 1) ^ (0u - (zp & 1u)));
          Check(prev_path >= 0 &&
                    static_cast<size_t>(prev_path) < info->object_paths.size(),
                "Invalid path index in v2 symbols");
          sym.object_path_ = info->object_paths[prev_path];
          sym.source_path_ = info->source_paths[prev_path];
          sym.component_ = info->components[path_components[prev_path]];
        }
      }
      Check(sym_idx == total_symbols, "Segment symbol count mismatch");
    }

    if (auto* alias_buf = find_blob("aliases")) {
      Reader ar(*alias_buf);
      uint32_t num_groups = ar.ReadU32();
      std::vector<uint32_t> first_deltas, alias_counts;
      ar.ReadSplit(num_groups, &first_deltas);
      ar.ReadSplit(num_groups, &alias_counts);
      uint32_t cur_idx = 0;
      for (uint32_t g = 0; g < num_groups; ++g) {
        cur_idx += first_deltas[g];
        uint32_t count = alias_counts[g];
        Check(cur_idx <= total_symbols && count <= total_symbols - cur_idx,
              "Invalid alias group bounds");
        auto& group = info->alias_groups.emplace_back();
        group.reserve(count);
        for (uint32_t j = 0; j < count; ++j) {
          info->raw_symbols[cur_idx + j].aliases_ = &group;
          group.push_back(&info->raw_symbols[cur_idx + j]);
        }
      }
    }

    std::vector<std::vector<char>*> name_shards;
    for (size_t i = 0; i < num_blobs; ++i) {
      if (toc[i].name.starts_with("names_")) {
        name_shards.push_back(&raw_blobs[i]);
      }
    }
    size_t arena_base = info->owned_strings.size();
    info->owned_strings.resize(arena_base + name_shards.size());
    ParallelFor(name_shards.size(), [&](size_t k) {
      Reader nr(*name_shards[k]);
      uint32_t start_idx = nr.ReadU32();
      uint32_t sym_count = nr.ReadU32();
      if (sym_count == 0) {
        return;
      }
      Check(
          start_idx <= total_symbols && sym_count <= total_symbols - start_idx,
          "Invalid names chunk bounds");
      std::vector<uint32_t> perm_deltas;
      nr.ReadSplit(sym_count, &perm_deltas);
      const uint8_t* lcp_ptr = nr.ReadBytes(sym_count);
      size_t total_bytes = static_cast<size_t>(nr.end - nr.cur);
      for (uint32_t i = 0; i < sym_count; ++i) {
        total_bytes += lcp_ptr[i];
      }
      std::string& arena = info->owned_strings[arena_base + k];
      arena.resize_and_overwrite(total_bytes, [&](char* out_ptr, size_t n) {
        const char* prev_ptr = out_ptr;
        size_t prev_len = 0;
        int32_t prev_perm = 0;
        for (uint32_t i = 0; i < sym_count; ++i) {
          uint32_t zp = perm_deltas[i];
          prev_perm += static_cast<int32_t>((zp >> 1) ^ (0u - (zp & 1u)));
          Check(prev_perm >= 0 && static_cast<uint32_t>(prev_perm) < sym_count,
                "Invalid perm_k index");
          size_t suf_len = 0;
          const char* suf = nr.ReadStr(&suf_len);
          size_t lcp = lcp_ptr[i];
          Check(lcp <= prev_len, "Invalid LCP length");
          size_t cur_len = lcp + suf_len;
          std::memcpy(out_ptr, prev_ptr, lcp);
          std::memcpy(out_ptr + lcp, suf, suf_len + 1);
          info->raw_symbols[start_idx + prev_perm].full_name_ =
              std::string_view(out_ptr, cur_len);
          prev_ptr = out_ptr;
          prev_len = cur_len;
          out_ptr += cur_len + 1;
        }
        return n;
      });
      std::vector<char>().swap(*name_shards[k]);
    });

    if (has_padding) {
      for (Symbol& sym : info->raw_symbols) {
        if (!sym.IsOverhead()) {
          sym.size_ += sym.padding_;
        }
      }
    } else {
      CalculatePadding(&info->raw_symbols);
    }
  }

  info->is_sparse = has_padding;

  if (auto* dis_buf = find_blob("disassembly")) {
    info->fields["has_disassembly"] = true;
    Reader dr(*dis_buf);
    uint32_t count = dr.ReadU32();
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t s_idx = dr.ReadU32();
      uint32_t byte_len = dr.ReadU32();
      Check(s_idx < info->raw_symbols.size(), "Invalid disassembly symbol");
      const char* p = reinterpret_cast<const char*>(dr.ReadBytes(byte_len));
      info->owned_strings.emplace_back(p, byte_len);
      info->raw_symbols[s_idx].disassembly_ = &info->owned_strings.back();
    }
  }
}

}  // namespace caspian
