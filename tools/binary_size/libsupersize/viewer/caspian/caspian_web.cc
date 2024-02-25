// Copyright 2019 The Chromium Authors
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
#include <string_view>

#include "third_party/jsoncpp/source/include/json/json.h"
#include "third_party/re2/src/re2/re2.h"
#include "tools/binary_size/libsupersize/viewer/caspian/diff.h"
#include "tools/binary_size/libsupersize/viewer/caspian/file_format.h"
#include "tools/binary_size/libsupersize/viewer/caspian/lens.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"
#include "tools/binary_size/libsupersize/viewer/caspian/tree_builder.h"

namespace caspian {
namespace {

class FilterBuffer {
 public:
  FilterBuffer() = default;
  FilterBuffer(const FilterBuffer&) = delete;
  FilterBuffer& operator=(const FilterBuffer&) = delete;

  size_t remaining() { return kFilterBufferSize - (cursor_ - data_); }
  void Reset() { cursor_ = data_; }
  std::string_view Get() { return std::string_view(data_, cursor_ - data_); }

  void Append(char c) {
    if (remaining() > 0) {
      cursor_[0] = c;
      ++cursor_;
    }
  }

  void Append(std::string_view str) {
    size_t nchars = std::min(remaining(), str.size());
    memcpy(cursor_, str.data(), nchars);
    cursor_ += nchars;
  }

 private:
  static constexpr size_t kFilterBufferSize = 4 * 1024;
  char data_[kFilterBufferSize];
  char* cursor_;
};

FilterBuffer filter_buffer;

std::unique_ptr<SizeInfo> info;
std::unique_ptr<SizeInfo> before_info;
std::vector<std::string> removed_sources;
std::vector<std::string> added_sources;
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

bool ContainsUpper(const char* str) {
  while (*str) {
    if (*str >= 'A' && *str <= 'Z') {
      return true;
    }
    ++str;
  }
  return false;
}

std::unique_ptr<RE2> CreateFilterRegex(const char* pattern) {
  RE2::Options options;
  options.set_case_sensitive(ContainsUpper(pattern));
  return std::make_unique<RE2>(pattern, options);
}

bool MatchesRegex(const GroupedPath& id_path,
                  const BaseSymbol& sym,
                  const RE2& regex) {
  // Write the entire path to a buffer so that regex to match across it.
  filter_buffer.Reset();
  filter_buffer.Append(id_path.group);
  filter_buffer.Append('/');
  filter_buffer.Append(id_path.path);
  filter_buffer.Append(':');
  filter_buffer.Append(sym.FullName());

  // Always match against container even when not grouping by container.
  if (RE2::PartialMatch(filter_buffer.Get(), regex)) {
    return true;
  }

  return RE2::PartialMatch(sym.ContainerName(), regex);
}

bool IsMultiContainer() {
  // If DeltaSizeInfo is active, still take |info| since it's the "after" info.
  return info->containers.size() > 1 || !info->containers[0].name.empty();
}

void ClearInfoAndBuilderObjects() {
  builder.reset(nullptr);
  diff_info.reset(nullptr);
  before_info.reset(nullptr);
  info.reset(nullptr);
  removed_sources = {};
  added_sources = {};
}

}  // namespace

extern "C" {
void LoadSizeFile(char* compressed, size_t size) {
  ClearInfoAndBuilderObjects();
  if (IsDiffSizeInfo(compressed, size)) {
    info = std::make_unique<SizeInfo>();
    before_info = std::make_unique<SizeInfo>();
    ParseDiffSizeInfo(compressed, size, before_info.get(), info.get(),
                      &removed_sources, &added_sources);
    // DeltaSizeInfo instantiation for sparse diff.
    diff_info.reset(new DeltaSizeInfo(
        Diff(before_info.get(), info.get(), &removed_sources, &added_sources)));
  } else {
    info = std::make_unique<SizeInfo>();
    ParseSizeInfo(compressed, size, info.get());
  }
}

void LoadBeforeSizeFile(const char* compressed, size_t size) {
  // Don't call ClearInfoAndBuilderObjects(): It's assumed that LoadSizeFile()
  // was called immediately before.
  before_info = std::make_unique<SizeInfo>();
  ParseSizeInfo(compressed, size, before_info.get());
}

// Updates |builder| with provided filters and constructs the new tree.
// Typically called when the front-end form updates, to apply any new filters.
// Returns: True if the resulting tree is a diff, false if it is a snapshot.
bool BuildTree(bool method_count_mode,
               const char* group_by,
               const char* include_regex_str,
               const char* exclude_regex_str,
               const char* include_sections,
               int minimum_size_bytes,
               int match_flag,
               bool non_overhead,
               bool disassembly_mode) {
  std::vector<TreeBuilder::FilterFunc> filters;

  const bool diff_mode = info && before_info;

  if (method_count_mode && diff_mode) {
    // include_sections is used to filter to just .dex.method symbols.
    // For diffs, we also want to filter to just adds & removes.
    filters.push_back([](const GroupedPath&, const BaseSymbol& sym) -> bool {
      DiffStatus diff_status = sym.GetDiffStatus();
      return diff_status == DiffStatus::kAdded ||
             diff_status == DiffStatus::kRemoved;
    });
  }

  if (minimum_size_bytes > 0) {
    if (!diff_mode) {
      filters.push_back([minimum_size_bytes](const GroupedPath&,
                                             const BaseSymbol& sym) -> bool {
        return sym.Pss() >= minimum_size_bytes;
      });
    } else {
      filters.push_back([minimum_size_bytes](const GroupedPath&,
                                             const BaseSymbol& sym) -> bool {
        return abs(sym.Pss()) >= minimum_size_bytes;
      });
    }
  }

  // It's currently not useful to filter on more than one flag, so
  // |match_flag| can be assumed to be a power of two.
  if (match_flag) {
    std::cout << "Filtering on flag matching " << match_flag << std::endl;
    filters.push_back(
        [match_flag](const GroupedPath&, const BaseSymbol& sym) -> bool {
          return match_flag & sym.Flags();
        });
  }

  if (non_overhead) {
    filters.push_back([](const GroupedPath&, const BaseSymbol& sym) -> bool {
      return !sym.IsOverhead();
    });
  }

  if (disassembly_mode) {
    filters.push_back([](const GroupedPath&, const BaseSymbol& sym) -> bool {
      return sym.Disassembly() != nullptr;
    });
  }

  std::array<bool, 256> include_sections_map{};
  if (include_sections) {
    std::cout << "Filtering on sections " << include_sections << std::endl;
    for (const char* c = include_sections; *c; c++) {
      include_sections_map[static_cast<uint8_t>(*c)] = true;
    }
    filters.push_back([&include_sections_map](const GroupedPath&,
                                              const BaseSymbol& sym) -> bool {
      return include_sections_map[static_cast<uint8_t>(sym.Section())];
    });
  }

  // Ensure lifetime of regex lasts until filter is used.
  std::unique_ptr<RE2> include_regex;
  if (include_regex_str && *include_regex_str) {
    include_regex = CreateFilterRegex(include_regex_str);
    if (include_regex->error_code() == RE2::NoError) {
      filters.push_back([&include_regex](const GroupedPath& id_path,
                                         const BaseSymbol& sym) -> bool {
        return MatchesRegex(id_path, sym, *include_regex);
      });
    }
  }

  std::unique_ptr<RE2> exclude_regex;
  if (exclude_regex_str && *exclude_regex_str) {
    exclude_regex = CreateFilterRegex(exclude_regex_str);
    if (exclude_regex->error_code() == RE2::NoError) {
      filters.push_back([&exclude_regex](const GroupedPath& id_path,
                                         const BaseSymbol& sym) -> bool {
        return !MatchesRegex(id_path, sym, *exclude_regex);
      });
    }
  }

  // BuildTree() is called every time a new filter is applied in the HTML
  // viewer, but if we already have a DeltaSizeInfo we can skip regenerating it
  // and let the TreeBuilder filter the symbols we care about.
  if (diff_mode && !diff_info) {
    // DeltaSizeInfo instantiation for dense diff.
    diff_info.reset(new DeltaSizeInfo(
        Diff(before_info.get(), info.get(), nullptr, nullptr)));
  }

  if (diff_mode) {
    builder.reset(new TreeBuilder(diff_info.get()));
  } else {
    builder.reset(new TreeBuilder(info.get()));
  }

  std::unique_ptr<BaseLens> lens;
  char sep = '/';
  std::cout << "group_by=" << group_by << std::endl;
  if (!strcmp(group_by, "source_path")) {
    lens = std::make_unique<IdPathLens>();
  } else if (!strcmp(group_by, "container")) {
    lens = std::make_unique<ContainerLens>();
  } else if (!strcmp(group_by, "component")) {
    lens = std::make_unique<ComponentLens>();
    sep = '>';
  } else if (!strcmp(group_by, "template")) {
    lens = std::make_unique<TemplateLens>();
    filters.push_back([](const GroupedPath&, const BaseSymbol& sym) -> bool {
      return sym.IsTemplate() && sym.IsNative();
    });
  } else if (!strcmp(group_by, "generated_type")) {
    lens = std::make_unique<GeneratedLens>();
  } else {
    std::cerr << "Unsupported group_by=" << group_by << std::endl;
    exit(1);
  }
  builder->Build(std::move(lens), sep, method_count_mode, filters);

  return bool(diff_info);
}

// Returns a JSON string representing root data.
const char* Open(const char* path) {
  static std::string cached_result;
  Json::Value v = builder->Open(path);
  cached_result = JsonSerialize(v);
  return cached_result.c_str();
}

// Returns a JSON string representing the metadata.
const char* GetMetadata() {
  static std::string cached_result;
  Json::Value v;
  v["size_file"] = info->fields;
  if (before_info != nullptr) {
    v["before_size_file"] = before_info->fields;
  }
  cached_result = JsonSerialize(v);
  return cached_result.c_str();
}

// Returns global properties.
const char* QueryProperty(const char* key) {
  if (!strcmp(key, "isMultiContainer")) {
    return IsMultiContainer() ? "true" : "false";
  }
  std::cerr << "Unknown property: " << key << std::endl;
  exit(1);
}

const char* QueryAncestryById(uint32_t id) {
  static std::string cached_result;
  Json::Value v;
  v["ancestorIds"] = builder->GetAncestryById(id);
  cached_result = JsonSerialize(v);
  return cached_result.c_str();
}

}  // extern "C"
}  // namespace caspian
