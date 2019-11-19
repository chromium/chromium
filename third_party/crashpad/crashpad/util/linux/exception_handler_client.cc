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

#include "util/linux/exception_handler_client.h"

#include <errno.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "third_party/lss/lss.h"
#include "util/file/file_io.h"
#include "util/linux/ptrace_broker.h"
#include "util/linux/socket.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/signals.h"

#if defined(OS_ANDROID)
#include <android/api-level.h>
#endif

namespace crashpad {

namespace {

class ScopedSigprocmaskRestore {
 public:
  explicit ScopedSigprocmaskRestore(const kernel_sigset_t& set_to_block)
      : orig_mask_(), mask_is_set_(false) {
    mask_is_set_ = sys_sigprocmask(SIG_BLOCK, &set_to_block, &orig_mask_) == 0;
    DPLOG_IF(ERROR, !mask_is_set_) << "sigprocmask";
  }

  ~ScopedSigprocmaskRestore() {
    if (mask_is_set_ &&
        sys_sigprocmask(SIG_SETMASK, &orig_mask_, nullptr) != 0) {
      DPLOG(ERROR) << "sigprocmask";
    }
  }

 private:
  kernel_sigset_t orig_mask_;
  bool mask_is_set_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSigprocmaskRestore);
};

}  // namespace

ExceptionHandlerClient::ExceptionHandlerClient(int sock, bool multiple_clients)
    : server_sock_(sock),
      ptracer_(-1),
      can_set_ptracer_(true),
      multiple_clients_(multiple_clients) {}

ExceptionHandlerClient::~ExceptionHandlerClient() = default;

bool ExceptionHandlerClient::GetHandlerCredentials(ucred* creds) {
  ExceptionHandlerProtocol::ClientToServerMessage message = {};
  message.type =
      ExceptionHandlerProtocol::ClientToServerMessage::kTypeCheckCredentials;
  if (UnixCredentialSocket::SendMsg(server_sock_, &message, sizeof(message)) !=
      0) {
    return false;
  }

  ExceptionHandlerProtocol::ServerToClientMessage response;
  return UnixCredentialSocket::RecvMsg(
      server_sock_, &response, sizeof(response), creds);
}

int ExceptionHandlerClient::RequestCrashDump(
    const ExceptionHandlerProtocol::ClientInformation& info) {
  VMAddress sp = FromPointerCast<VMAddress>(&sp);

  if (multiple_clients_) {
    return SignalCrashDump(info, sp);
  }

  int status = SendCrashDumpRequest(info, sp);
  if (status != 0) {
    return status;
  }
  return WaitForCrashDumpComplete();
}

int ExceptionHandlerClient::SetPtracer(pid_t pid) {
  if (ptracer_ == pid) {
    return 0;
  }

  if (!can_set_ptracer_) {
    return EPERM;
  }

  if (prctl(PR_SET_PTRACER, pid, 0, 0, 0) == 0) {
    return 0;
  }
  return errno;
}

void ExceptionHandlerClient::SetCanSetPtracer(bool can_set_ptracer) {
  can_set_ptracer_ = can_set_ptracer;
}

int ExceptionHandlerClient::SignalCrashDump(
    const ExceptionHandlerProtocol::ClientInformation& info,
    VMAddress stack_pointer) {
  kernel_sigset_t dump_done_sigset;
  sys_sigemptyset(&dump_done_sigset);
  sys_sigaddset(&dump_done_sigset, ExceptionHandlerProtocol::kDumpDoneSignal);
  ScopedSigprocmaskRestore scoped_block(dump_done_sigset);

  int status = SendCrashDumpRequest(info, stack_pointer);
  if (status != 0) {
    return status;
  }

  siginfo_t siginfo = {};
  timespec timeout;
  timeout.tv_sec = 5;
  timeout.tv_nsec = 0;
  if (HANDLE_EINTR(sys_sigtimedwait(&dump_done_sigset, &siginfo, &timeout)) <
      0) {
    return errno;
  }

  return 0;
}

int ExceptionHandlerClient::SendCrashDumpRequest(
    const ExceptionHandlerProtocol::ClientInformation& info,
    VMAddress stack_pointer) {
  ExceptionHandlerProtocol::ClientToServerMessage message;
  message.type =
      ExceptionHandlerProtocol::ClientToServerMessage::kTypeCrashDumpRequest;
  message.requesting_thread_stack_address = stack_pointer;
  message.client_info = info;
  return UnixCredentialSocket::SendMsg(server_sock_, &message, sizeof(message));
}

int ExceptionHandlerClient::WaitForCrashDumpComplete() {
  ExceptionHandlerProtocol::ServerToClientMessage message;

  // If the server hangs up, ReadFileExactly will return false without setting
  // errno.
  errno = 0;
  while (ReadFileExactly(server_sock_, &message, sizeof(message))) {
    switch (message.type) {
      case ExceptionHandlerProtocol::ServerToClientMessage::kTypeForkBroker: {
        Signals::InstallDefaultHandler(SIGCHLD);

        pid_t pid = fork();
        if (pid <= 0) {
          ExceptionHandlerProtocol::Errno error = pid < 0 ? errno : 0;
          if (!WriteFile(server_sock_, &error, sizeof(error))) {
            return errno;
          }
        }

        if (pid < 0) {
          continue;
        }

        if (pid == 0) {
#if defined(ARCH_CPU_64_BITS)
          constexpr bool am_64_bit = true;
#else
          constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

          PtraceBroker broker(server_sock_, getppid(), am_64_bit);
          _exit(broker.Run());
        }

        int status = 0;
        pid_t child = HANDLE_EINTR(waitpid(pid, &status, 0));
        DCHECK_EQ(child, pid);

        if (child == pid && status != 0) {
          return status;
        }
        continue;
      }

      case ExceptionHandlerProtocol::ServerToClientMessage::kTypeSetPtracer: {
        ExceptionHandlerProtocol::Errno result = SetPtracer(message.pid);
        if (!WriteFile(server_sock_, &result, sizeof(result))) {
          return errno;
        }
        continue;
      }

      case ExceptionHandlerProtocol::ServerToClientMessage::kTypeCredentials:
        DCHECK(false);
        continue;

      case ExceptionHandlerProtocol::ServerToClientMessage::
          kTypeCrashDumpComplete:
      case ExceptionHandlerProtocol::ServerToClientMessage::
          kTypeCrashDumpFailed:
        return 0;
    }

    DCHECK(false);
  }

  return errno;
}

}  // namespace crashpad
