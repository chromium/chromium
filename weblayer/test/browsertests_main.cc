// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/launcher/test_launcher.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/network_service_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "weblayer/test/test_launcher_delegate_impl.h"
#include "weblayer/utility/content_utility_client_impl.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs();

  // Set up a working test environment for the network service in case it's
  // used. Only create this object in the utility process, so that its members
  // don't interfere with other test objects in the browser process.
  std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper;
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType) == switches::kUtilityProcess) {
    network_service_test_helper =
        std::make_unique<content::NetworkServiceTestHelper>();
    weblayer::ContentUtilityClientImpl::
        SetNetworkBinderCreationCallbackForTests(base::BindRepeating(
            [](content::NetworkServiceTestHelper* helper,
               service_manager::BinderRegistry* registry) {
              helper->RegisterNetworkBinders(registry);
            },
            network_service_test_helper.get()));
  }
  weblayer::TestLauncherDelegateImpl launcher_delegate;
  return content::LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
