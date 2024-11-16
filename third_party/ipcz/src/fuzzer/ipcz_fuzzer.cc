// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

#include "fuzzer/fuzzer.h"
#include "fuzzer/ipcz_fuzzer_testcase.h"
#include "third_party/abseil-cpp/absl/types/span.h"

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  if (size <= sizeof(ipcz::fuzzer::Fuzzer::FuzzConfig)) {
    return 0;
  }

  const auto& config =
      *reinterpret_cast<const ipcz::fuzzer::Fuzzer::FuzzConfig*>(data);
  ipcz::fuzzer::Fuzzer fuzzer(
      config, absl::MakeSpan(data, size).subspan(sizeof(config)));
  ipcz::fuzzer::IpczFuzzerTestcase testcase(fuzzer);
  testcase.Run();
  return 0;
}
