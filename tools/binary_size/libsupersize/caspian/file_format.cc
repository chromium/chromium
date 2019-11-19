// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* C++ implementation of a .size file parser.
 * The .size file spec is found in libsupersize/file_format.py
 */

#include "tools/binary_size/libsupersize/caspian/file_format.h"

#include <assert.h>
#include <stdint.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "third_party/jsoncpp/source/include/json/json.h"
#include "third_party/zlib/google/compression_utils_portable.h"
#include "tools/binary_size/libsupersize/caspian/model.h"

namespace {

const char SERIALIZATION_VERSION[] = "Size File Format v1";

int ReadLoneInt(char** rest) {
  char* token = strsep(rest, "\n");
  return std::strtol(token, nullptr, 10);
}

void Decompress(const char* gzipped,
                unsigned long len,
                std::vector<char>* uncompressed) {
  // gzip format stores the uncompressed size in the last four bytes.
  if (len < sizeof(uint32_t)) {
    std::cerr << "Input too short to be gzipped" << std::endl;
    exit(1);
  }
  uint32_t uncompressed_size = *reinterpret_cast<const uint32_t*>(
      &gzipped[len - sizeof(uncompressed_size)]);

  // Should be little-endian.
  int num = 1;
  if (*reinterpret_cast<char*>(&num) != 1) {
    uncompressed_size = __builtin_bswap32(uncompressed_size);
  }

  // Using resize() instead of reserve() here is significantly slower when
  // compiled to WebAssembly.
  uncompressed->reserve(uncompressed_size + 1);
  // Add terminating null for safety.
  (*uncompressed)[uncompressed_size] = '\0';

  unsigned long long_uncompressed_size(uncompressed_size);
  auto ret = zlib_internal::GzipUncompressHelper(
      reinterpret_cast<Bytef*>(&(*uncompressed)[0]), &long_uncompressed_size,
      reinterpret_cast<const Bytef*>(gzipped), len);
  if (Z_OK == ret) {
    return;
  }
  std::cerr << "Failed to decompress. Zlib code: " << ret << std::endl;
  exit(1);
}

std::vector<const char*> ReadValuesFromLine(char** rest,
                                            const char* delimiter) {
  char* rest_of_line = strsep(rest, "\n");

  std::vector<const char*> ret;
  while (true) {
    char* token = strsep(&rest_of_line, delimiter);
    if (!token)
      break;
    ret.push_back(token);
  }
  return ret;
}

template <typename T>
std::vector<T> ReadIntList(char** rest,
                           const char* delim,
                           int n,
                           bool stored_as_delta) {
  char* rest_of_line = strsep(rest, "\n");

  std::vector<T> result;
  result.resize(n);
  for (int i = 0; i < n; i++) {
    char* token = strsep(&rest_of_line, delim);
    result[i] = std::strtol(token, nullptr, 10);
  }

  if (stored_as_delta)
    std::partial_sum(result.begin(), result.end(), result.begin());
  return result;
}

template <typename T>
std::vector<std::vector<T>> ReadIntListForEachSection(
    char** rest,
    const std::vector<int>& section_counts,
    bool stored_as_delta) {
  std::vector<std::vector<T>> ret;
  ret.reserve(section_counts.size());
  for (int nsymbols : section_counts) {
    ret.push_back(ReadIntList<T>(rest, " ", nsymbols, stored_as_delta));
  }
  return ret;
}

void ReadJsonBlob(char** rest, Json::Value* metadata) {
  // Metadata begins with its length in bytes, followed by a json blob.
  int metadata_len = ReadLoneInt(rest);
  if (metadata_len < 0) {
    std::cerr << "Unexpected negative metadata length: " << metadata_len
              << std::endl;
    exit(1);
  }
  char* json_start = *rest;
  *rest += metadata_len + 1;

  std::unique_ptr<Json::CharReader> reader;
  reader.reset(Json::CharReaderBuilder().newCharReader());
  std::string json_errors;
  if (!reader->parse(json_start, json_start + metadata_len, metadata,
                     &json_errors)) {
    std::cerr << "Failed to parse JSON metadata:" << *rest << std::endl;
    std::cerr << json_errors << std::endl;
    exit(1);
  }
}

void CheckNoNonEmptyLinesRemain(char* rest) {
  if (rest) {
    int lines_remaining = 50;
    bool newlines_only = true;
    char* line = nullptr;
    while (lines_remaining > 0 && (line = strsep(&rest, "\n"))) {
      if (strcmp("", line)) {
        std::cerr << "Unparsed line: " << line << std::endl;
        newlines_only = false;
        lines_remaining--;
      }
    }
    if (!newlines_only) {
      exit(1);
    }
  }
}
}  // namespace

namespace caspian {

void CalculatePadding(std::vector<Symbol>* raw_symbols) {
  std::set<const char*> seen_sections;
  for (size_t i = 1; i < raw_symbols->size(); i++) {
    const Symbol& prev_symbol = (*raw_symbols)[i - 1];
    Symbol& symbol = (*raw_symbols)[i];

    if (symbol.IsOverhead()) {
      symbol.padding_ = symbol.size_;
    }
    if (prev_symbol.SectionName() != symbol.SectionName()) {
      if (seen_sections.count(symbol.section_name_)) {
        std::cerr << "Input symbols must be sorted by section, then address: "
                  << prev_symbol << ", " << symbol << std::endl;
        exit(1);
      }
      seen_sections.insert(symbol.SectionName());
      continue;
    }

    if (symbol.Address() <= 0 || prev_symbol.Address() <= 0 ||
        !symbol.IsNative() || !prev_symbol.IsNative()) {
      continue;
    }

    if (symbol.Address() == prev_symbol.Address()) {
      if (symbol.aliases_ && symbol.aliases_ == prev_symbol.aliases_) {
        symbol.padding_ = prev_symbol.padding_;
        symbol.size_ = prev_symbol.size_;
        continue;
      }
      if (prev_symbol.SizeWithoutPadding() != 0) {
        // Padding-only symbols happen for ** symbol gaps.
        std::cerr << "Found duplicate symbols: " << prev_symbol << ", "
                  << symbol << std::endl;
        exit(1);
      }
    }

    int32_t padding = symbol.Address() - prev_symbol.EndAddress();
    symbol.padding_ = padding;
    symbol.size_ += padding;
    if (symbol.size_ < 0) {
      std::cerr << "Symbol has negative size (likely not sorted properly):"
                << symbol << std::endl;
      std::cerr << "prev symbol: " << prev_symbol << std::endl;
      exit(1);
    }
  }
}

void ParseSizeInfo(const char* gzipped,
                   unsigned long len,
                   ::caspian::SizeInfo* info) {
  // To avoid memory allocations, all the char* in our final Symbol set will
  // be pointers into the region originally pointed to by |decompressed_start|.
  // Calls to strsep() replace delimiter characters with null terminators.
  Decompress(gzipped, len, &info->raw_decompressed);
  char* rest = &info->raw_decompressed[0];

  // Ignore generated header
  char* line = strsep(&rest, "\n");

  // Serialization version
  line = strsep(&rest, "\n");
  if (std::strcmp(line, SERIALIZATION_VERSION)) {
    std::cerr << "Serialization version: '" << line << "' not recognized."
              << std::endl;
    exit(1);
  }

  ReadJsonBlob(&rest, &info->metadata);

  const bool has_components = info->metadata["has_components"].asBool();

  // List of paths: (object_path, [source_path])
  int n_paths = ReadLoneInt(&rest);
  if (n_paths < 0) {
    std::cerr << "Unexpected negative path list length: " << n_paths
              << std::endl;
    exit(1);
  }
  std::cout << "Reading " << n_paths << " paths" << std::endl;

  info->object_paths.reserve(n_paths);
  info->source_paths.reserve(n_paths);
  for (int i = 0; i < n_paths; i++) {
    char* line = strsep(&rest, "\n");
    char* first = strsep(&line, "\t");
    char* second = strsep(&line, "\t");
    if (second) {
      info->object_paths.push_back(first);
      info->source_paths.push_back(second);
    } else if (first) {
      info->object_paths.push_back(first);
      info->source_paths.push_back("");
    } else if (line) {
      std::cerr << "Too many tokens on path row: " << i << std::endl;
      exit(1);
    } else {
      info->object_paths.push_back("");
      info->source_paths.push_back("");
    }
  }

  // List of component names
  int n_components = ReadLoneInt(&rest);
  if (n_components <= 0) {
    std::cerr << "Unexpected non-positive components list length: "
              << n_components << std::endl;
    exit(1);
  }
  std::cout << "Reading " << n_components << " components" << std::endl;

  info->components.reserve(n_components);
  for (int i = 0; i < n_components; i++) {
    info->components.push_back(strsep(&rest, "\n"));
  }

  // Section names
  info->section_names = ReadValuesFromLine(&rest, "\t");
  int n_sections = info->section_names.size();

  // Symbol counts for each section
  std::vector<int> section_counts =
      ReadIntList<int>(&rest, "\t", n_sections, false);
  std::cout << "Section counts:" << std::endl;
  int total_symbols =
      std::accumulate(section_counts.begin(), section_counts.end(), 0);

  for (int section_idx = 0; section_idx < n_sections; section_idx++) {
    std::cout << "  " << info->section_names[section_idx] << '\t'
              << section_counts[section_idx] << std::endl;
  }

  std::vector<std::vector<int64_t>> addresses =
      ReadIntListForEachSection<int64_t>(&rest, section_counts, true);
  std::vector<std::vector<int32_t>> sizes =
      ReadIntListForEachSection<int32_t>(&rest, section_counts, false);
  std::vector<std::vector<int32_t>> path_indices =
      ReadIntListForEachSection<int32_t>(&rest, section_counts, true);
  std::vector<std::vector<int32_t>> component_indices;
  if (has_components) {
    component_indices =
        ReadIntListForEachSection<int32_t>(&rest, section_counts, true);
  } else {
    component_indices.resize(addresses.size());
  }

  info->raw_symbols.reserve(total_symbols);
  // Construct raw symbols
  for (int section_idx = 0; section_idx < n_sections; section_idx++) {
    const char* cur_section_name = info->section_names[section_idx];
    caspian::SectionId cur_section_id =
        info->ShortSectionName(cur_section_name);
    const int cur_section_count = section_counts[section_idx];
    const std::vector<int64_t>& cur_addresses = addresses[section_idx];
    const std::vector<int32_t>& cur_sizes = sizes[section_idx];
    const std::vector<int32_t>& cur_path_indices = path_indices[section_idx];
    const std::vector<int32_t>& cur_component_indices =
        component_indices[section_idx];
    int32_t alias_counter = 0;

    for (int i = 0; i < cur_section_count; i++) {
      info->raw_symbols.emplace_back();
      caspian::Symbol& new_sym = info->raw_symbols.back();

      int32_t flags = 0;
      int32_t num_aliases = 0;
      char* line = strsep(&rest, "\n");
      if (*line) {
        new_sym.full_name_ = strsep(&line, "\t");
        char* first = nullptr;
        char* second = nullptr;
        if (line) {
          first = strsep(&line, "\t");
        }
        if (line) {
          second = strsep(&line, "\t");
        }
        if (second) {
          num_aliases = std::strtol(first, nullptr, 16);
          flags = std::strtol(second, nullptr, 16);
        } else if (first) {
          if (first[0] == '0') {
            // full_name  aliases_part
            num_aliases = std::strtol(first, nullptr, 16);
          } else {
            // full_name  flags_part
            flags = std::strtol(first, nullptr, 16);
          }
        }
      }
      new_sym.section_id_ = cur_section_id;
      new_sym.address_ = cur_addresses[i];
      new_sym.size_ = cur_sizes[i];
      new_sym.section_name_ = cur_section_name;
      new_sym.object_path_ = info->object_paths[cur_path_indices[i]];
      new_sym.source_path_ = info->source_paths[cur_path_indices[i]];
      if (has_components) {
        new_sym.component_ = info->components[cur_component_indices[i]];
      }
      new_sym.flags_ = flags;
      new_sym.size_info_ = info;

      // When we encounter a symbol with an alias count, the next N symbols we
      // encounter should be placed in the same symbol group.
      if (num_aliases) {
        assert(alias_counter == 0);
        info->alias_groups.emplace_back();
        alias_counter = num_aliases;
      }
      if (alias_counter > 0) {
        new_sym.aliases_ = &info->alias_groups.back();
        new_sym.aliases_->push_back(&new_sym);
        alias_counter--;
      }
    }
  }

  CalculatePadding(&info->raw_symbols);

  // If there are unparsed non-empty lines, something's gone wrong.
  CheckNoNonEmptyLinesRemain(rest);

  std::cout << "Parsed " << info->raw_symbols.size() << " symbols" << std::endl;
}

}  // namespace caspian
