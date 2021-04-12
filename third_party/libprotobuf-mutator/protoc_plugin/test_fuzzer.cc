// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test fuzzer that when built successfully proves that lpm_protoc_plugin is
// working. Building this fuzzer without using lpm_protoc_plugin will fail
// because of test_fuzzer_input.proto

#include <iostream>

#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

#include "third_party/libprotobuf-mutator/protoc_plugin/test_fuzzer_input.pb.h"

DEFINE_PROTO_FUZZER(
    const lpm_protoc_plugin_test_fuzzer::TestFuzzerInput& input) {
  std::cout << input.imported().imported_publicly().input() << std::endl;
}
