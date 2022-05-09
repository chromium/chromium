// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/single_process_reference_driver.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "ipcz/ipcz.h"
#include "reference_drivers/object.h"
#include "reference_drivers/random.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz::reference_drivers {

namespace {

// Provides shared ownership of a transport object given to the driver by ipcz
// during driver transport activation. Within ipcz this corresponds to a
// DriverTransport object.
class TransportWrapper : public RefCounted {
 public:
  TransportWrapper(IpczHandle transport,
                   IpczTransportActivityHandler activity_handler)
      : transport_(transport), activity_handler_(activity_handler) {}

  IpczResult Notify(absl::Span<const uint8_t> data,
                    absl::Span<const IpczDriverHandle> handles) {
    IpczResult result =
        activity_handler_(transport_, data.data(), data.size(), handles.data(),
                          handles.size(), IPCZ_NO_FLAGS, nullptr);
    if (result != IPCZ_RESULT_OK && result != IPCZ_RESULT_UNIMPLEMENTED) {
      NotifyError();
    }
    return result;
  }

  IpczResult NotifyError() {
    return activity_handler_(transport_, nullptr, 0, nullptr, 0,
                             IPCZ_TRANSPORT_ACTIVITY_ERROR, nullptr);
  }

 private:
  ~TransportWrapper() override {
    activity_handler_(transport_, nullptr, 0, nullptr, 0,
                      IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED, nullptr);
  }

  const IpczHandle transport_;
  const IpczTransportActivityHandler activity_handler_;
};

struct SavedMessage {
  std::vector<uint8_t> data;
  std::vector<IpczDriverHandle> handles;
};

// The driver transport implementation for the single-process reference driver.
//
// Each InProcessTransport holds a direct reference to the other endpoint, and
// transmitting from one endpoint directly notifies the peer endpoint, calling
// directly into its ipcz-side DriverTransport's Listener.
//
// This means that cross-node communications through this driver function as
// synchronous calls from one node into another, and as such the implementation
// must be safe for arbitrary reentrancy.
class InProcessTransport
    : public ObjectImpl<InProcessTransport, Object::kTransport> {
 public:
  InProcessTransport() = default;
  ~InProcessTransport() override = default;

  // Object:
  IpczResult Close() override {
    Deactivate();

    Ref<InProcessTransport> peer;
    {
      absl::MutexLock lock(&mutex_);
      peer = std::move(peer_);
    }

    if (peer) {
      // NOTE: Although nothing should ever call back into `this` after Close(),
      // for consistency with other methods we still take precaution not to call
      // into the peer while holding `mutex_`.
      peer->OnPeerClosed();
    }
    return IPCZ_RESULT_OK;
  }

  void SetPeer(Ref<InProcessTransport> peer) {
    absl::MutexLock lock(&mutex_);
    ABSL_ASSERT(peer && !peer_);
    peer_ = std::move(peer);
  }

  IpczResult Activate(IpczHandle transport,
                      IpczTransportActivityHandler activity_handler) {
    Ref<InProcessTransport> peer;
    {
      absl::MutexLock lock(&mutex_);
      ABSL_ASSERT(!transport_);
      transport_ =
          MakeRefCounted<TransportWrapper>(transport, activity_handler);
      peer = peer_;
    }

    // Let the peer know that it can now call into us directly. This may
    // re-enter this InProcessTransport, as the peer will synchronously flush
    // any queued transmissions before returning, and the logic handling the
    // receipt of those transmissions may perform additional operations on this
    // transport; all before OnPeerActivated() returns. We must therefore ensure
    // `mutex_` is not held while calling this.
    peer->OnPeerActivated();
    return IPCZ_RESULT_OK;
  }

  void Deactivate() {
    Ref<TransportWrapper> transport;
    {
      absl::MutexLock lock(&mutex_);

      // NOTE: Dropping this reference may in turn lead to ipcz dropping its own
      // last reference to `this`, so we must be careful not hold `mutex_` when
      // that happens.
      transport = std::move(transport_);
    }
  }

  IpczResult Transmit(absl::Span<const uint8_t> data,
                      absl::Span<const IpczDriverHandle> handles) {
    Ref<InProcessTransport> peer;
    Ref<TransportWrapper> peer_transport;
    {
      absl::MutexLock lock(&mutex_);
      if (!peer_active_) {
        SavedMessage message;
        message.data = std::vector<uint8_t>(data.begin(), data.end());
        message.handles =
            std::vector<IpczDriverHandle>(handles.begin(), handles.end());
        saved_messages_.push_back(std::move(message));
        return IPCZ_RESULT_OK;
      }

      peer = peer_;
      ABSL_ASSERT(peer);
    }

    {
      absl::MutexLock lock(&peer->mutex_);
      peer_transport = peer->transport_;
    }

    if (peer_transport) {
      // NOTE: Notifying the peer of anything may re-enter `this`, so we must be
      // careful not hold `mutex_` while doing that.
      peer_transport->Notify(data, handles);
    }
    return IPCZ_RESULT_OK;
  }

 private:
  void OnPeerActivated() {
    for (;;) {
      Ref<InProcessTransport> peer;
      std::vector<SavedMessage> saved_messages;
      {
        absl::MutexLock lock(&mutex_);
        ABSL_ASSERT(!peer_active_);
        if (saved_messages_.empty()) {
          peer_active_ = true;
          return;
        }

        std::swap(saved_messages_, saved_messages);
        saved_messages_.clear();

        peer = peer_;
      }

      Ref<TransportWrapper> peer_transport;
      {
        absl::MutexLock lock(&peer->mutex_);
        peer_transport = peer->transport_;
      }

      if (!peer_transport) {
        // NOTE: We could have Close() retain the peer reference if there are
        // queued messages, and only drop the reference after flushing them.
        // This is not necessary in practice, but if we ever have test scenarios
        // that may close one transport before activating its peer, that may be
        // a useful thing to do.
        return;
      }

      for (SavedMessage& m : saved_messages) {
        peer_transport->Notify(m.data, m.handles);
      }
    }
  }

  void OnPeerClosed() {
    Ref<TransportWrapper> transport;
    {
      absl::MutexLock lock(&mutex_);
      transport = std::move(transport_);
    }

    if (transport) {
      // NOTE: Notifying ipcz of an error here may re-enter this
      // InProcessTransport (e.g. to close it), so we must be careful not to
      // hold `mutex_` while doing that.
      transport->NotifyError();
    }
  }

  absl::Mutex mutex_;
  Ref<InProcessTransport> peer_ ABSL_GUARDED_BY(mutex_);
  Ref<TransportWrapper> transport_ ABSL_GUARDED_BY(mutex_);
  bool peer_active_ ABSL_GUARDED_BY(mutex_) = false;
  std::vector<SavedMessage> saved_messages_ ABSL_GUARDED_BY(mutex_);
};

// Shared memory regions for the single-process driver are just regular private
// heap allocations.
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
                              void* data,
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

IpczResult IPCZ_API Deserialize(const void* data,
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

IpczResult IPCZ_API CreateTransports(IpczDriverHandle transport0,
                                     IpczDriverHandle transport1,
                                     uint32_t flags,
                                     const void* options,
                                     IpczDriverHandle* new_transport0,
                                     IpczDriverHandle* new_transport1) {
  auto first = MakeRefCounted<InProcessTransport>();
  auto second = MakeRefCounted<InProcessTransport>();
  first->SetPeer(second);
  second->SetPeer(first);
  *new_transport0 = Object::ReleaseAsHandle(std::move(first));
  *new_transport1 = Object::ReleaseAsHandle(std::move(second));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API ActivateTransport(IpczDriverHandle driver_transport,
                                      IpczHandle transport,
                                      IpczTransportActivityHandler handler,
                                      uint32_t flags,
                                      const void* options) {
  return InProcessTransport::FromHandle(driver_transport)
      ->Activate(transport, handler);
}

IpczResult IPCZ_API DeactivateTransport(IpczDriverHandle driver_transport,
                                        uint32_t flags,
                                        const void* options) {
  InProcessTransport::FromHandle(driver_transport)->Deactivate();
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API Transmit(IpczDriverHandle driver_transport,
                             const void* data,
                             size_t num_bytes,
                             const IpczDriverHandle* handles,
                             size_t num_handles,
                             uint32_t flags,
                             const void* options) {
  return InProcessTransport::FromHandle(driver_transport)
      ->Transmit(absl::MakeSpan(static_cast<const uint8_t*>(data), num_bytes),
                 absl::MakeSpan(handles, num_handles));
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
                                    void** address,
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
