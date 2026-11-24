/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>

namespace unwindstack {

// A bit map of warnings, multiple warnings can be set at the same time.
enum WarningCode : uint64_t {
  WARNING_NONE = 0,
  WARNING_DEX_PC_NOT_IN_MAP = 0x1,  // A dex pc was found, but it doesn't exist
                                    // in any valid map.
};

enum ErrorCode : uint8_t {
  ERROR_NONE,                   // No error.
  ERROR_MEMORY_INVALID,         // Memory read failed.
  ERROR_UNWIND_INFO,            // Unable to use unwind information to unwind.
  ERROR_UNSUPPORTED,            // Encountered unsupported feature.
  ERROR_INVALID_MAP,            // Unwind in an invalid map.
  ERROR_MAX_FRAMES_EXCEEDED,    // The number of frames exceed the total allowed.
  ERROR_REPEATED_FRAME,         // The last frame has the same pc/sp as the next.
  ERROR_INVALID_ELF,            // Unwind in an invalid elf.
  ERROR_THREAD_DOES_NOT_EXIST,  // Attempt to unwind a local thread that does
                                // not exist.
  ERROR_THREAD_TIMEOUT,         // Timeout trying to unwind a local thread.
  ERROR_SYSTEM_CALL,            // System call failed while unwinding.
  ERROR_BAD_ARCH,               // Arch invalid (none, or mismatched).
  ERROR_MAPS_PARSE,             // Failed to parse maps data.
  ERROR_INVALID_PARAMETER,      // Invalid parameter passed to function.
  ERROR_PTRACE_CALL,            // Ptrace call failed while unwinding.
  ERROR_MAX = ERROR_INVALID_PARAMETER,
};

static inline const char* GetErrorCodeString(ErrorCode error) {
  switch (error) {
    case ERROR_NONE:
      return "None";
    case ERROR_MEMORY_INVALID:
      return "Memory Invalid";
    case ERROR_UNWIND_INFO:
      return "Unwind Info";
    case ERROR_UNSUPPORTED:
      return "Unsupported";
    case ERROR_INVALID_MAP:
      return "Invalid Map";
    case ERROR_MAX_FRAMES_EXCEEDED:
      return "Maximum Frames Exceeded";
    case ERROR_REPEATED_FRAME:
      return "Repeated Frame";
    case ERROR_INVALID_ELF:
      return "Invalid Elf";
    case ERROR_THREAD_DOES_NOT_EXIST:
      return "Thread Does Not Exist";
    case ERROR_THREAD_TIMEOUT:
      return "Thread Timeout";
    case ERROR_SYSTEM_CALL:
      return "System Call Failed";
    case ERROR_BAD_ARCH:
      return "Invalid arch detected";
    case ERROR_MAPS_PARSE:
      return "Failed to parse maps";
    case ERROR_INVALID_PARAMETER:
      return "Invalid parameter";
    case ERROR_PTRACE_CALL:
      return "Ptrace Call Failed";
  }
}

struct ErrorData {
  ErrorCode code;
  uint64_t address;  // Only valid when code is ERROR_MEMORY_INVALID.
                     // Indicates the failing address.
};

}  // namespace unwindstack
