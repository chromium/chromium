// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/async_reference_driver.h"

#include <cstdint>
#include <memory>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "ipcz/ipcz.h"
#include "reference_drivers/object.h"
#include "reference_drivers/single_process_reference_driver_base.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz::reference_drivers {

namespace {

// The driver transport implementation for the async reference driver. Each
// AsyncTransport holds a direct reference to its peer, and transmissions are
// tasks posted to the peer's task queue. Task are run on a dedicated background
// thread for each transport.
class AsyncTransport : public ObjectImpl<AsyncTransport, Object::kTransport> {
 public:
  enum class NodeType {
    kBroker,
    kNonBroker,
  };

  struct TransportType {
    NodeType local;
    NodeType remote;
  };

  explicit AsyncTransport(const TransportType& type) : type_(type) {}

  NodeType local_type() const { return type_.local; }
  NodeType remote_type() const { return type_.remote; }

  using Pair = std::pair<Ref<AsyncTransport>, Ref<AsyncTransport>>;
  static Pair CreatePair(NodeType node0_type, NodeType node1_type) {
    Pair pair{MakeRefCounted<AsyncTransport>(
                  TransportType{.local = node0_type, .remote = node1_type}),
              MakeRefCounted<AsyncTransport>(
                  TransportType{.local = node1_type, .remote = node0_type})};
    std::tie(pair.second->peer_, pair.first->peer_) = pair;
    return pair;
  }

  void Activate(IpczHandle transport, IpczTransportActivityHandler handler) {
    absl::MutexLock lock(&mutex_);
    transport_ = transport;
    handler_ = handler;
    active_ = true;
    task_thread_ =
        std::make_unique<std::thread>(&RunTaskThread, WrapRefCounted(this));
  }

  void Deactivate() {
    std::unique_ptr<std::thread> task_thread_to_join;
    {
      absl::MutexLock lock(&mutex_);
      // Join the task thread if we're not on it; otherwise detach it.
      // Detachment is safe: the thread owns a ref to `this` as long as it's
      // running, and it will terminate very soon after deactivation.
      ABSL_HARDENING_ASSERT(task_thread_);
      if (task_thread_->get_id() != std::this_thread::get_id()) {
        task_thread_to_join = std::move(task_thread_);
      } else {
        task_thread_->detach();
        task_thread_.reset();
      }
      active_ = false;
      NotifyTaskThread();
    }
    if (task_thread_to_join) {
      task_thread_to_join->join();
    }
  }

  IpczResult Transmit(absl::Span<const uint8_t> data,
                      absl::Span<const IpczDriverHandle> handles) {
    peer_->PostTask({data, handles});
    return IPCZ_RESULT_OK;
  }

  // Object:
  IpczResult Close() override {
    peer_->PostTask(Task{IPCZ_TRANSPORT_ACTIVITY_ERROR});
    peer_.reset();
    return IPCZ_RESULT_OK;
  }

 private:
  class Task {
   public:
    Task(absl::Span<const uint8_t> data,
         absl::Span<const IpczDriverHandle> handles)
        : data_(data.begin(), data.end()),
          handles_(handles.begin(), handles.end()) {}
    explicit Task(IpczTransportActivityFlags flags) : flags_(flags) {}
    Task(Task&&) = default;
    ~Task() {
      for (IpczDriverHandle handle : handles_) {
        Object::TakeFromHandle(handle)->Close();
      }
    }

    IpczResult Run(AsyncTransport& transport) {
      std::vector<IpczDriverHandle> handles = std::move(handles_);
      return transport.Notify(flags_, data_, handles);
    }

   private:
    std::vector<uint8_t> data_;
    std::vector<IpczDriverHandle> handles_;
    IpczTransportActivityFlags flags_ = IPCZ_NO_FLAGS;
  };

  void PostTask(Task task) {
    absl::MutexLock lock(&mutex_);
    tasks_.push_back(std::move(task));
    NotifyTaskThread();
  }

  static void RunTaskThread(Ref<AsyncTransport> transport) {
    transport->RunTasksUntilDeactivation();
    transport->Notify(IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED);
  }

  void RunTasksUntilDeactivation() {
    for (;;) {
      std::vector<Task> tasks;
      absl::Notification* wait_for_task = nullptr;
      {
        absl::MutexLock lock(&mutex_);
        if (!active_) {
          return;
        }
        tasks.swap(tasks_);
        if (tasks.empty()) {
          wait_for_task = wait_for_task_.get();
        }
      }

      if (wait_for_task) {
        wait_for_task->WaitForNotification();
        absl::MutexLock lock(&mutex_);
        wait_for_task_ = std::make_unique<absl::Notification>();
        continue;
      }

      for (Task& task : tasks) {
        const IpczResult result = task.Run(*this);
        if (result != IPCZ_RESULT_OK && result != IPCZ_RESULT_UNIMPLEMENTED) {
          Notify(IPCZ_TRANSPORT_ACTIVITY_ERROR);
          return;
        }
      }
    }
  }

  IpczResult Notify(IpczTransportActivityFlags flags,
                    absl::Span<const uint8_t> data = {},
                    absl::Span<const IpczDriverHandle> handles = {}) {
    return handler_(transport_, data.data(), data.size(), handles.data(),
                    handles.size(), flags, nullptr);
  }

  void NotifyTaskThread() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (wait_for_task_ && !wait_for_task_->HasBeenNotified()) {
      wait_for_task_->Notify();
    }
  }

  const TransportType type_;

  Ref<AsyncTransport> peer_;
  IpczHandle transport_ = IPCZ_INVALID_HANDLE;
  IpczTransportActivityHandler handler_;

  absl::Mutex mutex_;
  bool active_ ABSL_GUARDED_BY(mutex_) = false;
  std::unique_ptr<std::thread> task_thread_ ABSL_GUARDED_BY(mutex_);
  std::vector<Task> tasks_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<absl::Notification> wait_for_task_ ABSL_GUARDED_BY(mutex_) =
      std::make_unique<absl::Notification>();
};

IpczResult IPCZ_API CreateTransports(IpczDriverHandle transport0,
                                     IpczDriverHandle transport1,
                                     uint32_t,
                                     const void*,
                                     IpczDriverHandle* new_transport0,
                                     IpczDriverHandle* new_transport1) {
  auto* target0 = AsyncTransport::FromHandle(transport0);
  auto* target1 = AsyncTransport::FromHandle(transport1);
  auto [first, second] = AsyncTransport::CreatePair(target0->remote_type(),
                                                    target1->remote_type());
  *new_transport0 = Object::ReleaseAsHandle(std::move(first));
  *new_transport1 = Object::ReleaseAsHandle(std::move(second));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API ActivateTransport(IpczDriverHandle transport,
                                      IpczHandle listener,
                                      IpczTransportActivityHandler handler,
                                      uint32_t,
                                      const void*) {
  AsyncTransport::FromHandle(transport)->Activate(listener, handler);
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API DeactivateTransport(IpczDriverHandle transport,
                                        uint32_t,
                                        const void*) {
  AsyncTransport::FromHandle(transport)->Deactivate();
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API Transmit(IpczDriverHandle transport,
                             const void* data,
                             size_t num_bytes,
                             const IpczDriverHandle* handles,
                             size_t num_handles,
                             uint32_t,
                             const void*) {
  AsyncTransport::FromHandle(transport)->Transmit(
      {static_cast<const uint8_t*>(data), num_bytes}, {handles, num_handles});
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API SerializeWithForcedBrokering(IpczDriverHandle handle,
                                                 IpczDriverHandle transport,
                                                 uint32_t flags,
                                                 const void* options,
                                                 volatile void* data,
                                                 size_t* num_bytes,
                                                 IpczDriverHandle* handles,
                                                 size_t* num_handles) {
  auto* target = AsyncTransport::FromHandle(transport);
  if (!target) {
    return IPCZ_RESULT_ABORTED;
  }

  if (target->local_type() == AsyncTransport::NodeType::kNonBroker &&
      target->remote_type() == AsyncTransport::NodeType::kNonBroker) {
    // Force ipcz to relay driver objects through a broker.
    return IPCZ_RESULT_PERMISSION_DENIED;
  }

  return kSingleProcessReferenceDriverBase.Serialize(
      handle, transport, flags, options, data, num_bytes, handles, num_handles);
}

}  // namespace

// Note that this driver inherits most of its implementation from the baseline
// single-process driver. Only transport operation is overridden here.
const IpczDriver kAsyncReferenceDriver = {
    sizeof(kAsyncReferenceDriver),
    kSingleProcessReferenceDriverBase.Close,
    kSingleProcessReferenceDriverBase.Serialize,
    kSingleProcessReferenceDriverBase.Deserialize,
    CreateTransports,
    ActivateTransport,
    DeactivateTransport,
    Transmit,
    kSingleProcessReferenceDriverBase.ReportBadTransportActivity,
    kSingleProcessReferenceDriverBase.AllocateSharedMemory,
    kSingleProcessReferenceDriverBase.GetSharedMemoryInfo,
    kSingleProcessReferenceDriverBase.DuplicateSharedMemory,
    kSingleProcessReferenceDriverBase.MapSharedMemory,
    kSingleProcessReferenceDriverBase.GenerateRandomBytes,
};

const IpczDriver kAsyncReferenceDriverWithForcedBrokering = {
    sizeof(kAsyncReferenceDriverWithForcedBrokering),
    kSingleProcessReferenceDriverBase.Close,
    SerializeWithForcedBrokering,
    kSingleProcessReferenceDriverBase.Deserialize,
    CreateTransports,
    ActivateTransport,
    DeactivateTransport,
    Transmit,
    kSingleProcessReferenceDriverBase.ReportBadTransportActivity,
    kSingleProcessReferenceDriverBase.AllocateSharedMemory,
    kSingleProcessReferenceDriverBase.GetSharedMemoryInfo,
    kSingleProcessReferenceDriverBase.DuplicateSharedMemory,
    kSingleProcessReferenceDriverBase.MapSharedMemory,
    kSingleProcessReferenceDriverBase.GenerateRandomBytes,
};

AsyncTransportPair CreateAsyncTransportPair() {
  AsyncTransport::Pair transports = AsyncTransport::CreatePair(
      AsyncTransport::NodeType::kBroker, AsyncTransport::NodeType::kNonBroker);
  return {
      .broker = Object::ReleaseAsHandle(std::move(transports.first)),
      .non_broker = Object::ReleaseAsHandle(std::move(transports.second)),
  };
}

std::pair<IpczDriverHandle, IpczDriverHandle>
CreateAsyncTransportPairForBrokers() {
  AsyncTransport::Pair transports = AsyncTransport::CreatePair(
      AsyncTransport::NodeType::kBroker, AsyncTransport::NodeType::kBroker);
  return {Object::ReleaseAsHandle(std::move(transports.first)),
          Object::ReleaseAsHandle(std::move(transports.second))};
}

}  // namespace ipcz::reference_drivers
