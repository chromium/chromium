// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/win/chromeexts/commands/gwp_asan_command.h"

#include <windows.h>

#include <dbgeng.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/gwp_asan/crash_handler/crash_handler.h"

namespace tools {
namespace win {
namespace chromeexts {

GwpAsanCommand::GwpAsanCommand() = default;

GwpAsanCommand::~GwpAsanCommand() = default;

HRESULT GwpAsanCommand::InitPlatformID() {
  ULONG win32_major, win32_minor;
  return GetDebugClientAs<IDebugControl4>()->GetSystemVersionValues(
      &platform_id_, &win32_major, &win32_minor, nullptr, nullptr);
}

HRESULT GwpAsanCommand::Execute() {
  ULONG debuggee_class;
  ULONG debuggee_qualifier;
  GetDebugClientAs<IDebugControl4>()->GetDebuggeeType(&debuggee_class,
                                                      &debuggee_qualifier);

  // If the debug target is a minidump, it may be a GWP-ASan dump.
  bool gwp_asan_dump = debuggee_class == DEBUG_CLASS_USER_WINDOWS &&
                       debuggee_qualifier == DEBUG_USER_WINDOWS_SMALL_DUMP;

  gwp_asan::Crash info;
  std::vector<char> buffer;
  HRESULT hr;

  if (gwp_asan_dump) {
    hr = ReadFromDumpStream(gwp_asan::internal::kGwpAsanMinidumpStreamType,
                            &buffer, 0);
    if (hr == E_NOINTERFACE) {
      // The request returns E_NOINTERFACE when the requested stream
      // type isn't found.
      gwp_asan_dump = false;
    } else if (FAILED(hr)) {
      PrintErrorf("Reading GWP-ASan info: %08X\n", hr);
      return hr;
    }
  }

  if (!gwp_asan_dump) {
    PrintErrorf("No GWP-ASan dump available.\n");
    return S_OK;
  }

  hr = InitPlatformID();
  if (FAILED(hr)) {
    PrintErrorf("Cannot determine the Platform");
    return hr;
  }

  ComPtr<IDebugControl3> debug_control3;
  hr = GetDebugClientAs<IDebugControl4>()->QueryInterface(
      IID_PPV_ARGS(&debug_control3));
  if (FAILED(hr)) {
    PrintErrorf("QI for IDebugControl3: %08X\n", hr);
    return hr;
  }
  hr = debug_control3->IsPointer64Bit();
  if (hr == S_FALSE) {
    Printf("Warning: GWP-ASan stream is not reliable for 32-bit\n");
  } else if (FAILED(hr)) {
    PrintErrorf("Cannot determine if effective processor uses 64 bit or not\n");
    return hr;
  }

  hr = GetDebugClientAs<IDebugControl4>()->QueryInterface(
      IID_PPV_ARGS(&debug_symbols_));
  if (FAILED(hr)) {
    PrintErrorf("QI for IDebugSymbols3: %08X\n", hr);
    return hr;
  }

  membuf stream_buffer(&buffer[0], &buffer[0] + buffer.size());
  std::istream istream_buffer(&stream_buffer);
  info.ParseFromIstream(&istream_buffer);

  Printf("\n");
  if (info.has_missing_metadata()) {
    if (info.missing_metadata()) {
      Printf("Missing metadata in GWP-ASan dump\n");
    }
  }
  if (info.has_internal_error()) {
    Printf("%-50s%s\n", "Internal error:", info.internal_error().c_str());
  }
  if (info.has_allocation()) {
    Printf("%-50s%d\n", "Allocation stack trace size:",
           info.allocation().stack_trace_size());
  }
  if (info.has_deallocation()) {
    Printf("%-50s%d\n", "Deallocation stack trace size:",
           info.deallocation().stack_trace_size());
  }
  if (info.has_allocation_address()) {
    Printf("%-50s%I64x\n", "Allocation address:", info.allocation_address());
  }
  if (info.has_allocation_size()) {
    Printf("%-50s%I64x\n", "Allocation size:", info.allocation_size());
  }
  if (info.has_error_type()) {
    PrintErrorType(info.error_type());
  }
  if (info.has_region_start()) {
    Printf("%-50s%I64x\n", "Region start:", info.region_start());
  }
  if (info.has_region_size()) {
    Printf("%-50s%I64x\n", "Region size:", info.region_size());
  }
  if (info.has_free_invalid_address()) {
    Printf("%-50s%x\n", "Free invalid address:", info.free_invalid_address());
  }
  if (info.has_allocator()) {
    Printf("%-50s", "Allocator Type:");
    switch (info.allocator()) {
      case gwp_asan::Crash_Allocator_MALLOC:
        Printf("Malloc\n");
        break;
      case gwp_asan::Crash_Allocator_PARTITIONALLOC:
        Printf("PartitionAlloc\n");
        break;
    }
  }
  if (info.has_mode()) {
    Printf("%-50s", "GWP-ASan Mode:");
    switch (info.mode()) {
      case gwp_asan::Crash_Mode_CLASSIC:
        Printf("Classic\n");
        break;
      case gwp_asan::Crash_Mode_LIGHTWEIGHT_DETECTOR_BRP:
        Printf("Lightweight UAF Detector (BRP sampling)\n");
        break;
      case gwp_asan::Crash_Mode_LIGHTWEIGHT_DETECTOR_RANDOM:
        Printf("Lightweight UAF Detector (random sampling)\n");
        break;
      case gwp_asan::Crash_Mode_EXTREME_LIGHTWEIGHT_DETECTOR:
        Printf("Extreme Lightweight UAF Detector\n");
        break;
      default:
        Printf("Unknown\n");
    }
  }

  ULONG64 base_address = 0;
  hr = GetBaseAddress(&base_address);
  if (hr == S_OK) {
    Printf("%-50s%I64x\n", "Base address:", base_address);
  }

  if (info.has_allocation()) {
    gwp_asan::Crash_AllocationInfo allocation = info.allocation();
    Printf("%-50s%I64u\n", "Allocation Thread ID:", allocation.thread_id());
    Printf("\nAllocation Stack Trace:\n\n");
    if (FAILED(SymbolizeStackTrace(allocation, &base_address))) {
      Printf("Error in Symbolizing Stack Trace");
    }
  }

  if (info.has_deallocation()) {
    gwp_asan::Crash_AllocationInfo deallocation = info.deallocation();
    Printf("%-50s%I64u\n", "Deallocation Thread ID:", deallocation.thread_id());
    Printf("\nDeallocation Stack Trace:\n\n");
    if (FAILED(SymbolizeStackTrace(deallocation, &base_address))) {
      Printf("Error in Symbolizing Stack Trace\n");
    }
  }

  Printf("\nEnd\n");

  return S_OK;
}

void GwpAsanCommand::PrintErrorType(const int& error_type) {
  Printf("%-50s", "Error type:");
  switch (error_type) {
    case gwp_asan::Crash::USE_AFTER_FREE:
      Printf("Use After Free");
      break;
    case gwp_asan::Crash::BUFFER_UNDERFLOW:
      Printf("Buffer Underflow");
      break;
    case gwp_asan::Crash::BUFFER_OVERFLOW:
      Printf("Buffer Overflow");
      break;
    case gwp_asan::Crash::DOUBLE_FREE:
      Printf("Double Free");
      break;
    case gwp_asan::Crash::UNKNOWN:
      Printf("Unknown");
      break;
    case gwp_asan::Crash::FREE_INVALID_ADDRESS:
      Printf("Free Invalid Address");
      break;
  }
  Printf("\n");
}

HRESULT GwpAsanCommand::GetBaseAddress(ULONG64* base_address) {
  PCSTR module_name = "chrome";
  ULONG index;

  // For MacOS
  if (platform_id_ == 33025) {
    module_name = "Google_Chrome_Framework";
  }

  return debug_symbols_->GetModuleByModuleName(module_name, 0, &index,
                                               base_address);
}

HRESULT GwpAsanCommand::UseWinDbgSymbolize(uint64_t* stack_address,
                                           int stack_trace_size) {
  constexpr ULONG METHOD_SIZE = 1024;
  char method_name[METHOD_SIZE], file_name[METHOD_SIZE];
  std::string file_name_str;
  ULONG line;
  size_t position;
  HRESULT hr;

  for (int i = 0; i < stack_trace_size; i++) {
    hr = debug_symbols_->GetNameByOffset(stack_address[i], method_name,
                                         METHOD_SIZE, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      Printf("%d\t0x%I64x\t%s", i, stack_address[i], method_name);
    } else {
      Printf("%d\t0x%I64x\t", i, stack_address[i]);
    }

    hr = debug_symbols_->GetLineByOffset(stack_address[i], &line, file_name,
                                         METHOD_SIZE, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      file_name_str = std::string(file_name);
      position = file_name_str.find("\\src\\");
      if (position != std::string::npos) {
        file_name_str = file_name_str.substr(position + 5, std::string::npos);
      }
      position = file_name_str.find("../../");
      if (position != std::string::npos) {
        file_name_str = file_name_str.substr(position + 6, std::string::npos);
      }
      base::ReplaceChars(file_name_str, "\\", "/", &file_name_str);
      Printf(" at %s:%d", file_name_str.c_str(), line);
    }
    Printf("\n");
  }
  Printf("\n");
  return S_OK;
}

HRESULT GwpAsanCommand::SymbolizeStackTrace(
    gwp_asan::Crash_AllocationInfo& allocation,
    ULONG64* base_address) {
  int stack_trace_size = allocation.stack_trace_size();
  uint64_t* stack_address = allocation.mutable_stack_trace()->mutable_data();

  if (platform_id_ == VER_PLATFORM_WIN32_NT || platform_id_ == 33025) {
    return UseWinDbgSymbolize(stack_address, stack_trace_size);
  }

  std::vector<std::string> stack_offset_addresses;
  std::string hexstring;
  char hex[15];
  uint64_t base = *base_address >> 32;
  uint64_t offset;

  for (int i = 0; i < stack_trace_size; i++) {
    if ((base & stack_address[i] >> 32) == base) {
      offset = stack_address[i] - *base_address;
      snprintf(hex, sizeof(hex), "0x%I64x", offset);
      std::string hex_address = std::string(hex);
      hexstring += " " + hex_address;
      stack_offset_addresses.push_back(hex_address);
    } else {
      snprintf(hex, sizeof(hex), "0x%I64x", stack_address[i]);
      stack_offset_addresses.push_back(std::string(hex));
    }
  }

  std::string json_string;
  ReadSymbols(hexstring, &json_string);
  if (json_string.empty()) {
    PrintErrorf("Cannot Symbolize stack trace\n");
    return E_FAIL;
  }

  std::optional<base::Value> symbolized_json =
      base::JSONReader::Read(json_string);
  if (!symbolized_json.has_value() && !symbolized_json->is_list()) {
    return E_FAIL;
  }

  base::Value::List& address_list = symbolized_json->GetList();
  if (address_list.empty()) {
    return E_FAIL;
  }

  std::map<std::string, base::Value::List*> address_symbol_map;
  for (size_t i = 0; i < address_list.size(); i++) {
    address_symbol_map[address_list[i].GetDict().Find("Address")->GetString()] =
        address_list[i].GetDict().FindList("Symbol");
  }

  for (size_t i = 0; i < stack_offset_addresses.size(); i++) {
    if (address_symbol_map.find(stack_offset_addresses[i]) ==
        address_symbol_map.end()) {
      Printf("#%d\t%s\n", i, stack_offset_addresses[i].c_str());
      continue;
    }

    base::Value::List& symbol_list =
        *(address_symbol_map[stack_offset_addresses[i]]);
    base::Value& symbol = symbol_list[symbol_list.size() - 1];
    const char* function_name =
        symbol.GetDict().Find("FunctionName")->GetString().c_str();
    const char* file_name =
        symbol.GetDict().Find("FileName")->GetString().c_str();
    int line_number = symbol.GetDict().Find("Line")->GetInt();
    Printf("#%d\t%s\t%s at %s:%d\n", i, stack_offset_addresses[i].c_str(),
           function_name, file_name, line_number);
  }
  return S_OK;
}

HRESULT GwpAsanCommand::ReadSymbols(std::string hexstring,
                                    std::string* json_string) {
  char buffer[128];
  FILE* output;
  auto argv = command_line().argv();
  if (argv.empty()) {
    return E_FAIL;
  }

  std::wstring wstr = argv.at(0);
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                        NULL, 0, NULL, NULL);
  std::string str(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0],
                      size_needed, NULL, NULL);

  std::string command_line_arg = str;

  ULONG64 position = command_line_arg.find(";");
  if (position == std::string::npos) {
    PrintErrorf("Pass in format <binary path>;<llvm symbolizer path>\n");
    return E_FAIL;
  }
  std::string chrome_path = command_line_arg.substr(0, position);
  std::ifstream chrome(chrome_path.c_str());
  if (!chrome.is_open()) {
    return E_FAIL;
  }

  std::string llvm_symbolizer_path =
      command_line_arg.substr(position + 1, command_line_arg.length());
  std::ifstream llvm_symbolizer(llvm_symbolizer_path.c_str());
  if (!llvm_symbolizer.is_open()) {
    return E_FAIL;
  }

  std::string command = llvm_symbolizer_path + " " + "--obj=\"" + chrome_path +
                        "\" " + "--output-style=JSON " + hexstring;

  if ((output = _popen(command.c_str(), "r")) == nullptr) {
    PrintErrorf("Cannot Read Symbols %s\n", command.c_str());
    return E_FAIL;
  }
  while (fgets(buffer, sizeof(buffer), output)) {
    *json_string += std::string(buffer);
  }
  if (!feof(output)) {
    PrintErrorf("Failed to read the pipe to the end.\n");
    return E_FAIL;
  }
  return S_OK;
}

HRESULT GwpAsanCommand::ReadFromDumpStream(uint32_t stream_type,
                                           std::vector<char>* buffer,
                                           uint64_t offset) {
  char temp_buffer[128];
  bool data_read = false;

  DEBUG_READ_USER_MINIDUMP_STREAM request;
  memset(&request, 0, sizeof(request));
  request.StreamType = stream_type;
  request.Buffer = temp_buffer;
  request.BufferSize = sizeof(temp_buffer);
  request.Offset = 0;

  // Read the GWP-ASan stream to verify this is a GWP-ASan dump.
  ComPtr<IDebugAdvanced3> debug_advanced;
  HRESULT hr = GetDebugClientAs<IDebugControl4>()->QueryInterface(
      IID_PPV_ARGS(&debug_advanced));
  if (FAILED(hr)) {
    PrintErrorf("QI for IDebugAdvanced3: %08X\n", hr);
    return hr;
  }

  do {
    hr =
        debug_advanced->Request(DEBUG_REQUEST_READ_USER_MINIDUMP_STREAM,
                                &request, sizeof(request), nullptr, 0, nullptr);

    if (hr == S_OK || hr == S_FALSE) {
      buffer->insert(buffer->end(), temp_buffer,
                     temp_buffer + request.BufferUsed);

      request.Offset = request.Offset + request.BufferUsed;
      data_read = true;
    }
  } while (SUCCEEDED(hr));

  if (hr == E_INVALIDARG && data_read) {
    return S_OK;
  }
  PrintErrorf("Failed To read the dump stream\n");
  return hr;
}

}  // namespace chromeexts
}  // namespace win
}  // namespace tools
