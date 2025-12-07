// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuzzer/driver.h"

#include <cstddef>
#include <cstdint>

#include "fuzzer/fuzzer.h"
#include "ipcz/ipcz.h"

using ipcz::fuzzer::Fuzzer;

extern "C" {

IpczResult IPCZ_API IpczFuzzerClose(IpczDriverHandle handle,
                                    uint32_t flags,
                                    const void* options) {
  return Fuzzer::Get().Close(handle);
}

IpczResult IPCZ_API IpczFuzzerSerialize(IpczDriverHandle handle,
                                        IpczDriverHandle transport,
                                        uint32_t flags,
                                        const void* options,
                                        volatile void* data,
                                        size_t* num_bytes,
                                        IpczDriverHandle* handles,
                                        size_t* num_handles) {
  return Fuzzer::Get().Serialize(handle, transport, data, num_bytes, handles,
                                 num_handles);
}

IpczResult IPCZ_API IpczFuzzerDeserialize(const volatile void* data,
                                          size_t num_bytes,
                                          const IpczDriverHandle* handles,
                                          size_t num_handles,
                                          IpczDriverHandle transport,
                                          uint32_t flags,
                                          const void* options,
                                          IpczDriverHandle* handle) {
  return Fuzzer::Get().Deserialize(data, num_bytes, handles, num_handles,
                                   transport, handle);
}

IpczResult IPCZ_API
IpczFuzzerCreateTransports(IpczDriverHandle transport0,
                           IpczDriverHandle transport1,
                           uint32_t flags,
                           const void* options,
                           IpczDriverHandle* new_transport0,
                           IpczDriverHandle* new_transport1) {
  // Allow object brokering for this transport if it's been created for an
  // automated introduction by ipcz, which is implied by input transports being
  // valid. This ensures that only transports between two non-brokers will
  // attempt to relay through a broker.
  const bool may_use_broker_relay = transport0 != IPCZ_INVALID_DRIVER_HANDLE &&
                                    transport1 != IPCZ_INVALID_DRIVER_HANDLE;
  return Fuzzer::Get().CreateTransports(new_transport0, new_transport1,
                                        may_use_broker_relay);
}

IpczResult IPCZ_API
IpczFuzzerActivateTransport(IpczDriverHandle transport,
                            IpczHandle listener,
                            IpczTransportActivityHandler handler,
                            uint32_t flags,
                            const void* options) {
  return Fuzzer::Get().ActivateTransport(transport, listener, handler);
}

IpczResult IPCZ_API IpczFuzzerDeactivateTransport(IpczDriverHandle transport,
                                                  uint32_t flags,
                                                  const void* options) {
  return Fuzzer::Get().DeactivateTransport(transport);
}

IpczResult IPCZ_API IpczFuzzerTransmit(IpczDriverHandle transport,
                                       const void* data,
                                       size_t num_bytes,
                                       const IpczDriverHandle* handles,
                                       size_t num_handles,
                                       uint32_t flags,
                                       const void* options) {
  return Fuzzer::Get().Transmit(transport, data, num_bytes, handles,
                                num_handles);
}

IpczResult IPCZ_API
IpczFuzzerReportBadTransportActivity(IpczDriverHandle transport,
                                     uintptr_t context,
                                     uint32_t flags,
                                     const void* options) {
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API IpczFuzzerAllocateSharedMemory(size_t num_bytes,
                                                   uint32_t flags,
                                                   const void* options,
                                                   IpczDriverHandle* memory) {
  return Fuzzer::Get().AllocateSharedMemory(num_bytes, memory);
}

IpczResult IPCZ_API IpczFuzzerGetSharedMemoryInfo(IpczDriverHandle memory,
                                                  uint32_t flags,
                                                  const void* options,
                                                  IpczSharedMemoryInfo* info) {
  return Fuzzer::Get().GetSharedMemoryInfo(memory, info);
}

IpczResult IPCZ_API
IpczFuzzerDuplicateSharedMemory(IpczDriverHandle memory,
                                uint32_t flags,
                                const void* options,
                                IpczDriverHandle* duplicate) {
  return Fuzzer::Get().DuplicateSharedMemory(memory, duplicate);
}

IpczResult IPCZ_API IpczFuzzerMapSharedMemory(IpczDriverHandle memory,
                                              uint32_t flags,
                                              const void* options,
                                              volatile void** address,
                                              IpczDriverHandle* mapping) {
  return Fuzzer::Get().MapSharedMemory(memory, address, mapping);
}

IpczResult IPCZ_API IpczFuzzerGenerateRandomBytes(size_t num_bytes,
                                                  uint32_t flags,
                                                  const void* options,
                                                  void* buffer) {
  return Fuzzer::Get().GenerateRandomBytes(
      absl::MakeSpan(static_cast<uint8_t*>(buffer), num_bytes));
}

}  // extern "C"

namespace ipcz::fuzzer {

const IpczDriver kDriver = {
    sizeof(kDriver),
    IpczFuzzerClose,
    IpczFuzzerSerialize,
    IpczFuzzerDeserialize,
    IpczFuzzerCreateTransports,
    IpczFuzzerActivateTransport,
    IpczFuzzerDeactivateTransport,
    IpczFuzzerTransmit,
    IpczFuzzerReportBadTransportActivity,
    IpczFuzzerAllocateSharedMemory,
    IpczFuzzerGetSharedMemoryInfo,
    IpczFuzzerDuplicateSharedMemory,
    IpczFuzzerMapSharedMemory,
    IpczFuzzerGenerateRandomBytes,
};

}  // namespace ipcz::fuzzer
