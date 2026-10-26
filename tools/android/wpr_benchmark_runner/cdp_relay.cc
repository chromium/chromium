// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <iterator>
#include <string_view>

namespace {

struct Buffer {
  std::array<char, 64 * 1024> data;
  size_t offset = 0;
  size_t size = 0;

  bool empty() const { return offset == size; }

  int Read(int fd) {
    ssize_t count = read(fd, data.data(), data.size());
    if (count < 0) {
      return errno == EINTR || errno == EAGAIN ? 1 : -1;
    }
    offset = 0;
    size = count;
    return count > 0 ? 1 : 0;
  }

  bool Write(int fd) {
    ssize_t count = write(fd, &data[offset], size - offset);
    if (count < 0) {
      return errno == EINTR || errno == EAGAIN;
    }
    offset += count;
    return true;
  }
};

bool SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

int Relay(int connection) {
  Buffer to_browser;
  Buffer to_runner;
  for (;;) {
    short socket_events =
        (to_runner.empty() ? POLLIN : 0) | (to_browser.empty() ? 0 : POLLOUT);
    pollfd descriptors[] = {
        {.fd = STDIN_FILENO,
         .events = static_cast<short>(to_browser.empty() ? POLLIN : 0),
         .revents = 0},
        {.fd = socket_events ? connection : -1,
         .events = socket_events,
         .revents = 0},
        {.fd = STDOUT_FILENO,
         .events = static_cast<short>(to_runner.empty() ? 0 : POLLOUT),
         .revents = 0},
    };
    if (poll(descriptors, std::size(descriptors), -1) < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("CDP relay poll");
      return 1;
    }
    if ((descriptors[0].revents & (POLLHUP | POLLERR | POLLNVAL)) ||
        (descriptors[2].revents & (POLLHUP | POLLERR | POLLNVAL))) {
      return 0;
    }
    if (descriptors[1].revents & (POLLERR | POLLNVAL)) {
      fprintf(stderr, "CDP relay socket failed\n");
      return 1;
    }
    if (descriptors[0].revents & POLLIN) {
      int result = to_browser.Read(STDIN_FILENO);
      if (result <= 0) {
        if (result < 0) {
          perror("CDP relay stdin");
        }
        return result < 0 ? 1 : 0;
      }
    }
    if (descriptors[1].revents & POLLOUT) {
      if (!to_browser.Write(connection)) {
        perror("CDP relay socket write");
        return 1;
      }
    }
    if (to_runner.empty() && (descriptors[1].revents & (POLLIN | POLLHUP))) {
      int result = to_runner.Read(connection);
      if (result <= 0) {
        if (result < 0) {
          perror("CDP relay socket read");
        }
        return result < 0 ? 1 : 0;
      }
    }
    if (descriptors[2].revents & POLLOUT) {
      if (!to_runner.Write(STDOUT_FILENO)) {
        perror("CDP relay stdout");
        return 1;
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  sockaddr_un address = {};
  address.sun_family = AF_UNIX;
  if (argc != 2) {
    fprintf(stderr, "Usage: cdp_relay ABSTRACT_SOCKET_NAME\n");
    return 1;
  }
  std::string_view name(argv[1]);
  if (name.empty() || name.size() > sizeof(address.sun_path) - 1) {
    fprintf(stderr, "Invalid abstract socket name\n");
    return 1;
  }
  name.copy(&address.sun_path[1], name.size());
  signal(SIGPIPE, SIG_IGN);
  int connection =
      socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
  if (connection < 0) {
    perror("CDP relay socket");
    return 1;
  }
  socklen_t length = offsetof(sockaddr_un, sun_path) + 1 + name.size();
  if (connect(connection, reinterpret_cast<sockaddr*>(&address), length) < 0) {
    perror("CDP relay connect");
    return 1;
  }
  if (!SetNonBlocking(STDIN_FILENO) || !SetNonBlocking(STDOUT_FILENO)) {
    perror("CDP relay pipes");
    return 1;
  }
  return Relay(connection);
}
