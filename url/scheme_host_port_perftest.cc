// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/scheme_host_port.h"

#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

// A benchmark for SchemeHostPort::Serialize().
namespace url {

namespace {

void BM_SchemeHostPortSerialize(benchmark::State& state) {
  SchemeHostPort scheme_host_port("https", "example.com", 443);
  benchmark::DoNotOptimize(scheme_host_port);
  for (auto _ : state) {
    std::string serialization = scheme_host_port.Serialize();
    benchmark::DoNotOptimize(serialization);
    benchmark::ClobberMemory();
  }
}

BENCHMARK(BM_SchemeHostPortSerialize);

}  // namespace

}  // namespace url
