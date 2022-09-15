// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Benchmarks SHA256 hashing.

#include <sched.h>
#include <stdlib.h>

#include <array>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "crypto/secure_hash.h"

namespace {

constexpr size_t kSha256HashBytes = 32;
using SHA256HashValue = std::array<uint8_t, kSha256HashBytes>;

std::vector<uint8_t> GenerateData(size_t size) {
  std::random_device device;
  std::mt19937 rng(device());
  std::uniform_int_distribution<uint32_t> dist{};

  std::vector<uint8_t> result;
  result.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    result.push_back(static_cast<uint8_t>(dist(rng)));
  }

  return result;
}

void LogResults(size_t chunk_size,
                size_t chunk_count,
                base::TimeTicks tick,
                base::TimeTicks tock) {
  size_t total_size = chunk_size * chunk_count;
  double elapsed_us = (tock - tick).InMicrosecondsF();
  double throughput = total_size / elapsed_us;
  double latency_us = elapsed_us / chunk_count;

  LOG(INFO) << chunk_size << "," << throughput << "," << latency_us;
}

void HashChunks(const std::vector<uint8_t>& data,
                size_t chunk_size,
                std::vector<SHA256HashValue>* hashes) {
  size_t chunk_count = data.size() / chunk_size;

  SHA256HashValue hash_value;
  for (size_t i = 0; i < chunk_count; ++i) {
    auto hasher = crypto::SecureHash::Create(crypto::SecureHash::SHA256);

    hasher->Update(&data[i * chunk_size], chunk_size);
    hasher->Finish(&hash_value[0], kSha256HashBytes);
    hashes->push_back(hash_value);
  }
}

void BenchmarkHashing(const std::vector<uint8_t>& data, size_t chunk_size) {
  std::vector<SHA256HashValue> hashes;
  auto tick = base::TimeTicks::Now();
  HashChunks(data, chunk_size, &hashes);
  auto tock = base::TimeTicks::Now();

  LogResults(chunk_size, hashes.size(), tick, tock);
}

bool RestrictToSpecificCore(int core_index) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(core_index, &set);

  return !sched_setaffinity(0, sizeof(set), &set);
}

}  // namespace

int main(int argc, char** argv) {
  LOG(INFO) << "Generating data (40MiB)";
  constexpr size_t target_size = 40 * 1024 * 1024;
  constexpr size_t kPageSize = 1 << 12;
  auto data = GenerateData(target_size);

  if (argc == 2) {
    int core = atoi(argv[1]);
    LOG(INFO) << "Restricting to core #" << core;
    if (!RestrictToSpecificCore(core)) {
      LOG(ERROR) << "Unable to restrict to core, exiting";
      return 1;
    }
  }

  LOG(INFO) << "Hashing";
  for (size_t size = kPageSize; size < data.size(); size *= 2) {
    BenchmarkHashing(data, size);
  }
  return 0;
}
