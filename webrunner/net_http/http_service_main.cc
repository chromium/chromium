// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "webrunner/net_http/http_service_impl.h"

int main(int argc, char** argv) {
  // Instantiate various global structures.
  base::TaskScheduler::CreateAndStartWithDefaultParams("HTTP Service");
  base::CommandLine::Init(argc, argv);
  base::MessageLoopForIO loop;
  base::AtExitManager exit_manager;

  // Bind the parent-supplied ServiceDirectory-request to a directory and
  // publish the HTTP service into it.
  base::fuchsia::ServiceDirectory* directory =
      base::fuchsia::ServiceDirectory::GetDefault();
  net_http::HttpServiceImpl http_service;
  base::fuchsia::ScopedServiceBinding<::fuchsia::net::oldhttp::HttpService>
      binding(directory, &http_service);

  base::RunLoop run_loop;

  // The main thread loop will be terminated when there are no more clients
  // connected to this service. The system service manager will restart the
  // service on demand as needed.
  binding.SetOnLastClientCallback(
      base::BindOnce(&base::RunLoop::Quit, base::Unretained(&run_loop)));
  run_loop.Run();

  return 0;
}
