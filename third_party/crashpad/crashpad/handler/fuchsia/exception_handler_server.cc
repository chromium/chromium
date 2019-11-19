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

#include "handler/fuchsia/exception_handler_server.h"

#include <lib/zx/exception.h>
#include <lib/zx/time.h>
#include <zircon/syscalls/exception.h>

#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "handler/fuchsia/crash_report_exception_handler.h"

namespace crashpad {

ExceptionHandlerServer::ExceptionHandlerServer(zx::job root_job,
                                               zx::channel exception_channel)
    : root_job_(std::move(root_job)),
      exception_channel_(std::move(exception_channel)) {}

ExceptionHandlerServer::~ExceptionHandlerServer() = default;

void ExceptionHandlerServer::Run(CrashReportExceptionHandler* handler) {
  while (true) {
    zx_signals_t signals;
    zx_status_t status = exception_channel_.wait_one(
        ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED,
        zx::time::infinite(),
        &signals);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_port_wait, aborting";
      return;
    }

    if (signals & ZX_CHANNEL_READABLE) {
      zx_exception_info_t info;
      zx::exception exception;
      status = exception_channel_.read(0,
                                       &info,
                                       exception.reset_and_get_address(),
                                       sizeof(info),
                                       1,
                                       nullptr,
                                       nullptr);
      if (status != ZX_OK) {
        ZX_LOG(ERROR, status) << "zx_channel_read, aborting";
        return;
      }

      zx::process process;
      status = exception.get_process(&process);
      if (status != ZX_OK) {
        ZX_LOG(ERROR, status) << "zx_exception_get_process, aborting";
        return;
      }

      zx::thread thread;
      status = exception.get_thread(&thread);
      if (status != ZX_OK) {
        ZX_LOG(ERROR, status) << "zx_exception_get_thread, aborting";
        return;
      }

      bool result =
          handler->HandleException(std::move(process), std::move(thread));
      if (!result) {
        LOG(ERROR) << "HandleException failed";
      }
    } else {
      // Job terminated, exit the loop.
      return;
    }
  }
}

}  // namespace crashpad
