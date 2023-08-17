// Copyright 2020 The Crashpad Authors
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

#include "util/mach/bootstrap.h"

#include <mach/mach.h>
#include <servers/bootstrap.h>

#include "base/apple/mach_logging.h"

namespace {

// This forms the internal implementation for BootstrapCheckIn() and
// BootstrapLookUp(), which follow the same logic aside from the routine called
// and the right type returned.

struct BootstrapCheckInTraits {
  using Type = base::apple::ScopedMachReceiveRight;
  static kern_return_t Call(mach_port_t bootstrap_port,
                            const char* service_name,
                            mach_port_t* service_port) {
    return bootstrap_check_in(bootstrap_port, service_name, service_port);
  }
  static constexpr char kName[] = "bootstrap_check_in";
};
constexpr char BootstrapCheckInTraits::kName[];

struct BootstrapLookUpTraits {
  using Type = base::apple::ScopedMachSendRight;
  static kern_return_t Call(mach_port_t bootstrap_port,
                            const char* service_name,
                            mach_port_t* service_port) {
    return bootstrap_look_up(bootstrap_port, service_name, service_port);
  }
  static constexpr char kName[] = "bootstrap_look_up";
};
constexpr char BootstrapLookUpTraits::kName[];

template <typename Traits>
typename Traits::Type BootstrapCheckInOrLookUp(
    const std::string& service_name) {
  // bootstrap_check_in() and bootstrap_look_up() silently truncate service
  // names longer than BOOTSTRAP_MAX_NAME_LEN. This check ensures that the name
  // will not be truncated.
  if (service_name.size() >= BOOTSTRAP_MAX_NAME_LEN) {
    LOG(ERROR) << Traits::kName << " " << service_name << ": name too long";
    return typename Traits::Type(MACH_PORT_NULL);
  }

  mach_port_t service_port;
  kern_return_t kr =
      Traits::Call(bootstrap_port, service_name.c_str(), &service_port);
  if (kr != BOOTSTRAP_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << Traits::kName << " " << service_name;
    service_port = MACH_PORT_NULL;
  }

  return typename Traits::Type(service_port);
}

}  // namespace

namespace crashpad {

base::apple::ScopedMachReceiveRight BootstrapCheckIn(
    const std::string& service_name) {
  return BootstrapCheckInOrLookUp<BootstrapCheckInTraits>(service_name);
}

base::apple::ScopedMachSendRight BootstrapLookUp(
    const std::string& service_name) {
  base::apple::ScopedMachSendRight send(
      BootstrapCheckInOrLookUp<BootstrapLookUpTraits>(service_name));

  // It’s possible to race the bootstrap server when the receive right
  // corresponding to the looked-up send right is destroyed immediately before
  // the bootstrap_look_up() call. If the bootstrap server believes that
  // |service_name| is still registered before processing the port-destroyed
  // notification sent to it by the kernel, it will respond to a
  // bootstrap_look_up() request with a send right that has become a dead name,
  // which will be returned to the bootstrap_look_up() caller, translated into
  // the caller’s IPC port name space, as the special MACH_PORT_DEAD port name.
  // Check for that and return MACH_PORT_NULL in its place, as though the
  // bootstrap server had fully processed the port-destroyed notification before
  // responding to bootstrap_look_up().
  if (send.get() == MACH_PORT_DEAD) {
    LOG(ERROR) << "bootstrap_look_up " << service_name << ": service is dead";
    send.reset();
  }

  return send;
}

base::apple::ScopedMachSendRight SystemCrashReporterHandler() {
  return BootstrapLookUp("com.apple.ReportCrash");
}

}  // namespace crashpad
