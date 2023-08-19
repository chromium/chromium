// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "builder.h"
#include "filesystem.h"
#include "init.h"
#include "sentencepiece_processor.h"

using sentencepiece::normalizer::Builder;

ABSL_FLAG(bool,
          output_precompiled_header,
          false,
          "make normalization_rule.h file");

namespace sentencepiece {
namespace {

std::string ToHexUInt64Array(
    const std::vector<std::pair<std::string, std::string>> &data,
    std::vector<size_t> *offset) {
  std::stringstream os;
  os.setf(std::ios_base::hex, std::ios_base::basefield);
  os.setf(std::ios_base::uppercase);
  os.setf(std::ios_base::right);
  os.fill('0');
  os.unsetf(std::ios_base::showbase);

  size_t num = 0;
  for (const auto &p : data) {
    const char *begin = p.second.data();
    const char *end = p.second.data() + p.second.size();

    offset->push_back(num);
    while (begin < end) {
      unsigned long long int n = 0;
      unsigned char *buf = reinterpret_cast<unsigned char *>(&n);
      const size_t size = std::min<size_t>(end - begin, sizeof(n));
      for (size_t i = 0; i < size; ++i) {
        buf[i] = static_cast<unsigned char>(begin[i]);
      }
      begin += sizeof(n);
      os << "0x" << std::setw(2 * sizeof(n)) << n << ", ";
      if (++num % 8 == 0) {
        os << "\n";
      }
    }
  }

  return os.str();
}

std::string ToHexData(absl::string_view data) {
  const char *begin = data.data();
  const char *end = data.data() + data.size();
  constexpr char kHex[] = "0123456789ABCDEF";
  constexpr size_t kNumOfBytesOnOneLine = 20;

  size_t output_count = 0;
  std::stringstream os;
  while (begin < end) {
    const size_t bucket_size =
        std::min<size_t>(end - begin, kNumOfBytesOnOneLine -
                                          output_count % kNumOfBytesOnOneLine);
    if (output_count % kNumOfBytesOnOneLine == 0 && bucket_size > 0) {
      os << "\"";
    }
    for (size_t i = 0; i < bucket_size; ++i) {
      os << "\\x" << kHex[(*begin & 0xF0) >> 4] << kHex[(*begin & 0x0F) >> 0];
      ++begin;
    }
    output_count += bucket_size;
    if (output_count % kNumOfBytesOnOneLine == 0 && bucket_size > 0 &&
        begin < end) {
      os << "\"\n";
    }
  }
  os << "\"\n";

  return os.str();
}

std::string MakeHeader(
    const std::vector<std::pair<std::string, std::string>> &data) {
  constexpr char kHeader[] =
      R"(#ifndef NORMALIZATION_RULE_H_
#define NORMALIZATION_RULE_H_
#include <cstdio>
namespace sentencepiece {
namespace {

struct BinaryBlob {
 const char *name;
 size_t size;
 const char *data;
};

)";

  constexpr char kFooter[] = R"(
}  // namespace
}  // namespace sentencepiece
#endif  // NORMALIZATION_RULE_H_
)";

  std::stringstream os;
  os << kHeader;

  os << "#if defined(_WIN32) && !defined(__CYGWIN__)\n";
  os << "constexpr unsigned long long int kNormalizationRules_blob_uint64[] = "
        "{\n";
  std::vector<size_t> offset;
  os << ToHexUInt64Array(data, &offset);
  CHECK_EQ(offset.size(), data.size());
  os << "};\n\n";
  os << "const BinaryBlob kNormalizationRules_blob[] = {\n";
  for (size_t i = 0; i < data.size(); ++i) {
    os << "{ \"" << data[i].first << "\", " << data[i].second.size() << ", ";
    os << "reinterpret_cast<const char *>(kNormalizationRules_blob_uint64 + "
       << offset[i] << ") },\n";
  }
  os << "};\n";
  os << "#else\n";
  os << "constexpr BinaryBlob kNormalizationRules_blob[] = {\n";
  for (size_t i = 0; i < data.size(); ++i) {
    os << "{ \"" << data[i].first << "\", " << data[i].second.size() << ", ";
    os << ToHexData(data[i].second) << "},\n";
  }
  os << "};\n";
  os << "#endif\n";

  os << "constexpr size_t kNormalizationRules_size = " << data.size() << ";\n";
  os << kFooter;

  return os.str();
}

}  // namespace
}  // namespace sentencepiece

int main(int argc, char **argv) {
  sentencepiece::ScopedResourceDestructor cleaner;
  sentencepiece::ParseCommandLineFlags(argv[0], &argc, &argv, true);

  const std::vector<
      std::pair<std::string,
                std::function<sentencepiece::util::Status(Builder::CharsMap*)>>>
      kRuleList = {{"nfkc", Builder::BuildNFKCMap},
                   {"nmt_nfkc", Builder::BuildNmtNFKCMap},
                   {"nfkc_cf", Builder::BuildNFKC_CFMap},
                   {"nmt_nfkc_cf", Builder::BuildNmtNFKC_CFMap},
                   {"nfkd", Builder::BuildNFKDMap}};

  std::vector<std::pair<std::string, std::string>> data;
  for (const auto &p : kRuleList) {
    Builder::CharsMap normalized_map;
    CHECK_OK(p.second(&normalized_map));

    // Write Header.
    std::string index;
    CHECK_OK(Builder::CompileCharsMap(normalized_map, &index));

    // Write TSV file.
    CHECK_OK(Builder::SaveCharsMap(p.first + ".tsv", normalized_map));

    // Do not make NFKD map as it is optionally created.
    if (p.first.find("nfkd") != std::string::npos) {
      continue;
    }

    data.emplace_back(p.first, index);
  }

  if (absl::GetFlag(FLAGS_output_precompiled_header)) {
    constexpr char kPrecompiledHeaderFileName[] = "normalization_rule.h";
    auto output =
        sentencepiece::filesystem::NewWritableFile(kPrecompiledHeaderFileName);
    CHECK_OK(output->status());
    output->Write(sentencepiece::MakeHeader(data));
  }

  return 0;
}
