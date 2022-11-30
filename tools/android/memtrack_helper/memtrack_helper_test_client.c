/*
 * Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "tools/android/memtrack_helper/memtrack_helper.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Usage: %s <pid>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char* const pid = argv[1];
  printf("Requesting memtrack dump for pid %s\n", pid);
  int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (sock < 0)
    exit_with_failure("socket");

  /* Connect to the daemon via the UNIX abstract socket. */
  struct sockaddr_un server_addr;
  init_memtrack_server_addr(&server_addr);

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    exit_with_failure("connect");

  if (send(sock, pid, strlen(pid) + 1, 0) < 0)
    exit_with_failure("send");

  char buf[4096];
  ssize_t rsize = recv(sock, buf, sizeof(buf) - 1, 0);
  if (rsize < 0)
    exit_with_failure("recv");
  buf[rsize] = '\0';

  puts(buf);
  return EXIT_SUCCESS;
}
