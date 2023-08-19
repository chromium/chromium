// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/single_process_reference_driver_base.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include "ipcz/ipcz.h"
#include "reference_drivers/object.h"
#include "reference_drivers/random.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"

namespace ipcz::reference_drivers {

namespace {

// Shared memory objects in single-process drivers are plain heap allocations.
class InProcessMemory : public ObjectImpl<InProcessMemory, Object::kMemory> {
 public:
  explicit InProcessMemory(size_t size)
      : size_(size), data_(new uint8_t[size]) {
    memset(&data_[0], 0, size_);
  }

  size_t size() const { return size_; }
  void* address() const { return &data_[0]; }

 private:
  ~InProcessMemory() override = default;

  const size_t size_;
  const std::unique_ptr<uint8_t[]> data_;
};

// As there's nothing to "map" from a heap allocation, single-process driver
// memory mappings simply hold an active reference to the underlying
// heap allocation.
class InProcessMapping : public ObjectImpl<InProcessMapping, Object::kMapping> {
 public:
  explicit InProcessMapping(Ref<InProcessMemory> memory)
      : memory_(std::move(memory)) {}

  size_t size() const { return memory_->size(); }
  void* address() const { return memory_->address(); }

 private:
  ~InProcessMapping() override = default;

  const Ref<InProcessMemory> memory_;
};

BadTransportActivityCallback& GetBadTransportActivityCallback() {
  static BadTransportActivityCallback* callback =
      new BadTransportActivityCallback();
  return *callback;
}

IpczResult IPCZ_API Close(IpczDriverHandle handle,
                          uint32_t flags,
                          const void* options) {
  Ref<Object> object = Object::TakeFromHandle(handle);
  if (!object) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  return object->Close();
}

IpczResult IPCZ_API Serialize(IpczDriverHandle handle,
                              IpczDriverHandle transport,
                              uint32_t flags,
                              const void* options,
                              volatile void* data,
                              size_t* num_bytes,
                              IpczDriverHandle* handles,
                              size_t* num_handles) {
  Object* object = Object::FromHandle(handle);
  if (!object) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (num_bytes) {
    *num_bytes = 0;
  }

  // Since this is all in-process, all driver handles can be transmitted as-is.
  const size_t handle_capacity = num_handles ? *num_handles : 0;
  if (num_handles) {
    *num_handles = 1;
  }
  if (handle_capacity < 1) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  handles[0] = handle;
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API Deserialize(const volatile void* data,
                                size_t num_bytes,
                                const IpczDriverHandle* handles,
                                size_t num_handles,
                                IpczDriverHandle transport,
                                uint32_t flags,
                                const void* options,
                                IpczDriverHandle* driver_handle) {
  ABSL_ASSERT(num_bytes == 0);
  ABSL_ASSERT(num_handles == 1);
  *driver_handle = handles[0];
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API ReportBadTransportActivity(IpczDriverHandle transport,
                                               uintptr_t context,
                                               uint32_t flags,
                                               const void* options) {
  auto& callback = GetBadTransportActivityCallback();
  if (callback) {
    callback(transport, context);
  }
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API AllocateSharedMemory(size_t num_bytes,
                                         uint32_t flags,
                                         const void* options,
                                         IpczDriverHandle* driver_memory) {
  if (num_bytes > std::numeric_limits<size_t>::max()) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  auto memory = MakeRefCounted<InProcessMemory>(static_cast<size_t>(num_bytes));
  *driver_memory = Object::ReleaseAsHandle(std::move(memory));
  return IPCZ_RESULT_OK;
}

IpczResult GetSharedMemoryInfo(IpczDriverHandle driver_memory,
                               uint32_t flags,
                               const void* options,
                               IpczSharedMemoryInfo* info) {
  Object* object = Object::FromHandle(driver_memory);
  if (!object || object->type() != Object::kMemory || !info ||
      info->size < sizeof(IpczSharedMemoryInfo)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  info->region_num_bytes = static_cast<InProcessMemory*>(object)->size();
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API DuplicateSharedMemory(IpczDriverHandle driver_memory,
                                          uint32_t flags,
                                          const void* options,
                                          IpczDriverHandle* new_driver_memory) {
  Ref<InProcessMemory> memory(InProcessMemory::FromHandle(driver_memory));
  *new_driver_memory = Object::ReleaseAsHandle(std::move(memory));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API MapSharedMemory(IpczDriverHandle driver_memory,
                                    uint32_t flags,
                                    const void* options,
                                    volatile void** address,
                                    IpczDriverHandle* driver_mapping) {
  Ref<InProcessMemory> memory(InProcessMemory::FromHandle(driver_memory));
  auto mapping = MakeRefCounted<InProcessMapping>(std::move(memory));
  *address = mapping->address();
  *driver_mapping = Object::ReleaseAsHandle(std::move(mapping));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API GenerateRandomBytes(size_t num_bytes,
                                        uint32_t flags,
                                        const void* options,
                                        void* buffer) {
  RandomBytes(absl::MakeSpan(static_cast<uint8_t*>(buffer), num_bytes));
  return IPCZ_RESULT_OK;
}

}  // namespace

const IpczDriver kSingleProcessReferenceDriverBase = {
    sizeof(kSingleProcessReferenceDriverBase),
    Close,
    Serialize,
    Deserialize,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    ReportBadTransportActivity,
    AllocateSharedMemory,
    GetSharedMemoryInfo,
    DuplicateSharedMemory,
    MapSharedMemory,
    GenerateRandomBytes,
};

void SetBadTransportActivityCallback(BadTransportActivityCallback callback) {
  GetBadTransportActivityCallback() = std::move(callback);
}

}  // namespace ipcz::reference_drivers
