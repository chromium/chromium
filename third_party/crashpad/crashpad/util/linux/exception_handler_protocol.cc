// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "util/linux/exception_handler_protocol.h"

namespace crashpad {

ExceptionHandlerProtocol::ClientInformation::ClientInformation()
    : exception_information_address(0),
      sanitization_information_address(0)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
      , crash_loop_before_time(0)
#endif  // OS_LINUX || OS_CHROMEOS
{}

ExceptionHandlerProtocol::ClientToServerMessage::ClientToServerMessage()
    : version(kVersion),
      type(kTypeCrashDumpRequest),
      requesting_thread_stack_address(0),
      client_info() {}

}  // namespace crashpad
