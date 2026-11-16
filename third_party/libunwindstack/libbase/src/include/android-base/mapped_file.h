/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <sys/types.h>

#include <memory>
#include <optional>

#include "android-base/macros.h"
#include "android-base/off64_t.h"
#include "android-base/unique_fd.h"

#if defined(_WIN32)
#include <windows.h>
#define PROT_READ 1
#define PROT_WRITE 2
using os_handle = HANDLE;
#else
#include <sys/mman.h>
using os_handle = int;
#endif

namespace android::base {

/**
 * A region of a file mapped into memory (for grepping: also known as MmapFile or file mapping).
 */
class MappedFile final {
 public:
  /**
   * New factory functions that don't allocate.
   *
   * Creates a new mapping of the file pointed to by either `fd`, or the raw OS file handle `h`
   * (instead of a CRT wrapper). Unlike the underlying OS primitives, `offset` does not need to be
   * page-aligned. If `PROT_WRITE` is set in `prot`, the mapping will be writable, otherwise it
   * will be read-only. Mappings are always `MAP_SHARED`.
   */
  static std::optional<MappedFile> Create(borrowed_fd fd, off64_t offset, size_t length,
                                          int prot) noexcept;
  static std::optional<MappedFile> Create(os_handle h, off64_t offset, size_t length,
                                          int prot) noexcept;

  /**
   * Legacy pre-move factory function.
   */
  static std::unique_ptr<MappedFile> FromFd(borrowed_fd fd, off64_t offset, size_t length,
                                            int prot);

  ~MappedFile() noexcept;

  /**
   * Not copyable but movable.
   */
  MappedFile(MappedFile&& other) noexcept;
  MappedFile& operator=(MappedFile&& other) noexcept;

  char* data() const noexcept { return base_ + offset_; }
  size_t size() const noexcept { return size_; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MappedFile);

  void close() noexcept;

  char* base_;
  size_t size_;

  size_t offset_;

#if defined(_WIN32)
  MappedFile(char* base, size_t size, size_t offset, HANDLE handle) noexcept
      : base_(base), size_(size), offset_(offset), handle_(handle) {}
  HANDLE handle_;
#else
  MappedFile(char* base, size_t size, size_t offset) noexcept
      : base_(base), size_(size), offset_(offset) {}
#endif
};

}  // namespace android::base
