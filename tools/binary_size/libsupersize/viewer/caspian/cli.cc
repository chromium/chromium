// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Command-line interface for checking the integrity of .size files.

#include <stdlib.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "tools/binary_size/libsupersize/viewer/caspian/diff.h"
#include "tools/binary_size/libsupersize/viewer/caspian/file_format.h"
#include "tools/binary_size/libsupersize/viewer/caspian/model.h"

void ParseSizeInfoFromFile(const char* filename, caspian::SizeInfo* info) {
  std::ifstream ifs(filename, std::ifstream::in);
  if (!ifs.good()) {
    std::cerr << "Unable to open file: " << filename << std::endl;
    exit(1);
  }
  std::string compressed((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
  caspian::ParseSizeInfo(compressed.data(), compressed.size(), info);
}

void ParseDiffSizeInfoFromFile(const char* filename,
                               caspian::SizeInfo* before,
                               caspian::SizeInfo* after) {
  std::ifstream ifs(filename, std::ifstream::in);
  if (!ifs.good()) {
    std::cerr << "Unable to open file: " << filename << std::endl;
    exit(1);
  }
  std::vector<std::string> removed_sources;
  std::vector<std::string> added_sources;
  std::string compressed((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
  caspian::ParseDiffSizeInfo(&compressed[0], compressed.size(), before, after,
                             &removed_sources, &added_sources);
  if (removed_sources.size()) {
    std::cout << "Removed " << removed_sources.size() << " files:" << std::endl;
    for (const std::string& s : removed_sources) {
      std::cout << "  " << s << std::endl;
    }
  }
  if (added_sources.size()) {
    std::cout << "Added " << added_sources.size() << " files:" << std::endl;
    for (const std::string& s : added_sources) {
      std::cout << "  " << s << std::endl;
    }
  }
}

void Diff(const char* before_filename, const char* after_filename) {
  caspian::SizeInfo before;
  ParseSizeInfoFromFile(before_filename, &before);

  caspian::SizeInfo after;
  ParseSizeInfoFromFile(after_filename, &after);

  caspian::DeltaSizeInfo diff = Diff(&before, &after, nullptr, nullptr);

  float pss = 0.0f;
  float size = 0.0f;
  float padding = 0.0f;
  for (const auto& sym : diff.delta_symbols) {
    pss += sym.Pss();
    size += sym.Size();
    padding += sym.Padding();
  }
  std::cout << "Pss: " << pss << std::endl;
  std::cout << "Size: " << size << std::endl;
  std::cout << "Padding: " << padding << std::endl;
}

void Validate(const char* filename) {
  caspian::SizeInfo info;
  ParseSizeInfoFromFile(filename, &info);
}

void ValidateDiff(const char* filename) {
  caspian::SizeInfo before;
  caspian::SizeInfo after;
  ParseDiffSizeInfoFromFile(filename, &before, &after);
}

void ShowDisassembly(const char* filename) {
  caspian::SizeInfo before;
  caspian::SizeInfo after;
  ParseDiffSizeInfoFromFile(filename, &before, &after);

  for (const auto& sym : after.raw_symbols) {
    if (sym.disassembly_ != nullptr) {
      std::cout << "Symbol Name: " << sym.full_name_ << std::endl;
      std::cout << "Disassembly:" << std::endl
                << *sym.disassembly_ << std::endl;
    }
  }
}

void PrintUsageAndExit() {
  std::cerr << "Must have exactly one of:" << std::endl;
  std::cerr << "  validate, diff" << std::endl;
  std::cerr << "Usage:" << std::endl;
  std::cerr << "  caspian_cli validate <.size file>" << std::endl;
  std::cerr << "  caspian_cli validatediff <.sizediff file>" << std::endl;
  std::cerr << "  caspian_cli diff <before_file> <after_file>" << std::endl;
  std::cerr << "  caspian_cli showdisassembly <.sizediff file>" << std::endl;
  exit(1);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    PrintUsageAndExit();
  }
  if (std::string_view(argv[1]) == "diff") {
    Diff(argv[2], argv[3]);
  } else if (std::string_view(argv[1]) == "validate") {
    Validate(argv[2]);
  } else if (std::string_view(argv[1]) == "validatediff") {
    ValidateDiff(argv[2]);
  } else if (std::string_view(argv[1]) == "showdisassembly") {
    ShowDisassembly(argv[2]);
  } else {
    PrintUsageAndExit();
  }
  return 0;
}
