// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/single_process_reference_driver.h"

#include <cstring>

#include "ipcz/ipcz.h"

namespace ipcz::reference_drivers {

namespace {

IpczResult IPCZ_API Close(IpczDriverHandle handle,
                          uint32_t flags,
                          const void* options) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API Serialize(IpczDriverHandle handle,
                              IpczDriverHandle transport,
                              uint32_t flags,
                              const void* options,
                              uint8_t* data,
                              uint32_t* num_bytes,
                              IpczDriverHandle* handles,
                              uint32_t* num_handles) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API Deserialize(const uint8_t* data,
                                uint32_t num_bytes,
                                const IpczDriverHandle* handles,
                                uint32_t num_handles,
                                IpczDriverHandle transport,
                                uint32_t flags,
                                const void* options,
                                IpczDriverHandle* driver_handle) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API CreateTransports(IpczDriverHandle transport0,
                                     IpczDriverHandle transport1,
                                     uint32_t flags,
                                     const void* options,
                                     IpczDriverHandle* new_transport0,
                                     IpczDriverHandle* new_transport1) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API ActivateTransport(IpczDriverHandle driver_transport,
                                      IpczHandle transport,
                                      IpczTransportActivityHandler handler,
                                      uint32_t flags,
                                      const void* options) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API DeactivateTransport(IpczDriverHandle driver_transport,
                                        uint32_t flags,
                                        const void* options) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API Transmit(IpczDriverHandle driver_transport,
                             const uint8_t* data,
                             uint32_t num_bytes,
                             const IpczDriverHandle* handles,
                             uint32_t num_handles,
                             uint32_t flags,
                             const void* options) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API AllocateSharedMemory(uint64_t num_bytes,
                                         uint32_t flags,
                                         const void* options,
                                         IpczDriverHandle* driver_memory) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult GetSharedMemoryInfo(IpczDriverHandle driver_memory,
                               uint32_t flags,
                               const void* options,
                               IpczSharedMemoryInfo* info) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API DuplicateSharedMemory(IpczDriverHandle driver_memory,
                                          uint32_t flags,
                                          const void* options,
                                          IpczDriverHandle* new_driver_memory) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API MapSharedMemory(IpczDriverHandle driver_memory,
                                    uint32_t flags,
                                    const void* options,
                                    void** address,
                                    IpczDriverHandle* driver_mapping) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API GenerateRandomBytes(uint32_t num_bytes,
                                        uint32_t flags,
                                        const void* options,
                                        void* buffer) {
  // Chosen by fair dice roll.
  memset(buffer, 4, num_bytes);
  return IPCZ_RESULT_OK;
}

}  // namespace

const IpczDriver kSingleProcessReferenceDriver = {
    sizeof(kSingleProcessReferenceDriver),
    Close,
    Serialize,
    Deserialize,
    CreateTransports,
    ActivateTransport,
    DeactivateTransport,
    Transmit,
    AllocateSharedMemory,
    GetSharedMemoryInfo,
    DuplicateSharedMemory,
    MapSharedMemory,
    GenerateRandomBytes,
};

}  // namespace ipcz::reference_drivers
