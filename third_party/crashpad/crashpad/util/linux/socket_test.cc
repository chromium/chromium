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

#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "gtest/gtest.h"
#include "util/linux/socket.h"

namespace crashpad {
namespace test {
namespace {

TEST(Socket, Credentials) {
  ScopedFileHandle send_sock, recv_sock;
  ASSERT_TRUE(
      UnixCredentialSocket::CreateCredentialSocketpair(&send_sock, &recv_sock));

  char msg = 42;
  ASSERT_EQ(UnixCredentialSocket::SendMsg(send_sock.get(), &msg, sizeof(msg)),
            0);

  char recv_msg = 0;
  ucred creds;
  ASSERT_TRUE(UnixCredentialSocket::RecvMsg(
      recv_sock.get(), &recv_msg, sizeof(recv_msg), &creds));
  EXPECT_EQ(recv_msg, msg);
  EXPECT_EQ(creds.pid, getpid());
  EXPECT_EQ(creds.uid, geteuid());
  EXPECT_EQ(creds.gid, getegid());
}

TEST(Socket, EmptyMessages) {
  ScopedFileHandle send_sock, recv_sock;
  ASSERT_TRUE(
      UnixCredentialSocket::CreateCredentialSocketpair(&send_sock, &recv_sock));

  ASSERT_EQ(UnixCredentialSocket::SendMsg(send_sock.get(), nullptr, 0), 0);

  ucred creds;
  ASSERT_TRUE(
      UnixCredentialSocket::RecvMsg(recv_sock.get(), nullptr, 0, &creds));
  EXPECT_EQ(creds.pid, getpid());
  EXPECT_EQ(creds.uid, geteuid());
  EXPECT_EQ(creds.gid, getegid());
}

TEST(Socket, Hangup) {
  ScopedFileHandle send_sock, recv_sock;
  ASSERT_TRUE(
      UnixCredentialSocket::CreateCredentialSocketpair(&send_sock, &recv_sock));

  send_sock.reset();

  char recv_msg = 0;
  ucred creds;
  EXPECT_FALSE(UnixCredentialSocket::RecvMsg(
      recv_sock.get(), &recv_msg, sizeof(recv_msg), &creds));
}

TEST(Socket, FileDescriptors) {
  ScopedFileHandle send_sock, recv_sock;
  ASSERT_TRUE(
      UnixCredentialSocket::CreateCredentialSocketpair(&send_sock, &recv_sock));

  ScopedFileHandle test_fd1, test_fd2;
  ASSERT_TRUE(
      UnixCredentialSocket::CreateCredentialSocketpair(&test_fd1, &test_fd2));

  char msg = 42;
  ASSERT_EQ(UnixCredentialSocket::SendMsg(
                send_sock.get(), &msg, sizeof(msg), &test_fd1.get(), 1),
            0);

  char recv_msg = 0;
  ucred creds;
  std::vector<ScopedFileHandle> fds;
  ASSERT_TRUE(UnixCredentialSocket::RecvMsg(
      recv_sock.get(), &recv_msg, sizeof(recv_msg), &creds, &fds));
  ASSERT_EQ(fds.size(), 1u);
}

TEST(Socket, RecvClosesFileDescriptors) {
  ScopedFileHandle send_sock, recv_sock;
  ASSERT_TRUE(
      UnixCredentialSocket::CreateCredentialSocketpair(&send_sock, &recv_sock));

  ScopedFileHandle send_fds[UnixCredentialSocket::kMaxSendRecvMsgFDs];
  ScopedFileHandle recv_fds[UnixCredentialSocket::kMaxSendRecvMsgFDs];
  int raw_recv_fds[UnixCredentialSocket::kMaxSendRecvMsgFDs];
  for (size_t index = 0; index < UnixCredentialSocket::kMaxSendRecvMsgFDs;
       ++index) {
    ASSERT_TRUE(UnixCredentialSocket::CreateCredentialSocketpair(
        &send_fds[index], &recv_fds[index]));
    raw_recv_fds[index] = recv_fds[index].get();
  }

  char msg = 42;
  ASSERT_EQ(
      UnixCredentialSocket::SendMsg(send_sock.get(),
                                    &msg,
                                    sizeof(msg),
                                    raw_recv_fds,
                                    UnixCredentialSocket::kMaxSendRecvMsgFDs),
      0);

  char recv_msg = 0;
  ucred creds;
  ASSERT_TRUE(UnixCredentialSocket::RecvMsg(
      recv_sock.get(), &recv_msg, sizeof(recv_msg), &creds));
  EXPECT_EQ(creds.pid, getpid());

  for (size_t index = 0; index < UnixCredentialSocket::kMaxSendRecvMsgFDs;
       ++index) {
    recv_fds[index].reset();
    char c;
    EXPECT_EQ(
        HANDLE_EINTR(send(send_fds[index].get(), &c, sizeof(c), MSG_NOSIGNAL)),
        -1);
    EXPECT_EQ(errno, EPIPE);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
