// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/393091624): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "fuzzer/fuzzer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "fuzzer/driver.h"
#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "util/ref_counted.h"

namespace ipcz::fuzzer {

namespace {

Fuzzer* fuzzer_instance;

class Transmission {
 public:
  Transmission(absl::Span<const uint8_t> data,
               absl::Span<const IpczDriverHandle> handles)
      : data_(data.begin(), data.end()),
        handles_(handles.begin(), handles.end()) {}

  Transmission(Transmission&& other) {
    data_.swap(other.data_);
    handles_.swap(other.handles_);
  }

  Transmission& operator=(Transmission&& other) {
    Reset();
    data_.swap(other.data_);
    handles_.swap(other.handles_);
    return *this;
  }

  ~Transmission() { Reset(); }

  void Dispatch(Fuzzer& fuzzer,
                IpczHandle listener,
                IpczTransportActivityHandler handler) {
    fuzzer.DispatchTransportActivity(listener, handler, data_, handles_);
    handles_.clear();
  }

 private:
  void Reset() {
    for (auto handle : handles_) {
      kDriver.Close(handle, IPCZ_NO_FLAGS, nullptr);
    }
    data_.clear();
    handles_.clear();
  }

  std::vector<uint8_t> data_;
  std::vector<IpczDriverHandle> handles_;
};

}  // namespace

// A trivial heap-based "shared memory" implementation.
class Fuzzer::Memory : public DriverObjectImpl<DriverObject::Type::kMemory> {
 public:
  explicit Memory(size_t num_bytes) : bytes_(num_bytes) {}
  absl::Span<uint8_t> bytes() { return absl::MakeSpan(bytes_); }

 private:
  ~Memory() override = default;
  std::vector<uint8_t> bytes_;
};

// Owns the activity queues and other state pertaining to both ends of an
// entangled driver transport pair. Every Transport object references one side
// of a TransportBackend.
class Fuzzer::TransportBackend : public RefCounted<Fuzzer::TransportBackend> {
 public:
  TransportBackend(Fuzzer& fuzzer, bool may_use_broker_relay)
      : fuzzer_(fuzzer), may_use_broker_relay_(may_use_broker_relay) {}

  // Indicates whether or not this backend's endpoints may force object
  // brokering upon transmission, in order to provide coverage of that code
  // path within ipcz. Only set on transports that go between two non-broker
  // nodes, and used exclusively in Fuzzer::Serialize().
  bool may_use_broker_relay() const { return may_use_broker_relay_; }

  // Closes one side (0 or 1) of this backend.
  void Close(int side) {
    auto& e = endpoints_[side];
    e.is_closed = true;
    e.handler = nullptr;
    e.activity.clear();
    if (!endpoints_[1 - side].is_closed) {
      endpoints_[1 - side].activity.push_back(Error{});
    }
  }

  // Enables a side (0 or 1) to dispatch activity during subsequent flushes.
  void Activate(int side,
                IpczHandle listener,
                IpczTransportActivityHandler handler) {
    auto& e = endpoints_[side];
    e.listener = listener;
    e.handler = handler;
  }

  // Marks a side (0 or 1) as deactivated, which will cause its handler to be
  // reset and activity to stop dispatching at the end of the next flush.
  void Deactivate(int side) { endpoints_[side].is_deactivated = true; }

  // Enqueues a transmission in the opposite side's queue.
  void EnqueueTransmissionFrom(int side,
                               absl::Span<const uint8_t> data,
                               absl::Span<const IpczDriverHandle> handles) {
    if (!endpoints_[1 - side].is_closed) {
      endpoints_[1 - side].activity.push_back(Transmission(data, handles));
    }
  }

  // Dispatches all pending transmissions and errors to active endpoints, and
  // finishes deactivating any which are marked for deactivation. Returns true
  // iff any interesting work was done during the flush.
  bool FlushActivity() {
    bool did_work = false;
    did_work |= FlushEndpoint(endpoints_[0]);
    did_work |= FlushEndpoint(endpoints_[1]);
    did_work |= MaybeFinalizeDeactivation(endpoints_[0]);
    did_work |= MaybeFinalizeDeactivation(endpoints_[1]);
    return did_work;
  }

 private:
  friend class RefCounted<TransportBackend>;

  struct Error {};
  using Activity = absl::variant<Transmission, Error>;

  struct Endpoint {
    bool is_closed = false;
    bool is_deactivated = false;
    std::vector<Activity> activity;
    IpczHandle listener = IPCZ_INVALID_HANDLE;
    IpczTransportActivityHandler handler = nullptr;
  };

  ~TransportBackend() = default;

  bool FlushEndpoint(Endpoint& e) {
    if (!e.handler || e.activity.empty()) {
      // Either not activated yet, or already closed, or simply idle.
      return false;
    }

    std::vector<Activity> activity;
    activity.swap(e.activity);
    for (auto& entry : activity) {
      if (absl::holds_alternative<Error>(entry)) {
        e.handler(e.listener, nullptr, 0, nullptr, 0,
                  IPCZ_TRANSPORT_ACTIVITY_ERROR, nullptr);
      } else {
        absl::get<Transmission>(entry).Dispatch(fuzzer_, e.listener, e.handler);
      }
    }
    return true;
  }

  bool MaybeFinalizeDeactivation(Endpoint& e) {
    if (!e.is_deactivated || !e.handler) {
      return false;
    }

    const auto listener = std::exchange(e.listener, IPCZ_INVALID_HANDLE);
    const auto handler = std::exchange(e.handler, nullptr);
    handler(listener, nullptr, 0, nullptr, 0,
            IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED, nullptr);
    return true;
  }

  Fuzzer& fuzzer_;
  const bool may_use_broker_relay_;
  std::array<Endpoint, 2> endpoints_ = {};
};

// One side (0 or 1) of a TransportBackend.
class Fuzzer::Transport
    : public DriverObjectImpl<DriverObject::Type::kTransport> {
 public:
  Transport(Ref<TransportBackend> backend, int side)
      : backend_(std::move(backend)), side_(side) {}

  TransportBackend& backend() { return *backend_; }

  IpczResult Close() override {
    backend_->Close(side_);
    return IPCZ_RESULT_OK;
  }

  void Activate(IpczHandle listener, IpczTransportActivityHandler handler) {
    backend_->Activate(side_, listener, handler);
  }

  void Deactivate() { backend_->Deactivate(side_); }

  void Transmit(absl::Span<const uint8_t> data,
                absl::Span<const IpczDriverHandle> handles) {
    backend_->EnqueueTransmissionFrom(side_, data, handles);
  }

 private:
  ~Transport() override = default;

  const Ref<TransportBackend> backend_;
  const int side_;
};

Fuzzer::Fuzzer() {
  ABSL_HARDENING_ASSERT(fuzzer_instance == nullptr);
  fuzzer_instance = this;
}

Fuzzer::Fuzzer(const FuzzConfig& config,
               absl::Span<const unsigned char> fuzz_data)
    : config_(config), fuzz_data_(fuzz_data) {
  ABSL_HARDENING_ASSERT(fuzzer_instance == nullptr);
  fuzzer_instance = this;
}

Fuzzer::~Fuzzer() {
  FlushTransports();

  memory_objects_.clear();
  driver_objects_.clear();
  transport_backends_.clear();

  ABSL_HARDENING_ASSERT(fuzzer_instance == this);
  fuzzer_instance = nullptr;
}

Fuzzer& Fuzzer::Get() {
  ABSL_HARDENING_ASSERT(fuzzer_instance);
  return *fuzzer_instance;
}

void Fuzzer::FlushTransports() {
  bool did_work;
  do {
    did_work = false;

    // NOTE: Flushing may mutate the `transport_backends_`, so it's important
    // not to iterate in-place. Since we'll loop until no new work is done, it's
    // fine to iterate over a potentially stale copy.
    const auto backends = transport_backends_;
    for (auto& backend : backends) {
      did_work |= backend->FlushActivity();
    }
  } while (did_work);
}

IpczDriverHandle Fuzzer::RegisterDriverObject(Ref<DriverObject> object) {
  const IpczDriverHandle handle = next_driver_handle_++;
  driver_objects_[handle] = std::move(object);
  return handle;
}

void Fuzzer::DispatchTransportActivity(
    IpczHandle listener,
    IpczTransportActivityHandler handler,
    absl::Span<const uint8_t> data,
    absl::Span<const IpczDriverHandle> handles) {
  std::vector<uint8_t> modified_data;
  const int this_transmission = current_transmission_++;
  if (this_transmission == config_.transmission_to_fuzz &&
      !fuzz_data_.empty()) {
    if (config_.fuzz_link_memory) {
      InjectFuzzDataIntoMemory();
    } else {
      const size_t offset = 4ul * config_.target_offset;
      modified_data.resize(std::max(fuzz_data_.size() + offset, data.size()));
      std::copy(data.begin(), data.end(), modified_data.begin());
      std::copy(fuzz_data_.begin(), fuzz_data_.end(),
                modified_data.begin() + offset);
      data = absl::MakeSpan(modified_data);
    }
  }

  const IpczResult result =
      handler(listener, data.data(), data.size(), handles.data(),
              handles.size(), IPCZ_NO_FLAGS, nullptr);
  if (result != IPCZ_RESULT_OK && result != IPCZ_RESULT_UNIMPLEMENTED) {
    handler(listener, nullptr, 0, nullptr, 0, IPCZ_TRANSPORT_ACTIVITY_ERROR,
            nullptr);
  }
}

IpczResult Fuzzer::Close(IpczDriverHandle handle) {
  auto it = driver_objects_.find(handle);
  if (it == driver_objects_.end()) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  Ref<DriverObject> object = std::move(it->second);
  driver_objects_.erase(it);
  return object->Close();
}

IpczResult Fuzzer::Serialize(IpczDriverHandle handle,
                             IpczDriverHandle transport_handle,
                             volatile void* data,
                             size_t* num_bytes,
                             IpczDriverHandle* handles,
                             size_t* num_handles) {
  const Ref<DriverObject> object = GetDriverObject(handle);
  if (!object) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  // Ensure we exercise object relay through a broker sometimes, since this
  // covers a few dedicated node message types. Arbitrarily force it for odd-
  // numbered handles whenever the transport allows.
  if (handle & 1) {
    if (transport_handle == IPCZ_INVALID_DRIVER_HANDLE) {
      return IPCZ_RESULT_ABORTED;
    }
    const auto transport = GetDriverObjectAs<Transport>(transport_handle);
    if (!transport) {
      return IPCZ_RESULT_INVALID_ARGUMENT;
    }
    if (transport->backend().may_use_broker_relay()) {
      return IPCZ_RESULT_PERMISSION_DENIED;
    }
  }

  // Handle serialization is otherwise a trivial pass-through.
  const size_t capacity = num_handles ? *num_handles : 0;
  if (num_handles) {
    *num_handles = 1;
  }
  if (capacity < 1) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  handles[0] = handle;
  return IPCZ_RESULT_OK;
}

IpczResult Fuzzer::Deserialize(const volatile void* data,
                               size_t num_bytes,
                               const IpczDriverHandle* handles,
                               size_t num_handles,
                               IpczDriverHandle transport,
                               IpczDriverHandle* handle) {
  if (num_handles != 1) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }
  if (!GetDriverObject(handles[0])) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }
  *handle = handles[0];
  return IPCZ_RESULT_OK;
}

IpczResult Fuzzer::CreateTransports(IpczDriverHandle* transport0,
                                    IpczDriverHandle* transport1,
                                    bool may_use_broker_relay) {
  auto backend = MakeRefCounted<TransportBackend>(*this, may_use_broker_relay);
  *transport0 = RegisterDriverObject(MakeRefCounted<Transport>(backend, 0));
  *transport1 = RegisterDriverObject(MakeRefCounted<Transport>(backend, 1));
  transport_backends_.push_back(std::move(backend));
  return IPCZ_RESULT_OK;
}

IpczResult Fuzzer::ActivateTransport(IpczDriverHandle transport_handle,
                                     IpczHandle listener,
                                     IpczTransportActivityHandler handler) {
  if (auto transport = GetDriverObjectAs<Transport>(transport_handle)) {
    transport->Activate(listener, handler);
    return IPCZ_RESULT_OK;
  }
  return IPCZ_RESULT_INVALID_ARGUMENT;
}

IpczResult Fuzzer::DeactivateTransport(IpczDriverHandle transport_handle) {
  if (auto transport = GetDriverObjectAs<Transport>(transport_handle)) {
    transport->Deactivate();
    return IPCZ_RESULT_OK;
  }
  return IPCZ_RESULT_INVALID_ARGUMENT;
}

IpczResult Fuzzer::Transmit(IpczDriverHandle transport_handle,
                            const void* data,
                            size_t num_bytes,
                            const IpczDriverHandle* handles,
                            size_t num_handles) {
  if (auto transport = GetDriverObjectAs<Transport>(transport_handle)) {
    transport->Transmit(
        absl::Span(static_cast<const uint8_t*>(data), num_bytes),
        absl::Span(handles, num_handles));
    return IPCZ_RESULT_OK;
  }
  return IPCZ_RESULT_INVALID_ARGUMENT;
}

IpczResult Fuzzer::AllocateSharedMemory(size_t num_bytes,
                                        IpczDriverHandle* memory) {
  auto new_memory = MakeRefCounted<Memory>(num_bytes);
  *memory = RegisterDriverObject(new_memory);
  memory_objects_.push_back(std::move(new_memory));
  return IPCZ_RESULT_OK;
}

IpczResult Fuzzer::GetSharedMemoryInfo(IpczDriverHandle memory_handle,
                                       IpczSharedMemoryInfo* info) {
  if (auto memory = GetDriverObjectAs<Memory>(memory_handle)) {
    info->region_num_bytes = memory->bytes().size();
    return IPCZ_RESULT_OK;
  }
  return IPCZ_RESULT_INVALID_ARGUMENT;
}

IpczResult Fuzzer::DuplicateSharedMemory(IpczDriverHandle memory_handle,
                                         IpczDriverHandle* duplicate) {
  if (auto memory = GetDriverObjectAs<Memory>(memory_handle)) {
    *duplicate = RegisterDriverObject(memory);
    return IPCZ_RESULT_OK;
  }
  return IPCZ_RESULT_INVALID_ARGUMENT;
}

IpczResult Fuzzer::MapSharedMemory(IpczDriverHandle memory_handle,
                                   volatile void** address,
                                   IpczDriverHandle* mapping) {
  if (auto memory = GetDriverObjectAs<Memory>(memory_handle)) {
    *address = const_cast<volatile uint8_t*>(memory->bytes().data());
    *mapping = RegisterDriverObject(memory);
    return IPCZ_RESULT_OK;
  }
  return IPCZ_RESULT_INVALID_ARGUMENT;
}

IpczResult Fuzzer::GenerateRandomBytes(absl::Span<uint8_t> bytes) {
  // Our "random" bytes are deterministic and not random. This ensures that
  // node names (and ultimately, the contents of well-formed messages throughout
  // fuzzer testcases) are consistent across runs.
  for (uint8_t& byte : bytes) {
    byte = next_random_byte_++;
  }
  return IPCZ_RESULT_OK;
}

void Fuzzer::InjectFuzzDataIntoMemory() {
  for (const auto& memory : memory_objects_) {
    const auto bytes = memory->bytes();
    const size_t offset = (4ul * config_.target_offset) % bytes.size();
    const size_t size = std::min(bytes.size() - offset, fuzz_data_.size());
    std::copy(fuzz_data_.begin(), fuzz_data_.begin() + size,
              bytes.begin() + offset);
  }
}

Ref<DriverObject> Fuzzer::GetDriverObject(IpczDriverHandle handle) const {
  auto it = driver_objects_.find(handle);
  if (it == driver_objects_.end()) {
    return nullptr;
  }

  return it->second;
}

}  // namespace ipcz::fuzzer
