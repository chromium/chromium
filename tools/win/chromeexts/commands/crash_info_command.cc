// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/win/chromeexts/commands/crash_info_command.h"

#include <windows.h>

#include <dbgeng.h>

#include <cstring>
#include <ctime>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/minidump/minidump_extensions.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"

namespace tools {
namespace win {
namespace chromeexts {

namespace {

template <typename T, size_t N>
constexpr size_t countof(T const (&)[N]) {
  return N;
}

// Get a readable string to represent a process integrity level.
const char* ProcessIntegrityString(ULONG32 integrity_level) {
  if (integrity_level < SECURITY_MANDATORY_LOW_RID) {
    return "Untrusted";
  }
  if (integrity_level < SECURITY_MANDATORY_MEDIUM_RID) {
    return "Low";
  }
  if (integrity_level < SECURITY_MANDATORY_MEDIUM_PLUS_RID) {
    return "Medium";
  }
  if (integrity_level < SECURITY_MANDATORY_HIGH_RID) {
    return "Medium Plus";
  }
  if (integrity_level < SECURITY_MANDATORY_SYSTEM_RID) {
    return "High";
  }
  if (integrity_level < SECURITY_MANDATORY_PROTECTED_PROCESS_RID) {
    return "System";
  }
  return "Protected Process";
}

}  // namespace

CrashInfoCommand::CrashInfoCommand() = default;

CrashInfoCommand::~CrashInfoCommand() = default;

HRESULT CrashInfoCommand::Execute() {
  ULONG debuggee_class;
  ULONG debuggee_qualifier;
  GetDebugClientAs<IDebugControl>()->GetDebuggeeType(&debuggee_class,
                                                     &debuggee_qualifier);

  // If the debug target is a minidump, it may be a crashpad dump.
  bool crashpad_dump = debuggee_class == DEBUG_CLASS_USER_WINDOWS &&
                       debuggee_qualifier == DEBUG_USER_WINDOWS_SMALL_DUMP;

  crashpad::MinidumpCrashpadInfo info;
  HRESULT hr;
  size_t bytes_read;

  if (crashpad_dump) {
    // Read the crashpad stream to verify this is a crashpad dump.
    hr = GetDebugClientAs<IDebugClient5>()->QueryInterface(
        IID_PPV_ARGS(&debug_advanced_));
    if (FAILED(hr)) {
      PrintErrorf("QI for IDebugAdvanced3: %08X\n", hr);
      return hr;
    }

    hr = ReadFromDumpStream(
        crashpad::MinidumpStreamType::kMinidumpStreamTypeCrashpadInfo, 0,
        sizeof(info), &info, &bytes_read);
    if (hr == E_NOINTERFACE) {
      // The request returns E_NOINTERFACE when the requested stream
      // type isn't found.
      crashpad_dump = false;
    } else if (FAILED(hr)) {
      PrintErrorf("Reading crashpad info: %08X\n", hr);
      return hr;
    }
  }

  if (!crashpad_dump) {
    PrintErrorf("This doesn't look like a crashpad dump.\n");
    return S_OK;
  }

  Printf("CrashpadInfo version: %d\n", info.version);
  Printf("Report ID: %s\n", info.report_id.ToString().c_str());
  Printf("Client ID: %s\n", info.client_id.ToString().c_str());

  DisplayMiscInfo();
  DisplayAnnotations();

  return S_OK;
}

void CrashInfoCommand::DisplayAnnotations() {
  // Find the dump file and re-open it directly.  Reading by streams doesn't
  // allow access to the extra data written by crashpad at the end of many
  // streams.
  std::unique_ptr<crashpad::FileReaderInterface> dump_file_reader =
      OpenDumpFileReader();
  if (!dump_file_reader) {
    PrintErrorf("Failed to open dump reader\n");
    return;
  }
  crashpad::ProcessSnapshotMinidump snapshot;
  if (!snapshot.Initialize(dump_file_reader.get())) {
    PrintErrorf("Failed to construct process snapshot\n");
    return;
  }
  Printf("\nDump annotations:\n\n");
  Printf("Process annotations:\n");
  for (const auto& kv : snapshot.AnnotationsSimpleMap()) {
    PrintfWithIndent(1, "%s = %s\n", kv.first.c_str(), kv.second.c_str());
  }
  constexpr char kModuleHeader[] = "Annotations for module: %s\n";
  for (const crashpad::ModuleSnapshot* module : snapshot.Modules()) {
    bool printed_header = false;
    if (module->AnnotationsSimpleMap().size() > 0) {
      Printf(kModuleHeader, module->Name().c_str());
      printed_header = true;
      for (const auto& kv : module->AnnotationsSimpleMap()) {
        PrintfWithIndent(1, "%s = %s\n", kv.first.c_str(), kv.second.c_str());
      }
    }
    if (module->AnnotationsVector().size() > 0) {
      if (!printed_header) {
        Printf(kModuleHeader, module->Name().c_str());
        printed_header = true;
      }
      PrintfWithIndent(1, "vector:\n");
      for (std::string annotation : module->AnnotationsVector()) {
        PrintfWithIndent(2, "%s\n", annotation.c_str());
      }
    }
    if (module->AnnotationObjects().size() > 0) {
      if (!printed_header) {
        Printf(kModuleHeader, module->Name().c_str());
        printed_header = true;
      }
      for (const crashpad::AnnotationSnapshot& annotation :
           module->AnnotationObjects()) {
        PrintfWithIndent(1, "%s = ", annotation.name.c_str());
        if (annotation.type ==
            static_cast<uint16_t>(crashpad::Annotation::Type::kString)) {
          std::string value(
              reinterpret_cast<const char*>(annotation.value.data()),
              annotation.value.size());
          Printf("%s\n", value.c_str());
        } else if (annotation.type >
                   static_cast<uint16_t>(
                       crashpad::Annotation::Type::kUserDefinedStart)) {
          Printf("user defined - type: %d, size: %d\n",
                 annotation.type -
                     static_cast<uint16_t>(
                         crashpad::Annotation::Type::kUserDefinedStart),
                 annotation.value.size());
        }
      }
    }
  }
}

void CrashInfoCommand::DisplayMiscInfo() {
  HRESULT hr;
  size_t bytes_read;
  MINIDUMP_MISC_INFO_5 misc_info;
  memset(&misc_info, 0, sizeof(misc_info));
  hr = ReadFromDumpStream(
      crashpad::MinidumpStreamType::kMinidumpStreamTypeMiscInfo, 0,
      sizeof(misc_info), &misc_info, &bytes_read);
  if (FAILED(hr)) {
    PrintErrorf("Reading misc info: %08X\n", hr);
    return;
  }

  if (misc_info.Flags1 & MINIDUMP_MISC1_PROCESS_ID) {
    Printf("Process ID: %ld\n", misc_info.ProcessId);
  }
  if (misc_info.Flags1 & MINIDUMP_MISC1_PROCESS_TIMES) {
    time_t create_time = misc_info.ProcessCreateTime;
    Printf("Create time: %s", std::ctime(&create_time));
    Printf("User time: %us\n", misc_info.ProcessUserTime);
    Printf("Kernel time: %us\n", misc_info.ProcessKernelTime);
  }
  if (misc_info.Flags1 & MINIDUMP_MISC1_PROCESSOR_POWER_INFO) {
    Printf("ProcessorMaxMhz: %u\n", misc_info.ProcessorMaxMhz);
    Printf("ProcessorCurrentMhz: %u\n", misc_info.ProcessorCurrentMhz);
    Printf("ProcessorMhzLimit: %u\n", misc_info.ProcessorMhzLimit);
    Printf("ProcessorMaxIdleState: %u\n", misc_info.ProcessorMaxIdleState);
    Printf("ProcessorCurrentIdleState: %u\n", misc_info.ProcessorMaxIdleState);
  }
  if (misc_info.Flags1 & MINIDUMP_MISC3_PROCESS_INTEGRITY) {
    Printf("ProcessIntegrityLevel: %s\n",
           ProcessIntegrityString(misc_info.ProcessIntegrityLevel));
  }
  if (misc_info.Flags1 & MINIDUMP_MISC3_PROCESS_EXECUTE_FLAGS) {
    Printf("ProcessExcecuteFlags: 0x%08x\n",
           ProcessIntegrityString(misc_info.ProcessExecuteFlags));
  }
  if (misc_info.Flags1 & MINIDUMP_MISC3_PROTECTED_PROCESS) {
    Printf("ProtectedProcess: 0x%08x\n",
           ProcessIntegrityString(misc_info.ProcessExecuteFlags));
  }
  if (misc_info.Flags1 & MINIDUMP_MISC3_TIMEZONE) {
    Printf("Standard Time Zone: %s\n",
           base::SysWideToNativeMB(
               std::wstring(misc_info.TimeZone.StandardName,
                            countof(misc_info.TimeZone.StandardName)))
               .c_str());
    Printf("Daylight Time Zone: %s\n",
           base::SysWideToNativeMB(
               std::wstring(misc_info.TimeZone.DaylightName,
                            countof(misc_info.TimeZone.DaylightName)))
               .c_str());
  }
  if (misc_info.Flags1 & MINIDUMP_MISC4_BUILDSTRING) {
    Printf("BuildString: %s\n",
           base::SysWideToNativeMB(std::wstring(misc_info.BuildString,
                                                countof(misc_info.BuildString)))
               .c_str());
    Printf("DbgBldStr: %s\n",
           base::SysWideToNativeMB(
               std::wstring(misc_info.DbgBldStr, countof(misc_info.DbgBldStr)))
               .c_str());
  }
  if (misc_info.Flags1 & MINIDUMP_MISC5_PROCESS_COOKIE) {
    Printf("Process Cookie: 0x%08x\n", misc_info.ProcessCookie);
  }
}

std::unique_ptr<crashpad::FileReaderInterface>
CrashInfoCommand::OpenDumpFileReader() {
  ULONG64 wide_dump_file_handle;
  ULONG dump_type;
  HRESULT hr = GetDebugClientAs<IDebugClient5>()->GetDumpFile(
      0, nullptr, 0, nullptr, &wide_dump_file_handle, &dump_type);
  if (FAILED(hr)) {
    PrintErrorf("getting dump file handle: %08x\n", hr);
    return nullptr;
  }

  crashpad::FileHandle dump_file_handle =
      reinterpret_cast<crashpad::FileHandle>(wide_dump_file_handle);
  return std::make_unique<crashpad::WeakFileHandleFileReader>(dump_file_handle);
}

HRESULT CrashInfoCommand::ReadFromDumpStream(uint32_t stream_type,
                                             uint64_t offset,
                                             size_t max_read,
                                             void* bytes,
                                             size_t* bytes_read) {
  *bytes_read = 0;

  DEBUG_READ_USER_MINIDUMP_STREAM request;
  memset(&request, 0, sizeof(request));
  request.StreamType = stream_type;
  request.Offset = offset;
  request.Buffer = bytes;
  request.BufferSize = max_read;

  HRESULT hr =
      debug_advanced_->Request(DEBUG_REQUEST_READ_USER_MINIDUMP_STREAM,
                               &request, sizeof(request), nullptr, 0, nullptr);
  if (SUCCEEDED(hr)) {
    *bytes_read = request.BufferUsed;
  }

  return hr;
}

}  // namespace chromeexts
}  // namespace win
}  // namespace tools
