// Copyright 2017 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "handler/linux/exception_handler_server.h"

#include <errno.h>
#include <linux/capability.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"
#include "util/linux/proc_task_reader.h"
#include "util/linux/socket.h"
#include "util/misc/as_underlying_type.h"

namespace crashpad {

namespace {

// Log an error for a socket after an EPOLLERR.
void LogSocketError(int sock) {
  int err;
  socklen_t err_len = sizeof(err);
  if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &err_len) != 0) {
    PLOG(ERROR) << "getsockopt";
  } else {
    errno = err;
    PLOG(ERROR) << "EPOLLERR";
  }
}

enum class PtraceScope {
  kClassic = 0,
  kRestricted,
  kAdminOnly,
  kNoAttach,
  kUnknown
};

PtraceScope GetPtraceScope() {
  const base::FilePath settings_file("/proc/sys/kernel/yama/ptrace_scope");
  if (!IsRegularFile(base::FilePath(settings_file))) {
    return PtraceScope::kClassic;
  }

  std::string contents;
  if (!LoggingReadEntireFile(settings_file, &contents)) {
    return PtraceScope::kUnknown;
  }

  if (contents.back() != '\n') {
    LOG(ERROR) << "format error";
    return PtraceScope::kUnknown;
  }
  contents.pop_back();

  int ptrace_scope;
  if (!base::StringToInt(contents, &ptrace_scope)) {
    LOG(ERROR) << "format error";
    return PtraceScope::kUnknown;
  }

  if (ptrace_scope < static_cast<int>(PtraceScope::kClassic) ||
      ptrace_scope >= static_cast<int>(PtraceScope::kUnknown)) {
    LOG(ERROR) << "invalid ptrace scope";
    return PtraceScope::kUnknown;
  }

  return static_cast<PtraceScope>(ptrace_scope);
}

bool HaveCapSysPtrace() {
  struct __user_cap_header_struct cap_header = {};
  struct __user_cap_data_struct cap_data = {};

  cap_header.pid = getpid();

  if (syscall(SYS_capget, &cap_header, &cap_data) != 0) {
    PLOG(ERROR) << "capget";
    return false;
  }

  if (cap_header.version != _LINUX_CAPABILITY_VERSION_3) {
    LOG(ERROR) << "Unexpected capability version " << std::hex
               << cap_header.version;
    return false;
  }

  return (cap_data.effective & (1 << CAP_SYS_PTRACE)) != 0;
}

bool SendMessageToClient(
    int client_sock,
    ExceptionHandlerProtocol::ServerToClientMessage::Type type) {
  ExceptionHandlerProtocol::ServerToClientMessage message = {};
  message.type = type;
  if (type ==
      ExceptionHandlerProtocol::ServerToClientMessage::kTypeSetPtracer) {
    message.pid = getpid();
  }
  return LoggingWriteFile(client_sock, &message, sizeof(message));
}

int tgkill(pid_t pid, pid_t tid, int signo) {
  return syscall(SYS_tgkill, pid, tid, signo);
}

void SendSIGCONT(pid_t pid, pid_t tid) {
  if (tid > 0) {
    if (tgkill(pid, tid, ExceptionHandlerProtocol::kDumpDoneSignal) != 0) {
      PLOG(ERROR) << "tgkill";
    }
    return;
  }

  std::vector<pid_t> threads;
  if (!ReadThreadIDs(pid, &threads)) {
    return;
  }
  for (const auto& thread : threads) {
    if (tgkill(pid, thread, ExceptionHandlerProtocol::kDumpDoneSignal) != 0) {
      PLOG(ERROR) << "tgkill";
    }
  }
}

bool SendCredentials(int client_sock) {
  ExceptionHandlerProtocol::ServerToClientMessage message = {};
  message.type =
      ExceptionHandlerProtocol::ServerToClientMessage::kTypeCredentials;
  return UnixCredentialSocket::SendMsg(
             client_sock, &message, sizeof(message)) == 0;
}

class PtraceStrategyDeciderImpl : public PtraceStrategyDecider {
 public:
  PtraceStrategyDeciderImpl() : PtraceStrategyDecider() {}
  ~PtraceStrategyDeciderImpl() = default;

  Strategy ChooseStrategy(int sock,
                          bool multiple_clients,
                          const ucred& client_credentials) override {
    if (client_credentials.pid <= 0) {
      LOG(ERROR) << "invalid credentials";
      return Strategy::kNoPtrace;
    }

    switch (GetPtraceScope()) {
      case PtraceScope::kClassic:
        if (getuid() == client_credentials.uid || HaveCapSysPtrace()) {
          return Strategy::kDirectPtrace;
        }
        return multiple_clients ? Strategy::kNoPtrace : TryForkingBroker(sock);

      case PtraceScope::kRestricted:
        if (multiple_clients) {
          return Strategy::kDirectPtrace;
        }
        if (!SendMessageToClient(sock,
                                 ExceptionHandlerProtocol::
                                     ServerToClientMessage::kTypeSetPtracer)) {
          return Strategy::kError;
        }

        ExceptionHandlerProtocol::Errno status;
        if (!LoggingReadFileExactly(sock, &status, sizeof(status))) {
          return Strategy::kError;
        }

        if (status != 0) {
          errno = status;
          PLOG(ERROR) << "Handler Client SetPtracer";
          return TryForkingBroker(sock);
        }
        return Strategy::kDirectPtrace;

      case PtraceScope::kAdminOnly:
        if (HaveCapSysPtrace()) {
          return Strategy::kDirectPtrace;
        }
        FALLTHROUGH;
      case PtraceScope::kNoAttach:
        LOG(WARNING) << "no ptrace";
        return Strategy::kNoPtrace;

      case PtraceScope::kUnknown:
        LOG(WARNING) << "Unknown ptrace scope";
        return Strategy::kError;
    }

    DCHECK(false);
    return Strategy::kError;
  }

 private:
  static Strategy TryForkingBroker(int client_sock) {
    if (!SendMessageToClient(
            client_sock,
            ExceptionHandlerProtocol::ServerToClientMessage::kTypeForkBroker)) {
      return Strategy::kError;
    }

    ExceptionHandlerProtocol::Errno status;
    if (!LoggingReadFileExactly(client_sock, &status, sizeof(status))) {
      return Strategy::kError;
    }

    if (status != 0) {
      errno = status;
      PLOG(ERROR) << "Handler Client ForkBroker";
      return Strategy::kNoPtrace;
    }
    return Strategy::kUseBroker;
  }
};

}  // namespace

ExceptionHandlerServer::ExceptionHandlerServer()
    : clients_(),
      shutdown_event_(),
      strategy_decider_(new PtraceStrategyDeciderImpl()),
      delegate_(nullptr),
      pollfd_(),
      keep_running_(true) {}

ExceptionHandlerServer::~ExceptionHandlerServer() = default;

void ExceptionHandlerServer::SetPtraceStrategyDecider(
    std::unique_ptr<PtraceStrategyDecider> decider) {
  strategy_decider_ = std::move(decider);
}

bool ExceptionHandlerServer::InitializeWithClient(ScopedFileHandle sock,
                                                  bool multiple_clients) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  pollfd_.reset(epoll_create1(EPOLL_CLOEXEC));
  if (!pollfd_.is_valid()) {
    PLOG(ERROR) << "epoll_create1";
    return false;
  }

  shutdown_event_ = std::make_unique<Event>();
  shutdown_event_->type = Event::Type::kShutdown;
  shutdown_event_->fd.reset(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK));
  if (!shutdown_event_->fd.is_valid()) {
    PLOG(ERROR) << "eventfd";
    return false;
  }

  epoll_event poll_event;
  poll_event.events = EPOLLIN;
  poll_event.data.ptr = shutdown_event_.get();
  if (epoll_ctl(pollfd_.get(),
                EPOLL_CTL_ADD,
                shutdown_event_->fd.get(),
                &poll_event) != 0) {
    PLOG(ERROR) << "epoll_ctl";
    return false;
  }

  if (!InstallClientSocket(std::move(sock),
                           multiple_clients ? Event::Type::kSharedSocketMessage
                                            : Event::Type::kClientMessage)) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void ExceptionHandlerServer::Run(Delegate* delegate) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  delegate_ = delegate;

  while (keep_running_ && clients_.size() > 0) {
    epoll_event poll_event;
    int res = HANDLE_EINTR(epoll_wait(pollfd_.get(), &poll_event, 1, -1));
    if (res < 0) {
      PLOG(ERROR) << "epoll_wait";
      return;
    }
    DCHECK_EQ(res, 1);

    Event* eventp = reinterpret_cast<Event*>(poll_event.data.ptr);
    if (eventp->type == Event::Type::kShutdown) {
      if (poll_event.events & EPOLLERR) {
        LogSocketError(eventp->fd.get());
      }
      keep_running_ = false;
    } else {
      HandleEvent(eventp, poll_event.events);
    }
  }
}

void ExceptionHandlerServer::Stop() {
  keep_running_ = false;
  if (shutdown_event_ && shutdown_event_->fd.is_valid()) {
    uint64_t value = 1;
    LoggingWriteFile(shutdown_event_->fd.get(), &value, sizeof(value));
  }
}

void ExceptionHandlerServer::HandleEvent(Event* event, uint32_t event_type) {
  DCHECK_NE(AsUnderlyingType(event->type),
            AsUnderlyingType(Event::Type::kShutdown));

  if (event_type & EPOLLERR) {
    LogSocketError(event->fd.get());
    UninstallClientSocket(event);
    return;
  }

  if (event_type & EPOLLIN) {
    if (!ReceiveClientMessage(event)) {
      UninstallClientSocket(event);
    }
    return;
  }

  if (event_type & EPOLLHUP || event_type & EPOLLRDHUP) {
    UninstallClientSocket(event);
    return;
  }

  LOG(ERROR) << "Unexpected event 0x" << std::hex << event_type;
  return;
}

bool ExceptionHandlerServer::InstallClientSocket(ScopedFileHandle socket,
                                                 Event::Type type) {
  // The handler may not have permission to set SO_PASSCRED on the socket, but
  // it doesn't need to if the client has already set it.
  // https://bugs.chromium.org/p/crashpad/issues/detail?id=252
  int optval;
  socklen_t optlen = sizeof(optval);
  if (getsockopt(socket.get(), SOL_SOCKET, SO_PASSCRED, &optval, &optlen) !=
      0) {
    PLOG(ERROR) << "getsockopt";
    return false;
  }
  if (!optval) {
    optval = 1;
    optlen = sizeof(optval);
    if (setsockopt(socket.get(), SOL_SOCKET, SO_PASSCRED, &optval, optlen) !=
        0) {
      PLOG(ERROR) << "setsockopt";
      return false;
    }
  }

  auto event = std::make_unique<Event>();
  event->type = type;
  event->fd.reset(socket.release());

  Event* eventp = event.get();

  if (!clients_.insert(std::make_pair(event->fd.get(), std::move(event)))
           .second) {
    LOG(ERROR) << "duplicate descriptor";
    return false;
  }

  epoll_event poll_event;
  poll_event.events = EPOLLIN | EPOLLRDHUP;
  poll_event.data.ptr = eventp;

  if (epoll_ctl(pollfd_.get(), EPOLL_CTL_ADD, eventp->fd.get(), &poll_event) !=
      0) {
    PLOG(ERROR) << "epoll_ctl";
    clients_.erase(eventp->fd.get());
    return false;
  }

  return true;
}

bool ExceptionHandlerServer::UninstallClientSocket(Event* event) {
  if (epoll_ctl(pollfd_.get(), EPOLL_CTL_DEL, event->fd.get(), nullptr) != 0) {
    PLOG(ERROR) << "epoll_ctl";
    return false;
  }

  if (clients_.erase(event->fd.get()) != 1) {
    LOG(ERROR) << "event not found";
    return false;
  }

  return true;
}

bool ExceptionHandlerServer::ReceiveClientMessage(Event* event) {
  ExceptionHandlerProtocol::ClientToServerMessage message;
  ucred creds;
  if (!UnixCredentialSocket::RecvMsg(
          event->fd.get(), &message, sizeof(message), &creds)) {
    return false;
  }

  switch (message.type) {
    case ExceptionHandlerProtocol::ClientToServerMessage::kTypeCheckCredentials:
      return SendCredentials(event->fd.get());

    case ExceptionHandlerProtocol::ClientToServerMessage::kTypeCrashDumpRequest:
      return HandleCrashDumpRequest(
          creds,
          message.client_info,
          message.requesting_thread_stack_address,
          event->fd.get(),
          event->type == Event::Type::kSharedSocketMessage);
  }

  DCHECK(false);
  LOG(ERROR) << "Unknown message type";
  return false;
}

bool ExceptionHandlerServer::HandleCrashDumpRequest(
    const ucred& creds,
    const ExceptionHandlerProtocol::ClientInformation& client_info,
    VMAddress requesting_thread_stack_address,
    int client_sock,
    bool multiple_clients) {
  pid_t client_process_id = creds.pid;
  pid_t requesting_thread_id = -1;
  uid_t client_uid = creds.uid;

  switch (
      strategy_decider_->ChooseStrategy(client_sock, multiple_clients, creds)) {
    case PtraceStrategyDecider::Strategy::kError:
      if (multiple_clients) {
        SendSIGCONT(client_process_id, requesting_thread_id);
      }
      return false;

    case PtraceStrategyDecider::Strategy::kNoPtrace:
      if (multiple_clients) {
        SendSIGCONT(client_process_id, requesting_thread_id);
        return true;
      }
      return SendMessageToClient(
          client_sock,
          ExceptionHandlerProtocol::ServerToClientMessage::
              kTypeCrashDumpFailed);

    case PtraceStrategyDecider::Strategy::kDirectPtrace: {
      delegate_->HandleException(client_process_id,
                                 client_uid,
                                 client_info,
                                 requesting_thread_stack_address,
                                 &requesting_thread_id);
      if (multiple_clients) {
        SendSIGCONT(client_process_id, requesting_thread_id);
        return true;
      }
      break;
    }

    case PtraceStrategyDecider::Strategy::kUseBroker:
      DCHECK(!multiple_clients);
      delegate_->HandleExceptionWithBroker(
          client_process_id, client_uid, client_info, client_sock);
      break;
  }

  return SendMessageToClient(
      client_sock,
      ExceptionHandlerProtocol::ServerToClientMessage::kTypeCrashDumpComplete);
}

}  // namespace crashpad
