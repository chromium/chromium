// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/mock_driver.h"

#include "base/no_destructor.h"
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

class MockDriverImpl : public IpczDriver {
 public:
  IpczResult Close(IpczDriverHandle handle,
                   uint32_t flags,
                   const void* options) const override {
    return GetDriver().Close(handle, flags, options);
  }

  IpczResult Serialize(IpczDriverHandle handle,
                       IpczDriverHandle transport,
                       uint32_t flags,
                       const void* options,
                       volatile void* data,
                       size_t* num_bytes,
                       IpczDriverHandle* handles,
                       size_t* num_handles) const override {
    return GetDriver().Serialize(handle, transport, flags, options, data,
                                 num_bytes, handles, num_handles);
  }

  IpczResult Deserialize(const volatile void* data,
                         size_t num_bytes,
                         const IpczDriverHandle* handles,
                         size_t num_handles,
                         IpczDriverHandle transport,
                         uint32_t flags,
                         const void* options,
                         IpczDriverHandle* driver_handle) const override {
    return GetDriver().Deserialize(data, num_bytes, handles, num_handles,
                                   transport, flags, options, driver_handle);
  }

  IpczResult CreateTransports(IpczDriverHandle transport0,
                              IpczDriverHandle transport1,
                              uint32_t flags,
                              const void* options,
                              IpczDriverHandle* new_transport0,
                              IpczDriverHandle* new_transport1) const override {
    return GetDriver().CreateTransports(transport0, transport1, flags, options,
                                        new_transport0, new_transport1);
  }

  IpczResult ActivateTransport(IpczDriverHandle driver_transport,
                               IpczHandle transport,
                               IpczTransportActivityHandler handler,
                               uint32_t flags,
                               const void* options) const override {
    return GetDriver().ActivateTransport(driver_transport, transport, handler,
                                         flags, options);
  }

  IpczResult DeactivateTransport(IpczDriverHandle driver_transport,
                                 uint32_t flags,
                                 const void* options) const override {
    return GetDriver().DeactivateTransport(driver_transport, flags, options);
  }

  IpczResult ReportBadTransportActivity(IpczDriverHandle transport,
                                        uintptr_t context,
                                        uint32_t flags,
                                        const void* options) const override {
    return GetDriver().ReportBadTransportActivity(transport, context, flags,
                                                  options);
  }

  IpczResult Transmit(IpczDriverHandle driver_transport,
                      const void* data,
                      size_t num_bytes,
                      const IpczDriverHandle* handles,
                      size_t num_handles,
                      uint32_t flags,
                      const void* options) const override {
    return GetDriver().Transmit(driver_transport, data, num_bytes, handles,
                                num_handles, flags, options);
  }

  IpczResult AllocateSharedMemory(
      size_t num_bytes,
      uint32_t flags,
      const void* options,
      IpczDriverHandle* driver_memory) const override {
    return GetDriver().AllocateSharedMemory(num_bytes, flags, options,
                                            driver_memory);
  }

  IpczResult GetSharedMemoryInfo(IpczDriverHandle driver_memory,
                                 uint32_t flags,
                                 const void* options,
                                 IpczSharedMemoryInfo* info) const override {
    return GetDriver().GetSharedMemoryInfo(driver_memory, flags, options, info);
  }

  IpczResult DuplicateSharedMemory(
      IpczDriverHandle driver_memory,
      uint32_t flags,
      const void* options,
      IpczDriverHandle* new_driver_memory) const override {
    return GetDriver().DuplicateSharedMemory(driver_memory, flags, options,
                                             new_driver_memory);
  }

  IpczResult MapSharedMemory(IpczDriverHandle driver_memory,
                             uint32_t flags,
                             const void* options,
                             volatile void** address,
                             IpczDriverHandle* driver_mapping) const override {
    return GetDriver().MapSharedMemory(driver_memory, flags, options, address,
                                       driver_mapping);
  }

  IpczResult GenerateRandomBytes(size_t num_bytes,
                                 uint32_t flags,
                                 const void* options,
                                 void* buffer) const override {
    return GetDriver().GenerateRandomBytes(num_bytes, flags, options, buffer);
  }
};

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

const IpczDriver& GetMockDriver() {
  static const base::NoDestructor<MockDriverImpl> driver;
  return *driver;
}

}  // namespace ipcz::test
