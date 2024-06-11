// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Benchmarks in-memory compression and decompression of an input file,
// comparing zlib and snappy.

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/brotli/include/brotli/decode.h"
#include "third_party/brotli/include/brotli/encode.h"
#include "third_party/snappy/src/snappy.h"
#include "third_party/zlib/google/compression_utils.h"

namespace {

enum class CompressionType { kSnappy, kZlib, kBrotli };

void LogResults(CompressionType compression_type,
                bool compression,
                size_t chunk_size,
                size_t chunk_count,
                double compression_ratio,
                base::TimeTicks tick,
                base::TimeTicks tock) {
  size_t total_size = chunk_size * chunk_count;
  double elapsed_us = (tock - tick).InMicrosecondsF();
  double throughput = total_size / elapsed_us;
  double latency_us = elapsed_us / chunk_count;

  std::string compression_name;
  switch (compression_type) {
    case CompressionType::kSnappy:
      compression_name = "snappy";
      break;
    case CompressionType::kZlib:
      compression_name = "zlib";
      break;
    case CompressionType::kBrotli:
      compression_name = "brotli";
      break;
  }
  LOG(INFO) << compression_name << ","
            << (compression ? "compression" : "decompression") << ","
            << chunk_size << "," << throughput << "," << latency_us << ","
            << compression_ratio;
}

void CompressChunks(const std::string& contents,
                    size_t chunk_size,
                    CompressionType compression_type,
                    std::vector<std::string>* compressed_chunks) {
  CHECK(compressed_chunks);
  size_t chunk_count = contents.size() / chunk_size;

  for (size_t i = 0; i < chunk_count; ++i) {
    std::string compressed;
    std::string_view input(contents.c_str() + i * chunk_size, chunk_size);

    switch (compression_type) {
      case CompressionType::kSnappy:
        CHECK(snappy::Compress(input.data(), input.size(), &compressed));
        break;
      case CompressionType::kZlib:
        CHECK(compression::GzipCompress(input, &compressed));
        break;
      case CompressionType::kBrotli: {
        std::vector<uint8_t> compressed_data(
            BrotliEncoderMaxCompressedSize(input.size()));
        size_t encoded_size = compressed_data.size();
        CHECK(BrotliEncoderCompress(
            3, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE, input.size(),
            reinterpret_cast<const uint8_t*>(input.data()), &encoded_size,
            reinterpret_cast<uint8_t*>(&compressed_data[0])));
        compressed.assign(reinterpret_cast<const char*>(&compressed_data[0]),
                          encoded_size);
      } break;
    }

    compressed_chunks->push_back(compressed);
  }
}

void BenchmarkDecompression(const std::string& contents,
                            size_t chunk_size,
                            CompressionType compression_type) {
  std::vector<std::string> compressed_chunks;
  CompressChunks(contents, chunk_size, compression_type, &compressed_chunks);

  auto tick = base::TimeTicks::Now();
  for (const auto& chunk : compressed_chunks) {
    std::string uncompressed;
    switch (compression_type) {
      case CompressionType::kSnappy:
        snappy::Uncompress(chunk.c_str(), chunk.size(), &uncompressed);
        break;
      case CompressionType::kZlib:
        CHECK(compression::GzipUncompress(chunk, &uncompressed));
        break;
      case CompressionType::kBrotli: {
        size_t decoded_size = chunk_size;
        std::vector<uint8_t> decoded_data(chunk_size);
        CHECK(BrotliDecoderDecompress(
            chunk.size(), reinterpret_cast<const uint8_t*>(&chunk[0]),
            &decoded_size, &decoded_data[0]));
        CHECK_EQ(chunk_size, decoded_size);
      } break;
    }
  }
  auto tock = base::TimeTicks::Now();

  LogResults(compression_type, false, chunk_size, compressed_chunks.size(), 0.,
             tick, tock);
}

void BenchmarkCompression(const std::string& contents,
                          size_t chunk_size,
                          CompressionType compression_type) {
  auto tick = base::TimeTicks::Now();
  std::vector<std::string> compressed_chunks;
  CompressChunks(contents, chunk_size, compression_type, &compressed_chunks);
  auto tock = base::TimeTicks::Now();

  size_t compressed_size = 0;
  for (const auto& compressed_chunk : compressed_chunks)
    compressed_size += compressed_chunk.size();

  double ratio = contents.size() / static_cast<double>(compressed_size);
  LogResults(compression_type, true, chunk_size, compressed_chunks.size(),
             ratio, tick, tock);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    LOG(FATAL) << "Usage: " << argv[0] << " <filename>\n\n"
               << "Where the file contains data to compress";
  }

  LOG(INFO) << "Reading the input file";
  auto path = base::FilePath(std::string(argv[1]));
  std::string contents;
  CHECK(base::ReadFileToString(path, &contents));

  // Make sure we have at least 40MiB.
  constexpr size_t kPageSize = 1 << 12;
  constexpr size_t target_size = 40 * 1024 * 1024;
  std::string repeated_contents;
  size_t repeats = target_size / contents.size() + 1;

  repeated_contents.reserve(repeats * contents.size());
  for (size_t i = 0; i < repeats; ++i)
    repeated_contents.append(contents);

  for (CompressionType compression_type :
       {CompressionType::kSnappy, CompressionType::kZlib,
        CompressionType::kBrotli}) {
    for (size_t size = kPageSize; size < contents.size(); size *= 2) {
      BenchmarkCompression(repeated_contents, size, compression_type);
      BenchmarkDecompression(repeated_contents, size, compression_type);
    }
  }
  return 0;
}
