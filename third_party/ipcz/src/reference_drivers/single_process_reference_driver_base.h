// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_SINGLE_PROCESS_REFERENCE_DRIVER_BASE_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_SINGLE_PROCESS_REFERENCE_DRIVER_BASE_H_

#include <functional>

#include "ipcz/ipcz.h"

namespace ipcz::reference_drivers {

class SingleProcessReferenceDriverBase : public IpczDriver {
 public:
  IpczResult Close(IpczDriverHandle handle,
                   uint32_t flags,
                   const void* options) const override;
  IpczResult Serialize(IpczDriverHandle handle,
                       IpczDriverHandle transport,
                       uint32_t flags,
                       const void* options,
                       volatile void* data,
                       size_t* num_bytes,
                       IpczDriverHandle* handles,
                       size_t* num_handles) const override;
  IpczResult Deserialize(const volatile void* data,
                         size_t num_bytes,
                         const IpczDriverHandle* driver_handles,
                         size_t num_driver_handles,
                         IpczDriverHandle transport,
                         uint32_t flags,
                         const void* options,
                         IpczDriverHandle* handle) const override;
  IpczResult ReportBadTransportActivity(IpczDriverHandle transport,
                                        uintptr_t context,
                                        uint32_t flags,
                                        const void* options) const override;
  IpczResult AllocateSharedMemory(
      size_t num_bytes,
      uint32_t flags,
      const void* options,
      IpczDriverHandle* driver_memory) const override;
  IpczResult GetSharedMemoryInfo(IpczDriverHandle driver_memory,
                                 uint32_t flags,
                                 const void* options,
                                 IpczSharedMemoryInfo* info) const override;
  IpczResult DuplicateSharedMemory(
      IpczDriverHandle driver_memory,
      uint32_t flags,
      const void* options,
      IpczDriverHandle* new_driver_memory) const override;
  IpczResult MapSharedMemory(IpczDriverHandle driver_memory,
                             uint32_t flags,
                             const void* options,
                             volatile void** address,
                             IpczDriverHandle* driver_mapping) const override;
  IpczResult GenerateRandomBytes(size_t num_bytes,
                                 uint32_t flags,
                                 const void* options,
                                 void* buffer) const override;
};

// Installs a hook to be invoked any time ReportBadTransportActivity() is called
// on any single-process reference driver. If called with null, any previously
// installed hook is removed.
using BadTransportActivityCallback =
    std::function<void(IpczDriverHandle, uintptr_t)>;
void SetBadTransportActivityCallback(BadTransportActivityCallback callback);

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_SINGLE_PROCESS_REFERENCE_DRIVER_BASE_H_
