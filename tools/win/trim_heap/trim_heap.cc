// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is an experimental tool which will inject a thread into a Chrome
// process (tested on the browser process) and run code to call
// HeapSetInformation with HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION. This
// tells Windows to trim unnecessary memory from the heaps in that process.
//
// This tool uses sketchy techniques such as copying memory from one
// executable to another (only works if the code is relocatable and has no
// external references), VirtualAllocEx, and CreateRemoteThread. This is not
// for production use.
//
// The bitness of this tool (32/64) must match that of the target process.
// This tool has only been tested on 64-bit processes. This tool only works
// when compiled with optimizations.
//
// Some error handling and resource cleanup is omitted in order to keep things
// simple.

#include <Windows.h>

// Psapi.h must come after Windows.h.
#include <Psapi.h>

#include <inttypes.h>
#include <stdio.h>

#include <vector>

#ifdef _DEBUG
#error This code only works in optimized (release) builds.
// Non-optimized code may include references to global variables. The
// "#pragma clang optimize on/off" directives do not work, by design, in debug
// builds. They can only lower the optimization level, not raise it.
#endif

#define ADDRESS_COOKIE reinterpret_cast<void*>(0x123456789ABCDEF0)

// Function suitable for copying into another process and invoking with
// CreateRemoteThread. The function address is a placeholder.
DWORD WINAPI ShrinkHeapThread(LPVOID) {
  auto pHeapSetInformation =
      reinterpret_cast<decltype(&::HeapSetInformation)>(ADDRESS_COOKIE);
  HEAP_OPTIMIZE_RESOURCES_INFORMATION info = {
      HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION, 0x0};
  pHeapSetInformation(nullptr, HeapOptimizeResources, &info, sizeof(info));
  return 0;
}

int main(int argc, char* argv[]) {
  const bool verbose = false;

  // Verify that we have the correct signature for ShrinkHeapThread.
  static_assert(
      std::is_same<decltype(ShrinkHeapThread)*, PTHREAD_START_ROUTINE>::value,
      "Callback function is wrong type.");

  // Copy the thread function's memory to a vector.
  std::vector<unsigned char> raw_bytes;
  auto* src = reinterpret_cast<uint8_t*>(&ShrinkHeapThread);
  // Assume that the only 0xc3 byte we will encounter will be the ret
  // instruction.
  uint8_t ret = 0xc3;
  while (*src != ret) {
    raw_bytes.push_back(*src++);
  }
  raw_bytes.push_back(ret);
  if (src[1] != 0xcc) {
    printf("Didn't find int 3 after ret. Exiting.\n");
    return 1;
  }
  // This can trigger if incremental linking is enabled since then the function
  // pointer will be to a JMP stub.
  if (raw_bytes.size() > 1000) {
    printf("Code size is suspiciously large - %zu bytes. Exiting.\n",
           raw_bytes.size());
    return 1;
  }

  // Update the function pointer address in the copy to match the current
  // address of HeapSetInformation. This assumes that the address will be the
  // same in all processes, which should be the case.
  for (auto* scan = &raw_bytes[0]; /**/; ++scan) {
    auto** scan_64 = reinterpret_cast<void**>(scan);
    if (*scan_64 == ADDRESS_COOKIE) {
      auto* pHeapSetInformation = reinterpret_cast<void*>(GetProcAddress(
          GetModuleHandleA("kernel32.dll"), "HeapSetInformation"));
      *scan_64 = pHeapSetInformation;
      if (verbose)
        printf("Found and updated HeapSetInformation.\n");
      break;
    }
  }

  if (argc < 2) {
    printf("Usage: %s PID.\n", argv[0]);
    printf(
        "Injects code into the target process to call HeapSetInformation with "
        "HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION.\n");
    printf(
        "May need to be run from an administrator command prompt for some "
        "processes.\n");
    return 1;
  }

  // Get the PIDs from the command line.
  for (int i = 1; i < argc; ++i) {
    int PID;
    if (sscanf(argv[i], "%d", &PID) != 1) {
      printf("Error getting PID.\n");
      return 1;
    }

    // Open the process. We'll leak the handle afterwards, but that's okay
    // because this is a short-lived tool.
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
                                      PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                                      PROCESS_CREATE_THREAD,
                                  false, PID);
    if (!hProcess) {
      printf("Error from OpenProcess is %lx.\n", GetLastError());
      return 1;
    }

#ifdef _M_X64
    BOOL wow_64_process = FALSE;
    if (!IsWow64Process(hProcess, &wow_64_process) || wow_64_process) {
      printf("Specified process is 32-bit. Code injection will not work.\n");
      return 1;
    }
#else
    // Update this with remote-process bitness tests if x86 works.
#error This code is only tested on x64 and may cause failures on x86.
#endif

    PROCESS_MEMORY_COUNTERS_EX memory_before = {sizeof(memory_before)};
    GetProcessMemoryInfo(
        hProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory_before),
        sizeof(memory_before));

    // Allocate memory in the other process.
    void* p = VirtualAllocEx(hProcess, nullptr, raw_bytes.size(),
                             MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (verbose)
      printf("Writing %zd bytes to process %d at address 0x%p.\n",
             raw_bytes.size(), PID, p);
    // Write to the remotely allocated memory.
    SIZE_T bytes_written = 0;
    if (!WriteProcessMemory(hProcess, p, &raw_bytes[0], raw_bytes.size(),
                            &bytes_written)) {
      printf("Error is %lx.\n", GetLastError());
      return 1;
    }

    if (verbose)
      printf("Wrote %zd bytes.\n", bytes_written);
    HANDLE hRemoteThread = CreateRemoteThread(
        hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(p),
        nullptr, 0, nullptr);
    if (!hRemoteThread) {
      printf("Failed to inject thread in process %d. Error code is %lx.\n", PID,
             GetLastError());
      return 1;
    }

    if (verbose)
      printf("Successfully injected thread into process %d.\n", PID);
    WaitForSingleObject(hRemoteThread, INFINITE);
    // Clean up the allocated memory after the thread exits.
    VirtualFreeEx(hProcess, p, 0, MEM_RELEASE);

    PROCESS_MEMORY_COUNTERS_EX memory_after = {sizeof(memory_after)};
    GetProcessMemoryInfo(
        hProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory_after),
        sizeof(memory_after));

    double MiB = 1024.0 * 1024.0;
    printf(
        "  Commit for process %6d went from %8.3f MiB to %8.3f MiB (%7.3f MiB "
        "savings).\n",
        PID, memory_before.PrivateUsage / MiB, memory_after.PrivateUsage / MiB,
        (memory_before.PrivateUsage - memory_after.PrivateUsage) / MiB);
  }
  return 0;
}
