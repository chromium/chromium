// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "handler/linux/cros_crash_report_exception_handler.h"

#include <vector>

#include "base/logging.h"
#include "client/settings.h"
#include "handler/linux/capture_snapshot.h"
#include "handler/minidump_to_upload_parameters.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/linux/process_snapshot_linux.h"
#include "snapshot/minidump/process_snapshot_minidump.h"
#include "snapshot/sanitized/process_snapshot_sanitized.h"
#include "util/file/file_writer.h"
#include "util/linux/direct_ptrace_connection.h"
#include "util/linux/ptrace_client.h"
#include "util/misc/metrics.h"
#include "util/misc/uuid.h"
#include "util/posix/double_fork_and_exec.h"

namespace crashpad {

namespace {

// Returns the process name for a pid.
const std::string GetProcessNameFromPid(pid_t pid) {
  // Symlink to process binary is at /proc/###/exe.
  std::string link_path = "/proc/" + std::to_string(pid) + "/exe";

  constexpr int kMaxSize = 4096;
  std::unique_ptr<char[]> buf(new char[kMaxSize]);
  ssize_t size = readlink(link_path.c_str(), buf.get(), kMaxSize);
  std::string result;
  if (size < 0) {
    PLOG(ERROR) << "Failed to readlink " << link_path;
  } else {
    result.assign(buf.get(), size);
    size_t last_slash_pos = result.rfind('/');
    if (last_slash_pos != std::string::npos) {
      result = result.substr(last_slash_pos + 1);
    }
  }
  return result;
}

bool WriteAnnotationsAndMinidump(
    const std::map<std::string, std::string>& parameters,
    MinidumpFileWriter& minidump,
    FileWriter& file_writer) {
  for (const auto& kv : parameters) {
    if (kv.first.find(':') != std::string::npos) {
      LOG(ERROR) << "Annotation key cannot have ':' in it " << kv.first;
      return false;
    }
    if (!file_writer.Write(kv.first.c_str(), strlen(kv.first.c_str()))) {
      return false;
    }
    if (!file_writer.Write(":", 1)) {
      return false;
    }
    size_t value_size = strlen(kv.second.c_str());
    std::string value_size_str = std::to_string(value_size);
    if (!file_writer.Write(value_size_str.c_str(), value_size_str.size())) {
      return false;
    }
    if (!file_writer.Write(":", 1)) {
      return false;
    }
    if (!file_writer.Write(kv.second.c_str(), strlen(kv.second.c_str()))) {
      return false;
    }
  }

  static constexpr char kMinidumpName[] =
      "upload_file_minidump\"; filename=\"dump\":";
  if (!file_writer.Write(kMinidumpName, sizeof(kMinidumpName) - 1)) {
    return false;
  }
  crashpad::FileOffset dump_size_start_offset = file_writer.Seek(0, SEEK_CUR);
  if (dump_size_start_offset < 0) {
    LOG(ERROR) << "Failed to get minidump size start offset";
    return false;
  }
  static constexpr char kMinidumpLengthFilling[] = "00000000000000000000:";
  if (!file_writer.Write(kMinidumpLengthFilling,
                         sizeof(kMinidumpLengthFilling) - 1)) {
    return false;
  }
  crashpad::FileOffset dump_start_offset = file_writer.Seek(0, SEEK_CUR);
  if (dump_start_offset < 0) {
    LOG(ERROR) << "Failed to get minidump start offset";
    return false;
  }
  if (!minidump.WriteEverything(&file_writer)) {
    return false;
  }
  crashpad::FileOffset dump_end_offset = file_writer.Seek(0, SEEK_CUR);
  if (dump_end_offset < 0) {
    LOG(ERROR) << "Failed to get minidump end offset";
    return false;
  }

  size_t dump_data_size = dump_end_offset - dump_start_offset;
  std::string dump_data_size_str = std::to_string(dump_data_size);
  file_writer.Seek(dump_size_start_offset + strlen(kMinidumpLengthFilling) - 1 -
                       dump_data_size_str.size(),
                   SEEK_SET);
  if (!file_writer.Write(dump_data_size_str.c_str(),
                         dump_data_size_str.size())) {
    return false;
  }
  return true;
}

}  // namespace

CrosCrashReportExceptionHandler::CrosCrashReportExceptionHandler(
    CrashReportDatabase* database,
    const std::map<std::string, std::string>* process_annotations,
    const UserStreamDataSources* user_stream_data_sources)
    : database_(database),
      process_annotations_(process_annotations),
      user_stream_data_sources_(user_stream_data_sources) {}

CrosCrashReportExceptionHandler::~CrosCrashReportExceptionHandler() = default;

bool CrosCrashReportExceptionHandler::HandleException(
    pid_t client_process_id,
    uid_t client_uid,
    const ExceptionHandlerProtocol::ClientInformation& info,
    VMAddress requesting_thread_stack_address,
    pid_t* requesting_thread_id,
    UUID* local_report_id) {
  Metrics::ExceptionEncountered();

  DirectPtraceConnection connection;
  if (!connection.Initialize(client_process_id)) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kDirectPtraceFailed);
    return false;
  }

  return HandleExceptionWithConnection(&connection,
                                       info,
                                       client_uid,
                                       requesting_thread_stack_address,
                                       requesting_thread_id,
                                       local_report_id);
}

bool CrosCrashReportExceptionHandler::HandleExceptionWithBroker(
    pid_t client_process_id,
    uid_t client_uid,
    const ExceptionHandlerProtocol::ClientInformation& info,
    int broker_sock,
    UUID* local_report_id) {
  Metrics::ExceptionEncountered();

  PtraceClient client;
  if (!client.Initialize(broker_sock, client_process_id)) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kBrokeredPtraceFailed);
    return false;
  }

  return HandleExceptionWithConnection(
      &client, info, client_uid, 0, nullptr, local_report_id);
}

bool CrosCrashReportExceptionHandler::HandleExceptionWithConnection(
    PtraceConnection* connection,
    const ExceptionHandlerProtocol::ClientInformation& info,
    uid_t client_uid,
    VMAddress requesting_thread_stack_address,
    pid_t* requesting_thread_id,
    UUID* local_report_id) {
  std::unique_ptr<ProcessSnapshotLinux> process_snapshot;
  std::unique_ptr<ProcessSnapshotSanitized> sanitized_snapshot;
  if (!CaptureSnapshot(connection,
                       info,
                       *process_annotations_,
                       client_uid,
                       requesting_thread_stack_address,
                       requesting_thread_id,
                       &process_snapshot,
                       &sanitized_snapshot)) {
    return false;
  }

  UUID client_id;
  Settings* const settings = database_->GetSettings();
  if (settings) {
    // If GetSettings() or GetClientID() fails, something else will log a
    // message and client_id will be left at its default value, all zeroes,
    // which is appropriate.
    settings->GetClientID(&client_id);
  }
  process_snapshot->SetClientID(client_id);

  UUID uuid;
  uuid.InitializeWithNew();
  process_snapshot->SetReportID(uuid);

  ProcessSnapshot* snapshot =
      sanitized_snapshot
          ? implicit_cast<ProcessSnapshot*>(sanitized_snapshot.get())
          : implicit_cast<ProcessSnapshot*>(process_snapshot.get());

  MinidumpFileWriter minidump;
  minidump.InitializeFromSnapshot(snapshot);
  AddUserExtensionStreams(user_stream_data_sources_, snapshot, &minidump);

  FileWriter file_writer;
  if (!file_writer.OpenMemfd(base::FilePath("/tmp/minidump"))) {
    Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kOpenMemfdFailed);
    return false;
  }

  std::map<std::string, std::string> parameters =
      BreakpadHTTPFormParametersFromMinidump(snapshot);
  // Used to differentiate between breakpad and crashpad while the switch is
  // ramping up.
  parameters.emplace("crash_library", "crashpad");

  if (!WriteAnnotationsAndMinidump(parameters, minidump, file_writer)) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kMinidumpWriteFailed);
    return false;
  }

  // CrOS uses crash_reporter instead of Crashpad to report crashes.
  // crash_reporter needs to know the pid and uid of the crashing process.
  std::vector<std::string> argv({"/sbin/crash_reporter"});

  argv.push_back("--chrome_memfd=" + std::to_string(file_writer.fd()));
  argv.push_back("--pid=" + std::to_string(*requesting_thread_id));
  argv.push_back("--uid=" + std::to_string(client_uid));
  std::string process_name = GetProcessNameFromPid(*requesting_thread_id);
  argv.push_back("--exe=" + (process_name.empty() ? "chrome" : process_name));

  if (info.crash_loop_before_time != 0) {
    argv.push_back("--crash_loop_before=" +
                   std::to_string(info.crash_loop_before_time));
  }
  if (!dump_dir_.empty()) {
    argv.push_back("--chrome_dump_dir=" + dump_dir_.value());
  }

  if (!DoubleForkAndExec(argv,
                         nullptr /* envp */,
                         file_writer.fd() /* preserve_fd */,
                         false /* use_path */,
                         nullptr /* child_function */)) {
    LOG(ERROR) << "DoubleForkAndExec failed";
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
    return false;
  }

  if (local_report_id != nullptr) {
    *local_report_id = uuid;
  }
  LOG(INFO) << "Successfully wrote report " << uuid.ToString();

  Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kSuccess);
  return true;
}

}  // namespace crashpad
