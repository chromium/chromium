// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Command-line interface for checking the integrity of .size files.
// Intended to be called from WebAssembly code.

#include <stdint.h>
#include <stdlib.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "third_party/jsoncpp/source/include/json/json.h"
#include "third_party/re2/src/re2/re2.h"
#include "tools/binary_size/libsupersize/caspian/diff.h"
#include "tools/binary_size/libsupersize/caspian/file_format.h"
#include "tools/binary_size/libsupersize/caspian/model.h"
#include "tools/binary_size/libsupersize/caspian/tree_builder.h"

namespace caspian {
namespace {
std::unique_ptr<SizeInfo> info;
std::unique_ptr<SizeInfo> before_info;
std::unique_ptr<DeltaSizeInfo> diff_info;
std::unique_ptr<TreeBuilder> builder;

std::unique_ptr<Json::StreamWriter> writer;

std::string JsonSerialize(const Json::Value& value) {
  if (!writer) {
    writer.reset(Json::StreamWriterBuilder().newStreamWriter());
  }
  std::stringstream s;
  writer->write(value, &s);
  return s.str();
}
}  // namespace

extern "C" {
void LoadSizeFile(const char* compressed, size_t size) {
  diff_info.reset(nullptr);
  info = std::make_unique<SizeInfo>();
  ParseSizeInfo(compressed, size, info.get());
}

void LoadBeforeSizeFile(const char* compressed, size_t size) {
  diff_info.reset(nullptr);
  before_info = std::make_unique<SizeInfo>();
  ParseSizeInfo(compressed, size, before_info.get());
}

void BuildTree(bool group_by_component,
               bool method_count_mode,
               const char* include_regex_str,
               const char* exclude_regex_str,
               const char* include_sections,
               int minimum_size_bytes,
               int match_flag) {
  std::vector<std::function<bool(const BaseSymbol&)>> filters;

  const bool diff_mode = info && before_info;

  if (minimum_size_bytes > 0) {
    if (!diff_mode) {
      filters.push_back([minimum_size_bytes](const BaseSymbol& sym) -> bool {
        return sym.Pss() >= minimum_size_bytes;
      });
    } else {
      filters.push_back([minimum_size_bytes](const BaseSymbol& sym) -> bool {
        return abs(sym.Pss()) >= minimum_size_bytes;
      });
    }
  }

  // It's currently not useful to filter on more than one flag, so
  // |match_flag| can be assumed to be a power of two.
  if (match_flag) {
    std::cout << "Filtering on flag matching " << match_flag << std::endl;
    filters.push_back([match_flag](const BaseSymbol& sym) -> bool {
      return match_flag & sym.Flags();
    });
  }

  std::array<bool, 256> include_sections_map{};
  if (include_sections) {
    std::cout << "Filtering on sections " << include_sections << std::endl;
    for (const char* c = include_sections; *c; c++) {
      include_sections_map[static_cast<uint8_t>(*c)] = true;
    }
    filters.push_back([&include_sections_map](const BaseSymbol& sym) -> bool {
      return include_sections_map[static_cast<uint8_t>(sym.Section())];
    });
  }

  std::unique_ptr<RE2> include_regex;
  if (include_regex_str && *include_regex_str) {
    include_regex.reset(new RE2(include_regex_str));
    if (include_regex->error_code() == RE2::NoError) {
      filters.push_back([&include_regex](const BaseSymbol& sym) -> bool {
        re2::StringPiece piece(sym.FullName().data(), sym.FullName().size());
        return RE2::PartialMatch(piece, *include_regex);
      });
    }
  }

  std::unique_ptr<RE2> exclude_regex;
  if (exclude_regex_str && *exclude_regex_str) {
    exclude_regex.reset(new RE2(exclude_regex_str));
    if (exclude_regex->error_code() == RE2::NoError) {
      filters.push_back([&exclude_regex](const BaseSymbol& sym) -> bool {
        re2::StringPiece piece(sym.FullName().data(), sym.FullName().size());
        return !RE2::PartialMatch(piece, *exclude_regex);
      });
    }
  }

  // BuildTree() is called every time a new filter is applied in the HTML
  // viewer, but if we already have a DeltaSizeInfo we can skip regenerating it
  // and let the TreeBuilder filter the symbols we care about.
  if (diff_mode && !diff_info) {
    diff_info.reset(new DeltaSizeInfo(Diff(before_info.get(), info.get())));
  }

  if (diff_mode) {
    builder.reset(new TreeBuilder(diff_info.get()));
  } else {
    builder.reset(new TreeBuilder(info.get()));
  }
  builder->Build(group_by_component, method_count_mode, filters);
}

const char* Open(const char* path) {
  // Returns a string that can be parsed to a JS object.
  static std::string result;
  Json::Value v = builder->Open(path);
  result = JsonSerialize(v);
  return result.c_str();
}
}  // extern "C"
}  // namespace caspian
