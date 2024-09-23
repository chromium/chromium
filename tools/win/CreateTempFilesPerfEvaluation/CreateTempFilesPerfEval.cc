// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tchar.h>
#include <windows.h>

#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

#include "base/win/shlwapi.h"

#pragma warning(disable : 4996)

// Create |count| number of temp files at |folder_path| using GUID based method.
// The time cost in millisecond of creating every 500 temp files or all the
// files if |count| < 500 is printed to the console.
bool CreateFilesUsingGuid(UINT count, const char* folder_path);

// Create |count| number of temp files at |folder_path| using GetTempFileName()
// API. The time cost in millisecond of creating every 500 temp files or all the
// files if |count| < 500 is printed to the console.
bool CreateFilesUsingGetTempFileName(UINT count, const char* folder_path);

// This method converts GUID to a string.
char* ConvertGuidToString(const GUID* id, char* out);

// If |folder_path| doesn't exist, creat it, otherwise check if it is empty.
bool CreateOrValidateTempDirectory(const char* folder_path);

// Deletes all the content in |folder_path|.
void DeleteDirectoryContent(const char* folder_path);

// Deletes |folder_path| including its contents and the raw directory.
bool DeleteDirectory(const char* folder_path);

// Deletes |folder_path| including its contents and the raw directory, and print
// the delete status to the console.
void DeleteDirectoryAndPrintMsg(const char* folder_path);

// Prints the elapsed time at current step for the latest cycle.
void FormatPrintElapsedTime(UINT cur_step,
                            UINT total_step,
                            const LARGE_INTEGER& elapsed_ms);

// Maximum number of temp files allowed to create. This is limited by the
// implementation of GetTempFileName().
// "This limits GetTempFileName to a maximum of 65,535 unique file names if the
// lpPathName and lpPrefixString parameters remain the same."
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa364991(v=vs.85).aspx
UINT kMaxFileCreate = 65535;

// Query the time cost each time when this amount of temp files are created.
UINT kFileCountPerMetric = 500;

int main() {
  // Gets the temp path env string.
  DWORD temp_path_ret = 0;
  CHAR temp_folder_path[MAX_PATH];
  temp_path_ret = ::GetTempPathA(MAX_PATH, temp_folder_path);
  if (temp_path_ret > MAX_PATH || temp_path_ret == 0) {
    std::cout << "GetTempPath failed" << std::endl;
    return 0;
  }

  // A temporary directory where the new temp files created by GetTempFileName()
  // are written.
  std::string temp_dir_gettempfilename(
      std::string(temp_folder_path).append("TempDirGetTempFileName\\"));

  // A temporary directory where the new temp files created by Guid-based method
  // are written.
  std::string temp_dir_guid(
      std::string(temp_folder_path).append("TempDirGuid\\"));

  UINT file_create_count;
  std::string user_input;

  while (true) {
    std::cout << "\nPlease enter # of files to create (maximum "
              << kMaxFileCreate << "), or \"quit\" to end the program : ";
    std::getline(std::cin, user_input);

    std::transform(user_input.begin(), user_input.end(), user_input.begin(),
                   ::tolower);
    if (user_input == "quit")
      break;

    std::cout << std::endl;
    std::stringstream ss(user_input);

    if (ss >> file_create_count && file_create_count <= kMaxFileCreate) {
      std::cout << "\nPlease select method to create temp file names,\n"
                << "\"t\" for GetTempFileName \n"
                << "\"g\" for GUID-based \n"
                << "\"b\" for both \n"
                << "or \"quit\" to end the program : ";
      std::getline(std::cin, user_input);

      std::transform(user_input.begin(), user_input.end(), user_input.begin(),
                     ::tolower);
      if (user_input == "quit")
        break;

      if (user_input == "t" || user_input == "b") {
        std::cout << "\nGetTempFileName Performance:\n [start -   end] / total "
                     "--- time "
                     "cost in ms"
                  << std::endl;
        if (CreateFilesUsingGetTempFileName(file_create_count,
                                            temp_dir_gettempfilename.c_str())) {
          std::cout << "File creation succeeds at " << temp_dir_gettempfilename
                    << ", now clean all of them!" << std::endl;
        }
        DeleteDirectoryAndPrintMsg(temp_dir_gettempfilename.c_str());
      }

      if (user_input == "g" || user_input == "b") {
        std::cout << "\nGUID-based Performance:\n [start -   end] / total --- "
                     "time cost in ms"
                  << std::endl;
        if (CreateFilesUsingGuid(file_create_count, temp_dir_guid.c_str())) {
          std::cout << "File creation succeeds at " << temp_dir_guid
                    << ", now clean all of them!" << std::endl;
        }
        DeleteDirectoryAndPrintMsg(temp_dir_guid.c_str());
      }
    } else {
      std::cout << "Input number is invalid, please enter # of files to create "
                   "(maximum "
                << kMaxFileCreate << "), or \"quit\" to end the program : ";
    }
    std::cout << std::endl;
  }
  return 0;
}

bool CreateFilesUsingGuid(UINT count, const char* dir_path) {
  if (!CreateOrValidateTempDirectory(dir_path))
    return false;

  LARGE_INTEGER starting_time, ending_time, elapsed_ms;
  ::QueryPerformanceCounter(&starting_time);
  LARGE_INTEGER frequency;
  ::QueryPerformanceFrequency(&frequency);

  for (UINT i = 1; i <= count; ++i) {
    GUID guid;
    ::CoCreateGuid(&guid);
    char buffer[37];
    ConvertGuidToString(&guid, buffer);
    std::string temp_name = std::string(dir_path).append(buffer).append(".tmp");

    HANDLE file_handle =
        ::CreateFileA(temp_name.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ::CloseHandle(file_handle);

    if (i % kFileCountPerMetric == 0 || i == count) {
      ::QueryPerformanceCounter(&ending_time);
      // Convert the elapsed number of ticks to milliseconds.
      elapsed_ms.QuadPart = (ending_time.QuadPart - starting_time.QuadPart) *
                            1000 / frequency.QuadPart;

      FormatPrintElapsedTime(i, count, elapsed_ms);

      ::QueryPerformanceCounter(&starting_time);
    }
  }
  return true;
}

bool CreateFilesUsingGetTempFileName(UINT count, const char* dir_path) {
  if (!CreateOrValidateTempDirectory(dir_path))
    return false;

  CHAR temp_name[MAX_PATH];
  LARGE_INTEGER starting_time, ending_time, elapsed_ms;
  ::QueryPerformanceCounter(&starting_time);
  LARGE_INTEGER frequency;
  ::QueryPerformanceFrequency(&frequency);

  for (UINT i = 1; i <= count; ++i) {
    ::GetTempFileNameA(dir_path, "", 0, temp_name);
    if (i % kFileCountPerMetric == 0 || i == count) {
      ::QueryPerformanceCounter(&ending_time);
      // Convert the elapsed number of ticks to milliseconds.
      elapsed_ms.QuadPart = (ending_time.QuadPart - starting_time.QuadPart) *
                            1000 / frequency.QuadPart;

      FormatPrintElapsedTime(i, count, elapsed_ms);

      ::QueryPerformanceCounter(&starting_time);
    }
  }
  return true;
}

char* ConvertGuidToString(const GUID* id, char* out) {
  int i;
  char* ret = out;
  out += sprintf(out, "%.8lX-%.4hX-%.4hX-", id->Data1, id->Data2, id->Data3);
  for (i = 0; i < sizeof(id->Data4); ++i) {
    out += sprintf(out, "%.2hhX", id->Data4[i]);
    if (i == 1)
      *(out++) = '-';
  }
  return ret;
}

bool CreateOrValidateTempDirectory(const char* folder_path) {
  if (::PathFileExistsA(folder_path)) {
    if (!::PathIsDirectoryEmptyA(folder_path)) {
      std::cout << folder_path
                << " directory is not empty, please remove all its content.";
      return false;
    }
    return true;
  } else if (::CreateDirectoryA(folder_path, NULL) == 0) {
    std::cout << folder_path << "directory creation fails.";
    return false;
  } else {
    return true;
  }
}

void DeleteDirectoryContent(const char* folder_path) {
  char file_found[MAX_PATH];
  WIN32_FIND_DATAA info;
  HANDLE hp;
  sprintf(file_found, "%s\\*.*", folder_path);
  hp = ::FindFirstFileA(file_found, &info);
  do {
    if ((strcmp(info.cFileName, ".") == 0) ||
        (strcmp(info.cFileName, "..") == 0)) {
      continue;
    }
    sprintf(file_found, "%s\\%s", folder_path, info.cFileName);
    ::DeleteFileA(file_found);
  } while (::FindNextFileA(hp, &info));
  ::FindClose(hp);
}

bool DeleteDirectory(const char* folder_path) {
  DeleteDirectoryContent(folder_path);
  return ::RemoveDirectoryA(folder_path) != 0;
}

void DeleteDirectoryAndPrintMsg(const char* folder_path) {
  if (DeleteDirectory(folder_path)) {
    std::cout << folder_path << " directory is deleted!" << std::endl;
  } else {
    std::cout << "[Attention] " << folder_path
              << " directory's deletion fails, please take a look by yourself!"
              << std::endl;
  }
}

void FormatPrintElapsedTime(UINT cur_step,
                            UINT total_step,
                            const LARGE_INTEGER& elapsed_ms) {
  UINT count_prev = 0;
  if (cur_step % kFileCountPerMetric == 0)
    count_prev = cur_step + 1 - kFileCountPerMetric;
  else if (cur_step > kFileCountPerMetric)
    count_prev = cur_step / kFileCountPerMetric * kFileCountPerMetric + 1;
  printf(" [%5d - %5d] / %d --- %lld\n", count_prev, cur_step, total_step,
         elapsed_ms.QuadPart);
}
