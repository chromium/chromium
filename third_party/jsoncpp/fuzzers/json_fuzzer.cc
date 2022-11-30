// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// JsonCpp fuzzing wrapper to help with automated fuzz testing.

#include <stdint.h>
#include <array>
#include <climits>
#include <cstdio>
#include <iostream>
#include <memory>
#include "third_party/jsoncpp/source/include/json/json.h"

namespace {
// JsonCpp has a few different parsing options. The code below makes sure that
// the most intersting variants are tested.
enum { kBuilderConfigDefault = 0, kBuilderConfigStrict, kNumBuilderConfig };
}  // namespace

static const std::array<Json::CharReaderBuilder, kNumBuilderConfig>&
Initialize() {
  static std::array<Json::CharReaderBuilder, kNumBuilderConfig> builders{};

  Json::CharReaderBuilder::strictMode(
      &builders[kBuilderConfigStrict].settings_);

  return builders;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const auto& reader_builders = Initialize();

  for (const auto& reader_builder : reader_builders) {
    // Parse Json.
    auto reader =
        std::unique_ptr<Json::CharReader>(reader_builder.newCharReader());
    Json::Value root;
    bool res = reader->parse(reinterpret_cast<const char*>(data),
                             reinterpret_cast<const char*>(data + size), &root,
                             nullptr /* errs */);
    if (!res) {
      continue;
    }

    // Write and re-read json.
    const Json::StreamWriterBuilder writer_builder;
    auto writer =
        std::unique_ptr<Json::StreamWriter>(writer_builder.newStreamWriter());
    std::stringstream out_stream;
    writer->write(root, &out_stream);
    std::string output_json = out_stream.str();

    Json::Value root_again;
    res = reader->parse(output_json.data(),
                        output_json.data() + output_json.length(), &root_again,
                        nullptr /* errs */);
    if (!res) {
      continue;
    }

    // Run equality test.
    // Note: This actually causes the Json::Value tree to be traversed and all
    // the values to be dereferenced (until two of them are found not equal),
    // which is great for detecting memory corruption bugs when compiled with
    // AddressSanitizer. The result of the comparison is ignored, as it is
    // expected that both the original and the re-read version will differ from
    // time to time (e.g. due to floating point accuracy loss).
    (void)(root == root_again);
  }

  return 0;
}
