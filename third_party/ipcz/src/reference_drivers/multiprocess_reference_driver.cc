// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/multiprocess_reference_driver.h"

#include <cstring>
#include <limits>
#include <memory>
#include <tuple>
#include <utility>

#include "ipcz/ipcz.h"
#include "reference_drivers/file_descriptor.h"
#include "reference_drivers/memfd_memory.h"
#include "reference_drivers/object.h"
#include "reference_drivers/random.h"
#include "reference_drivers/socket_transport.h"
#include "reference_drivers/wrapped_file_descriptor.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "util/ref_counted.h"
#include "util/safe_math.h"

namespace ipcz::reference_drivers {

namespace {

// A transport implementation based on a SocketTransport.
class MultiprocessTransport
    : public ObjectImpl<MultiprocessTransport, Object::kTransport> {
 public:
  explicit MultiprocessTransport(Ref<SocketTransport> transport)
      : transport_(std::move(transport)) {}
  MultiprocessTransport(const MultiprocessTransport&) = delete;
  MultiprocessTransport& operator=(const MultiprocessTransport&) = delete;

  void Activate(IpczHandle transport,
                IpczTransportActivityHandler activity_handler) {
    was_activated_ = true;
    ipcz_transport_ = transport;
    activity_handler_ = activity_handler;

    absl::MutexLock lock(&transport_mutex_);
    transport_->Activate(
        [transport = WrapRefCounted(this)](SocketTransport::Message message) {
          return transport->OnMessage(message);
        },
        [transport = WrapRefCounted(this)]() { transport->OnError(); });
  }

  void Deactivate() {
    Ref<SocketTransport> transport;
    {
      absl::MutexLock lock(&transport_mutex_);
      transport = std::move(transport_);
    }

    transport->Deactivate([ipcz_transport = ipcz_transport_,
                           activity_handler = activity_handler_] {
      if (activity_handler) {
        activity_handler(ipcz_transport, nullptr, 0, nullptr, 0,
                         IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED, nullptr);
      }
    });
  }

  IpczResult Transmit(absl::Span<const uint8_t> data,
                      absl::Span<const IpczDriverHandle> handles) {
    std::vector<FileDescriptor> descriptors(handles.size());
    for (size_t i = 0; i < handles.size(); ++i) {
      ABSL_ASSERT(Object::FromHandle(handles[i])->type() ==
                  Object::kFileDescriptor);
      descriptors[i] =
          WrappedFileDescriptor::TakeFromHandle(handles[i])->TakeDescriptor();
    }

    {
      absl::MutexLock lock(&transport_mutex_);
      if (transport_) {
        transport_->Send({data, absl::MakeSpan(descriptors)});
        return IPCZ_RESULT_OK;
      }
    }

    return IPCZ_RESULT_OK;
  }

  FileDescriptor TakeDescriptor() {
    ABSL_ASSERT(!was_activated_);
    absl::MutexLock lock(&transport_mutex_);
    return transport_->TakeDescriptor();
  }

 private:
  ~MultiprocessTransport() override = default;

  bool OnMessage(const SocketTransport::Message& message) {
    std::vector<IpczDriverHandle> handles(message.descriptors.size());
    for (size_t i = 0; i < handles.size(); ++i) {
      handles[i] =
          Object::ReleaseAsHandle(MakeRefCounted<WrappedFileDescriptor>(
              std::move(message.descriptors[i])));
    }

    ABSL_ASSERT(activity_handler_);
    IpczResult result = activity_handler_(
        ipcz_transport_, message.data.data(), message.data.size(),
        handles.data(), handles.size(), IPCZ_NO_FLAGS, nullptr);
    return result == IPCZ_RESULT_OK || result == IPCZ_RESULT_UNIMPLEMENTED;
  }

  void OnError() {
    activity_handler_(ipcz_transport_, nullptr, 0, nullptr, 0,
                      IPCZ_TRANSPORT_ACTIVITY_ERROR, nullptr);
  }

  IpczHandle ipcz_transport_ = IPCZ_INVALID_HANDLE;
  IpczTransportActivityHandler activity_handler_;
  bool was_activated_ = false;

  absl::Mutex transport_mutex_;
  Ref<SocketTransport> transport_ ABSL_GUARDED_BY(transport_mutex_);
};

class MultiprocessMemoryMapping
    : public ObjectImpl<MultiprocessMemoryMapping, Object::kMapping> {
 public:
  explicit MultiprocessMemoryMapping(MemfdMemory::Mapping mapping)
      : mapping_(std::move(mapping)) {}

  void* address() const { return mapping_.base(); }

 private:
  ~MultiprocessMemoryMapping() override = default;

  const MemfdMemory::Mapping mapping_;
};

class MultiprocessMemory
    : public ObjectImpl<MultiprocessMemory, Object::kMemory> {
 public:
  explicit MultiprocessMemory(size_t num_bytes) : memory_(num_bytes) {}
  MultiprocessMemory(FileDescriptor descriptor, size_t num_bytes)
      : memory_(std::move(descriptor), num_bytes) {}

  size_t size() const { return memory_.size(); }

  Ref<MultiprocessMemory> Clone() {
    return MakeRefCounted<MultiprocessMemory>(memory_.Clone().TakeDescriptor(),
                                              memory_.size());
  }

  Ref<MultiprocessMemoryMapping> Map() {
    return MakeRefCounted<MultiprocessMemoryMapping>(memory_.Map());
  }

  FileDescriptor TakeDescriptor() { return memory_.TakeDescriptor(); }

 private:
  ~MultiprocessMemory() override = default;

  MemfdMemory memory_;
};

// Header at the start of every driver object serialized by this driver.
struct IPCZ_ALIGN(8) SerializedObjectHeader {
  // Enumeration indicating which type of driver object this is.
  Object::Type type;

  // For a memory object, the size of the underlying region. Ignored otherwise.
  uint32_t memory_size;
};

IpczResult IPCZ_API Close(IpczDriverHandle handle,
                          uint32_t flags,
                          const void* options) {
  Ref<Object> object = Object::TakeFromHandle(handle);
  if (!object) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  object->Close();
  return IPCZ_RESULT_OK;
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

  // First compute the serialized dimensions.
  size_t required_num_bytes = sizeof(SerializedObjectHeader);
  size_t required_num_handles;
  switch (object->type()) {
    case Object::kTransport:
    case Object::kMemory:
      required_num_handles = 1;
      break;

    default:
      return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const size_t data_capacity = num_bytes ? *num_bytes : 0;
  const size_t handle_capacity = num_handles ? *num_handles : 0;
  if (num_bytes) {
    *num_bytes = required_num_bytes;
  }
  if (num_handles) {
    *num_handles = required_num_handles;
  }
  const bool need_more_space = data_capacity < required_num_bytes ||
                               handle_capacity < required_num_handles;
  if (need_more_space) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  auto& header = *reinterpret_cast<volatile SerializedObjectHeader*>(data);
  header.type = object->type();
  header.memory_size = 0;

  switch (object->type()) {
    case Object::kTransport:
      handles[0] = WrappedFileDescriptor::Create(
          MultiprocessTransport::TakeFromObject(object)->TakeDescriptor());
      break;

    case Object::kMemory: {
      auto memory = MultiprocessMemory::TakeFromObject(object);
      header.memory_size = checked_cast<uint32_t>(memory->size());
      handles[0] = WrappedFileDescriptor::Create(memory->TakeDescriptor());
      break;
    }

    default:
      return IPCZ_RESULT_INVALID_ARGUMENT;
  }

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
  const auto& header =
      *static_cast<const volatile SerializedObjectHeader*>(data);
  if (num_bytes < sizeof(header)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  Ref<Object> object;
  switch (header.type) {
    case Object::kTransport:
      if (num_handles == 1) {
        object = MakeRefCounted<MultiprocessTransport>(
            MakeRefCounted<SocketTransport>(
                WrappedFileDescriptor::UnwrapHandle(handles[0])));
      }
      break;

    case Object::kMemory:
      if (num_handles == 1) {
        object = MakeRefCounted<MultiprocessMemory>(
            WrappedFileDescriptor::UnwrapHandle(handles[0]),
            header.memory_size);
      }
      break;

    default:
      break;
  }

  if (!object) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  *driver_handle = Object::ReleaseAsHandle(std::move(object));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API CreateTransports(IpczDriverHandle transport0,
                                     IpczDriverHandle transport1,
                                     uint32_t flags,
                                     const void* options,
                                     IpczDriverHandle* new_transport0,
                                     IpczDriverHandle* new_transport1) {
  auto [first_socket, second_socket] = SocketTransport::CreatePair();
  auto first = MakeRefCounted<MultiprocessTransport>(std::move(first_socket));
  auto second = MakeRefCounted<MultiprocessTransport>(std::move(second_socket));
  *new_transport0 = Object::ReleaseAsHandle(std::move(first));
  *new_transport1 = Object::ReleaseAsHandle(std::move(second));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API
ActivateTransport(IpczDriverHandle transport,
                  IpczHandle listener,
                  IpczTransportActivityHandler activity_handler,
                  uint32_t flags,
                  const void* options) {
  MultiprocessTransport::FromHandle(transport)->Activate(listener,
                                                         activity_handler);
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API DeactivateTransport(IpczDriverHandle transport,
                                        uint32_t flags,
                                        const void* options) {
  MultiprocessTransport::FromHandle(transport)->Deactivate();
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API Transmit(IpczDriverHandle transport,
                             const void* data,
                             size_t num_bytes,
                             const IpczDriverHandle* handles,
                             size_t num_handles,
                             uint32_t flags,
                             const void* options) {
  return MultiprocessTransport::FromHandle(transport)->Transmit(
      absl::MakeSpan(static_cast<const uint8_t*>(data), num_bytes),
      absl::MakeSpan(handles, num_handles));
}

IpczResult IPCZ_API ReportBadTransportActivity(IpczDriverHandle transport,
                                               uintptr_t context,
                                               uint32_t flags,
                                               const void* options) {
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API AllocateSharedMemory(size_t num_bytes,
                                         uint32_t flags,
                                         const void* options,
                                         IpczDriverHandle* driver_memory) {
  auto memory =
      MakeRefCounted<MultiprocessMemory>(static_cast<size_t>(num_bytes));
  *driver_memory = Object::ReleaseAsHandle(std::move(memory));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API DuplicateSharedMemory(IpczDriverHandle driver_memory,
                                          uint32_t flags,
                                          const void* options,
                                          IpczDriverHandle* new_driver_memory) {
  auto memory = MultiprocessMemory::FromHandle(driver_memory)->Clone();
  *new_driver_memory = Object::ReleaseAsHandle(std::move(memory));
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

  info->region_num_bytes = static_cast<MultiprocessMemory*>(object)->size();
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API MapSharedMemory(IpczDriverHandle driver_memory,
                                    uint32_t flags,
                                    const void* options,
                                    volatile void** address,
                                    IpczDriverHandle* driver_mapping) {
  auto mapping = MultiprocessMemory::FromHandle(driver_memory)->Map();
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

const IpczDriver kMultiprocessReferenceDriver = {
    sizeof(kMultiprocessReferenceDriver),
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

IpczDriverHandle CreateMultiprocessTransport(Ref<SocketTransport> transport) {
  return Object::ReleaseAsHandle(
      MakeRefCounted<MultiprocessTransport>(std::move(transport)));
}

FileDescriptor TakeMultiprocessTransportDescriptor(IpczDriverHandle transport) {
  Ref<MultiprocessTransport> released_transport =
      MultiprocessTransport::TakeFromHandle(transport);
  return released_transport->TakeDescriptor();
}

}  // namespace ipcz::reference_drivers
