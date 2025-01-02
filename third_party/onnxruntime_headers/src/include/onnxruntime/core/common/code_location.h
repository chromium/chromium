// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace onnxruntime {
/**
   CodeLocation captures information on where in the source code a message came from.
*/
struct CodeLocation {
  /**
     @param file_path Usually the value of __FILE__
     @param line Usually the value of __LINE__
     @param func Usually the value of __PRETTY_FUNCTION__ or __FUNCTION__
  */
  CodeLocation(const char* file_path, const int line, const char* func)
      : file_and_path{file_path}, line_num{line}, function{func} {
  }

  /**
     @param file_path Usually the value of __FILE__
     @param line Usually the value of __LINE__
     @param func Usually the value of __PRETTY_FUNCTION__ or __FUNCTION__
     @param stacktrace Stacktrace from source of message.
  */
  CodeLocation(const char* file_path, const int line, const char* func, const std::vector<std::string>& stacktrace)
      : file_and_path{file_path}, line_num{line}, function{func}, stacktrace(stacktrace) {
  }

  std::string FileNoPath() const {
    // assuming we always have work to do, so not trying to avoid creating a new string if
    // no path was removed.
    return file_and_path.substr(file_and_path.find_last_of("/\\") + 1);
  }

  enum Format {
    kFilename,
    kFilenameAndPath
  };

  std::string ToString(Format format = Format::kFilename) const {
    std::ostringstream out;
    out << (format == Format::kFilename ? FileNoPath() : file_and_path) << ":" << line_num << " " << function;
    return out.str();
  }
  // utf-8. Because on Windows we compile our code with "/utf-8". And we assume the other platforms only use utf-8.
  const std::string file_and_path;
  const int line_num;
  // utf-8
  const std::string function;
  const std::vector<std::string> stacktrace;
};

}  // namespace onnxruntime
