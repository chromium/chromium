// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <cstdio>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "tools/android/forwarder2/common.h"
#include "tools/android/forwarder2/daemon.h"
#include "tools/android/forwarder2/host_controllers_manager.h"
#include "tools/android/forwarder2/pipe_notifier.h"

namespace forwarder2 {

// Needs to be global to be able to be accessed from the signal handler.
PipeNotifier* g_notifier = NULL;

namespace {

const char kLogFilePath[] = "/tmp/host_forwarder_log";
const char kDaemonIdentifier[] = "chrome_host_forwarder_daemon";

const int kBufSize = 256;

// Lets the daemon fetch the exit notifier file descriptor.
int GetExitNotifierFD() {
  DCHECK(g_notifier);
  return g_notifier->receiver_fd();
}

void KillHandler(int signal_number) {
  char buf[kBufSize];
  if (signal_number != SIGTERM && signal_number != SIGINT) {
    snprintf(buf, sizeof(buf), "Ignoring unexpected signal %d.", signal_number);
    SIGNAL_SAFE_LOG(WARNING, buf);
    return;
  }
  snprintf(buf, sizeof(buf), "Received signal %d.", signal_number);
  SIGNAL_SAFE_LOG(WARNING, buf);
  static int s_kill_handler_count = 0;
  CHECK(g_notifier);
  // If for some reason the forwarder get stuck in any socket waiting forever,
  // we can send a SIGKILL or SIGINT three times to force it die
  // (non-nicely). This is useful when debugging.
  ++s_kill_handler_count;
  if (!g_notifier->Notify() || s_kill_handler_count > 2)
    exit(1);
}

class ServerDelegate : public Daemon::ServerDelegate {
 public:
  explicit ServerDelegate(const std::string& adb_path)
      : adb_path_(adb_path),
        has_failed_(false),
        controllers_manager_(base::BindRepeating(&GetExitNotifierFD)) {}

  ServerDelegate(const ServerDelegate&) = delete;
  ServerDelegate& operator=(const ServerDelegate&) = delete;

  bool has_failed() const {
    return has_failed_ || controllers_manager_.has_failed();
  }

  // Daemon::ServerDelegate:
  void Init() override {
    LOG(INFO) << "Starting host process daemon (pid=" << getpid() << ")";
    DCHECK(!g_notifier);
    g_notifier = new PipeNotifier();
    signal(SIGTERM, KillHandler);
    signal(SIGINT, KillHandler);
  }

  void OnClientConnected(std::unique_ptr<Socket> client_socket) override {
    char buf[kBufSize];
    const int bytes_read = client_socket->Read(buf, sizeof(buf));
    if (bytes_read <= 0) {
      if (client_socket->DidReceiveEvent())
        return;
      PError("Read()");
      has_failed_ = true;
      return;
    }
    const base::Pickle command_pickle =
        base::Pickle::WithUnownedBuffer(base::as_bytes(
            base::span(buf, base::checked_cast<size_t>(bytes_read))));
    base::PickleIterator pickle_it(command_pickle);

    std::string device_serial;
    CHECK(pickle_it.ReadString(&device_serial));

    int command;
    if (!pickle_it.ReadInt(&command)) {
      client_socket->WriteString("Error: missing command\n");
      return;
    }

    int device_port;
    if (!pickle_it.ReadInt(&device_port))
      device_port = -1;

    int host_port;
    if (!pickle_it.ReadInt(&host_port))
      host_port = -1;
    controllers_manager_.HandleRequest(adb_path_, device_serial, command,
                                       device_port, host_port,
                                       std::move(client_socket));
  }

 private:
  std::string adb_path_;
  bool has_failed_;
  HostControllersManager controllers_manager_;
};

class ClientDelegate : public Daemon::ClientDelegate {
 public:
  explicit ClientDelegate(const base::Pickle& command_pickle)
      : command_pickle_(command_pickle), has_failed_(false) {}

  bool has_failed() const { return has_failed_; }

  // Daemon::ClientDelegate:
  void OnDaemonReady(Socket* daemon_socket) override {
    // Send the forward command to the daemon.
    CHECK_EQ(static_cast<long>(command_pickle_.size()),
             daemon_socket->WriteNumBytes(command_pickle_.data(),
                                          command_pickle_.size()));
    char buf[kBufSize];
    const int bytes_read = daemon_socket->Read(
        buf, sizeof(buf) - 1 /* leave space for null terminator */);
    CHECK_GT(bytes_read, 0);
    DCHECK(static_cast<size_t>(bytes_read) < sizeof(buf));
    buf[bytes_read] = 0;
    std::string_view msg(buf, bytes_read);
    if (base::StartsWith(msg, "ERROR")) {
      LOG(ERROR) << msg;
      has_failed_ = true;
      return;
    }
    printf("%s\n", buf);
  }

 private:
  const base::Pickle command_pickle_;
  bool has_failed_;
};

void ExitWithUsage() {
  std::cerr << "Usage: host_forwarder [options]\n\n"
               "Options:\n"
               "  --serial-id=[0-9A-Z]{16}]\n"
               "  --map DEVICE_PORT HOST_PORT\n"
               "  --unmap DEVICE_PORT\n"
               "  --unmap-all\n"
               "  --adb PATH_TO_ADB\n"
               "  --kill-server\n";
  exit(1);
}

int PortToInt(const std::string& s) {
  int value;
  // Note that 0 is a valid port (used for dynamic port allocation).
  if (!base::StringToInt(s, &value) || value < 0 ||
      value > std::numeric_limits<uint16_t>::max()) {
    LOG(ERROR) << "Could not convert string " << s << " to port";
    ExitWithUsage();
  }
  return value;
}

int RunHostForwarder(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  std::string adb_path = "adb";
  bool kill_server = false;

  base::Pickle pickle;
  pickle.WriteString(
      cmd_line.HasSwitch("serial-id") ?
          cmd_line.GetSwitchValueASCII("serial-id") : std::string());

  const std::vector<std::string> args = cmd_line.GetArgs();
  if (cmd_line.HasSwitch("kill-server")) {
    kill_server = true;
  } else if (cmd_line.HasSwitch("unmap")) {
    if (args.size() != 1)
      ExitWithUsage();
    pickle.WriteInt(UNMAP);
    pickle.WriteInt(PortToInt(args[0]));
  } else if (cmd_line.HasSwitch("unmap-all")) {
    pickle.WriteInt(UNMAP_ALL);
  } else if (cmd_line.HasSwitch("map")) {
    if (args.size() != 2)
      ExitWithUsage();
    pickle.WriteInt(MAP);
    pickle.WriteInt(PortToInt(args[0]));
    pickle.WriteInt(PortToInt(args[1]));
  } else {
    ExitWithUsage();
  }

  if (cmd_line.HasSwitch("adb")) {
    adb_path = cmd_line.GetSwitchValueASCII("adb");
  }

  if (kill_server && args.size() > 0)
    ExitWithUsage();

  ClientDelegate client_delegate(pickle);
  ServerDelegate daemon_delegate(adb_path);
  Daemon daemon(
      kLogFilePath, kDaemonIdentifier, &client_delegate, &daemon_delegate,
      &GetExitNotifierFD);

  if (kill_server)
    return !daemon.Kill();
  if (!daemon.SpawnIfNeeded())
    return 1;

  return client_delegate.has_failed() || daemon_delegate.has_failed();
}

}  // namespace
}  // namespace forwarder2

int main(int argc, char** argv) {
  return forwarder2::RunHostForwarder(argc, argv);
}
