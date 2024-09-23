// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <limits>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/debug/proc_maps_linux.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace {

using base::debug::MappedMemoryRegion;
constexpr size_t kPageSize = 1 << 12;

// See https://www.kernel.org/doc/Documentation/vm/pagemap.txt.
struct PageMapEntry {
  uint64_t pfn_or_swap : 55;
  uint64_t soft_dirty : 1;
  uint64_t exclusively_mapped : 1;
  uint64_t unused : 4;
  uint64_t file_mapped_or_shared_anon : 1;
  uint64_t swapped : 1;
  uint64_t present : 1;
};
static_assert(sizeof(PageMapEntry) == sizeof(uint64_t), "Wrong bitfield size");

// Calls ptrace() on a process, and detaches in the destructor.
class ScopedPtracer {
 public:
  ScopedPtracer(pid_t pid) : pid_(pid), is_attached_(false) {
    // ptrace() delivers a SIGSTOP signal to one thread in the target process,
    // unless it is already stopped. Since we want to stop the whole process,
    // kill() it first.
    if (kill(pid, SIGSTOP)) {
      PLOG(ERROR) << "Cannot stop the process group of " << pid;
      return;
    }

    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr)) {
      PLOG(ERROR) << "Unable to attach to " << pid;
      return;
    }
    // ptrace(PTRACE_ATTACH) sends a SISTOP signal to the process, need to wait
    // for it.
    int status;
    pid_t ret = HANDLE_EINTR(waitpid(pid, &status, 0));
    if (ret != pid) {
      PLOG(ERROR) << "Waiting for the process failed";
      return;
    }
    if (!WIFSTOPPED(status)) {
      LOG(ERROR) << "The process is not stopped";
      ptrace(PTRACE_DETACH, pid, 0, 0);
      return;
    }
    is_attached_ = true;
  }

  ~ScopedPtracer() {
    if (!is_attached_)
      return;
    if (ptrace(PTRACE_DETACH, pid_, 0, 0)) {
      PLOG(ERROR) << "Cannot detach from " << pid_;
    }
    pid_t process_group_id = getpgid(pid_);
    if (killpg(process_group_id, SIGCONT)) {
      PLOG(ERROR) << "Cannot resume the process " << pid_;
      return;
    }
  }

  bool IsAttached() const { return is_attached_; }

 private:
  pid_t pid_;
  bool is_attached_;
};

bool ParseProcMaps(pid_t pid, std::vector<MappedMemoryRegion>* regions) {
  std::string path = base::StringPrintf("/proc/%d/maps", pid);
  std::string proc_maps;
  bool ok = base::ReadFileToString(base::FilePath(path), &proc_maps);
  if (!ok) {
    LOG(ERROR) << "Cannot read " << path;
    return false;
  }
  ok = base::debug::ParseProcMaps(proc_maps, regions);
  if (!ok) {
    LOG(ERROR) << "Cannot parse " << path;
    return false;
  }
  return true;
}

// Keep anonynmous rw-p regions.
bool ShouldDump(const MappedMemoryRegion& region) {
  const auto rw_p = MappedMemoryRegion::READ | MappedMemoryRegion::WRITE |
                    MappedMemoryRegion::PRIVATE;
  if (region.permissions != rw_p)
    return false;
  if (base::StartsWith(region.path, "/", base::CompareCase::SENSITIVE) ||
      base::StartsWith(region.path, "[stack]", base::CompareCase::SENSITIVE)) {
    return false;
  }
  return true;
}

base::File OpenProcPidFile(const char* filename, pid_t pid) {
  std::string path = base::StringPrintf("/proc/%d/%s", pid, filename);
  auto file = base::File(base::FilePath(path),
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    PLOG(ERROR) << "Cannot open " << path;
  }
  return file;
}

bool DumpRegion(const MappedMemoryRegion& region,
                pid_t pid,
                base::File* proc_mem,
                base::File* proc_pagemap) {
  size_t size_in_pages = (region.end - region.start) / kPageSize;
  std::string output_path = base::StringPrintf("%d-%" PRIuS "-%" PRIuS ".dump",
                                               pid, region.start, region.end);
  base::File output_file(base::FilePath(output_path),
                         base::File::FLAG_WRITE | base::File::FLAG_CREATE);
  if (!output_file.IsValid()) {
    PLOG(ERROR) << "Cannot open " << output_path;
    return false;
  }
  std::string metadata_path = output_path + std::string(".metadata");
  base::File metadata_file(base::FilePath(metadata_path),
                           base::File::FLAG_WRITE | base::File::FLAG_CREATE);
  if (!metadata_file.IsValid()) {
    PLOG(ERROR) << "Cannot open " << metadata_path;
    return false;
  }

  // Dump metadata.
  // Important: Metadata must be dumped before the data, as reading from
  // /proc/pid/mem will move the data back from swap, so dumping metadata
  // later would not show anything in swap.
  // This also means that dumping the same process twice will result in
  // inaccurate metadata.
  for (size_t i = 0; i < size_in_pages; ++i) {
    // See https://www.kernel.org/doc/Documentation/vm/pagemap.txt
    // 64 bits per page.
    int64_t pagemap_offset =
        ((region.start / kPageSize) + i) * sizeof(PageMapEntry);
    PageMapEntry entry;
    proc_pagemap->Seek(base::File::FROM_BEGIN, pagemap_offset);
    int size_read = proc_pagemap->ReadAtCurrentPos(
        reinterpret_cast<char*>(&entry), sizeof(PageMapEntry));
    if (size_read != sizeof(PageMapEntry)) {
      PLOG(ERROR) << "Cannot read from /proc/pid/pagemap at offset "
                  << pagemap_offset;
      return false;
    }
    std::string metadata = base::StringPrintf(
        "%c%c\n", entry.present ? '1' : '0', entry.swapped ? '1' : '0');
    metadata_file.WriteAtCurrentPos(base::as_byte_span(metadata));
  }

  // Writing data page by page to avoid allocating too much memory.
  std::vector<char> buffer(kPageSize);
  for (size_t i = 0; i < size_in_pages; ++i) {
    uint64_t address = region.start + i * kPageSize;
    // Works because the upper half of the address space is reserved for the
    // kernel on at least ARM64 and x86_64 bit architectures.
    CHECK(address <= std::numeric_limits<int64_t>::max());
    proc_mem->Seek(base::File::FROM_BEGIN, static_cast<int64_t>(address));
    int size_read = proc_mem->ReadAtCurrentPos(&buffer[0], kPageSize);
    if (size_read != kPageSize) {
      PLOG(ERROR) << "Cannot read from /proc/pid/mem at offset " << address;
      return false;
    }

    int64_t output_offset = i * kPageSize;
    int size_written = output_file.Write(output_offset, &buffer[0], kPageSize);
    if (size_written != kPageSize) {
      PLOG(ERROR) << "Cannot write to output file";
      return false;
    }
  }

  return true;
}

// Dumps the content of all the anonymous rw-p mappings in a given process to
// disk.
bool DumpMappings(pid_t pid) {
  LOG(INFO) << "Attaching to " << pid;
  // ptrace() is not required to read the process's memory, but the permissions
  // to attach to the target process is.
  // Attach anyway to make it clearer when this fails.
  ScopedPtracer tracer(pid);
  if (!tracer.IsAttached())
    return false;

  LOG(INFO) << "Reading /proc/pid/maps";
  std::vector<base::debug::MappedMemoryRegion> regions;
  bool ok = ParseProcMaps(pid, &regions);
  if (!ok)
    return false;

  base::File proc_mem = OpenProcPidFile("mem", pid);
  if (!proc_mem.IsValid())
    return false;
  base::File proc_pagemap = OpenProcPidFile("pagemap", pid);
  if (!proc_pagemap.IsValid())
    return false;

  for (const auto& region : regions) {
    if (!ShouldDump(region))
      continue;
    std::string message =
        base::StringPrintf("%" PRIuS "-%" PRIuS " (size %" PRIuS ")",
                           region.start, region.end, region.end - region.start);
    LOG(INFO) << "Dumping " << message;
    ok = DumpRegion(region, pid, &proc_mem, &proc_pagemap);
    if (!ok) {
      LOG(WARNING) << "Failed to dump region";
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  CHECK(sysconf(_SC_PAGESIZE) == kPageSize);

  if (argc != 2) {
    LOG(ERROR) << "Usage: " << argv[0] << " <pid>";
    return 1;
  }
  pid_t pid;
  bool ok = base::StringToInt(argv[1], &pid);
  if (!ok) {
    LOG(ERROR) << "Cannot parse PID";
    return 1;
  }

  ok = DumpMappings(pid);
  return ok ? 0 : 1;
}
