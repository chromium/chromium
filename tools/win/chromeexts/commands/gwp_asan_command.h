// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_WIN_CHROMEEXTS_COMMANDS_GWP_ASAN_COMMAND_H_
#define TOOLS_WIN_CHROMEEXTS_COMMANDS_GWP_ASAN_COMMAND_H_

#include <streambuf>

#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "third_party/crashpad/crashpad/util/file/file_reader.h"
#include "tools/win/chromeexts/chrome_exts_command.h"
#include "tools/win/chromeexts/commands/task_trace_utils.h"

namespace tools::win::chromeexts {

struct membuf : std::streambuf {
  membuf(char* begin, char* end) { this->setg(begin, begin, end); }

  membuf() {}
};

class GwpAsanCommand : public ChromeExtsCommand {
 public:
  GwpAsanCommand();
  ~GwpAsanCommand() override;

 protected:
  HRESULT Execute() override;
  HRESULT InitPlatformID();
  HRESULT ReadFromDumpStream(uint32_t stream_type,
                             std::vector<char>* buffer,
                             uint64_t offset);
  HRESULT GetBaseAddress(ULONG64* base_address);
  HRESULT UseWinDbgSymbolize(uint64_t* stack_address, int stack_trace_size);
  HRESULT SymbolizeStackTrace(gwp_asan::Crash_AllocationInfo& allocation,
                              ULONG64* base_address);
  HRESULT ReadSymbols(std::string hexstring, std::string* json_string);
  void PrintErrorType(const int& error_type);

  // Prints task trace entries with DML-clickable links.
  void PrintTaskTraces(const std::vector<TaskTraceEntry>& traces);
  // Symbolizes a single address for task trace output.
  void SymbolizeTaskAddress(uint64_t address, int frame_index);

 private:
  ComPtr<IDebugSymbols3> debug_symbols_;
  ULONG platform_id_{0};
};

}  // namespace tools::win::chromeexts

#endif  // TOOLS_WIN_CHROMEEXTS_COMMANDS_GWP_ASAN_COMMAND_H_
