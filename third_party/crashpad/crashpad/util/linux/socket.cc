// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/linux/socket.h"

#include <unistd.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "third_party/lss/lss.h"

namespace crashpad {

constexpr size_t UnixCredentialSocket::kMaxSendRecvMsgFDs;

// static
bool UnixCredentialSocket::CreateCredentialSocketpair(ScopedFileHandle* sock1,
                                                      ScopedFileHandle* sock2) {
  int socks[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socks) != 0) {
    PLOG(ERROR) << "socketpair";
    return false;
  }
  ScopedFileHandle local_sock1(socks[0]);
  ScopedFileHandle local_sock2(socks[1]);

  int optval = 1;
  socklen_t optlen = sizeof(optval);
  if (setsockopt(local_sock1.get(), SOL_SOCKET, SO_PASSCRED, &optval, optlen) !=
          0 ||
      setsockopt(local_sock2.get(), SOL_SOCKET, SO_PASSCRED, &optval, optlen) !=
          0) {
    PLOG(ERROR) << "setsockopt";
    return false;
  }

  sock1->swap(local_sock1);
  sock2->swap(local_sock2);
  return true;
}

// static
int UnixCredentialSocket::SendMsg(int fd,
                                  const void* buf,
                                  size_t buf_size,
                                  const int* fds,
                                  size_t fd_count) {
  // This function is intended to be used after a crash. fds is an integer
  // array instead of a vector to avoid forcing callers to provide a vector,
  // which they would have to create prior to the crash.
  if (fds && fd_count > kMaxSendRecvMsgFDs) {
    DLOG(ERROR) << "too many fds " << fd_count;
    return EINVAL;
  }

  iovec iov;
  iov.iov_base = const_cast<void*>(buf);
  iov.iov_len = buf_size;

  msghdr msg = {};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char cmsg_buf[CMSG_SPACE(sizeof(int) * kMaxSendRecvMsgFDs)];
  if (fds) {
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = CMSG_SPACE(sizeof(int) * fd_count);

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    DCHECK(cmsg);

    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fd_count);
    memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * fd_count);
  }

  // TODO(jperaza): Use sys_sendmsg when lss has macros for maniuplating control
  // messages. https://crbug.com/crashpad/265
  if (HANDLE_EINTR(sendmsg(fd, &msg, MSG_NOSIGNAL)) < 0) {
    DPLOG(ERROR) << "sendmsg";
    return errno;
  }
  return 0;
}

// static
bool UnixCredentialSocket::RecvMsg(int fd,
                                   void* buf,
                                   size_t buf_size,
                                   ucred* creds,
                                   std::vector<ScopedFileHandle>* fds) {
  iovec iov;
  iov.iov_base = buf;
  iov.iov_len = buf_size;

  msghdr msg = {};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char cmsg_buf[CMSG_SPACE(sizeof(ucred)) +
                CMSG_SPACE(sizeof(int) * kMaxSendRecvMsgFDs)];
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);

  int res = HANDLE_EINTR(recvmsg(fd, &msg, 0));
  if (res < 0) {
    PLOG(ERROR) << "recvmsg";
    return false;
  }

  ucred* local_creds = nullptr;
  std::vector<ScopedFileHandle> local_fds;
  bool unhandled_cmsgs = false;

  for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
       cmsg;
       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      int* fdp = reinterpret_cast<int*>(CMSG_DATA(cmsg));
      size_t fd_count = (reinterpret_cast<char*>(cmsg) + cmsg->cmsg_len -
                         reinterpret_cast<char*>(fdp)) /
                        sizeof(int);
      DCHECK_LE(fd_count, kMaxSendRecvMsgFDs);
      for (size_t index = 0; index < fd_count; ++index) {
        if (fds) {
          local_fds.emplace_back(fdp[index]);
        } else if (IGNORE_EINTR(close(fdp[index])) != 0) {
          PLOG(ERROR) << "close";
        }
      }
      continue;
    }

    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS) {
      DCHECK(!local_creds);
      local_creds = reinterpret_cast<ucred*>(CMSG_DATA(cmsg));
      continue;
    }

    LOG(ERROR) << "unhandled cmsg " << cmsg->cmsg_level << ", "
               << cmsg->cmsg_type;
    unhandled_cmsgs = true;
  }

  if (unhandled_cmsgs) {
    return false;
  }

  if (msg.msg_name != nullptr || msg.msg_namelen != 0) {
    LOG(ERROR) << "unexpected msg name";
    return false;
  }

  if (msg.msg_flags & MSG_TRUNC || msg.msg_flags & MSG_CTRUNC) {
    LOG(ERROR) << "truncated msg";
    return false;
  }

  // Credentials are missing from the message either when the recv socket wasn't
  // configured with SO_PASSCRED or when all sending sockets have been closed.
  // In the latter case, res == 0. This case is also indistinguishable from an
  // empty message sent to a recv socket which hasn't set SO_PASSCRED.
  if (!local_creds) {
    LOG_IF(ERROR, res != 0) << "missing credentials";
    return false;
  }

  if (static_cast<size_t>(res) != buf_size) {
    LOG(ERROR) << "incorrect payload size " << res;
    return false;
  }

  *creds = *local_creds;
  if (fds) {
    fds->swap(local_fds);
  }
  return true;
}

}  // namespace crashpad
