// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tchar.h>
#include <windows.h>

#include <stdio.h>
#include <tlhelp32.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>

// List all thread names in a process specified.
BOOL ListProcessThreadNames(DWORD owner_pid);
// Print the error message.
void printError(TCHAR* msg);

// The GetThreadDescription API is available since Windows 10, version 1607.
// The reason why this API is bound in this way rather than just using the
// Windows SDK, is that this API isn't yet available in the SDK that Chrome
// builds with.
// Binding SetThreadDescription API in Chrome can only be done by
// GetProcAddress, rather than the import library.
typedef HRESULT(WINAPI* GETTHREADDESCRIPTION)(HANDLE hThread,
                                              PWSTR* threadDescription);

int main(void) {
  DWORD process_Id;
  std::string user_input;
  while (true) {
    std::cout
        << "\nPlease enter the process Id, or \"quit\" to end the program : ";
    std::getline(std::cin, user_input);
    // Convert the user input to lower case.
    std::transform(user_input.begin(), user_input.end(), user_input.begin(),
                   ::tolower);
    if (user_input == "quit")
      break;
    std::cout << std::endl;
    std::stringstream ss(user_input);
    if (ss >> process_Id) {
      ListProcessThreadNames(process_Id);
    } else {
      std::cout << "Input is invalid" << std::endl;
    }
    std::cout << std::endl;
  }
  return 0;
}

BOOL ListProcessThreadNames(DWORD owner_pid) {
  auto get_thread_description_func =
      reinterpret_cast<GETTHREADDESCRIPTION>(::GetProcAddress(
          ::GetModuleHandle(L"Kernel32.dll"), "GetThreadDescription"));

  if (!get_thread_description_func) {
    printError(TEXT("GetThreadDescription"));
    return (FALSE);
  }

  HANDLE thread_snapshot = INVALID_HANDLE_VALUE;
  // Take a snapshot of all running threads.
  thread_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (thread_snapshot == INVALID_HANDLE_VALUE) {
    printError(TEXT("CreateToolhelp32Snapshot"));
    return (FALSE);
  }

  THREADENTRY32 te32;
  te32.dwSize = sizeof(THREADENTRY32);

  // Retrieve information about the first thread, and exit if unsuccessful.
  if (!Thread32First(thread_snapshot, &te32)) {
    printError(TEXT("Thread32First"));
    CloseHandle(thread_snapshot);
    return (FALSE);
  }

  // Walk the thread list of the system, and display ID and name about each
  // thread associated with the process specified.
  std::cout << "thread_ID   thread_name" << std::endl;
  std::multimap<std::wstring, DWORD> name_id_map;
  do {
    if (te32.th32OwnerProcessID == owner_pid) {
      HANDLE thread_handle =
          OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
      if (thread_handle) {
        PWSTR data;
        HRESULT hr = get_thread_description_func(thread_handle, &data);
        if (SUCCEEDED(hr)) {
          std::wstring thread_name(data);
          LocalFree(data);
          name_id_map.insert(std::make_pair(thread_name, te32.th32ThreadID));
        } else {
          printError(TEXT("GetThreadDescription"));
        }
        CloseHandle(thread_handle);
      } else {
        printError(TEXT("OpenThread"));
      }
    }
  } while (Thread32Next(thread_snapshot, &te32));

  // Clean up the snapshot object.
  CloseHandle(thread_snapshot);

  // Show all thread ID/name pairs.
  for (auto name_id_pair : name_id_map) {
    std::cout << name_id_pair.second << "\t";
    std::wcout << name_id_pair.first << std::endl;
  }

  return (TRUE);
}

void printError(TCHAR* msg) {
  DWORD eNum;
  TCHAR sysMsg[256];
  TCHAR* p;

  eNum = GetLastError();
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, eNum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), sysMsg,
                256, NULL);

  // Trim the end of the line and terminate it with a null.
  p = sysMsg;
  while ((*p > 31) || (*p == 9))
    ++p;
  do {
    *p-- = 0;
  } while ((p >= sysMsg) && ((*p == '.') || (*p < 33)));

  // Display the message.
  _tprintf(TEXT("\n  WARNING: %s failed with error %d (%s)"), msg, eNum,
           sysMsg);
}
