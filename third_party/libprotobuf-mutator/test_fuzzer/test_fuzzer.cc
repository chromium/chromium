// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test fuzzer that when built successfully proves that fuzzable_proto_library
// is working. Building this fuzzer without using fuzzable_proto_library will
// fail because of test_fuzzer_input.proto

#include <iostream>

#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

#include "third_party/libprotobuf-mutator/test_fuzzer/test_fuzzer_input.pb.h"

DEFINE_PROTO_FUZZER(const lpm_test_fuzzer::TestFuzzerInput& input) {
  std::cout << input.imported().imported_publicly().input() << std::endl;
}
