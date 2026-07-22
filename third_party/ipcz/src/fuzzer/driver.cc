// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuzzer/driver.h"

#include <cstddef>
#include <cstdint>

#include "base/no_destructor.h"
#include "fuzzer/fuzzer.h"
#include "ipcz/ipcz.h"

using ipcz::fuzzer::Fuzzer;

class FuzzerDriverImpl : public IpczDriver {
 public:
  IpczResult Close(IpczDriverHandle handle,
                   uint32_t flags,
                   const void* options) const override {
    return Fuzzer::Get().Close(handle);
  }

  IpczResult Serialize(IpczDriverHandle handle,
                       IpczDriverHandle transport,
                       uint32_t flags,
                       const void* options,
                       volatile void* data,
                       size_t* num_bytes,
                       IpczDriverHandle* handles,
                       size_t* num_handles) const override {
    return Fuzzer::Get().Serialize(handle, transport, data, num_bytes, handles,
                                   num_handles);
  }

  IpczResult Deserialize(const volatile void* data,
                         size_t num_bytes,
                         const IpczDriverHandle* handles,
                         size_t num_handles,
                         IpczDriverHandle transport,
                         uint32_t flags,
                         const void* options,
                         IpczDriverHandle* handle) const override {
    return Fuzzer::Get().Deserialize(data, num_bytes, handles, num_handles,
                                     transport, handle);
  }

  IpczResult CreateTransports(IpczDriverHandle transport0,
                              IpczDriverHandle transport1,
                              uint32_t flags,
                              const void* options,
                              IpczDriverHandle* new_transport0,
                              IpczDriverHandle* new_transport1) const override {
    // Allow object brokering for this transport if it's been created for an
    // automated introduction by ipcz, which is implied by input transports
    // being valid. This ensures that only transports between two non-brokers
    // will attempt to relay through a broker.
    const bool may_use_broker_relay =
        transport0 != IPCZ_INVALID_DRIVER_HANDLE &&
        transport1 != IPCZ_INVALID_DRIVER_HANDLE;
    return Fuzzer::Get().CreateTransports(new_transport0, new_transport1,
                                          may_use_broker_relay);
  }

  IpczResult ActivateTransport(IpczDriverHandle transport,
                               IpczHandle listener,
                               IpczTransportActivityHandler handler,
                               uint32_t flags,
                               const void* options) const override {
    return Fuzzer::Get().ActivateTransport(transport, listener, handler);
  }

  IpczResult DeactivateTransport(IpczDriverHandle transport,
                                 uint32_t flags,
                                 const void* options) const override {
    return Fuzzer::Get().DeactivateTransport(transport);
  }

  IpczResult Transmit(IpczDriverHandle transport,
                      const void* data,
                      size_t num_bytes,
                      const IpczDriverHandle* handles,
                      size_t num_handles,
                      uint32_t flags,
                      const void* options) const override {
    return Fuzzer::Get().Transmit(transport, data, num_bytes, handles,
                                  num_handles);
  }

  IpczResult ReportBadTransportActivity(IpczDriverHandle transport,
                                        uintptr_t context,
                                        uint32_t flags,
                                        const void* options) const override {
    return IPCZ_RESULT_OK;
  }

  IpczResult AllocateSharedMemory(size_t num_bytes,
                                  uint32_t flags,
                                  const void* options,
                                  IpczDriverHandle* memory) const override {
    return Fuzzer::Get().AllocateSharedMemory(num_bytes, memory);
  }

  IpczResult GetSharedMemoryInfo(IpczDriverHandle memory,
                                 uint32_t flags,
                                 const void* options,
                                 IpczSharedMemoryInfo* info) const override {
    return Fuzzer::Get().GetSharedMemoryInfo(memory, info);
  }

  IpczResult DuplicateSharedMemory(IpczDriverHandle memory,
                                   uint32_t flags,
                                   const void* options,
                                   IpczDriverHandle* duplicate) const override {
    return Fuzzer::Get().DuplicateSharedMemory(memory, duplicate);
  }

  IpczResult MapSharedMemory(IpczDriverHandle memory,
                             uint32_t flags,
                             const void* options,
                             volatile void** address,
                             IpczDriverHandle* mapping) const override {
    return Fuzzer::Get().MapSharedMemory(memory, address, mapping);
  }

  IpczResult GenerateRandomBytes(size_t num_bytes,
                                 uint32_t flags,
                                 const void* options,
                                 void* buffer) const override {
    return Fuzzer::Get().GenerateRandomBytes(
        absl::MakeSpan(static_cast<uint8_t*>(buffer), num_bytes));
  }
};

namespace ipcz::fuzzer {

const IpczDriver& GetFuzzerDriver() {
  static const base::NoDestructor<FuzzerDriverImpl> driver;
  return *driver;
}

}  // namespace ipcz::fuzzer
