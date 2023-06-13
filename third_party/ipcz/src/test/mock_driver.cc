// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/mock_driver.h"

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::test {

namespace {

MockDriver*& GetDriverPtr() {
  static MockDriver* driver = nullptr;
  return driver;
}

MockDriver& GetDriver() {
  MockDriver*& ptr = GetDriverPtr();
  ABSL_ASSERT(ptr != nullptr);
  return *ptr;
}

IpczResult IPCZ_API Close(IpczDriverHandle handle,
                          uint32_t flags,
                          const void* options) {
  return GetDriver().Close(handle, flags, options);
}

IpczResult IPCZ_API Serialize(IpczDriverHandle handle,
                              IpczDriverHandle transport,
                              uint32_t flags,
                              const void* options,
                              volatile void* data,
                              size_t* num_bytes,
                              IpczDriverHandle* handles,
                              size_t* num_handles) {
  return GetDriver().Serialize(handle, transport, flags, options, data,
                               num_bytes, handles, num_handles);
}

IpczResult IPCZ_API Deserialize(const volatile void* data,
                                size_t num_bytes,
                                const IpczDriverHandle* handles,
                                size_t num_handles,
                                IpczDriverHandle transport,
                                uint32_t flags,
                                const void* options,
                                IpczDriverHandle* driver_handle) {
  return GetDriver().Deserialize(data, num_bytes, handles, num_handles,
                                 transport, flags, options, driver_handle);
}

IpczResult IPCZ_API CreateTransports(IpczDriverHandle transport0,
                                     IpczDriverHandle transport1,
                                     uint32_t flags,
                                     const void* options,
                                     IpczDriverHandle* new_transport0,
                                     IpczDriverHandle* new_transport1) {
  return GetDriver().CreateTransports(transport0, transport1, flags, options,
                                      new_transport0, new_transport1);
}

IpczResult IPCZ_API ActivateTransport(IpczDriverHandle driver_transport,
                                      IpczHandle transport,
                                      IpczTransportActivityHandler handler,
                                      uint32_t flags,
                                      const void* options) {
  return GetDriver().ActivateTransport(driver_transport, transport, handler,
                                       flags, options);
}

IpczResult IPCZ_API DeactivateTransport(IpczDriverHandle driver_transport,
                                        uint32_t flags,
                                        const void* options) {
  return GetDriver().DeactivateTransport(driver_transport, flags, options);
}

IpczResult IPCZ_API ReportBadTransportActivity(IpczDriverHandle transport,
                                               uintptr_t context,
                                               uint32_t flags,
                                               const void* options) {
  return GetDriver().ReportBadTransportActivity(transport, context, flags,
                                                options);
}

IpczResult IPCZ_API Transmit(IpczDriverHandle driver_transport,
                             const void* data,
                             size_t num_bytes,
                             const IpczDriverHandle* handles,
                             size_t num_handles,
                             uint32_t flags,
                             const void* options) {
  return GetDriver().Transmit(driver_transport, data, num_bytes, handles,
                              num_handles, flags, options);
}

IpczResult IPCZ_API AllocateSharedMemory(size_t num_bytes,
                                         uint32_t flags,
                                         const void* options,
                                         IpczDriverHandle* driver_memory) {
  return GetDriver().AllocateSharedMemory(num_bytes, flags, options,
                                          driver_memory);
}

IpczResult GetSharedMemoryInfo(IpczDriverHandle driver_memory,
                               uint32_t flags,
                               const void* options,
                               IpczSharedMemoryInfo* info) {
  return GetDriver().GetSharedMemoryInfo(driver_memory, flags, options, info);
}

IpczResult IPCZ_API DuplicateSharedMemory(IpczDriverHandle driver_memory,
                                          uint32_t flags,
                                          const void* options,
                                          IpczDriverHandle* new_driver_memory) {
  return GetDriver().DuplicateSharedMemory(driver_memory, flags, options,
                                           new_driver_memory);
}

IpczResult IPCZ_API MapSharedMemory(IpczDriverHandle driver_memory,
                                    uint32_t flags,
                                    const void* options,
                                    volatile void** address,
                                    IpczDriverHandle* driver_mapping) {
  return GetDriver().MapSharedMemory(driver_memory, flags, options, address,
                                     driver_mapping);
}

IpczResult IPCZ_API GenerateRandomBytes(size_t num_bytes,
                                        uint32_t flags,
                                        const void* options,
                                        void* buffer) {
  return GetDriver().GenerateRandomBytes(num_bytes, flags, options, buffer);
}

}  // namespace

MockDriver::MockDriver() {
  MockDriver*& ptr = GetDriverPtr();
  ABSL_ASSERT(ptr == nullptr);
  ptr = this;
}

MockDriver::~MockDriver() {
  MockDriver*& ptr = GetDriverPtr();
  ABSL_ASSERT(ptr == this);
  ptr = nullptr;
}

const IpczDriver kMockDriver = {
    sizeof(kMockDriver),
    Close,
    Serialize,
    Deserialize,
    CreateTransports,
    ActivateTransport,
    DeactivateTransport,
    Transmit,
    ReportBadTransportActivity,
    AllocateSharedMemory,
    GetSharedMemoryInfo,
    DuplicateSharedMemory,
    MapSharedMemory,
    GenerateRandomBytes,
};

}  // namespace ipcz::test
