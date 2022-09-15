/*
 * Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tools/android/memtrack_helper/memtrack_helper.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * This is a helper daemon for Android which makes memtrack graphics information
 * accessible via a UNIX socket. It is used by telemetry and memory-infra.
 * More description in the design-doc: https://goo.gl/4Y30p9 .
 */

static const char kShutdownRequest = 'Q';

// See $ANDROID/system/core/include/memtrack/memtrack.h.
typedef void* memtrack_proc_handle;
typedef int (*memtrack_init_t)(void);
typedef memtrack_proc_handle (*memtrack_proc_new_t)(void);
typedef void (*memtrack_proc_destroy_t)(memtrack_proc_handle);
typedef int (*memtrack_proc_get_t)(memtrack_proc_handle, pid_t);
typedef ssize_t (*memtrack_proc_graphics_total_t)(memtrack_proc_handle);
typedef ssize_t (*memtrack_proc_graphics_pss_t)(memtrack_proc_handle);
typedef ssize_t (*memtrack_proc_gl_total_t)(memtrack_proc_handle);
typedef ssize_t (*memtrack_proc_gl_pss_t)(memtrack_proc_handle);
typedef ssize_t (*memtrack_proc_other_total_t)(memtrack_proc_handle);
typedef ssize_t (*memtrack_proc_other_pss_t)(memtrack_proc_handle);

static memtrack_init_t memtrack_init;
static memtrack_proc_new_t memtrack_proc_new;
static memtrack_proc_destroy_t memtrack_proc_destroy;
static memtrack_proc_get_t memtrack_proc_get;
static memtrack_proc_graphics_total_t memtrack_proc_graphics_total;
static memtrack_proc_graphics_pss_t memtrack_proc_graphics_pss;
static memtrack_proc_gl_total_t memtrack_proc_gl_total;
static memtrack_proc_gl_pss_t memtrack_proc_gl_pss;
static memtrack_proc_other_total_t memtrack_proc_other_total;
static memtrack_proc_other_pss_t memtrack_proc_other_pss;

static void send_response(int client_sock, const char* resp) {
  send(client_sock, resp, strlen(resp) + 1, 0);
}

static void send_shutdown_request(struct sockaddr_un* addr) {
  int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  connect(sock, (struct sockaddr*)addr, sizeof(*addr));
  send(sock, &kShutdownRequest, 1, 0);
  close(sock);
}

static void handle_one_request(int client_sock) {
  char buf[32];
  char response[4096] = "";
  ssize_t rsize = recv(client_sock, buf, sizeof(buf) - 1, 0);
  if (rsize < 1)
    return;
  buf[rsize] = '\0';

  if (buf[0] == kShutdownRequest)
    exit(EXIT_SUCCESS);

  pid_t pid = -1;
  if (sscanf(buf, "%d", &pid) != 1 || pid < 0)
    return send_response(client_sock, "ERR invalid pid");

  memtrack_proc_handle handle = memtrack_proc_new();
  if (!handle)
    return send_response(client_sock, "ERR memtrack_proc_new()");

  if (memtrack_proc_get(handle, pid)) {
    memtrack_proc_destroy(handle);
    return send_response(client_sock, "ERR memtrack_proc_get()");
  }

  char* response_ptr = &response[0];
  if (memtrack_proc_graphics_total) {
    response_ptr += sprintf(response_ptr, "graphics_total %zd\n",
                            memtrack_proc_graphics_total(handle));
  }
  if (memtrack_proc_graphics_pss) {
    response_ptr += sprintf(response_ptr, "graphics_pss %zd\n",
                            memtrack_proc_graphics_pss(handle));
  }
  if (memtrack_proc_gl_total) {
    response_ptr += sprintf(response_ptr, "gl_total %zd\n",
                            memtrack_proc_gl_total(handle));
  }
  if (memtrack_proc_gl_pss) {
    response_ptr += sprintf(response_ptr, "gl_pss %zd\n",
                            memtrack_proc_gl_pss(handle));
  }
  if (memtrack_proc_other_total) {
    response_ptr += sprintf(response_ptr, "other_total %zd\n",
                            memtrack_proc_other_total(handle));
  }
  if (memtrack_proc_other_pss) {
    response_ptr += sprintf(response_ptr, "other_pss %zd\n",
                            memtrack_proc_other_pss(handle));
  }

  memtrack_proc_destroy(handle);
  send_response(client_sock, response);
}

static void daemonize() {
  pid_t pid;

  pid = fork();
  if (pid < 0)
    exit_with_failure("fork");
  if (pid > 0) {
    // Main process keeps TTY while intermediate child do daemonization
    // because adb can immediately kill a process disconnected from adb's TTY.
    int ignore;
    wait(&ignore);
    exit(EXIT_SUCCESS);
  }

  if (setsid() == -1)
    exit_with_failure("setsid");

  chdir("/");
  umask(0);
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  open("/dev/null", O_RDONLY);
  open("/dev/null", O_WRONLY);
  open("/dev/null", O_RDWR);

  pid = fork();
  if (pid < 0)
    exit_with_failure("fork");
  if (pid > 0)
    exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
  int res;

  if (getuid() != 0) {
    fprintf(stderr, "FATAL: %s must be run as root!\n", argv[0]);
    return EXIT_FAILURE;
  }

  void* const libhandle = dlopen("libmemtrack.so", RTLD_GLOBAL | RTLD_NOW);
  if (!libhandle)
    exit_with_failure("dlopen() libmemtrack.so");
  memtrack_init = (memtrack_init_t)dlsym(libhandle, "memtrack_init");
  memtrack_proc_new =
      (memtrack_proc_new_t)dlsym(libhandle, "memtrack_proc_new");
  memtrack_proc_destroy =
      (memtrack_proc_destroy_t)dlsym(libhandle, "memtrack_proc_destroy");
  memtrack_proc_get =
      (memtrack_proc_get_t)dlsym(libhandle, "memtrack_proc_get");
  memtrack_proc_graphics_total = (memtrack_proc_graphics_total_t)dlsym(
      libhandle, "memtrack_proc_graphics_total");
  memtrack_proc_graphics_pss = (memtrack_proc_graphics_pss_t)dlsym(
      libhandle, "memtrack_proc_graphics_pss");
  memtrack_proc_gl_total =
      (memtrack_proc_gl_total_t)dlsym(libhandle, "memtrack_proc_gl_total");
  memtrack_proc_gl_pss =
      (memtrack_proc_gl_pss_t)dlsym(libhandle, "memtrack_proc_gl_pss");
  memtrack_proc_other_total = (memtrack_proc_other_total_t)dlsym(
      libhandle, "memtrack_proc_other_total");
  memtrack_proc_other_pss =
      (memtrack_proc_other_pss_t)dlsym(libhandle, "memtrack_proc_other_pss");

  if (!memtrack_proc_new || !memtrack_proc_destroy || !memtrack_proc_get) {
    exit_with_failure("dlsym() libmemtrack.so");
  }

  const int server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (server_fd < 0)
    exit_with_failure("socket");

  /* Initialize the socket */
  struct sockaddr_un server_addr;
  init_memtrack_server_addr(&server_addr);

  /* Shutdown previously running instances if any. */
  int i;
  for (i = 0; i < 3; ++i) {
    res = bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (res && errno == EADDRINUSE) {
      send_shutdown_request(&server_addr);
      usleep(250000);
      continue;
    }
    break;
  }

  if (res)
    exit_with_failure("bind");

  if (argc > 1 && strcmp(argv[1], "-d") == 0)
    daemonize();

  long pid = getpid();
  fprintf(stderr, "pid=%ld\n", pid);
  __android_log_print(ANDROID_LOG_INFO, kLogTag, "pid=%ld\n", pid);

  if (memtrack_init) {
    res = memtrack_init();
    if (res == -ENOENT) {
      exit_with_failure("Unable to load memtrack module in libhardware. "
                        "Probably implementation is missing in this ROM.");
    } else if (res != 0) {
      exit_with_failure("memtrack_init() returned non-zero status.");
    }
  }

  if (listen(server_fd, 128 /* max number of queued requests */))
    exit_with_failure("listen");

  for (;;) {
    struct sockaddr_un client_addr;
    socklen_t len = sizeof(client_addr);
    int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &len);
    handle_one_request(client_sock);
    close(client_sock);
  }

  return EXIT_SUCCESS;
}
