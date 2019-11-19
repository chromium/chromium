// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"

#if defined(OS_WIN)
#define PidToStringType base::NumberToString16
#define MESSAGE_DUMP_EXPORT __declspec(dllexport)
#else
#define PidToStringType base::NumberToString
#define MESSAGE_DUMP_EXPORT __attribute__((visibility("default")))
#endif

namespace ipc_fuzzer {

class IPCDump : public IPC::ChannelProxy::OutgoingMessageFilter {
 public:
  ~IPCDump() {
    base::FilePath::StringType pid_string =
        PidToStringType(base::Process::Current().Pid());
    base::FilePath output_file_path =
        dump_directory().Append(pid_string + FILE_PATH_LITERAL(".ipcdump"));

    MessageFile::Write(output_file_path, messages_);
  }

  IPC::Message* Rewrite(IPC::Message* message) override {
    messages_.push_back(std::make_unique<IPC::Message>(*message));
    return message;
  }

  base::FilePath dump_directory() const { return dump_directory_; }

  void set_dump_directory(const base::FilePath& dump_directory) {
    dump_directory_ = dump_directory;
  }

 private:
  MessageVector messages_;
  base::FilePath dump_directory_;
};

IPCDump g_ipcdump;

}  // namespace ipc_fuzzer

// Entry point avoiding mangled names.
extern "C" {
MESSAGE_DUMP_EXPORT IPC::ChannelProxy::OutgoingMessageFilter* GetFilter(void);
MESSAGE_DUMP_EXPORT void SetDumpDirectory(const base::FilePath& dump_directory);
}

IPC::ChannelProxy::OutgoingMessageFilter* GetFilter(void) {
  return &ipc_fuzzer::g_ipcdump;
}

void SetDumpDirectory(const base::FilePath& dump_directory) {
  ipc_fuzzer::g_ipcdump.set_dump_directory(dump_directory);
}
