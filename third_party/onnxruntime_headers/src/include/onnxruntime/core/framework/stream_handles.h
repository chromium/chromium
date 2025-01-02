// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <functional>
#include <unordered_map>
#include "core/framework/allocator.h"
#include "core/framework/ortdevice.h"
#include "core/common/status.h"

namespace onnxruntime {
class IExecutionProvider;
// this opaque handle could be anything the target device generated.
// it could be a cuda event, or a npu notification implementation
using NotificationHandle = void*;
// it can be either a cuda stream, or even nullptr for device doesn't have stream support like cpu.
using StreamHandle = void*;

namespace synchronize {
class Notification;
}

// a stream abstraction which hold an opaque handle, and a reference to which OrtDevice instance this stream belong to.
// it need to be OrtDevice instance as we might have different stream on different OrtDevice with same type.
// i.e. different cuda stream on different GPU.
class Stream {
 public:
  Stream(StreamHandle h, const OrtDevice& d) : handle_(h), device_(d) {}

  virtual ~Stream() = default;
  virtual std::unique_ptr<synchronize::Notification> CreateNotification(size_t /*num_consumers*/) {
    return {};
  };
  // block the host thread until all the tasks in the stream finished.
  virtual void Flush() {};
  // The framework may reuse the stream instance for multiple iterations.
  // This is the API that provide a chance to let the device stream cleanup
  // resource at the end of a iteration.
  virtual Status CleanUpOnRunEnd() { return Status::OK(); };

  StreamHandle GetHandle() const { return handle_; }

  const OrtDevice& GetDevice() const { return device_; }

  // We use the timestamp based vector clocks to optimize the resource sharing
  // between different streams.
  // Each stream maintain following data structure:
  // 1. Current timestamp
  // 2. A lookup table that for a given stream, what is its timestamp when the
  //    last synchronization happened with current stream.
  // 3. When a notification is activated, it take a snapshot of current stream's
  //    lookup table.
  // 4. When synchronization happened (current stream wait on a notification),
  //    update its lookup table with the table snapshot in notification.
  // The memory reusing strategy is:
  // A kernel in current stream is safe to reuse another stream's memory chunk
  // as long as the reused chunk's timestamp is less than the last synchronized
  // timestamp recorded in the lookup table.

  // Get the current timestamp
  uint64_t GetCurrentTimestamp() const { return timestamp_; }

  // return the timestamp when the last synchronization happened between target stream and current stream.
  // return 0 if no synchronization happened.
  // if target_stream is nullptr, it means it is a sequence running on device doesn't support Stream (i.e. CPU)
  // we can safely return 0 in that case to save a lookup.
  uint64_t GetLastSyncTimestampWithTargetStream(Stream* target_stream) const {
    if (!target_stream)
      return 0;
    auto it = other_stream_clock_.find(target_stream);
    return it == other_stream_clock_.end() ? 0 : it->second;
  }

  // make a copy of the current stream lookup table.
  // this is used to create a snapshot of the stream lookup table in notification.
  void CloneCurrentStreamSyncTable(std::unordered_map<Stream*, uint64_t>& output) const {
    output.reserve(other_stream_clock_.size());
    output.insert(other_stream_clock_.begin(), other_stream_clock_.end());
  }

  // bump the current timestamp
  // When a notification get activated, bump the snapshot in its owner.
  // Stream is not shared across threads, BumpTimeStampAndReturn will only be invoked on the current thread
  // where the stream is executed on, so there is no race condition.
  uint64_t BumpTimeStampAndReturn() {
    return ++timestamp_;
  }

  // update the stream lookup table with the snapshot saved in notification.
  void UpdateStreamClock(const std::unordered_map<Stream*, uint64_t>& clock) {
    for (const auto& kv : clock) {
      auto ret = other_stream_clock_.insert(kv);
      if (!ret.second) {
        ret.first->second = std::max(ret.first->second, kv.second);
      }
    }
  }

  virtual void* GetResource(int /*version*/, int /*id*/) const {
    return nullptr;
  }

  virtual WaitNotificationFn GetWaitNotificationFn() const { return nullptr; }

 private:
  StreamHandle handle_;
  const OrtDevice& device_;
  uint64_t timestamp_{0};
  // TODO: use inline container.
  // currently this class is header only, but abseil doesn't compile with nvcc
  // we need to add new symbol to provider_bridge and hide abseil from the header.
  std::unordered_map<Stream*, uint64_t> other_stream_clock_{};

  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(Stream);
};

namespace synchronize {
// An abstraction used for synchronization between streams. See its concrete subclass (CudaNotification, etc.) how the activate
// and wait works for a specific stream
class Notification {
 public:
  explicit Notification(Stream& s) : stream_(s) {}
  virtual ~Notification() = default;

  // this api will perform three operations:
  // 1. activate the notification on device, for example, record an event on GPU.
  // 2. take a snapshot of the timestamp lookup table in current stream.
  // 3. bump the timestamp for current stream.
  void ActivateAndUpdate() {
    Activate();
    stream_.CloneCurrentStreamSyncTable(stream_clock_);
    stream_clock_[&stream_] = stream_.BumpTimeStampAndReturn();
  }

  // return the timestamp lookup table saved in the notification.
  const std::unordered_map<Stream*, uint64_t>& GetStreamSyncTable() {
    return stream_clock_;
  }

 protected:
  virtual void Activate() = 0;
  // which stream create this notification.
  Stream& stream_;
  // TODO: use inline container.
  // currently this class is header only, but abseil doesn't compile with nvcc
  // we need to add new symbol to provider_bridge and hide abseil from the header.
  std::unordered_map<Stream*, uint64_t> stream_clock_{};
};
}  // namespace synchronize

// the definition for the handle for stream commands
// EP can register the handle to the executor.
// in the POC, just use primitive function pointer
// TODO: use a better way to dispatch handles.
using CreateStreamFn = std::function<std::unique_ptr<Stream>(const OrtDevice&)>;

// an interface of a simple registry which hold the handles EP registered.
// make it interface so we can pass it through shared library based execution providers
class IStreamCommandHandleRegistry {
 public:
  virtual ~IStreamCommandHandleRegistry() = default;
  // Wait is a little special as we need to consider the source stream the notification generated, and the stream we are waiting.
  // i.e., for an cuda event what notify the memory copy, it could be wait on a CPU stream, or on another cuda stream.
  [[nodiscard]] virtual WaitNotificationFn GetWaitHandle(OrtDevice::DeviceType notification_ower_device_type,
                                                         OrtDevice::DeviceType executor_device_type) const = 0;
  // Get the stream creation function registered on the given device type.
  [[nodiscard]] virtual CreateStreamFn GetCreateStreamFn(OrtDevice::DeviceType execution_device_type) const = 0;
  // register a wait methond which will be invoked when we wait a notification (created by 'notification_device_type' device) on a stream at 'device_type' device.
  virtual void RegisterWaitFn(OrtDevice::DeviceType notification_device_type,
                              OrtDevice::DeviceType device_type,
                              WaitNotificationFn fn) = 0;
  // register a handle about how to create stream on given device type.
  virtual void RegisterCreateStreamFn(OrtDevice::DeviceType device_type, CreateStreamFn f) = 0;
};

}  // namespace onnxruntime
