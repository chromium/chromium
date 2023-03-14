// Copyright 2015 The Crashpad Authors
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

#include "util/thread/thread_log_messages.h"

#include <sys/types.h>

#include "base/check_op.h"
#include "base/logging.h"

namespace crashpad {

namespace {

thread_local std::vector<std::string>* thread_local_log_messages;

bool LogMessageHandler(logging::LogSeverity severity,
                       const char* file_path,
                       int line,
                       size_t message_start,
                       const std::string& string) {
  if (thread_local_log_messages) {
    thread_local_log_messages->push_back(string);
  }

  // Don’t consume the message. Allow it to be logged as if nothing was set as
  // the log message handler.
  return false;
}

}  // namespace

ThreadLogMessages::ThreadLogMessages()
    : log_messages_(),
      reset_thread_local_log_messages_(&thread_local_log_messages,
                                       &log_messages_) {
  [[maybe_unused]] static bool initialized = [] {
    DCHECK(!logging::GetLogMessageHandler());
    logging::SetLogMessageHandler(LogMessageHandler);
    return true;
  }();
}

}  // namespace crashpad
