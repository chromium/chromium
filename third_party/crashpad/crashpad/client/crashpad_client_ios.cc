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

#include "client/crashpad_client.h"

#include "build/buildflag.h"
#include "client/ios_handler/in_process_handler.h"

#if !BUILDFLAG(IS_IOS_TVOS)
#include "client/crash_handler_ios.h"
#else
#include "client/crash_handler_tvos.h"
#endif

namespace crashpad {

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

// static
bool CrashpadClient::StartCrashpadInProcessHandler(
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    ProcessPendingReportsObservationCallback callback) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  return crash_handler->Initialize(database, url, annotations, callback);
}

// static
void CrashpadClient::ProcessIntermediateDumps(
    const std::map<std::string, std::string>& annotations,
    const UserStreamDataSources* user_stream_sources) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->ProcessIntermediateDumps(annotations, user_stream_sources);
}

// static
void CrashpadClient::ProcessIntermediateDump(
    const base::FilePath& file,
    const std::map<std::string, std::string>& annotations) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->ProcessIntermediateDump(file, annotations);
}

// static
void CrashpadClient::StartProcessingPendingReports(
    UploadBehavior upload_behavior) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->StartProcessingPendingReports(upload_behavior);
}

// static
void CrashpadClient::DumpWithoutCrash(NativeCPUContext* context) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->DumpWithoutCrash(context, /*process_dump=*/true);
}

// static
void CrashpadClient::DumpWithoutCrashAndDeferProcessing(
    NativeCPUContext* context) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->DumpWithoutCrash(context, /*process_dump=*/false);
}

// static
void CrashpadClient::DumpWithoutCrashAndDeferProcessingAtPath(
    NativeCPUContext* context,
    const base::FilePath path) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->DumpWithoutCrashAtPath(context, path);
}

void CrashpadClient::ResetForTesting() {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->ResetForTesting();
}

void CrashpadClient::SetExceptionCallbackForTesting(void (*callback)()) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->SetExceptionCallbackForTesting(callback);
}

uint64_t CrashpadClient::GetThreadIdForTesting() {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  return crash_handler->GetThreadIdForTesting();
}

}  // namespace crashpad
