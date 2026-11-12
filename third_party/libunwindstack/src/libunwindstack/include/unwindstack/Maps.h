/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <unwindstack/MapInfo.h>

namespace unwindstack {

// Special flag to indicate a map is in /dev/. However, a map in
// /dev/ashmem/... does not set this flag.
static constexpr int MAPS_FLAGS_DEVICE_MAP = 0x8000;
// Special flag to indicate that this map represents an elf file
// created by ART for use with the gdb jit debug interface.
// This should only ever appear in offline maps data.
static constexpr int MAPS_FLAGS_JIT_SYMFILE_MAP = 0x4000;

class Maps {
 public:
  virtual ~Maps() = default;

  Maps() = default;

  // Maps are not copyable but movable, because they own pointers to MapInfo
  // objects.
  Maps(const Maps&) = delete;
  Maps& operator=(const Maps&) = delete;
  Maps(Maps&&) = default;
  Maps& operator=(Maps&&) = default;

  virtual std::shared_ptr<MapInfo> Find(uint64_t pc);

  virtual bool Parse();

  virtual const std::string GetMapsFile() const { return ""; }

  void Add(uint64_t start, uint64_t end, uint64_t offset, uint64_t flags, const std::string& name);
  void Add(uint64_t start, uint64_t end, uint64_t offset, uint64_t flags, const std::string& name,
           uint64_t load_bias);

  void Sort();

  typedef std::vector<std::shared_ptr<MapInfo>>::iterator iterator;
  iterator begin() { return maps_.begin(); }
  iterator end() { return maps_.end(); }

  typedef std::vector<std::shared_ptr<MapInfo>>::const_iterator const_iterator;
  const_iterator begin() const { return maps_.begin(); }
  const_iterator end() const { return maps_.end(); }

  size_t Total() { return maps_.size(); }

  std::shared_ptr<MapInfo> Get(size_t index) {
    if (index >= maps_.size()) return nullptr;
    return maps_[index];
  }

 protected:
  std::vector<std::shared_ptr<MapInfo>> maps_;
};

class RemoteMaps : public Maps {
 public:
  RemoteMaps(pid_t pid) : pid_(pid) {}
  virtual ~RemoteMaps() = default;

  virtual const std::string GetMapsFile() const override;

 private:
  pid_t pid_;
};

class LocalMaps : public RemoteMaps {
 public:
  LocalMaps() : RemoteMaps(getpid()) {}
  virtual ~LocalMaps() = default;
};

class LocalUpdatableMaps : public Maps {
 public:
  LocalUpdatableMaps();
  virtual ~LocalUpdatableMaps() = default;

  std::shared_ptr<MapInfo> Find(uint64_t pc) override;

  bool Parse() override;

  const std::string GetMapsFile() const override;

  bool Reparse(/*out*/ bool* any_changed = nullptr);

 private:
  pthread_rwlock_t maps_rwlock_;
};

class BufferMaps : public Maps {
 public:
  BufferMaps(const char* buffer) : buffer_(buffer) {}
  virtual ~BufferMaps() = default;

  bool Parse() override;

 private:
  const char* buffer_;
};

class FileMaps : public Maps {
 public:
  FileMaps(const std::string& file) : file_(file) {}
  virtual ~FileMaps() = default;

  const std::string GetMapsFile() const override { return file_; }

 protected:
  const std::string file_;
};

}  // namespace unwindstack
