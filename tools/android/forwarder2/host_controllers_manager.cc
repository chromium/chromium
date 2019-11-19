// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/host_controllers_manager.h"

#include "base/bind.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "tools/android/forwarder2/util.h"

namespace forwarder2 {

HostControllersManager::HostControllersManager(
    base::RepeatingCallback<int()> exit_notifier_fd_callback)
    : controllers_(new HostControllerMap()),
      exit_notifier_fd_callback_(exit_notifier_fd_callback),
      has_failed_(false),
      weak_ptr_factory_(this) {}

HostControllersManager::~HostControllersManager() {
  if (!thread_.get())
    return;
  // Delete the controllers on the thread they were created on.
  thread_->task_runner()->DeleteSoon(FROM_HERE, controllers_.release());
}

void HostControllersManager::HandleRequest(
    const std::string& adb_path,
    const std::string& device_serial,
    int command,
    int device_port,
    int host_port,
    std::unique_ptr<Socket> client_socket) {
  // Lazy initialize so that the CLI process doesn't get this thread created.
  InitOnce();
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&HostControllersManager::HandleRequestOnInternalThread,
                     base::Unretained(this), adb_path, device_serial, command,
                     device_port, host_port, std::move(client_socket)));
}

// static
std::string HostControllersManager::MakeHostControllerMapKey(int adb_port,
                                                             int device_port) {
  return base::StringPrintf("%d:%d", adb_port, device_port);
}

void HostControllersManager::InitOnce() {
  if (thread_.get())
    return;
  at_exit_manager_.reset(new base::AtExitManager());
  thread_.reset(new base::Thread("HostControllersManagerThread"));
  thread_->Start();
}

// static
void HostControllersManager::DeleteHostController(
    const base::WeakPtr<HostControllersManager>& manager_ptr,
    std::unique_ptr<HostController> host_controller) {
  HostController* const controller = host_controller.release();
  HostControllersManager* const manager = manager_ptr.get();
  if (!manager) {
    // Note that |controller| is not leaked in this case since the host
    // controllers manager owns the controllers. If the manager was deleted
    // then all the controllers (including |controller|) were also deleted.
    return;
  }
  DCHECK(manager->thread_->task_runner()->RunsTasksInCurrentSequence());
  // Note that this will delete |controller| which is owned by the map.
  DeleteRefCountedValueInMap(
      MakeHostControllerMapKey(controller->adb_port(),
                               controller->device_port()),
      manager->controllers_.get());
}

void HostControllersManager::Map(const std::string& adb_path,
                                 const std::string& device_serial,
                                 int adb_port,
                                 int device_port,
                                 int host_port,
                                 Socket* client_socket) {
  if (host_port < 0) {
    SendMessage("ERROR: missing host port\n", client_socket);
    return;
  }
  const bool use_dynamic_port_allocation = device_port == 0;
  if (!use_dynamic_port_allocation) {
    const std::string controller_key =
        MakeHostControllerMapKey(adb_port, device_port);
    if (controllers_->find(controller_key) != controllers_->end()) {
      LOG(INFO) << "Already forwarding device port " << device_port
                << " to host port " << host_port;
      SendMessage(base::StringPrintf("%d:%d", device_port, host_port),
                  client_socket);
      return;
    }
  }
  // Create a new host controller.
  std::unique_ptr<HostController> host_controller(HostController::Create(
      device_serial, device_port, host_port, adb_port,
      exit_notifier_fd_callback_.Run(),
      base::BindOnce(&HostControllersManager::DeleteHostController,
                     weak_ptr_factory_.GetWeakPtr())));
  if (!host_controller.get()) {
    has_failed_ = true;
    SendMessage("ERROR: Connection to device failed.\n", client_socket);
    LogExistingControllers(client_socket);
    return;
  }
  // Get the current allocated port.
  device_port = host_controller->device_port();
  LOG(INFO) << "Forwarding device port " << device_port << " to host port "
            << host_port;
  const std::string msg = base::StringPrintf("%d:%d", device_port, host_port);
  if (!SendMessage(msg, client_socket))
    return;
  host_controller->Start();
  controllers_->emplace(MakeHostControllerMapKey(adb_port, device_port),
                        std::move(host_controller));
}

void HostControllersManager::Unmap(const std::string& adb_path,
                                   const std::string& device_serial,
                                   int adb_port,
                                   int device_port,
                                   Socket* client_socket) {
  // Remove the previously created host controller.
  const std::string controller_key =
      MakeHostControllerMapKey(adb_port, device_port);
  const bool controller_did_exist =
      DeleteRefCountedValueInMap(controller_key, controllers_.get());
  if (!controller_did_exist) {
    SendMessage("ERROR: could not unmap port.\n", client_socket);
    LogExistingControllers(client_socket);
  } else {
    SendMessage("OK", client_socket);
  }

  RemoveAdbPortForDeviceIfNeeded(adb_path, device_serial);
}

void HostControllersManager::UnmapAll(const std::string& adb_path,
                                      const std::string& device_serial,
                                      int adb_port,
                                      Socket* client_socket) {
  const std::string adb_port_str = base::StringPrintf("%d", adb_port);
  for (auto controller_key = controllers_->begin();
       controller_key != controllers_->end(); ++controller_key) {
    std::vector<std::string> pieces =
        base::SplitString(controller_key->first, ":", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_ALL);
    if (pieces.size() == 2) {
      if (pieces[0] == adb_port_str) {
        DeleteRefCountedValueInMapFromIterator(controller_key,
                                               controllers_.get());
      }
    } else {
      LOG(ERROR) << "Unexpected controller key: " << controller_key->first;
    }
  }

  RemoveAdbPortForDeviceIfNeeded(adb_path, device_serial);
  SendMessage("OK", client_socket);
}

void HostControllersManager::HandleRequestOnInternalThread(
    const std::string& adb_path,
    const std::string& device_serial,
    int command,
    int device_port,
    int host_port,
    std::unique_ptr<Socket> client_socket) {
  const int adb_port = GetAdbPortForDevice(adb_path, device_serial);
  if (adb_port < 0) {
    SendMessage(
        "ERROR: could not get adb port for device. You might need to add "
        "'adb' to your PATH or provide the device serial id.\n",
        client_socket.get());
    return;
  }
  switch (command) {
    case MAP:
      Map(adb_path, device_serial, adb_port, device_port, host_port,
          client_socket.get());
      break;
    case UNMAP:
      Unmap(adb_path, device_serial, adb_port, device_port,
            client_socket.get());
      break;
    case UNMAP_ALL:
      UnmapAll(adb_path, device_serial, adb_port, client_socket.get());
      break;
    default:
      SendMessage(
          base::StringPrintf("ERROR: unrecognized command %d\n", command),
          client_socket.get());
      break;
  }
}

void HostControllersManager::LogExistingControllers(Socket* client_socket) {
  SendMessage("ERROR: Existing controllers:\n", client_socket);
  for (const auto& controller : *controllers_) {
    SendMessage(base::StringPrintf("ERROR: %s\n", controller.first.c_str()),
                client_socket);
  }
}

bool HostControllersManager::Adb(const std::string& adb_path,
                                 const std::string& device_serial,
                                 const std::string& command,
                                 std::string* output_and_error) {
  // We use the vector version of GetAppOutputAndError rather than the
  // more standard base::CommandLine version because base::CommandLine
  // reorders the command s.t. switches precede arguments and doing so
  // here creates an invalid adb command.
  std::vector<std::string> adb_command{adb_path};
  if (!device_serial.empty()) {
    adb_command.push_back("-s");
    adb_command.push_back(device_serial);
  }
  const std::vector<std::string> split_command = base::SplitString(
      command, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  adb_command.insert(adb_command.end(), split_command.begin(),
                     split_command.end());
  return GetAppOutputAndError(adb_command, output_and_error);
}

void HostControllersManager::RemoveAdbPortForDeviceIfNeeded(
    const std::string& adb_path,
    const std::string& device_serial) {
  std::unordered_map<std::string, int>::const_iterator it =
      device_serial_to_adb_port_map_.find(device_serial);
  if (it == device_serial_to_adb_port_map_.end())
    return;

  int port = it->second;
  const std::string prefix = base::StringPrintf("%d:", port);
  for (auto others = controllers_->begin(); others != controllers_->end();
       ++others) {
    if (base::StartsWith(others->first, prefix, base::CompareCase::SENSITIVE))
      return;
  }
  // No other port is being forwarded to this device:
  // - Remove it from our internal serial -> adb port map.
  // - Remove from "adb forward" command.
  LOG(INFO) << "Device " << device_serial << " has no more ports.";
  device_serial_to_adb_port_map_.erase(device_serial);
  const std::string command =
      base::StringPrintf("forward --remove tcp:%d", port);
  std::string output;
  if (!Adb(adb_path, device_serial, command, &output)) {
    LOG(ERROR) << command << " failed. output: \"" << output << "\"";
  } else {
    LOG(INFO) << command << " (output: \"" << output << "\")";
  }
  // Wait for the socket to be fully unmapped.
  const std::string port_mapped_cmd = base::StringPrintf("lsof -nPi:%d", port);
  const std::vector<std::string> port_mapped_split_cmd = base::SplitString(
      port_mapped_cmd, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  const int poll_interval_us = 500 * 1000;
  int retries = 3;
  while (retries) {
    // lsof failure means the port was successfully unmapped.
    bool port_unmapped = !GetAppOutputAndError(port_mapped_split_cmd, &output);
    LOG(INFO) << "Device " << device_serial << " port " << port
              << (port_unmapped ? "" : " not") << " unmapped";
    if (port_unmapped)
      break;
    --retries;
    usleep(poll_interval_us);
  }
}

int HostControllersManager::GetAdbPortForDevice(
    const std::string adb_path,
    const std::string& device_serial) {
  std::unordered_map<std::string, int>::const_iterator it =
      device_serial_to_adb_port_map_.find(device_serial);
  if (it != device_serial_to_adb_port_map_.end())
    return it->second;
  Socket bind_socket;
  CHECK(bind_socket.BindTcp("127.0.0.1", 0));
  const int port = bind_socket.GetPort();
  bind_socket.Close();
  const std::string command = base::StringPrintf(
      "forward tcp:%d localabstract:chrome_device_forwarder", port);
  std::string output;
  if (!Adb(adb_path, device_serial, command, &output)) {
    LOG(ERROR) << command << " failed. output: " << output;
    return -1;
  }
  LOG(INFO) << command;
  device_serial_to_adb_port_map_[device_serial] = port;
  return port;
}

bool HostControllersManager::SendMessage(const std::string& msg,
                                         Socket* client_socket) {
  bool result = client_socket->WriteString(msg);
  DCHECK(result);
  if (!result)
    has_failed_ = true;
  return result;
}

bool HostControllersManager::GetAppOutputAndError(
    const std::vector<std::string>& argv,
    std::string* output) {
  return base::GetAppOutputAndError(argv, output);
}

}  // namespace forwarder2
