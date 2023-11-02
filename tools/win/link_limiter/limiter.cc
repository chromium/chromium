// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

typedef std::basic_string<TCHAR> tstring;

namespace {
  const bool g_is_debug = (_wgetenv(L"LIMITER_DEBUG") != NULL);
}

// Don't use stderr for errors because VS has large buffers on them, leading
// to confusing error output.
static void Error(const wchar_t* msg, ...) {
  tstring new_msg = tstring(L"limiter fatal error: ") + msg + L"\n";
  va_list args;
  va_start(args, msg);
  vwprintf(new_msg.c_str(), args);
  va_end(args);
}

static void Warn(const wchar_t* msg, ...) {
  if (!g_is_debug)
    return;
  tstring new_msg = tstring(L"limiter warning: ") + msg + L"\n";
  va_list args;
  va_start(args, msg);
  vwprintf(new_msg.c_str(), args);
  va_end(args);
}

static tstring ErrorMessageToString(DWORD err) {
  TCHAR* msg_buf = NULL;
  DWORD rc = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL,   // lpSource
      err,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPTSTR>(&msg_buf),
      0,      // nSize
      NULL);  // Arguments
  if (!rc)
    return L"unknown error";
  tstring ret(msg_buf);
  LocalFree(msg_buf);
  return ret;
}

static DWORD RunExe(const tstring& exe_name) {
  STARTUPINFO startup_info = { sizeof(STARTUPINFO) };
  PROCESS_INFORMATION process_info;
  DWORD exit_code;

  GetStartupInfo(&startup_info);
  tstring cmdline = tstring(GetCommandLine());

  size_t first_space = cmdline.find(' ');
  if (first_space == -1) {
    // I'm not sure why this would ever happen, but just in case...
    cmdline = exe_name;
  } else {
    cmdline = exe_name  + cmdline.substr(first_space);
  }

  if (!CreateProcess(NULL,  // lpApplicationName
                     &cmdline[0],
                     NULL,  // lpProcessAttributes
                     NULL,  // lpThreadAttributes
                     TRUE,  // bInheritHandles
                     0,     // dwCreationFlags,
                     NULL,  // lpEnvironment,
                     NULL,  // lpCurrentDirectory,
                     &startup_info,
                     &process_info)) {
    Error(L"Error in CreateProcess[%s]: %s",
          cmdline.c_str(), ErrorMessageToString(GetLastError()).c_str());
    return MAXDWORD;
  }
  CloseHandle(process_info.hThread);
  WaitForSingleObject(process_info.hProcess, INFINITE);
  GetExitCodeProcess(process_info.hProcess, &exit_code);
  CloseHandle(process_info.hProcess);
  return exit_code;
}

// Returns 0 if there was an error
static int CpuConcurrencyMetric(const tstring& envvar_name) {
  int max_concurrent = 0;
  std::vector<char> buffer(1);
  BOOL ok = false;
  DWORD last_error = 0;
  do {
    DWORD bufsize = buffer.size();
    ok = GetLogicalProcessorInformation(
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(&buffer[0]),
        &bufsize);
    last_error = GetLastError();
    if (!ok && last_error == ERROR_INSUFFICIENT_BUFFER &&
        bufsize > buffer.size()) {
      buffer.resize(bufsize);
    }
  } while (!ok && last_error == ERROR_INSUFFICIENT_BUFFER);

  if (!ok) {
    Warn(L"Error while getting number of cores. Try setting the "
         L" environment variable '%s'  to (num_cores - 1): %s",
         envvar_name.c_str(), ErrorMessageToString(last_error).c_str());
    return 0;
  }

  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pproc_info =
      reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(&buffer[0]);
  int num_entries = buffer.size() /
      sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

  for (int i = 0; i < num_entries; ++i) {
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION& info = pproc_info[i];
    if (info.Relationship == RelationProcessorCore) {
      ++max_concurrent;
    }
  }

  // Leave one core for other tasks
  return max_concurrent - 1;
}

// TODO(defaults): Create a better heuristic than # of CPUs. It seems likely
// that the right value will, in fact, be based on the memory capacity of the
// machine, not on the number of CPUs.
enum ConcurrencyMetricEnum {
  CONCURRENCY_METRIC_ONE,
  CONCURRENCY_METRIC_CPU,
  CONCURRENCY_METRIC_DEFAULT = CONCURRENCY_METRIC_CPU
};

static int GetMaxConcurrency(const tstring& base_pipename,
                             ConcurrencyMetricEnum metric) {
  static int max_concurrent = -1;

  if (max_concurrent == -1) {
    tstring envvar_name = base_pipename + L"_MAXCONCURRENCY";

    const LPTSTR max_concurrent_str = _wgetenv(envvar_name.c_str());
    max_concurrent = max_concurrent_str ? _wtoi(max_concurrent_str) : 0;

    if (max_concurrent == 0) {
      switch (metric) {
        case CONCURRENCY_METRIC_CPU:
          max_concurrent = CpuConcurrencyMetric(envvar_name);
          if (max_concurrent)
            break;
          // else fall through
        case CONCURRENCY_METRIC_ONE:
          max_concurrent = 1;
          break;
      }
    }

    max_concurrent = std::min(std::max(max_concurrent, 1),
                              PIPE_UNLIMITED_INSTANCES);
  }

  return max_concurrent;
}

static HANDLE WaitForPipe(const tstring& pipename,
                          HANDLE event,
                          int max_concurrency) {
  // We're using a named pipe instead of a semaphore so the Kernel can clean up
  // after us if we crash while holding onto the pipe (A real semaphore will
  // not release on process termination).
  HANDLE pipe = INVALID_HANDLE_VALUE;
  for (;;) {
    pipe = CreateNamedPipe(
        pipename.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE,
        max_concurrency,
        1,      // nOutBufferSize
        1,      // nInBufferSize
        0,      // nDefaultTimeOut
        NULL);  // Default security attributes (noinherit)
    if (pipe != INVALID_HANDLE_VALUE)
      break;

    DWORD error = GetLastError();
    if (error == ERROR_PIPE_BUSY) {
      if (event) {
        WaitForSingleObject(event, 60 * 1000 /* ms */);
      } else {
        // TODO(iannucci): Maybe we should error out here instead of falling
        // back to a sleep-poll
        Sleep(5 * 1000 /* ms */);
      }
    } else {
      Warn(L"Got error %d while waiting for pipe: %s", error,
           ErrorMessageToString(error).c_str());
      return INVALID_HANDLE_VALUE;
    }
  }

  return pipe;
}

static int WaitAndRun(const tstring& shimmed_exe,
                      const tstring& base_pipename) {
  ULONGLONG start_time = 0, end_time = 0;
  tstring pipename = L"\\\\.\\pipe\\" + base_pipename;
  tstring event_name = L"Local\\EVENT_" + base_pipename;

  // This event lets us do better than strict polling, but we don't rely on it
  // (in case a process crashes before signalling the event).
  HANDLE event = CreateEvent(
      NULL,   // Default security attributes
      FALSE,  // Manual reset
      FALSE,  // Initial state
      event_name.c_str());

  if (g_is_debug)
    start_time = GetTickCount64();

  HANDLE pipe =
    WaitForPipe(pipename, event,
                GetMaxConcurrency(base_pipename, CONCURRENCY_METRIC_DEFAULT));

  if (g_is_debug) {
    end_time = GetTickCount64();
    wprintf(L"  took %.2fs to acquire semaphore.\n",
        (end_time - start_time) / 1000.0);
  }

  DWORD ret = RunExe(shimmed_exe);

  if (pipe != INVALID_HANDLE_VALUE)
    CloseHandle(pipe);
  if (event != NULL)
    SetEvent(event);

  return ret;
}

void Usage(const tstring& msg) {
  tstring usage(msg);
  usage += L"\n"
           L"Usage: SHIMED_NAME__SEMAPHORE_NAME\n"
           L"\n"
           L"  SHIMMED_NAME   - ex. 'link.exe' or 'lib.exe'\n"
           L"                 - can be exe, bat, or com\n"
           L"                 - must exist in PATH\n"
           L"\n"
           L"  SEMAPHORE_NAME - ex. 'SOME_NAME' or 'GROOVY_SEMAPHORE'\n"
           L"\n"
           L"  Example:\n"
           L"    link.exe__LINK_LIMITER.exe\n"
           L"    lib.exe__LINK_LIMITER.exe\n"
           L"      * Both will limit on the same semaphore\n"
           L"\n"
           L"    link.exe__LINK_LIMITER.exe\n"
           L"    lib.exe__LIB_LIMITER.exe\n"
           L"      * Both will limit on independent semaphores\n"
           L"\n"
           L"  This program is meant to be run after renaming it into the\n"
           L"  above format. Once you have done so, executing it will block\n"
           L"  on the availability of the semaphore SEMAPHORE_NAME. Once\n"
           L"  the semaphore is obtained, it will execute SHIMMED_NAME, \n"
           L"  passing through all arguments as-is.\n"
           L"\n"
           L"  The maximum concurrency can be manually set by setting the\n"
           L"  environment variable <SEMAPHORE_NAME>_MAXCONCURRENCY to an\n"
           L"  integer value (1, 254).\n"
           L"    * This value must be set the same for ALL invocations.\n"
           L"    * If the value is not set, it defaults to (num_cores-1).\n"
           L"\n"
           L"  The semaphore is automatically released when the program\n"
           L"  completes normally, OR if the program crashes (or even if\n"
           L"  limiter itself crashes).\n";
  Error(usage.c_str());
  exit(-1);
}

// Input command line is assumed to be of the form:
//
// thing.exe__PIPE_NAME.exe ...
//
// Specifically, wait for a semaphore (whose concurrency is specified by
// LIMITER_MAXCONCURRENT), and then pass through everything once we have
// acquired the semaphore.
//
// argv[0] is parsed for:
//   * exe_to_shim_including_extension.exe
//     * This could also be a bat or com. Anything that CreateProcess will
//       accept.
//   * "__"
//     * We search for this separator from the end of argv[0], so the exe name
//       could contain a double underscore if necessary.
//   * PIPE_NAME
//     * Can only contain single underscores, not a double underscore.
//     * i.e. HELLO_WORLD_PIPE will work, but HELLO__WORLD_PIPE will not.
//     * This would allow the shimmed exe to contain arbitrary numbers of
//       underscores. We control the pipe name, but not necessarily the thing
//       we're shimming.
//
int wmain(int, wchar_t** argv) {
  tstring shimmed_plus_pipename = argv[0];
  size_t last_slash = shimmed_plus_pipename.find_last_of(L"/\\");
  if (last_slash != tstring::npos) {
    shimmed_plus_pipename = shimmed_plus_pipename.substr(last_slash + 1);
  }

  size_t separator = shimmed_plus_pipename.rfind(L"__");
  if (separator == tstring::npos) {
    Usage(L"Cannot parse argv[0]. No '__' found. "
          L"Should be like '[...(\\|/)]link.exe__PIPE_NAME.exe'");
  }
  tstring shimmed_exe   = shimmed_plus_pipename.substr(0, separator);
  tstring base_pipename = shimmed_plus_pipename.substr(separator + 2);

  size_t dot = base_pipename.find(L'.');
  if (dot == tstring::npos) {
    Usage(L"Expected an executable extension in argv[0]. No '.' found.");
  }
  base_pipename = base_pipename.substr(0, dot);

  return WaitAndRun(shimmed_exe, base_pipename);
}

