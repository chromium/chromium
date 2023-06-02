// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* C++ implementation of a .size file parser.
 * The .size file spec is found in libsupersize/file_format.py
 */

#include "tools/binary_size/libsupersize/viewer/caspian/file_format.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

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
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

namespace {
const char kDiffHeader[] = "# Created by //tools/binary_size\nDIFF\n";
const char kSerializationVersionSingleContainer[] = "Size File Format v1";
const char kSerializationVersionMultiContainer[] = "Size File Format v1.1";

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

  uncompressed->resize(uncompressed_size + 1);
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
  std::vector<const char*> ret;
  char* rest_of_line = strsep(rest, "\n");
  // Check for empty line (otherwise "" is added).
  if (!*rest_of_line)
    return ret;
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
    const std::vector<int>& symbol_counts,
    bool stored_as_delta) {
  std::vector<std::vector<T>> ret;
  ret.reserve(symbol_counts.size());
  for (int nsymbols : symbol_counts) {
    ret.push_back(ReadIntList<T>(rest, " ", nsymbols, stored_as_delta));
  }
  return ret;
}

void ReadJsonBlob(char** rest, Json::Value* fields) {
  // Metadata begins with its length in bytes, followed by a json blob.
  int fields_len = ReadLoneInt(rest);
  if (fields_len < 0) {
    std::cerr << "Unexpected negative fields length: " << fields_len
              << std::endl;
    exit(1);
  }
  char* json_start = *rest;
  *rest += fields_len + 1;

  std::unique_ptr<Json::CharReader> reader;
  reader.reset(Json::CharReaderBuilder().newCharReader());
  std::string json_errors;
  if (!reader->parse(json_start, json_start + fields_len, fields,
                     &json_errors)) {
    std::cerr << "Failed to parse JSON fields:" << *rest << std::endl;
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

void ParseSizeInfo(const char* gzipped, unsigned long len, SizeInfo* info) {
  // To avoid memory allocations, all the char* in our final Symbol set will
  // be pointers into the region originally pointed to by |decompressed_start|.
  // Calls to strsep() replace delimiter characters with null terminators.
  Decompress(gzipped, len, &info->raw_decompressed);
  char* rest = &info->raw_decompressed[0];

  // Ignore generated header.
  char* line = strsep(&rest, "\n");

  // Serialization version.
  line = strsep(&rest, "\n");
  bool has_multi_containers = false;
  if (!std::strcmp(line, kSerializationVersionSingleContainer)) {
    has_multi_containers = false;
  } else if (!std::strcmp(line, kSerializationVersionMultiContainer)) {
    has_multi_containers = true;
  } else {
    std::cerr << "Serialization version: '" << line << "' not recognized."
              << std::endl;
    exit(1);
  }

  ReadJsonBlob(&rest, &info->fields);
  if (has_multi_containers) {
    const Json::Value& container_values = info->fields["containers"];
    for (const auto& container_value : container_values) {
      const std::string name = container_value["name"].asString();
      info->containers.push_back(Container(name));
    }
  } else {
    info->containers.push_back(Container(""));
  }

  const bool has_components = info->fields["has_components"].asBool();
  const bool has_padding = info->fields["has_padding"].asBool();
  const bool has_disassembly = info->fields["has_disassembly"].asBool();

  // List of paths: (object_path, [source_path]).
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
    line = strsep(&rest, "\n");
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

  if (has_components) {
    // List of component names.
    int n_components = ReadLoneInt(&rest);
    if (n_components < 0) {
      std::cerr << "Unexpected negative components list length: "
                << n_components << std::endl;
      exit(1);
    }
    std::cout << "Reading " << n_components << " components" << std::endl;

    info->components.reserve(n_components);
    for (int i = 0; i < n_components; i++) {
      info->components.push_back(strsep(&rest, "\n"));
    }
  }

  // Segments = List of (Container, section name).
  std::vector<const char*> segment_names;
  segment_names = ReadValuesFromLine(&rest, "\t");
  int n_segments = segment_names.size();

  // Parse segment name into Container pointers and section names.
  std::vector<Container*> segment_containers(n_segments);
  std::vector<const char*> segment_section_names(n_segments);
  for (int segment_idx = 0; segment_idx < n_segments; segment_idx++) {
    const char* segment_name = segment_names[segment_idx];
    if (has_multi_containers) {
      // |segment_name| is formatted as "<container_idx>section_name".
      std::string t = segment_name;
      assert(t.length() > 0 && t[0] == '<');
      size_t sep_pos = t.find('>');
      assert(sep_pos != std::string::npos);
      std::string container_idx_str = t.substr(1, sep_pos - 1);
      int container_idx = std::atoi(container_idx_str.c_str());
      assert(container_idx >= 0 &&
             container_idx < static_cast<int>(info->containers.size()));
      segment_containers[segment_idx] = &info->containers[container_idx];
      segment_section_names[segment_idx] = segment_name + (sep_pos + 1);
    } else {
      // Segments are already container names.
      segment_containers[segment_idx] = &info->containers[0];
      segment_section_names[segment_idx] = segment_name;
    }
  }

  // Symbol counts for each section.
  std::vector<int> symbol_counts =
      ReadIntList<int>(&rest, "\t", n_segments, false);
  std::cout << "Symbol counts:" << std::endl;
  int total_symbols =
      std::accumulate(symbol_counts.begin(), symbol_counts.end(), 0);

  for (int segment_idx = 0; segment_idx < n_segments; segment_idx++) {
    std::cout << "  ";
    if (has_multi_containers) {
      std::cout << "<" << segment_containers[segment_idx]->name << ">";
    }
    std::cout << segment_section_names[segment_idx];
    std::cout << '\t' << symbol_counts[segment_idx] << std::endl;
  }

  std::vector<std::vector<int64_t>> addresses =
      ReadIntListForEachSection<int64_t>(&rest, symbol_counts, true);
  std::vector<std::vector<int32_t>> sizes =
      ReadIntListForEachSection<int32_t>(&rest, symbol_counts, false);
  std::vector<std::vector<int32_t>> paddings;
  if (has_padding) {
    paddings = ReadIntListForEachSection<int32_t>(&rest, symbol_counts, false);
  } else {
    paddings.resize(addresses.size());
  }
  std::vector<std::vector<int32_t>> path_indices =
      ReadIntListForEachSection<int32_t>(&rest, symbol_counts, true);
  std::vector<std::vector<int32_t>> component_indices;
  if (has_components) {
    component_indices =
        ReadIntListForEachSection<int32_t>(&rest, symbol_counts, true);
  } else {
    component_indices.resize(addresses.size());
  }

  info->raw_symbols.reserve(total_symbols);
  // Construct raw symbols.
  for (int segment_idx = 0; segment_idx < n_segments; segment_idx++) {
    const Container* cur_container = segment_containers[segment_idx];
    const char* cur_section_name = segment_section_names[segment_idx];
    caspian::SectionId cur_section_id =
        info->ShortSectionName(cur_section_name);
    const int cur_section_count = symbol_counts[segment_idx];
    const std::vector<int64_t>& cur_addresses = addresses[segment_idx];
    const std::vector<int32_t>& cur_sizes = sizes[segment_idx];
    const std::vector<int32_t>& cur_paddings = paddings[segment_idx];
    const std::vector<int32_t>& cur_path_indices = path_indices[segment_idx];
    const std::vector<int32_t>& cur_component_indices =
        component_indices[segment_idx];
    int32_t alias_counter = 0;

    for (int i = 0; i < cur_section_count; i++) {
      info->raw_symbols.emplace_back();
      caspian::Symbol& new_sym = info->raw_symbols.back();

      int32_t flags = 0;
      int32_t num_aliases = 0;
      line = strsep(&rest, "\n");
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
      if (has_padding) {
        new_sym.padding_ = cur_paddings[i];
        if (!new_sym.IsOverhead()) {
          new_sym.size_ += new_sym.padding_;
        }
      }
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
      new_sym.container_ = cur_container;
    }
  }

  info->is_sparse = has_padding;
  if (!has_padding) {
    CalculatePadding(&info->raw_symbols);
  }

  if (has_disassembly) {
    std::vector<const char*> disassemby_list = ReadValuesFromLine(&rest, " ");
    for (const char* disassembly_idx : disassemby_list) {
      int num_bytes_disassembly = ReadLoneInt(&rest);
      int index = atoi(disassembly_idx);
      std::string disassembly(rest, rest + num_bytes_disassembly);
      info->owned_strings.push_back(disassembly);
      info->raw_symbols[index].disassembly_ = &(info->owned_strings.back());
      rest += num_bytes_disassembly;
    }
  }

  // If there are unparsed non-empty lines, something's gone wrong.
  CheckNoNonEmptyLinesRemain(rest);

  std::cout << "Parsed " << info->raw_symbols.size() << " symbols" << std::endl;
}

void ParseCompressedStringList(const char* gzipped,
                               unsigned long len,
                               std::vector<std::string>* string_list) {
  std::vector<char> decompressed;
  Decompress(gzipped, len, &decompressed);
  char* rest = &decompressed[0];
  int n_strings = ReadLoneInt(&rest);
  string_list->resize(n_strings);
  for (int i = 0; i < n_strings; i++) {
    (*string_list)[i] = strsep(&rest, "\n");
  }
}

bool IsDiffSizeInfo(const char* file, unsigned long len) {
  return !strncmp(file, kDiffHeader, 4);
}

void ParseDiffSizeInfo(char* file,
                       unsigned long len,
                       SizeInfo* before,
                       SizeInfo* after,
                       std::vector<std::string>* removed_sources,
                       std::vector<std::string>* added_sources) {
  // Skip "DIFF" header.
  char* rest = file;
  rest += strlen(kDiffHeader);
  Json::Value fields;
  ReadJsonBlob(&rest, &fields);

  if (fields["version"].asInt() != 1) {
    std::cerr << ".sizediff version mismatch, write some upgrade code. version="
              << fields["version"] << std::endl;
    exit(1);
  }

  auto get_uint_field = [&](const char* key) -> unsigned long {
    return fields.isMember(key) ? fields[key].asUInt() : 0;
  };

  unsigned long header_len = rest - file;
  unsigned long removed_sources_len = get_uint_field("removed_sources_length");
  unsigned long added_sources_len = get_uint_field("added_sources_length");
  unsigned long before_len = get_uint_field("before_length");
  unsigned long after_len =
      len - header_len - before_len - removed_sources_len - added_sources_len;

  if (removed_sources_len) {
    ParseCompressedStringList(rest, removed_sources_len, removed_sources);
    rest += removed_sources_len;
  }
  if (added_sources_len) {
    ParseCompressedStringList(rest, added_sources_len, added_sources);
    rest += added_sources_len;
  }

  ParseSizeInfo(rest, before_len, before);
  rest += before_len;
  ParseSizeInfo(rest, after_len, after);
}

}  // namespace caspian
