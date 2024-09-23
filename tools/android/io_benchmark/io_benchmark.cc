// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Benchmarks the IO system on a device, by writing and then readind a file
// filled with random data.

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_file_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace {

constexpr int kPageSize = 1 << 12;

std::mt19937 RandomEngine() {
  std::random_device r;
  std::seed_seq seed({r(), r(), r(), r(), r(), r(), r(), r()});
  return std::mt19937(seed);
}

std::vector<uint8_t> RandomData(size_t size, std::mt19937* engine) {
  std::uniform_int_distribution<uint32_t> dist(0, 255);
  std::vector<uint8_t> data(size);
  for (size_t i = 0; i < size; ++i)
    data[i] = static_cast<uint8_t>(dist(*engine));

  return data;
}

std::string DurationLogMessage(const char* prefix,
                               const base::TimeTicks& tick,
                               const base::TimeTicks& tock,
                               int size) {
  const base::TimeDelta delta = tock - tick;
  const double mb_per_second = size * delta.ToHz() / 1'000'000;
  return base::StringPrintf("%s %d = %.0fus (%.02fMB/s)", prefix, size,
                            delta.InMicrosecondsF(), mb_per_second);
}

// Returns {write_us, read_us}.
std::pair<int64_t, int64_t> WriteReadData(int size,
                                          const std::string& filename,
                                          bool drop_cache) {
  int64_t read_us, write_us;

  // Using random data for two reasons:
  // - Some filesystems do transparent compression.
  // - Some flash controllers do transparent compression
  //
  // To defeat it and get the actual IO throughput and latency, make the data
  // incompressible (which is also the case when writing compressed data).
  auto engine = RandomEngine();
  std::vector<uint8_t> data = RandomData(size, &engine);

  auto path = base::FilePath(filename);
  // Write.
  {
    auto f = base::File(
        path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    CHECK(f.IsValid());

    auto tick = base::TimeTicks::Now();
    CHECK(f.WriteAtCurrentPosAndCheck(data));
    auto tock = base::TimeTicks::Now();

    LOG(INFO) << DurationLogMessage("\tWrite", tick, tock, size);
    write_us = (tock - tick).InMicroseconds();

    CHECK(f.Flush());
  }

  if (drop_cache) {
    CHECK(base::EvictFileFromSystemCache(path));
    // Sleeping, as posix_fadvise() is asynchronous. On the other hand, we
    // don't need to sleep for too long, as all the pages are already clean
    // after the fsync() above, so no writeback is required here.
    base::PlatformThread::Sleep(base::Seconds(1));
  }

  // Read.
  {
    auto f = base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(f.IsValid());

    auto tick = base::TimeTicks::Now();
    int read = f.ReadAtCurrentPos(reinterpret_cast<char*>(&data[0]), size);
    CHECK_EQ(size, read);
    auto tock = base::TimeTicks::Now();

    LOG(INFO) << DurationLogMessage("\tRead", tick, tock, size);
    read_us = (tock - tick).InMicroseconds();
  }

  CHECK(base::DeleteFile(path));
  return {write_us, read_us};
}

// Will constantly do 4k random IO to |filename| until |should_stop| is true.
void RandomlyReadWrite(std::atomic<bool>* should_stop,
                       const std::string& filename,
                       int i) {
  constexpr int kPages = 1 << 10;
  constexpr int kSize = kPages * kPageSize;  // 4MiB (2**10 4k Pages).
  auto path = base::FilePath(filename);
  auto engine = RandomEngine();
  std::vector<uint8_t> data = RandomData(kSize, &engine);

  LOG(INFO) << "Noisy neighbor " << i << ": initial file write";
  {
    auto f = base::File(
        path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    CHECK(f.IsValid());
    CHECK(f.WriteAtCurrentPosAndCheck(data));
  }

  auto dist = std::uniform_int_distribution<int>(0, kPages - 1);

  LOG(INFO) << "Noisy neighbor " << i << ": Go";
  {
    // Opening the file ourselves as base::File doesn't have flags for O_DIRECT.
    //
    // O_DIRECT is used to make sure that reads and writes are not cached,
    // and come straight from the storage device.
    int fd = open(filename.c_str(), O_RDWR | O_DIRECT | O_SYNC);
    CHECK_NE(fd, -1);
    auto f = base::File(fd);
    // O_DIRECT has special requirements on read/write buffers alignment,
    // which are unspecified in "man open(2)". However a page-aligned buffer
    // works with linux filesystems (512 bytes is usually enough).
    std::unique_ptr<char, base::AlignedFreeDeleter> page_buffer(
        static_cast<char*>(base::AlignedAlloc(kPageSize, kPageSize)));

    while (!should_stop->load()) {
      int random = dist(engine);
      int offset = random * kPageSize;
      int size_read = f.Read(offset, page_buffer.get(), kPageSize);
      CHECK_EQ(size_read, kPageSize);

      std::vector<uint8_t> random_page = RandomData(kPageSize, &engine);
      int written =
          f.Write(offset, reinterpret_cast<char*>(&random_page[0]), kPageSize);
      CHECK_EQ(written, kPageSize);
    }
  }

  LOG(INFO) << "Noisy neighbor " << i << ": Finishing";
  base::DeleteFile(path);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) {
    LOG(ERROR) << "\nUsage: " << argv[0]
               << "FILENAME DROP_CACHES NUM_NOISY_NEIGBORS\n\n"
               << "Where: FILENAME            path to the test file "
               << "(writable).\n"
               << "       DROP_CACHES         1 to drop the filesystem cache, "
               << "0 otherwise.\n"
               << "       NUM_NOISY_NEIGBORS  number of noisy neighbor threads "
               << "to start.";
    return 1;
  }
  char* filename = argv[1];
  int drop_caches = atoi(argv[2]);
  int neighbors = atoi(argv[3]);

  std::atomic<bool> should_stop;
  should_stop.store(false);
  std::atomic<bool>* should_stop_ptr = &should_stop;

  std::vector<std::thread> noisy_neighbors;
  for (int i = 0; i < neighbors; ++i) {
    std::string path = base::StringPrintf("%s-noisy_neighbor-%d", filename, i);
    noisy_neighbors.emplace_back(
        [=]() { RandomlyReadWrite(should_stop_ptr, path, i); });
    base::PlatformThread::Sleep(base::Seconds(2));
  }

  for (int i = 0; i < 12; i++) {  // Max 1 << 11 pages = 8MiB.
    int size = (1 << i) * kPageSize;
    LOG(INFO) << "Size = " << size;

    auto write_read_us =
        WriteReadData(size, std::string(filename), drop_caches != 0);
    std::string csv_log =
        base::StringPrintf("%d,%d,%d,%d,%d", drop_caches, neighbors, size,
                           static_cast<int>(write_read_us.first),
                           static_cast<int>(write_read_us.second));
    LOG(INFO) << "CSV: " << csv_log;
  }

  should_stop.store(true);
  for (int i = 0; i < neighbors; ++i) {
    noisy_neighbors[i].join();
  }

  return 0;
}
