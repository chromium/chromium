// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Checks for the reparent notify events that is a signal that a WM has been
// started. Returns 0 on success and 1 on failure. This program must be started
// BEFORE the Wm starts.
//

#include <time.h>

#include <cerrno>
#include <cstdio>

#include "base/command_line.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"

void CalculateTimeout(const timespec& now,
                      const timespec& deadline,
                      timeval* timeout) {
  // 1s == 1e+6 us.
  // 1nsec == 1e-3 us
  timeout->tv_usec = (deadline.tv_sec - now.tv_sec) * 1000000 +
                     (deadline.tv_nsec - now.tv_nsec) / 1000;
  timeout->tv_sec = 0;
}

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);

  // Connects to a X server specified in the current process' env value DISPLAY.
  x11::Connection connection;

  // No display found - fail early.
  if (!connection.Ready()) {
    fprintf(stderr, "Couldn't connect to the X11 server.\n");
    return 1;
  }

  auto dummy_window = connection.GenerateId<x11::Window>();
  auto req = connection.CreateWindow({
      .wid = dummy_window,
      .parent = connection.default_root(),
      .width = 1,
      .height = 1,
      // We are only interested in the ReparentNotify events that are sent
      // whenever our dummy window is reparented because of a wm start.
      .event_mask = x11::EventMask::StructureNotify,
  });
  if (req.Sync().error) {
    fprintf(stderr, "Couldn't create a dummy window.");
    return 1;
  }

  connection.MapWindow({dummy_window});
  connection.Flush();

  int display_fd = connection.GetFd();

  // Set deadline as 30s.
  struct timespec now, deadline;
  clock_gettime(CLOCK_REALTIME, &now);
  deadline = now;
  deadline.tv_sec += 30;

  // Calculate first timeout.
  struct timeval tv;
  CalculateTimeout(now, deadline, &tv);

  do {
    fd_set in_fds;
    FD_ZERO(&in_fds);
    FD_SET(display_fd, &in_fds);

    int ret = select(display_fd + 1, &in_fds, nullptr, nullptr, &tv);
    if (ret == -1) {
      if (errno != EINTR) {
        perror("Error occured while polling the display fd");
        break;
      }
    } else if (ret > 0) {
      connection.ReadResponses();
      for (const auto& event : connection.events()) {
        // If we got ReparentNotify, a wm has started up and we can stop
        // execution.
        if (event.As<x11::ReparentNotifyEvent>())
          return 0;
      }
      connection.events().clear();
    }
    // Calculate next timeout. If it's less or equal to 0, give up.
    clock_gettime(CLOCK_REALTIME, &now);
    CalculateTimeout(now, deadline, &tv);
  } while (tv.tv_usec >= 0);

  return 1;
}

#if defined(LEAK_SANITIZER)
// XOpenDisplay leaks memory if it takes more than one try to connect. This
// causes LSan bots to fail. We don't care about memory leaks in xwmstartupcheck
// anyway, so just disable LSan completely.
// This function isn't referenced from the executable itself. Make sure it isn't
// stripped by the linker.
__attribute__((used)) __attribute__((visibility("default"))) extern "C" int
__lsan_is_turned_off() {
  return 1;
}
#endif
