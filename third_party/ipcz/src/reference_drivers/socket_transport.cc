// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/socket_transport.h"

#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "reference_drivers/file_descriptor.h"
#include "reference_drivers/handle_eintr.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/log.h"
#include "util/ref_counted.h"
#include "util/safe_math.h"

namespace ipcz::reference_drivers {

namespace {

constexpr size_t kMaxDescriptorsPerMessage = 64;

bool CreateNonBlockingSocketPair(FileDescriptor& first,
                                 FileDescriptor& second) {
  int fds[2];
  int result = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  if (result != 0) {
    return false;
  }

  bool ok = fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0;
  ok = ok && (fcntl(fds[1], F_SETFL, O_NONBLOCK) == 0);
  if (!ok) {
    close(fds[0]);
    close(fds[1]);
    return false;
  }

  first = FileDescriptor(fds[0]);
  second = FileDescriptor(fds[1]);
  return true;
}

// Assuming `occupied` is either empty or a subspan of `container`, this ensures
// that `container` has at least `capacity` elements of storage available beyond
// the end of `occupied`, allocating additional storage if necessary. Returns
// the span of elements between the end of `occupied` and the end of
// `container`, with a length of at least `capacity`.
template <typename T>
absl::Span<T> EnsureCapacity(std::vector<T>& container,
                             absl::Span<T>& occupied,
                             size_t capacity) {
  const size_t occupied_start =
      occupied.empty() ? 0 : occupied.data() - container.data();
  const size_t occupied_length = occupied.size();
  const size_t available_start = occupied_start + occupied_length;
  const auto available = absl::MakeSpan(container).subspan(available_start);
  if (available.size() >= capacity) {
    return available;
  }

  const size_t required_new_capacity = capacity - available.size();
  const size_t double_size = container.size() * 2;
  const size_t just_enough_size = container.size() + required_new_capacity;
  const size_t new_size = std::max(double_size, just_enough_size);

  container.resize(new_size);
  occupied = absl::MakeSpan(container).subspan(occupied_start, occupied_length);
  return absl::MakeSpan(container).subspan(available_start);
}

}  // namespace

SocketTransport::SocketTransport() = default;

SocketTransport::SocketTransport(FileDescriptor fd) : socket_(std::move(fd)) {
  const bool ok = CreateNonBlockingSocketPair(signal_sender_, signal_receiver_);
  ABSL_ASSERT(ok);
}

SocketTransport::~SocketTransport() {
  absl::MutexLock lock(&io_thread_mutex_);
  ABSL_HARDENING_ASSERT(!io_thread_);
}

void SocketTransport::Activate(MessageHandler message_handler,
                               ErrorHandler error_handler) {
  ABSL_ASSERT(!has_been_activated_);
  has_been_activated_ = true;
  message_handler_ = std::move(message_handler);
  error_handler_ = std::move(error_handler);

  absl::MutexLock lock(&io_thread_mutex_);
  ABSL_ASSERT(!io_thread_);
  io_thread_ = std::make_unique<std::thread>(&RunIOThreadForTransport,
                                             WrapRefCounted(this));
}

void SocketTransport::Deactivate(std::function<void()> shutdown_callback) {
  {
    // Initiate asynchronous shutdown of the I/O thread.
    absl::MutexLock lock(&notify_mutex_);
    if (!is_io_thread_done_) {
      ABSL_ASSERT(!shutdown_callback_);
      std::swap(shutdown_callback, shutdown_callback_);
      WakeIOThread();
    }
  }

  std::unique_ptr<std::thread> io_thread;
  {
    absl::MutexLock lock(&io_thread_mutex_);
    io_thread = std::move(io_thread_);
  }

  if (io_thread && io_thread->get_id() == std::this_thread::get_id()) {
    // If we're running on the I/O thread, we can detach now. The thread will
    // terminate soon and will run `shutdown_callback_` when finished. This is
    // safe because the thread owns a reference to `this`.
    io_thread->detach();
  } else if (io_thread) {
    // Otherwise the I/O thread is or was some other thread, which we can join.
    io_thread->join();
  }

  if (shutdown_callback) {
    // If the callback is still valid, then the I/O thread had already begun
    // shutdown before deactivation was started. This means it's safe to invoke
    // immediately.
    shutdown_callback();
  }
}

bool SocketTransport::Send(Message message) {
  Header header = {
      .num_bytes =
          checked_cast<uint32_t>(CheckAdd(message.data.size(), sizeof(Header))),
      .num_descriptors = checked_cast<uint32_t>(message.descriptors.size()),
  };
  auto header_bytes =
      absl::MakeSpan(reinterpret_cast<uint8_t*>(&header), sizeof(header));

  {
    absl::MutexLock lock(&queue_mutex_);
    if (!outgoing_queue_.empty()) {
      outgoing_queue_.emplace_back(header_bytes, message);
      return true;
    }

    std::optional<size_t> bytes_sent = TrySend(header_bytes, message);
    if (!bytes_sent.has_value()) {
      return false;
    }

    if (*bytes_sent == header.num_bytes) {
      return true;
    }

    if (*bytes_sent < header_bytes.size()) {
      header_bytes.remove_prefix(*bytes_sent);
    } else {
      *bytes_sent -= header_bytes.size();
      header_bytes = {};
    }

    outgoing_queue_.emplace_back(
        header_bytes,
        Message{
            .data = message.data.subspan(*bytes_sent),

            // sendmsg() on Linux will return EAGAIN/EWOULDBLOCK if there's not
            // enough socket capacity to convey at least one byte of message
            // data in addition to the complete ancillary data which conveys all
            // FDs. So either some data has been sent AND all descriptors have
            // been sent; OR no data or descriptors have been sent.
            .descriptors = *bytes_sent ? absl::Span<FileDescriptor>()
                                       : message.descriptors,
        });
  }

  // Ensure the I/O loop is restarted at least once after the outgoing queue is
  // modified, since it only watches for non-blocking writability if the queue
  // is non-empty.
  absl::MutexLock lock(&notify_mutex_);
  WakeIOThread();
  return true;
}

FileDescriptor SocketTransport::TakeDescriptor() {
  ABSL_ASSERT(!has_been_activated());
  return std::move(socket_);
}

std::optional<size_t> SocketTransport::TrySend(absl::Span<uint8_t> header,
                                               Message message) {
  ABSL_ASSERT(socket_.is_valid());

  iovec iovs[] = {
      {header.data(), header.size()},
      {const_cast<uint8_t*>(message.data.data()), message.data.size()},
  };

  const size_t num_descriptors = message.descriptors.size();
  ABSL_ASSERT(num_descriptors <= kMaxDescriptorsPerMessage);
  char cmsg_buf[CMSG_SPACE(kMaxDescriptorsPerMessage * sizeof(int))];
  struct msghdr msg = {};
  msg.msg_iov = &iovs[0];
  msg.msg_iovlen = 2;
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = CMSG_LEN(num_descriptors * sizeof(int));
  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(num_descriptors * sizeof(int));
  size_t next_descriptor = 0;
  for (const FileDescriptor& fd : message.descriptors) {
    ABSL_ASSERT(fd.is_valid());
    reinterpret_cast<int*>(CMSG_DATA(cmsg))[next_descriptor++] = fd.get();
  }

  for (;;) {
    const ssize_t result =
        HANDLE_EINTR(sendmsg(socket_.get(), &msg, MSG_NOSIGNAL));
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Whole message deferred.
        return 0;
      }

      if (errno == EPIPE || errno == ECONNRESET) {
        // Peer closed. Not an error condition per se, but it means we can
        // terminate the transport anyway.
        return std::nullopt;
      }

      // Unrecoverable error.
      const char* error = strerror(errno);
      LOG(FATAL) << "sendmsg: " << error;
    }

    return static_cast<size_t>(result);
  }
}

// static
void SocketTransport::RunIOThreadForTransport(Ref<SocketTransport> transport) {
  transport->RunIOThread();

  std::function<void()> shutdown_callback;
  {
    absl::MutexLock lock(&transport->notify_mutex_);
    shutdown_callback = std::move(transport->shutdown_callback_);
    transport->is_io_thread_done_ = true;
  }

  if (shutdown_callback) {
    shutdown_callback();
  }
}

void SocketTransport::RunIOThread() {
  static constexpr size_t kChannelFdIndex = 0;
  static constexpr size_t kNotifyFdIndex = 1;
  for (;;) {
    pollfd poll_fds[2];

    poll_fds[kChannelFdIndex].fd = socket_.get();
    poll_fds[kChannelFdIndex].events =
        POLLIN | (!IsOutgoingQueueEmpty() ? POLLOUT : 0);

    poll_fds[kNotifyFdIndex].fd = signal_receiver_.get();
    poll_fds[kNotifyFdIndex].events = POLLIN;

    int poll_result;
    do {
      poll_result = HANDLE_EINTR(poll(poll_fds, std::size(poll_fds), -1));
    } while (poll_result == -1 && errno == EAGAIN);
    ABSL_ASSERT(poll_result > 0);

    if (poll_fds[kChannelFdIndex].revents & POLLERR) {
      NotifyError();
      return;
    }

    if (poll_fds[kChannelFdIndex].revents & POLLOUT) {
      TryFlushingOutgoingQueue();
    }

    if (poll_fds[kNotifyFdIndex].revents & POLLIN) {
      absl::MutexLock lock(&notify_mutex_);
      ClearIOThreadSignal();
      if (shutdown_callback_) {
        return;
      }

      // If this wasn't a shutdown notification, then it was to notify about
      // a new outgoing message being queued. All we need to do is restart the
      // poll() loop to ensure we're now watching for POLLOUT on the Channel's
      // socket.
      continue;
    }

    if ((poll_fds[kChannelFdIndex].revents & POLLIN) == 0) {
      // No incoming data on the Channel's socket, so go back to sleep.
      continue;
    }

    constexpr size_t kDefaultReadSize = 4096;
    absl::Span<uint8_t> storage = EnsureReadCapacity(kDefaultReadSize);
    struct iovec iov = {storage.data(), storage.size()};
    char cmsg_buf[CMSG_SPACE(kMaxDescriptorsPerMessage * sizeof(int))];
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    ssize_t read_result = HANDLE_EINTR(recvmsg(socket_.get(), &msg, 0));
    if (read_result <= 0) {
      if (read_result < 0) {
        const char* error = strerror(errno);
        LOG(FATAL) << "recvmsg: " << error;
      }
      NotifyError();
      return;
    }

    std::vector<FileDescriptor> descriptors;
    if (msg.msg_controllen > 0) {
      for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
           cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
          size_t payload_length = cmsg->cmsg_len - CMSG_LEN(0);
          ABSL_ASSERT(payload_length % sizeof(int) == 0);
          size_t num_fds = payload_length / sizeof(int);
          const int* fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
          descriptors.resize(num_fds);
          for (size_t i = 0; i < num_fds; ++i) {
            descriptors[i] = FileDescriptor(fds[i]);
          }
        }
      }
      ABSL_ASSERT((msg.msg_flags & MSG_CTRUNC) == 0);
    }

    CommitRead(static_cast<size_t>(read_result), std::move(descriptors));
    if (!TryDispatchMessages()) {
      NotifyError();
      return;
    }
  }
}

bool SocketTransport::IsOutgoingQueueEmpty() {
  absl::MutexLock lock(&queue_mutex_);
  return outgoing_queue_.empty();
}

absl::Span<uint8_t> SocketTransport::EnsureReadCapacity(size_t num_bytes) {
  return EnsureCapacity(data_buffer_, occupied_data_, num_bytes);
}

void SocketTransport::CommitRead(size_t num_bytes,
                                 std::vector<FileDescriptor> descriptors) {
  if (occupied_data_.empty()) {
    occupied_data_ = {data_buffer_.data(), num_bytes};
  } else {
    occupied_data_ = {occupied_data_.data(), occupied_data_.size() + num_bytes};
  }

  if (descriptors.empty()) {
    return;
  }

  absl::Span<FileDescriptor> descriptor_storage = EnsureCapacity(
      descriptor_buffer_, occupied_descriptors_, descriptors.size());
  for (size_t i = 0; i < descriptors.size(); ++i) {
    descriptor_storage[i] = std::move(descriptors[i]);
  }
  if (occupied_descriptors_.empty()) {
    occupied_descriptors_ = {descriptor_buffer_.data(), descriptors.size()};
  } else {
    occupied_descriptors_ = {occupied_descriptors_.data(),
                             occupied_descriptors_.size() + descriptors.size()};
  }
}

void SocketTransport::NotifyError() {
  if (error_handler_) {
    error_handler_();
  }
}

bool SocketTransport::TryDispatchMessages() {
  while (occupied_data_.size() >= sizeof(Header)) {
    const Header header = *reinterpret_cast<Header*>(occupied_data_.data());
    if (occupied_data_.size() < header.num_bytes ||
        occupied_descriptors_.size() < header.num_descriptors) {
      // Not enough stuff to dispatch our next message.
      return true;
    }

    if (header.num_bytes < sizeof(Header)) {
      // Invalid header value.
      return false;
    }

    auto data_view =
        occupied_data_.subspan(0, header.num_bytes).subspan(sizeof(Header));
    auto descriptor_view =
        occupied_descriptors_.subspan(0, header.num_descriptors);
    if (!message_handler_({data_view, descriptor_view})) {
      DLOG(ERROR) << "Disconnecting SocketTransport for bad message";
      return false;
    }

    occupied_data_.remove_prefix(header.num_bytes);
    occupied_descriptors_.remove_prefix(header.num_descriptors);
  }

  return true;
}

void SocketTransport::TryFlushingOutgoingQueue() {
  for (;;) {
    size_t i = 0;
    for (;; ++i) {
      Message m;
      {
        absl::MutexLock lock(&queue_mutex_);
        if (i >= outgoing_queue_.size()) {
          break;
        }
        m = outgoing_queue_[i].AsMessage();
      }

      std::optional<size_t> bytes_sent = TrySend({}, m);
      if (!bytes_sent.has_value()) {
        // Error!
        NotifyError();
        return;
      }

      if (*bytes_sent < m.data.size()) {
        // Still at least partially blocked.
        absl::MutexLock lock(&queue_mutex_);
        outgoing_queue_[i] = DeferredMessage({}, m);
        break;
      }
    }

    absl::MutexLock lock(&queue_mutex_);
    if (i == outgoing_queue_.size()) {
      // Finished!
      outgoing_queue_.clear();
      return;
    }

    if (i == 0) {
      // No progress.
      return;
    }

    // Partial progress. Remove any fully transmitted messages from queue.
    std::move(outgoing_queue_.begin() + i, outgoing_queue_.end(),
              outgoing_queue_.begin());
    outgoing_queue_.resize(outgoing_queue_.size() - i);
  }
}

void SocketTransport::WakeIOThread() {
  notify_mutex_.AssertHeld();
  const uint8_t msg = 1;
  int result = HANDLE_EINTR(write(signal_sender_.get(), &msg, 1));
  ABSL_ASSERT(result == 1);
}

void SocketTransport::ClearIOThreadSignal() {
  notify_mutex_.AssertHeld();
  ssize_t result;
  do {
    uint8_t msg;
    result = HANDLE_EINTR(read(signal_receiver_.get(), &msg, 1));
  } while (result == 1);
}

SocketTransport::DeferredMessage::DeferredMessage() = default;

SocketTransport::DeferredMessage::DeferredMessage(absl::Span<uint8_t> header,
                                                  Message message) {
  data = std::vector<uint8_t>(header.size() + message.data.size());
  std::copy(header.begin(), header.end(), data.begin());
  std::copy(message.data.begin(), message.data.end(),
            data.begin() + header.size());

  descriptors.resize(message.descriptors.size());
  std::move(message.descriptors.begin(), message.descriptors.end(),
            descriptors.begin());
}

SocketTransport::DeferredMessage::DeferredMessage(DeferredMessage&&) = default;

SocketTransport::DeferredMessage& SocketTransport::DeferredMessage::operator=(
    DeferredMessage&&) = default;

SocketTransport::DeferredMessage::~DeferredMessage() = default;

SocketTransport::Message SocketTransport::DeferredMessage::AsMessage() {
  return {absl::MakeSpan(data), absl::MakeSpan(descriptors)};
}

// static
SocketTransport::Pair SocketTransport::CreatePair() {
  FileDescriptor first;
  FileDescriptor second;
  if (!CreateNonBlockingSocketPair(first, second)) {
    return {};
  }

  return {MakeRefCounted<SocketTransport>(std::move(first)),
          MakeRefCounted<SocketTransport>(std::move(second))};
}

}  // namespace ipcz::reference_drivers
