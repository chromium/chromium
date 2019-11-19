// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/device_listener.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "tools/android/forwarder2/command.h"
#include "tools/android/forwarder2/forwarder.h"
#include "tools/android/forwarder2/socket.h"

namespace forwarder2 {

// static
std::unique_ptr<DeviceListener> DeviceListener::Create(
    std::unique_ptr<Socket> host_socket,
    int listener_port,
    ErrorCallback error_callback) {
  std::unique_ptr<Socket> listener_socket(new Socket());
  std::unique_ptr<DeviceListener> device_listener;
  if (!listener_socket->BindTcp("", listener_port)) {
    LOG(ERROR) << "Device could not bind and listen to local port "
               << listener_port;
    SendCommand(command::BIND_ERROR, listener_port, host_socket.get());
    return device_listener;
  }
  // In case the |listener_port_| was zero, GetPort() will return the
  // currently (non-zero) allocated port for this socket.
  listener_port = listener_socket->GetPort();
  SendCommand(command::BIND_SUCCESS, listener_port, host_socket.get());
  device_listener.reset(
      new DeviceListener(std::move(listener_socket), std::move(host_socket),
                         listener_port, std::move(error_callback)));
  return device_listener;
}

DeviceListener::~DeviceListener() {
  DCHECK(deletion_task_runner_->RunsTasksInCurrentSequence());
  deletion_notifier_.Notify();
}

void DeviceListener::Start() {
  thread_.Start();
  AcceptNextClientSoon();
}

void DeviceListener::SetAdbDataSocket(std::unique_ptr<Socket> adb_data_socket) {
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeviceListener::OnAdbDataSocketReceivedOnInternalThread,
                     base::Unretained(this), std::move(adb_data_socket)));
}

DeviceListener::DeviceListener(std::unique_ptr<Socket> listener_socket,
                               std::unique_ptr<Socket> host_socket,
                               int port,
                               ErrorCallback error_callback)
    : self_deleter_helper_(this, std::move(error_callback)),
      listener_socket_(std::move(listener_socket)),
      host_socket_(std::move(host_socket)),
      listener_port_(port),
      deletion_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      thread_("DeviceListener") {
  CHECK(host_socket_.get());
  DCHECK(deletion_task_runner_.get());
  host_socket_->AddEventFd(deletion_notifier_.receiver_fd());
  listener_socket_->AddEventFd(deletion_notifier_.receiver_fd());
}

void DeviceListener::AcceptNextClientSoon() {
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DeviceListener::AcceptClientOnInternalThread,
                                base::Unretained(this)));
}

void DeviceListener::AcceptClientOnInternalThread() {
  device_data_socket_.reset(new Socket());
  if (!listener_socket_->Accept(device_data_socket_.get())) {
    if (listener_socket_->DidReceiveEvent()) {
      LOG(INFO) << "Received exit notification, stopped accepting clients.";
      OnInternalThreadError();
      return;
    }
    LOG(WARNING) << "Could not Accept in ListenerSocket.";
    SendCommand(command::ACCEPT_ERROR, listener_port_, host_socket_.get());
    OnInternalThreadError();
    return;
  }
  SendCommand(command::ACCEPT_SUCCESS, listener_port_, host_socket_.get());
  if (!ReceivedCommand(command::HOST_SERVER_SUCCESS,
                       host_socket_.get())) {
    SendCommand(command::ACK, listener_port_, host_socket_.get());
    LOG(ERROR) << "Host could not connect to server.";
    device_data_socket_->Close();
    if (host_socket_->has_error()) {
      LOG(ERROR) << "Adb Control connection lost. "
                 << "Listener port: " << listener_port_;
      OnInternalThreadError();
      return;
    }
    // It can continue if the host forwarder could not connect to the host
    // server but the control connection is still alive (no errors). The device
    // acknowledged that (above), and it can re-try later.
    AcceptNextClientSoon();
    return;
  }
}

void DeviceListener::OnAdbDataSocketReceivedOnInternalThread(
    std::unique_ptr<Socket> adb_data_socket) {
  DCHECK(adb_data_socket);
  SendCommand(command::ADB_DATA_SOCKET_SUCCESS, listener_port_,
              host_socket_.get());
  forwarders_manager_.CreateAndStartNewForwarder(std::move(device_data_socket_),
                                                 std::move(adb_data_socket));
  AcceptNextClientSoon();
}

void DeviceListener::OnInternalThreadError() {
  self_deleter_helper_.MaybeSelfDeleteSoon();
}

}  // namespace forwarder
